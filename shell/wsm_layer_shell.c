#include "wsm_layer_shell.h"
#include "wsm_server.h"
#include "wsm_log.h"
#include "wsm_scene.h"
#include "wsm_seat.h"
#include "wsm_output.h"
#include "wsm_cursor.h"
#include "wsm_arrange.h"
#include "wsm_transaction.h"
#include "wsm_input_manager.h"
#include "wsm_workspace.h"
#include "wsm_layer_popup.h"
#include "node/wsm_node_descriptor.h"

#include <stdlib.h>

#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_subcompositor.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/types/wlr_fractional_scale_v1.h>

#define WSM_LAYER_SHELL_VERSION 4

static struct wlr_scene_tree *wsm_layer_get_scene(struct wsm_output *output,
		enum zwlr_layer_shell_v1_layer type) {
	switch (type) {
	case ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND:
		return output->layers.shell_background;
	case ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM:
		return output->layers.shell_bottom;
	case ZWLR_LAYER_SHELL_V1_LAYER_TOP:
		return output->layers.shell_top;
	case ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY:
		return output->layers.shell_overlay;
	}

	return NULL;
}

static struct wsm_layer_surface *wsm_layer_surface_create(
		struct wlr_scene_layer_surface_v1 *scene) {
	struct wsm_layer_surface *surface = calloc(1, sizeof(struct wsm_layer_surface));
	if (!surface) {
		wsm_log(WSM_ERROR, "Could not create wsm_layer_surface: allocation failed!");
		return NULL;
	}

	struct wlr_scene_tree *popups = wlr_scene_tree_create(global_server.scene->layers.popup);
	if (!popups) {
		wsm_log(WSM_ERROR, "Could not allocate a scene_layer popup node");
		free(surface);
		return NULL;
	}

	surface->desc.relative = &scene->tree->node;

	if (!wsm_scene_descriptor_assign(&popups->node,
			WSM_SCENE_DESC_POPUP, &surface->desc)) {
		wsm_log(WSM_ERROR, "Failed to allocate a popup scene descriptor");
		wlr_scene_node_destroy(&popups->node);
		free(surface);
		return NULL;
	}

	surface->tree = scene->tree;
	surface->scene = scene;
	surface->layer_surface_wlr = scene->layer_surface;
	surface->popups = popups;
	surface->layer_surface_wlr->data = surface;

	return surface;
}

static void handle_surface_commit(struct wl_listener *listener, void *data) {
	struct wsm_layer_surface *surface =
		wl_container_of(listener, surface, surface_commit);

	struct wlr_layer_surface_v1 *layer_surface = surface->layer_surface_wlr;
	if (!layer_surface->initialized) {
		return;
	}

	uint32_t committed = layer_surface->current.committed;
	if (committed & WLR_LAYER_SURFACE_V1_STATE_LAYER) {
		enum zwlr_layer_shell_v1_layer layer_type = layer_surface->current.layer;
		struct wlr_scene_tree *output_layer = wsm_layer_get_scene(
			surface->output, layer_type);
		wlr_scene_node_reparent(&surface->scene->tree->node, output_layer);
	}

	if (layer_surface->initial_commit || committed ||
			layer_surface->surface->mapped != surface->mapped) {
		surface->mapped = layer_surface->surface->mapped;
		wsm_arrange_layers(surface->output);
		transaction_commit_dirty();
	}
}

static void handle_map(struct wl_listener *listener, void *data) {
	struct wsm_layer_surface *surface = wl_container_of(listener, surface, map);
	struct wlr_layer_surface_v1 *layer_surface =
		surface->scene->layer_surface;

	if (layer_surface->current.keyboard_interactive &&
			(layer_surface->current.layer == ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY ||
			layer_surface->current.layer == ZWLR_LAYER_SHELL_V1_LAYER_TOP)) {
		struct wsm_seat *seat;
		wl_list_for_each(seat, &global_server.input_manager->seats, link) {
			if (!seat->focused_layer_wlr ||
				seat->focused_layer_wlr->current.layer >= layer_surface->current.layer) {
				seat_set_focus_layer(seat, layer_surface);
			}
		}
		wsm_arrange_layers(surface->output);
	}

	cursor_rebase_all();
}

