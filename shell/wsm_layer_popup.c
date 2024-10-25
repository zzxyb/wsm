#include "wsm_log.h"
#include "wsm_output.h"
#include "wsm_layer_popup.h"
#include "wsm_layer_shell.h"

#include <stdlib.h>

#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_subcompositor.h>
#include <wlr/types/wlr_xdg_shell.h>

static void popup_handle_new_popup(struct wl_listener *listener, void *data) {
	struct wsm_layer_popup *wsm_layer_popup =
		wl_container_of(listener, wsm_layer_popup, new_popup);
	struct wlr_xdg_popup *wlr_popup = data;
	wsm_layer_popup_create(wlr_popup, wsm_layer_popup->toplevel, wsm_layer_popup->scene);
}

static void popup_handle_destroy(struct wl_listener *listener, void *data) {
	struct wsm_layer_popup *popup =
		wl_container_of(listener, popup, destroy);

	wl_list_remove(&popup->destroy.link);
	wl_list_remove(&popup->new_popup.link);
	wl_list_remove(&popup->commit.link);
	free(popup);
}

static void popup_handle_commit(struct wl_listener *listener, void *data) {
	struct wsm_layer_popup *popup = wl_container_of(listener, popup, commit);
	if (popup->xdg_popup->base->initial_commit) {
		wsm_layer_popup_unconstrain(popup);
	}
}

struct wsm_layer_popup *wsm_layer_popup_create(struct wlr_xdg_popup *wlr_popup,
		struct wsm_layer_surface *toplevel, struct wlr_scene_tree *parent) {
	struct wsm_layer_popup *popup = calloc(1, sizeof(struct wsm_layer_popup));
	if (!popup) {
		wsm_log(WSM_ERROR, "Could not create wsm_layer_popup: allocation failed!");
		return NULL;
	}

	popup->toplevel = toplevel;
	popup->xdg_popup = wlr_popup;
	popup->scene = wlr_scene_xdg_surface_create(parent,
		wlr_popup->base);

	if (!popup->scene) {
		free(popup);
		return NULL;
	}

	popup->destroy.notify = popup_handle_destroy;
	wl_signal_add(&wlr_popup->base->events.destroy, &popup->destroy);
	popup->new_popup.notify = popup_handle_new_popup;
	wl_signal_add(&wlr_popup->base->events.new_popup, &popup->new_popup);
	popup->commit.notify = popup_handle_commit;
	wl_signal_add(&wlr_popup->base->surface->events.commit, &popup->commit);

	return popup;
}

void wsm_layer_popup_unconstrain(struct wsm_layer_popup *popup) {
	struct wlr_xdg_popup *wlr_popup = popup->xdg_popup;
	struct wsm_output *output = popup->toplevel->output;

	if (!output) {
		return;
	}

	int lx, ly;
	wlr_scene_node_coords(&popup->toplevel->scene->tree->node, &lx, &ly);
	struct wlr_box output_toplevel_sx_box = {
		.x = output->lx - lx,
		.y = output->ly - ly,
		.width = output->width,
		.height = output->height,
	};

	wlr_xdg_popup_unconstrain_from_box(wlr_popup, &output_toplevel_sx_box);
}
