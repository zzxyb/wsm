/*
MIT License

Copyright (c) 2024 YaoBing Xiao

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

#include "wsm_xdg_shell.h"
#include "wsm_xdg_popup.h"
#include "wsm_server.h"
#include "wsm_log.h"
#include "wsm_view.h"
#include "wsm_seat.h"
#include "wsm_scene.h"
#include "wsm_output.h"
#include "wsm_arrange.h"
#include "wsm_transaction.h"
#include "wsm_workspace.h"
#include "wsm_xdg_decoration.h"
#include "wsm_server_decoration.h"
#include "wsm_input_manager.h"
#include "node/wsm_node_descriptor.h"
#include "wsm_seatop_move_floating.h"
#include "wsm_seatop_resize_floating.h"

#include <float.h>
#include <stdlib.h>
#include <assert.h>

#include <wlr/util/edges.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/backend/wayland.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/types/wlr_xdg_activation_v1.h>
#include <wlr/types/wlr_layer_shell_v1.h>
#include <wlr/types/wlr_xdg_decoration_v1.h>
#include <wlr/types/wlr_server_decoration.h>
#include <wlr/types/wlr_foreign_toplevel_management_v1.h>
#include <wlr/types/wlr_fractional_scale_v1.h>

#define WSM_XDG_SHELL_VERSION 5
#define CONFIGURE_TIMEOUT_MS 100

static struct wsm_xdg_shell_view *xdg_shell_view_from_view(
		struct wsm_view *view) {
	if (!wsm_assert(view->type == WSM_VIEW_XDG_SHELL,
					"Expected xdg_shell view")) {
		return NULL;
	}
	return (struct wsm_xdg_shell_view *)view;
}

static void get_constraints(struct wsm_view *view, double *min_width,
							double *max_width, double *min_height, double *max_height) {
	struct wlr_xdg_toplevel_state *state =
			&view->wlr_xdg_toplevel->current;
	*min_width = state->min_width > 0 ? state->min_width : DBL_MIN;
	*max_width = state->max_width > 0 ? state->max_width : DBL_MAX;
	*min_height = state->min_height > 0 ? state->min_height : DBL_MIN;
	*max_height = state->max_height > 0 ? state->max_height : DBL_MAX;
}

static const char *get_string_prop(struct wsm_view *view,
								   enum wsm_view_prop prop) {
	if (xdg_shell_view_from_view(view) == NULL) {
		return NULL;
	}
	switch (prop) {
	case VIEW_PROP_TITLE:
		return view->wlr_xdg_toplevel->title;
	case VIEW_PROP_APP_ID:
		return view->wlr_xdg_toplevel->app_id;
	default:
		return NULL;
	}
}

static uint32_t configure(struct wsm_view *view, double lx, double ly,
						  int width, int height) {
	struct wsm_xdg_shell_view *xdg_shell_view =
			xdg_shell_view_from_view(view);
	if (xdg_shell_view == NULL) {
		return 0;
	}
	return wlr_xdg_toplevel_set_size(view->wlr_xdg_toplevel,
									 width, height);
}

static void set_activated(struct wsm_view *view, bool activated) {
	if (xdg_shell_view_from_view(view) == NULL) {
		return;
	}
	wlr_xdg_toplevel_set_activated(view->wlr_xdg_toplevel, activated);
}

static void set_tiled(struct wsm_view *view, bool tiled) {
	if (xdg_shell_view_from_view(view) == NULL) {
		return;
	}
	if (wl_resource_get_version(view->wlr_xdg_toplevel->resource) >=
		XDG_TOPLEVEL_STATE_TILED_LEFT_SINCE_VERSION) {
		enum wlr_edges edges = WLR_EDGE_NONE;
		if (tiled) {
			edges = WLR_EDGE_LEFT | WLR_EDGE_RIGHT | WLR_EDGE_TOP |
					WLR_EDGE_BOTTOM;
		}
		wlr_xdg_toplevel_set_tiled(view->wlr_xdg_toplevel, edges);
	} else {
		wlr_xdg_toplevel_set_maximized(view->wlr_xdg_toplevel, tiled);
	}
}

static void set_fullscreen(struct wsm_view *view, bool fullscreen) {
	if (xdg_shell_view_from_view(view) == NULL) {
		return;
	}
	wlr_xdg_toplevel_set_fullscreen(view->wlr_xdg_toplevel, fullscreen);
}

static void set_resizing(struct wsm_view *view, bool resizing) {
	if (xdg_shell_view_from_view(view) == NULL) {
		return;
	}
	wlr_xdg_toplevel_set_resizing(view->wlr_xdg_toplevel, resizing);
}

static bool wants_floating(struct wsm_view *view) {
	struct wlr_xdg_toplevel *toplevel = view->wlr_xdg_toplevel;
	struct wlr_xdg_toplevel_state *state = &toplevel->current;
	return (state->min_width != 0 && state->min_height != 0
			&& (state->min_width == state->max_width
				|| state->min_height == state->max_height))
		   || toplevel->parent;
}

static bool is_transient_for(struct wsm_view *child,
							 struct wsm_view *ancestor) {
	if (xdg_shell_view_from_view(child) == NULL) {
		return false;
	}
	struct wlr_xdg_toplevel *toplevel = child->wlr_xdg_toplevel;
	while (toplevel) {
		if (toplevel->parent == ancestor->wlr_xdg_toplevel) {
			return true;
		}
		toplevel = toplevel->parent;
	}
	return false;
}

static void _close(struct wsm_view *view) {
	if (xdg_shell_view_from_view(view) == NULL) {
		return;
	}
	wlr_xdg_toplevel_send_close(view->wlr_xdg_toplevel);
}

static void _maximize(struct wsm_view *view, bool maximize) {
	if (xdg_shell_view_from_view(view) == NULL) {
		return;
	}
	wlr_xdg_toplevel_set_maximized(view->wlr_xdg_toplevel, maximize);
}

static void _minimize(struct wsm_view *view, bool minimize) {
	view_set_enable(view, minimize);
}

static void close_popups(struct wsm_view *view) {
	struct wlr_xdg_popup *popup, *tmp;
	wl_list_for_each_safe(popup, tmp, &view->wlr_xdg_toplevel->base->popups, link) {
		wlr_xdg_popup_destroy(popup);
	}
}

static void destroy(struct wsm_view *view) {
	struct wsm_xdg_shell_view *xdg_shell_view =
			xdg_shell_view_from_view(view);
	if (xdg_shell_view == NULL) {
		return;
	}
	free(xdg_shell_view);
}

static const struct wsm_view_impl view_impl = {
		.get_constraints = get_constraints,
		.get_string_prop = get_string_prop,
		.configure = configure,
		.set_activated = set_activated,
		.set_tiled = set_tiled,
		.set_fullscreen = set_fullscreen,
		.set_resizing = set_resizing,
		.wants_floating = wants_floating,
		.is_transient_for = is_transient_for,
		.maximize = _maximize,
		.minimize = _minimize,
		.close = _close,
		.close_popups = close_popups,
		.destroy = destroy,
};

static void handle_commit(struct wl_listener *listener, void *data) {
	struct wsm_xdg_shell_view *xdg_shell_view =
			wl_container_of(listener, xdg_shell_view, commit);
	struct wsm_view *view = &xdg_shell_view->view;
	struct wlr_xdg_surface *xdg_surface = view->wlr_xdg_toplevel->base;
	
	if (xdg_surface->initial_commit) {
		if (view->xdg_decoration != NULL) {
			set_xdg_decoration_mode(view->xdg_decoration);
		}
		wlr_xdg_surface_schedule_configure(xdg_surface);
		wlr_xdg_toplevel_set_wm_capabilities(view->wlr_xdg_toplevel,
											 XDG_TOPLEVEL_WM_CAPABILITIES_FULLSCREEN);
		return;
	}
	
	if (!xdg_surface->surface->mapped) {
		return;
	}
	
	struct wlr_box new_geo;
	wlr_xdg_surface_get_geometry(xdg_surface, &new_geo);
	bool new_size = new_geo.width != view->geometry.width ||
					new_geo.height != view->geometry.height ||
					new_geo.x != view->geometry.x ||
					new_geo.y != view->geometry.y;
	
	if (new_size) {
		memcpy(&view->geometry, &new_geo, sizeof(struct wlr_box));
		if (container_is_floating(view->container) && (xdg_surface->initial_commit && view->using_csd)) {
			view_update_size(view);
			if (view->container->current.width) {
				wlr_xdg_toplevel_set_size(view->wlr_xdg_toplevel, view->geometry.width,
										  view->geometry.height);
			}
			transaction_commit_dirty_client();
		}
		
		view_center_and_clip_surface(view);
	}
	
	if (view->container->node.instruction) {
		bool successful = transaction_notify_view_ready_by_serial(view,
																  xdg_surface->current.configure_serial);
		
		if (view->saved_surface_tree && !successful) {
			view_send_frame_done(view);
		}
	}
}

static void handle_set_title(struct wl_listener *listener, void *data) {
	struct wsm_xdg_shell_view *xdg_shell_view =
			wl_container_of(listener, xdg_shell_view, set_title);
	struct wsm_view *view = &xdg_shell_view->view;
	view_update_title(view, false);
}

static void handle_set_app_id(struct wl_listener *listener, void *data) {
	struct wsm_xdg_shell_view *xdg_shell_view =
			wl_container_of(listener, xdg_shell_view, set_app_id);
	struct wsm_view *view = &xdg_shell_view->view;
	view_update_app_id(view);
}

static void handle_new_popup(struct wl_listener *listener, void *data) {
	struct wsm_xdg_shell_view *xdg_shell_view =
			wl_container_of(listener, xdg_shell_view, new_popup);
	struct wlr_xdg_popup *wlr_popup = data;
	
	struct wsm_xdg_popup *popup = wsm_xdg_popup_create(wlr_popup,
													   &xdg_shell_view->view, global_server.wsm_scene->layers.popup);
	if (!popup) {
		return;
	}
	
	int lx, ly;
	wlr_scene_node_coords(&popup->view->content_tree->node, &lx, &ly);
	wlr_scene_node_set_position(&popup->scene_tree->node, lx, ly);
}

static void handle_request_maximize(struct wl_listener *listener, void *data) {
	struct wsm_xdg_shell_view *xdg_shell_view =
			wl_container_of(listener, xdg_shell_view, request_maximize);
	struct wlr_xdg_toplevel *toplevel = xdg_shell_view->view.wlr_xdg_toplevel;
	wlr_xdg_surface_schedule_configure(toplevel->base);
}

static void handle_request_fullscreen(struct wl_listener *listener, void *data) {
	struct wsm_xdg_shell_view *xdg_shell_view =
			wl_container_of(listener, xdg_shell_view, request_fullscreen);
	struct wlr_xdg_toplevel *toplevel = xdg_shell_view->view.wlr_xdg_toplevel;
	struct wsm_view *view = &xdg_shell_view->view;
	
	if (!toplevel->base->surface->mapped) {
		return;
	}
	
	struct wsm_container *container = view->container;
	struct wlr_xdg_toplevel_requested *req = &toplevel->requested;
	if (req->fullscreen && req->fullscreen_output && req->fullscreen_output->data) {
		struct wsm_output *output = req->fullscreen_output->data;
		struct wsm_workspace *ws = output_get_active_workspace(output);
		if (ws && !container_is_scratchpad_hidden(container) &&
			container->pending.workspace != ws) {
			if (container_is_floating(container)) {
				workspace_add_floating(ws, container);
			} else {
				container = workspace_add_tiling(ws, container);
			}
		}
	}
	
	container_set_fullscreen(container, req->fullscreen);
	
	arrange_root_auto();
	transaction_commit_dirty();
}

static void handle_request_move(struct wl_listener *listener, void *data) {
	struct wsm_xdg_shell_view *xdg_shell_view =
			wl_container_of(listener, xdg_shell_view, request_move);
	struct wsm_view *view = &xdg_shell_view->view;
	if (!container_is_floating(view->container) ||
		view->container->pending.fullscreen_mode) {
		return;
	}
	struct wlr_xdg_toplevel_move_event *e = data;
	struct wsm_seat *seat = e->seat->seat->data;
	if (e->serial == seat->last_button_serial) {
		seatop_begin_move_floating(seat, view->container);
	}
}

static void handle_request_resize(struct wl_listener *listener, void *data) {
	struct wsm_xdg_shell_view *xdg_shell_view =
			wl_container_of(listener, xdg_shell_view, request_resize);
	struct wsm_view *view = &xdg_shell_view->view;
	if (!container_is_floating(view->container)) {
		return;
	}
	struct wlr_xdg_toplevel_resize_event *e = data;
	struct wsm_seat *seat = e->seat->seat->data;
	if (e->serial == seat->last_button_serial) {
		seatop_begin_resize_floating(seat, view->container, e->edges);
	}
}

static void handle_unmap(struct wl_listener *listener, void *data) {
	struct wsm_xdg_shell_view *xdg_shell_view =
			wl_container_of(listener, xdg_shell_view, unmap);
	struct wsm_view *view = &xdg_shell_view->view;
	
	if (!wsm_assert(view->surface, "Cannot unmap unmapped view")) {
		return;
	}
	
	view_unmap(view);
	
	wl_list_remove(&xdg_shell_view->new_popup.link);
	wl_list_remove(&xdg_shell_view->request_maximize.link);
	wl_list_remove(&xdg_shell_view->request_fullscreen.link);
	wl_list_remove(&xdg_shell_view->request_move.link);
	wl_list_remove(&xdg_shell_view->request_resize.link);
	wl_list_remove(&xdg_shell_view->set_title.link);
	wl_list_remove(&xdg_shell_view->set_app_id.link);
}

static void handle_map(struct wl_listener *listener, void *data) {
	struct wsm_xdg_shell_view *xdg_shell_view =
			wl_container_of(listener, xdg_shell_view, map);
	struct wsm_view *view = &xdg_shell_view->view;
	struct wlr_xdg_toplevel *toplevel = view->wlr_xdg_toplevel;
	
	view->natural_width = toplevel->base->current.geometry.width;
	view->natural_height = toplevel->base->current.geometry.height;
	if (!view->natural_width && !view->natural_height) {
		view->natural_width = toplevel->base->surface->current.width;
		view->natural_height = toplevel->base->surface->current.height;
	}
	
	bool csd = false;
	
	if (view->xdg_decoration) {
		enum wlr_xdg_toplevel_decoration_v1_mode mode =
				view->xdg_decoration->wlr_xdg_decoration->requested_mode;
		csd = mode == WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE;
	} else {
		struct wsm_server_decoration *deco =
				decoration_from_surface(toplevel->base->surface);
		csd = !deco || deco->wlr_server_decoration->mode ==
							   WLR_SERVER_DECORATION_MANAGER_MODE_CLIENT;
	}
	
	view_map(view, toplevel->base->surface,
			 toplevel->requested.fullscreen,
			 toplevel->requested.fullscreen_output,
			 csd);
	
	transaction_commit_dirty();
	
	xdg_shell_view->new_popup.notify = handle_new_popup;
	wl_signal_add(&toplevel->base->events.new_popup,
				  &xdg_shell_view->new_popup);
	
	xdg_shell_view->request_maximize.notify = handle_request_maximize;
	wl_signal_add(&toplevel->events.request_maximize,
				  &xdg_shell_view->request_maximize);
	
	xdg_shell_view->request_fullscreen.notify = handle_request_fullscreen;
	wl_signal_add(&toplevel->events.request_fullscreen,
				  &xdg_shell_view->request_fullscreen);
	
	xdg_shell_view->request_move.notify = handle_request_move;
	wl_signal_add(&toplevel->events.request_move,
				  &xdg_shell_view->request_move);
	
	xdg_shell_view->request_resize.notify = handle_request_resize;
	wl_signal_add(&toplevel->events.request_resize,
				  &xdg_shell_view->request_resize);
	
	xdg_shell_view->set_title.notify = handle_set_title;
	wl_signal_add(&toplevel->events.set_title,
				  &xdg_shell_view->set_title);
	
	xdg_shell_view->set_app_id.notify = handle_set_app_id;
	wl_signal_add(&toplevel->events.set_app_id,
				  &xdg_shell_view->set_app_id);
}

static void handle_destroy(struct wl_listener *listener, void *data) {
	struct wsm_xdg_shell_view *xdg_shell_view =
			wl_container_of(listener, xdg_shell_view, destroy);
	struct wsm_view *view = &xdg_shell_view->view;
	if (!wsm_assert(view->surface == NULL, "Tried to destroy a mapped view")) {
		return;
	}
	wl_list_remove(&xdg_shell_view->destroy.link);
	wl_list_remove(&xdg_shell_view->map.link);
	wl_list_remove(&xdg_shell_view->unmap.link);
	wl_list_remove(&xdg_shell_view->commit.link);
	view->wlr_xdg_toplevel = NULL;
	if (view->xdg_decoration) {
		view->xdg_decoration->view = NULL;
	}
	view_begin_destroy(view);
}

void handle_xdg_shell_toplevel(struct wl_listener *listener, void *data) {
	struct wlr_xdg_toplevel *xdg_toplevel = data;
	
	wsm_log(WSM_DEBUG, "New xdg_shell toplevel title='%s' app_id='%s'",
			xdg_toplevel->title, xdg_toplevel->app_id);
	wlr_xdg_surface_ping(xdg_toplevel->base);
	
	struct wsm_xdg_shell_view *xdg_shell_view =
			calloc(1, sizeof(struct wsm_xdg_shell_view));
	if (!wsm_assert(xdg_shell_view, "Could not create wsm_xdg_shell_view: allocation failed!")) {
		return;
	}
	
	if (!view_init(&xdg_shell_view->view, WSM_VIEW_XDG_SHELL, &view_impl)) {
		free(xdg_shell_view);
		return;
	}
	xdg_shell_view->view.wlr_xdg_toplevel = xdg_toplevel;
	
	xdg_shell_view->map.notify = handle_map;
	wl_signal_add(&xdg_toplevel->base->surface->events.map, &xdg_shell_view->map);
	
	xdg_shell_view->unmap.notify = handle_unmap;
	wl_signal_add(&xdg_toplevel->base->surface->events.unmap, &xdg_shell_view->unmap);
	
	xdg_shell_view->commit.notify = handle_commit;
	wl_signal_add(&xdg_toplevel->base->surface->events.commit,
				  &xdg_shell_view->commit);
	
	xdg_shell_view->destroy.notify = handle_destroy;
	wl_signal_add(&xdg_toplevel->events.destroy, &xdg_shell_view->destroy);
	
	wlr_scene_xdg_surface_create(xdg_shell_view->view.content_tree, xdg_toplevel->base);
	
	xdg_toplevel->base->data = xdg_shell_view;
}

void xdg_activation_v1_handle_new_token(struct wl_listener *listener, void *data) {
	// struct wlr_xdg_activation_token_v1 *token = data;
	// struct wsm_seat *seat = token->seat ? token->seat->data :
	//                              input_manager_current_seat();
	
		   // struct wsm_workspace *ws = seat_get_focused_workspace(seat);
}

void xdg_activation_v1_handle_request_activate(struct wl_listener *listener,
											   void *data) {
	const struct wlr_xdg_activation_v1_request_activate_event *event = data;
	
	struct wlr_xdg_surface *xdg_surface =
			wlr_xdg_surface_try_from_wlr_surface(event->surface);
	if (xdg_surface == NULL) {
		return;
	}
	struct wsm_view *view = xdg_surface->data;
	if (view == NULL) {
		return;
	}
}

struct wsm_xdg_shell *wsm_xdg_shell_create(const struct wsm_server* server) {
	struct wsm_xdg_shell *shell = calloc(1, sizeof(struct wsm_xdg_shell));
	if (!wsm_assert(shell, "Could not create wsm_xdg_shell: allocation failed!")) {
		return NULL;
	}
	
	shell->wlr_xdg_shell = wlr_xdg_shell_create(server->wl_display, WSM_XDG_SHELL_VERSION);
	shell->xdg_shell_toplevel.notify = handle_xdg_shell_toplevel;
	wl_signal_add(&shell->wlr_xdg_shell->events.new_toplevel,
				  &shell->xdg_shell_toplevel);
	
	shell->xdg_activation_v1 = wlr_xdg_activation_v1_create(server->wl_display);
	if (!wsm_assert(shell->xdg_activation_v1, "unable to create wlr_xdg_activation_v1 interface")) {
		return NULL;
	}
	shell->xdg_activation_request.notify = xdg_activation_v1_handle_request_activate;
	wl_signal_add(&shell->xdg_activation_v1->events.request_activate,
				  &shell->xdg_activation_request);
	
	shell->xdg_activation_v1_request_activate.notify =
			xdg_activation_v1_handle_request_activate;
	wl_signal_add(&shell->xdg_activation_v1->events.request_activate,
				  &shell->xdg_activation_v1_request_activate);
	
	shell->xdg_activation_v1_new_token.notify =
			xdg_activation_v1_handle_new_token;
	wl_signal_add(&shell->xdg_activation_v1->events.new_token,
				  &shell->xdg_activation_v1_new_token);
	return shell;
}

void wsm_xdg_shell_destroy(struct wsm_xdg_shell *shell) {
	if (!shell) {
		wsm_log(WSM_ERROR, "wsm_xdg_shell is NULL!");
		return;
	}
	
	wl_list_remove(&shell->xdg_shell_toplevel.link);
	
	free(shell);
}

struct wsm_view *view_from_wlr_xdg_surface(
		struct wlr_xdg_surface *xdg_surface) {
	return xdg_surface->data;
}