static void handle_unmap(struct wl_listener *listener, void *data) {
	struct wsm_layer_surface *surface = wl_container_of(
		listener, surface, unmap);
	struct wsm_seat *seat;
	wl_list_for_each(seat, &global_server.input_manager->seats, link) {
		if (seat->focused_layer_wlr == surface->layer_surface_wlr) {
			seat_set_focus_layer(seat, NULL);
		}
	}

	cursor_rebase_all();
}

static void handle_new_popup(struct wl_listener *listener, void *data) {
	struct wsm_layer_surface *wsm_layer_surface =
		wl_container_of(listener, wsm_layer_surface, new_popup);
	struct wlr_xdg_popup *wlr_popup = data;
	wsm_layer_popup_create(wlr_popup, wsm_layer_surface, wsm_layer_surface->popups);
}

static void handle_output_destroy(struct wl_listener *listener, void *data) {
	struct wsm_layer_surface *layer =
		wl_container_of(listener, layer, output_destroy);

	layer->output = NULL;
	wlr_scene_node_destroy(&layer->scene->tree->node);
}

static struct wsm_layer_surface *find_mapped_layer_by_client(
	struct wl_client *client, struct wsm_output *ignore_output) {
	for (int i = 0; i < global_server.scene->outputs->length; ++i) {
		struct wsm_output *output = global_server.scene->outputs->items[i];
		if (output == ignore_output) {
			continue;
		}

		struct wlr_scene_node *node;
		wl_list_for_each (node, &output->layers.shell_overlay->children, link) {
			struct wsm_layer_surface *surface = wsm_scene_descriptor_try_get(node,
				WSM_SCENE_DESC_LAYER_SHELL);
			if (!surface) {
				continue;
			}

			struct wlr_layer_surface_v1 *layer_surface = surface->layer_surface_wlr;
			struct wl_resource *resource = layer_surface->resource;
			if (wl_resource_get_client(resource) == client
					&& layer_surface->surface->mapped) {
				return surface;
			}
		}
	}

	return NULL;
}

static void handle_node_destroy(struct wl_listener *listener, void *data) {
	struct wsm_layer_surface *layer =
		wl_container_of(listener, layer, node_destroy);

	wsm_scene_descriptor_destroy(&layer->tree->node, WSM_SCENE_DESC_LAYER_SHELL);
	struct wsm_seat *seat = input_manager_get_default_seat();
	struct wl_client *client =
		wl_resource_get_client(layer->layer_surface_wlr->resource);
	if (!global_server.session_lock.lock) {
		struct wsm_layer_surface *consider_layer =
			find_mapped_layer_by_client(client, layer->output);
		if (consider_layer) {
			seat_set_focus_layer(seat, consider_layer->layer_surface_wlr);
		}
	}

	if (layer->output) {
		wsm_arrange_layers(layer->output);
		transaction_commit_dirty();
	}

	wlr_scene_node_destroy(&layer->popups->node);

	wl_list_remove(&layer->map.link);
	wl_list_remove(&layer->unmap.link);
	wl_list_remove(&layer->surface_commit.link);
	wl_list_remove(&layer->node_destroy.link);
	wl_list_remove(&layer->output_destroy.link);

	layer->layer_surface_wlr->data = NULL;
	free(layer);
}

void handle_layer_shell_surface(struct wl_listener *listener, void *data) {
	struct wlr_layer_surface_v1 *layer_surface = data;
	wsm_log(WSM_DEBUG, "new layer surface: namespace %s layer %d anchor %" PRIu32
		" size %" PRIu32 "x%" PRIu32 " margin %" PRIu32 ",%" PRIu32 ",%" PRIu32 ",%" PRIu32 ",",
		layer_surface->namespace,
		layer_surface->pending.layer,
		layer_surface->pending.anchor,
		layer_surface->pending.desired_width,
		layer_surface->pending.desired_height,
		layer_surface->pending.margin.top,
		layer_surface->pending.margin.right,
		layer_surface->pending.margin.bottom,
		layer_surface->pending.margin.left);

	if (!layer_surface->output) {
		struct wsm_output *output = NULL;
		struct wsm_seat *seat = input_manager_get_default_seat();
		if (seat) {
			struct wsm_workspace *ws = seat_get_focused_workspace(seat);
			if (ws != NULL) {
				wsm_log(WSM_ERROR, "seat_get_focused_workspace failed!");
				output = ws->output;
			}
		}
		if (!output || output == global_server.scene->fallback_output) {
			if (!global_server.scene->outputs->length) {
				wsm_log(WSM_ERROR,
					"no output to auto-assign layer surface '%s' to",
					layer_surface->namespace);
				wlr_layer_surface_v1_destroy(layer_surface);

				return;
			}
			output = global_server.scene->outputs->items[0];
		}
		layer_surface->output = output->wlr_output;
	}

	struct wsm_output *output = layer_surface->output->data;
	enum zwlr_layer_shell_v1_layer layer_type = layer_surface->pending.layer;
	struct wlr_scene_tree *output_layer = wsm_layer_get_scene(
		output, layer_type);
	struct wlr_scene_layer_surface_v1 *scene_surface =
		wlr_scene_layer_surface_v1_create(output_layer, layer_surface);

	if (!scene_surface) {
		wsm_log(WSM_ERROR, "Could not allocate a layer_surface_v1");
		return;
	}

	struct wsm_layer_surface *surface =
		wsm_layer_surface_create(scene_surface);
	if (!surface) {
		wlr_layer_surface_v1_destroy(layer_surface);
		wsm_log(WSM_ERROR, "Could not allocate a wsm_layer_surface");

		return;
	}

	if (!wsm_scene_descriptor_assign(&scene_surface->tree->node,
				WSM_SCENE_DESC_LAYER_SHELL, surface)) {
		wsm_log(WSM_ERROR, "Failed to allocate a layer surface descriptor");
		wlr_layer_surface_v1_destroy(layer_surface);
		return;
	}

	surface->output = output;

	wlr_fractional_scale_v1_notify_scale(surface->layer_surface_wlr->surface,
		layer_surface->output->scale);
	wlr_surface_set_preferred_buffer_scale(surface->layer_surface_wlr->surface,
		ceil(layer_surface->output->scale));

	surface->surface_commit.notify = handle_surface_commit;
	wl_signal_add(&layer_surface->surface->events.commit,
		&surface->surface_commit);
	surface->map.notify = handle_map;
	wl_signal_add(&layer_surface->surface->events.map, &surface->map);
	surface->unmap.notify = handle_unmap;
	wl_signal_add(&layer_surface->surface->events.unmap, &surface->unmap);
	surface->new_popup.notify = handle_new_popup;
	wl_signal_add(&layer_surface->events.new_popup, &surface->new_popup);

	surface->output_destroy.notify = handle_output_destroy;
	wl_signal_add(&output->events.disable, &surface->output_destroy);

	surface->node_destroy.notify = handle_node_destroy;
	wl_signal_add(&scene_surface->tree->node.events.destroy, &surface->node_destroy);
}

struct wsm_layer_shell *wsm_layer_shell_create(const struct wsm_server *server) {
	struct wsm_layer_shell *layer_shell = calloc(1, sizeof(struct wsm_layer_shell));
	if (!layer_shell) {
		wsm_log(WSM_ERROR, "Could not create wsm_layer_shell: allocation failed!");
		return NULL;
	}

	layer_shell->wlr_layer_shell = wlr_layer_shell_v1_create(server->wl_display,
		WSM_LAYER_SHELL_VERSION);
	layer_shell->layer_shell_surface.notify = handle_layer_shell_surface;
	wl_signal_add(&layer_shell->wlr_layer_shell->events.new_surface,
		&layer_shell->layer_shell_surface);

	return layer_shell;
}

struct wlr_layer_surface_v1 *toplevel_layer_surface_from_surface(
	struct wlr_surface *surface) {
	struct wlr_layer_surface_v1 *layer;
	do {
		if (!surface) {
			return NULL;
		}

		if ((layer = wlr_layer_surface_v1_try_from_wlr_surface(surface))) {
			return layer;
		}

		if (wlr_subsurface_try_from_wlr_surface(surface)) {
			surface = wlr_surface_get_root_surface(surface);
			continue;
		}

		struct wlr_xdg_surface *xdg_surface = NULL;
		if ((xdg_surface = wlr_xdg_surface_try_from_wlr_surface(surface)) &&
				xdg_surface->role == WLR_XDG_SURFACE_ROLE_POPUP && xdg_surface->popup != NULL) {
			if (!xdg_surface->popup->parent) {
				return NULL;
			}

			surface = wlr_surface_get_root_surface(xdg_surface->popup->parent);
			continue;
		}

		return NULL;
	} while (true);
}
