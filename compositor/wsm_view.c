#include "wsm_log.h"
#include "wsm_list.h"
#include "wsm_seat.h"
#include "wsm_view.h"
#include "wsm_config.h"
#include "wsm_output.h"
#include "wsm_cursor.h"
#include "wsm_server.h"
#include "wsm_scene.h"
#include "wsm_pango.h"
#include "wsm_common.h"
#include "wsm_titlebar.h"
#include "wsm_window.h"
#include "wsm_workspace.h"
#include "wsm_xdg_shell.h"
#include "wsm_transaction.h"
#include "node/wsm_node_descriptor.h"
#include "node/wsm_text_node.h"
#include "wsm_idle_inhibit_v1.h"
#include "wsm_input_manager.h"
#include "wsm_xdg_decoration.h"
#include "wsm_arrange.h"

#include <float.h>
#include <stdlib.h>

#include <wayland-server.h>

#if HAVE_XWAYLAND
#include <xcb/xcb_icccm.h>
#include <wlr/xwayland.h>
#include <wlr/xwayland/xwayland.h>
#endif
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/types/wlr_layer_shell_v1.h>
#include <wlr/types/wlr_subcompositor.h>
#include <wlr/types/wlr_fractional_scale_v1.h>
#include <wlr/types/wlr_foreign_toplevel_management_v1.h>
#include <wlr/types/wlr_xdg_decoration_v1.h>
#include <wlr/types/wlr_pointer_constraints_v1.h>
#include <wlr/types/wlr_ext_foreign_toplevel_list_v1.h>

bool view_init(struct wsm_view *view, enum wsm_view_type type,
		const struct wsm_view_impl *impl) {
	bool failed = false;
	view->scene_tree = alloc_scene_tree(global_server.scene->staging, &failed);
	view->content_tree = alloc_scene_tree(view->scene_tree, &failed);

	if (!failed && !wsm_scene_descriptor_assign(&view->scene_tree->node,
			WSM_SCENE_DESC_VIEW, view)) {
		failed = true;
	}

	if (failed) {
		wlr_scene_node_destroy(&view->scene_tree->node);
		return false;
	}

	view->type = type;
	view->impl = impl;
	view->allow_request_urgent = true;
	view->enabled = true;
	wl_signal_init(&view->events.unmap);
	return true;
}

void view_destroy(struct wsm_view *view) {
	if (!wsm_assert(view->surface == NULL, "Tried to free mapped view")) {
		return;
	}
	if (!wsm_assert(view->destroying,
			"Tried to free view which wasn't marked as destroying")) {
		return;
	}
	if (!wsm_assert(view->window == NULL,
			"Tried to free view which still has a window "
			"(might have a pending transaction?)")) {
		return;
	}
	wl_list_remove(&view->events.unmap.listener_list);

	wlr_scene_node_destroy(&view->scene_tree->node);
	free(view->title_format);
	free(view->app_id);
	free(view->app_icon_path);

	if (view->impl->destroy) {
		view->impl->destroy(view);
	} else {
		free(view);
	}
}

void view_begin_destroy(struct wsm_view *view) {
	if (!wsm_assert(view->surface == NULL, "Tried to destroy a mapped view")) {
		return;
	}
	view->destroying = true;

	if (!view->window) {
		view_destroy(view);
	}
}

const char *view_get_title(struct wsm_view *view) {
	if (view->impl->get_string_prop) {
		return view->impl->get_string_prop(view, VIEW_PROP_TITLE);
	}
	return NULL;
}

const char *view_get_app_id(struct wsm_view *view) {
	if (view->impl->get_string_prop) {
		return view->impl->get_string_prop(view, VIEW_PROP_APP_ID);
	}
	return NULL;
}

const char *view_get_class(struct wsm_view *view) {
	if (view->impl->get_string_prop) {
		return view->impl->get_string_prop(view, VIEW_PROP_CLASS);
	}
	return NULL;
}

const char *view_get_instance(struct wsm_view *view) {
	if (view->impl->get_string_prop) {
		return view->impl->get_string_prop(view, VIEW_PROP_INSTANCE);
	}
	return NULL;
}
#if HAVE_XWAYLAND
uint32_t view_get_x11_window_id(struct wsm_view *view) {
	if (view->impl->get_int_prop) {
		return view->impl->get_int_prop(view, VIEW_PROP_X11_WINDOW_ID);
	}
	return 0;
}

uint32_t view_get_x11_parent_id(struct wsm_view *view) {
	if (view->impl->get_int_prop) {
		return view->impl->get_int_prop(view, VIEW_PROP_X11_PARENT_ID);
	}
	return 0;
}
#endif
const char *view_get_window_role(struct wsm_view *view) {
	if (view->impl->get_string_prop) {
		return view->impl->get_string_prop(view, VIEW_PROP_WINDOW_ROLE);
	}
	return NULL;
}

uint32_t view_get_window_type(struct wsm_view *view) {
	if (view->impl->get_int_prop) {
		return view->impl->get_int_prop(view, VIEW_PROP_WINDOW_TYPE);
	}
	return 0;
}

const char *view_get_shell(struct wsm_view *view) {
	switch(view->type) {
	case WSM_VIEW_XDG_SHELL:
		return "xdg_shell";
#if HAVE_XWAYLAND
	case WSM_VIEW_XWAYLAND:
		return "xwayland";
#endif
	}
	return "unknown";
}

void view_get_constraints(struct wsm_view *view, double *min_width,
		double *max_width, double *min_height, double *max_height) {
	if (view->impl->get_constraints) {
		view->impl->get_constraints(view,
			min_width, max_width, min_height, max_height);
	} else {
		*min_width = DBL_MIN;
		*max_width = DBL_MAX;
		*min_height = DBL_MIN;
		*max_height = DBL_MAX;
	}
}

uint32_t view_configure(struct wsm_view *view, double lx, double ly, int width,
		int height) {
	if (view->impl->configure) {
		return view->impl->configure(view, lx, ly, width, height);
	}
	return 0;
}

bool view_inhibit_idle(struct wsm_view *view) {
	struct wsm_idle_inhibitor_v1 *user_inhibitor =
		wsm_idle_inhibit_v1_user_inhibitor_for_view(view);

	struct wsm_idle_inhibitor_v1 *application_inhibitor =
		wsm_idle_inhibit_v1_application_inhibitor_for_view(view);

	if (!user_inhibitor && !application_inhibitor) {
		return false;
	}

	if (!user_inhibitor) {
		return wsm_idle_inhibit_v1_is_active(application_inhibitor);
	}

	if (!application_inhibitor) {
		return wsm_idle_inhibit_v1_is_active(user_inhibitor);
	}

	return wsm_idle_inhibit_v1_is_active(user_inhibitor)
			|| wsm_idle_inhibit_v1_is_active(application_inhibitor);
}

void view_autoconfigure(struct wsm_view *view) {
	struct wsm_window *window = view->window;
	struct wsm_workspace *ws = window->pending.workspace;

	if (window_is_scratchpad_hidden(window) &&
		window->pending.fullscreen_mode != FULLSCREEN_GLOBAL) {
		return;
	}
	struct wsm_output *output = ws ? ws->output : NULL;

	if (window->pending.fullscreen_mode == FULLSCREEN_WORKSPACE) {
		window->pending.content_x = output->lx;
		window->pending.content_y = output->ly;
		window->pending.content_width = output->width;
		window->pending.content_height = output->height;
		return;
	} else if (window->pending.fullscreen_mode == FULLSCREEN_GLOBAL) {
		window->pending.content_x = global_server.scene->x;
		window->pending.content_y = global_server.scene->y;
		window->pending.content_width = global_server.scene->width;
		window->pending.content_height = global_server.scene->height;
		return;
	}

	window->pending.border_top = window->pending.border_bottom = true;
	window->pending.border_left = window->pending.border_right = true;

	bool show_border = true;
	window->pending.border_left &= show_border;
	window->pending.border_right &= show_border;
	window->pending.border_top &= show_border;
	window->pending.border_bottom &= show_border;

	double x, y, width, height;
	int max_thickness;
	switch (window->pending.border) {
	default:
	case B_CSD:
	case B_NONE:
		x = window->pending.x;
		y = window->pending.y;
		width = window->pending.width;
		height = window->pending.height;
		break;
	case B_NORMAL:
		max_thickness = get_max_thickness(window->pending);
		x = window->pending.x + max_thickness * window->pending.border_left;
		y = window->pending.y + window_titlebar_height()
			+ max_thickness * window->pending.border_top;
		width = window->pending.width
			- max_thickness * window->pending.border_left
			- max_thickness * window->pending.border_right;
		height = window->pending.height - window_titlebar_height()
			- max_thickness * window->pending.border_bottom
			- max_thickness * window->pending.border_top;
		break;
	}

	window->pending.content_x = x;
	window->pending.content_y = y;
	window->pending.content_width = width;
	window->pending.content_height = height;
}

void view_set_activated(struct wsm_view *view, bool activated) {
	if (view->impl->set_activated) {
		view->impl->set_activated(view, activated);
	}
	if (view->foreign_toplevel) {
		wlr_foreign_toplevel_handle_v1_set_activated(
			view->foreign_toplevel, activated);
	}
}

void view_request_activate(struct wsm_view *view, struct wsm_seat *seat) {
	struct wsm_workspace *ws = view->window->pending.workspace;
	if (!seat) {
		seat = input_manager_current_seat();
	}

	if (ws && workspace_is_visible(ws)) {
		seat_set_focus_window(seat, view->window);
		window_raise(view->window);
	} else {
		view_set_urgent(view, true);
	}
	transaction_commit_dirty();
}

void view_set_csd_from_server(struct wsm_view *view, bool enabled) {
	wsm_log(WSM_DEBUG, "Telling view %p to set CSD to %i", view, enabled);
	if (view->xdg_decoration) {
		uint32_t mode = enabled ?
			WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE :
			WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE;
		wlr_xdg_toplevel_decoration_v1_set_mode(
			view->xdg_decoration->xdg_decoration_wlr, mode);
	}
	view->using_csd = enabled;
}

void view_update_csd_from_client(struct wsm_view *view, bool enabled) {
	wsm_log(WSM_DEBUG, "View %p updated CSD to %i", view, enabled);
	struct wsm_window *window = view->window;
	if (enabled && window && window->pending.border != B_CSD) {
		window->saved_border = window->pending.border;
		if (window_is_managed(window)) {
			window->pending.border = B_CSD;
		}
	} else if (!enabled && window && window->pending.border == B_CSD) {
		window->pending.border = window->saved_border;
	}
	view->using_csd = enabled;
}

void view_set_tiled(struct wsm_view *view, bool tiled) {
	if (view->impl->set_tiled) {
		view->impl->set_tiled(view, tiled);
	}
}

void view_maximize(struct wsm_view *view, bool maximize) {
	if (view->impl->maximize) {
		view->impl->maximize(view, maximize);
	}
}

bool view_can_maximize(struct wsm_view *view) {
	if (!view || !view->impl->maximize) {
		return false;
	}

	double min_width, max_width, min_height, max_height;
	view_get_constraints(view, &min_width, &max_width, &min_height, &max_height);
	if ((min_width > 0 && max_width < DBL_MAX && min_width == max_width) ||
			(min_height > 0 && max_height < DBL_MAX && min_height == max_height)) {
		return false;
	}

	return true;
}

void view_minimize(struct wsm_view *view, bool minimize) {
	if (view->impl->minimize) {
		view->impl->minimize(view, minimize);
	}
}

bool view_can_minimize(struct wsm_view *view) {
	if (!view || !view->impl->minimize) {
		return false;
	}

	return true;
}

void view_close(struct wsm_view *view) {
	if (view->impl->close) {
		view->impl->close(view);
	}
}

bool view_has_popups(struct wsm_view *view) {
	return view && view->impl->has_popups && view->impl->has_popups(view);
}

void view_close_popups(struct wsm_view *view) {
	if (view->impl->close_popups) {
		view->impl->close_popups(view);
	}
}

void view_set_enable(struct wsm_view *view, bool enable) {
	if (view->scene_tree) {
		wlr_scene_node_set_enabled(&view->scene_tree->node, enable);

		if (view->window) {
			if (view->window->scene_tree) {
				wlr_scene_node_set_enabled(&view->window->scene_tree->node, enable);
			}

			if (view->window->title_bar) {
				wlr_scene_node_set_enabled(&view->window->title_bar->tree->node, enable);
			}

			if (view->window->sensing.tree) {
				wlr_scene_node_set_enabled(&view->window->sensing.tree->node, enable);
			}
		} else {
			wsm_log(WSM_ERROR, "wsm_view's window is NULL");
		}
	} else {
		wsm_log(WSM_ERROR, "wsm_view's scene_tree is NULL");
	}
	view->enabled = enable;
}

static void view_populate_pid(struct wsm_view *view) {
	pid_t pid;
	switch (view->type) {
#if HAVE_XWAYLAND
	case WSM_VIEW_XWAYLAND:;
		struct wlr_xwayland_surface *surf =
			wlr_xwayland_surface_try_from_wlr_surface(view->surface);
		pid = surf->pid;
		break;
#endif
	case WSM_VIEW_XDG_SHELL:;
		struct wl_client *client =
			wl_resource_get_client(view->surface->resource);
		wl_client_get_credentials(client, &pid, NULL, NULL);
		break;
	}
	view->pid = pid;
}

static struct wsm_workspace *select_workspace(struct wsm_view *view) {
	struct wsm_seat *seat = input_manager_current_seat();

	struct wsm_node *node = seat_get_focus_inactive(seat, &global_server.scene->node);
	if (node && node->type == N_WORKSPACE) {
		return node->workspace;
	} else if (node && node->type == N_WINDOW) {
		return node->window->pending.workspace;
	}

	wsm_assert(false, "Expected to find a workspace");
	return NULL;
}

static bool should_focus(struct wsm_view *view) {
	struct wsm_seat *seat = input_manager_current_seat();
	struct wsm_window *prev_con = seat_get_focused_window(seat);
	struct wsm_workspace *prev_ws = seat_get_focused_workspace(seat);
	struct wsm_workspace *map_ws = view->window->pending.workspace;

	if (view->window->pending.fullscreen_mode == FULLSCREEN_GLOBAL) {
		return true;
	}

	if (global_server.scene->fullscreen_global || !map_ws || map_ws->fullscreen) {
		return false;
	}

	if (prev_ws != map_ws) {
		return false;
	}

	if (!prev_con && view->window->pending.workspace &&
			view->window->pending.workspace->windows->length == 1) {
		return true;
	}

	return true;
}

static void handle_foreign_activate_request(
		struct wl_listener *listener, void *data) {
	struct wsm_view *view = wl_container_of(
		listener, view, foreign_activate_request);
	struct wlr_foreign_toplevel_handle_v1_activated_event *event = data;
	struct wsm_seat *seat;
	wl_list_for_each(seat, &global_server.input_manager->seats, link) {
		if (seat->seat == event->seat) {
			if (!view->enabled) {
				view_minimize(view, false);
			}
			if (window_is_scratchpad_hidden(view->window)) {
				root_scratchpad_show(view->window);
			}
			seat_set_focus_window(seat, view->window);
			seat_consider_warp_to_focus(seat);
			window_raise(view->window);
			break;
		}
	}
	transaction_commit_dirty();
}

static void handle_foreign_fullscreen_request(
	struct wl_listener *listener, void *data) {
	struct wsm_view *view = wl_container_of(
		listener, view, foreign_fullscreen_request);
	struct wlr_foreign_toplevel_handle_v1_fullscreen_event *event = data;
	struct wsm_window *window = view->window;

	if (event->fullscreen && event->output && event->output->data) {
		struct wsm_output *output = event->output->data;
		struct wsm_workspace *ws = output_get_active_workspace(output);
		if (ws && !window_is_scratchpad_hidden(view->window)) {
			workspace_add_window(ws, view->window);
		}
	}

	window_set_fullscreen(window,
		event->fullscreen ? FULLSCREEN_WORKSPACE : FULLSCREEN_NONE);
	if (event->fullscreen) {
		arrange_root_auto();
	} else if (window->pending.workspace) {
		wsm_arrange_workspace_auto(window->pending.workspace);
	}
	transaction_commit_dirty();
}

static void handle_foreign_close_request(
	struct wl_listener *listener, void *data) {
	struct wsm_view *view = wl_container_of(
		listener, view, foreign_close_request);
	view_close(view);
}

static void handle_foreign_destroy(
	struct wl_listener *listener, void *data) {
	struct wsm_view *view = wl_container_of(
		listener, view, foreign_destroy);

	wl_list_remove(&view->foreign_activate_request.link);
	wl_list_remove(&view->foreign_fullscreen_request.link);
	wl_list_remove(&view->foreign_close_request.link);
	wl_list_remove(&view->foreign_destroy.link);
}

void view_map(struct wsm_view *view, struct wlr_surface *wlr_surface,
		bool fullscreen, struct wlr_output *fullscreen_output, bool decoration) {
	if (!wsm_assert(view->surface == NULL, "cannot map mapped view")) {
		return;
	}
	view->surface = wlr_surface;
	view_populate_pid(view);
	view->window = window_create(view);

	struct wsm_workspace *ws = NULL;
	if (fullscreen_output && fullscreen_output->data) {
		struct wsm_output *output = fullscreen_output->data;
		ws = output_get_active_workspace(output);
	}
	if (!ws) {
		ws = select_workspace(view);
	}

	struct wlr_ext_foreign_toplevel_handle_v1_state foreign_toplevel_state = {
		.app_id = view_get_app_id(view),
		.title = view_get_title(view),
	};
	view->ext_foreign_toplevel =
		wlr_ext_foreign_toplevel_handle_v1_create(global_server.foreign_toplevel_list, &foreign_toplevel_state);

	view->foreign_toplevel =
		wlr_foreign_toplevel_handle_v1_create(global_server.foreign_toplevel_manager);
	view->foreign_activate_request.notify = handle_foreign_activate_request;
	wl_signal_add(&view->foreign_toplevel->events.request_activate,
		&view->foreign_activate_request);
	view->foreign_fullscreen_request.notify = handle_foreign_fullscreen_request;
	wl_signal_add(&view->foreign_toplevel->events.request_fullscreen,
		&view->foreign_fullscreen_request);
	view->foreign_close_request.notify = handle_foreign_close_request;
	wl_signal_add(&view->foreign_toplevel->events.request_close,
		&view->foreign_close_request);
	view->foreign_destroy.notify = handle_foreign_destroy;
	wl_signal_add(&view->foreign_toplevel->events.destroy,
		&view->foreign_destroy);

	struct wsm_window *window = view->window;
	if (ws) {
		workspace_add_window(ws, window);
	}

	if (decoration) {
		view_update_csd_from_client(view, decoration);
	}

	view->window->pending.border = global_config.window_border;
	view->window->pending.border_thickness = global_config.window_border_thickness;
	view->window->pending.sensing_thickness = global_config.sensing_border_thickness;
	view_set_tiled(view, false);
	window_set_default_size(view->window);
	window_resize_and_center(view->window);

	if (global_config.popup_during_fullscreen == POPUP_LEAVE &&
		window->pending.workspace &&
		window->pending.workspace->fullscreen &&
		window->pending.workspace->fullscreen->view) {
		struct wsm_window *fs = window->pending.workspace->fullscreen;
		if (view_is_transient_for(view, fs->view)) {
			window_set_fullscreen(fs, FULLSCREEN_NONE);
		}
	}

	view_update_title(view, false);
	if (window->pending.workspace) {
		workspace_update_representation(window->pending.workspace);
	}

	if (fullscreen) {
		window_set_fullscreen(view->window, FULLSCREEN_WORKSPACE);
		wsm_arrange_workspace_auto(view->window->pending.workspace);
	} else {
		if (window->pending.workspace) {
			wsm_arrange_workspace_auto(window->pending.workspace);
		}
	}

	bool set_focus = should_focus(view);

#if HAVE_XWAYLAND
	struct wlr_xwayland_surface *xsurface;
	if ((xsurface = wlr_xwayland_surface_try_from_wlr_surface(wlr_surface))) {
		set_focus &= wlr_xwayland_surface_icccm_input_model(xsurface) !=
			WLR_ICCCM_INPUT_MODEL_NONE;
	}
#endif

	if (set_focus) {
		input_manager_set_focus(&view->window->node);
	}

	const char *app_id;
	const char *class;
	if ((app_id = view_get_app_id(view)) != NULL) {
		wlr_foreign_toplevel_handle_v1_set_app_id(view->foreign_toplevel, app_id);
	} else if ((class = view_get_class(view)) != NULL) {
		wlr_foreign_toplevel_handle_v1_set_app_id(view->foreign_toplevel, class);
	}
}

void view_unmap(struct wsm_view *view) {
	wl_signal_emit_mutable(&view->events.unmap, view);

	if (view->urgent_timer) {
		wl_event_source_remove(view->urgent_timer);
		view->urgent_timer = NULL;
	}

	if (view->foreign_toplevel) {
		wlr_foreign_toplevel_handle_v1_destroy(view->foreign_toplevel);
		view->foreign_toplevel = NULL;
	}

	struct wsm_workspace *ws = view->window->pending.workspace;
	window_begin_destroy(view->window);
	if (ws) {
		workspace_consider_destroy(ws);
	}

	if (global_server.scene->fullscreen_global) {
		arrange_root_auto();
	} else if (ws && !ws->node.destroying) {
		wsm_arrange_workspace_auto(ws);
		workspace_detect_urgent(ws);
	}

	struct wsm_seat *seat;
	wl_list_for_each(seat, &global_server.input_manager->seats, link) {
		seat->cursor->image_surface_wlr = NULL;
		if (seat->cursor->active_constraint_wlr) {
			struct wlr_surface *constrain_surface =
					seat->cursor->active_constraint_wlr->surface;
			if (view_from_wlr_surface(constrain_surface) == view) {
				wsm_cursor_constrain(seat->cursor, NULL);
			}
		}
		seat_consider_warp_to_focus(seat);
	}

	transaction_commit_dirty();
	view->surface = NULL;
}

void view_update_size(struct wsm_view *view) {
	struct wsm_window *window = view->window;
	window->pending.content_width = view->geometry.width;
	window->pending.content_height = view->geometry.height;
	window_set_geometry_from_content(window);
}

void view_center_and_clip_surface(struct wsm_view *view) {
	struct wsm_window *window = view->window;
	bool clip_to_geometry = true;
	if (window_is_managed(window)) {
		clip_to_geometry = !view->using_csd;
	} else {
		wlr_scene_node_set_position(&view->content_tree->node, 0, 0);
	}

	if (!wl_list_empty(&window->view->content_tree->children)) {
		struct wlr_box clip = {0};
		if (clip_to_geometry) {
			clip = (struct wlr_box){
				.x = window->view->geometry.x,
				.y = window->view->geometry.y,
				.width = window->current.content_width,
				.height = window->current.content_height,
			};
		}
		wlr_scene_subsurface_tree_set_clip(&window->view->content_tree->node, &clip);
	}
}

struct wsm_view *view_from_wlr_surface(struct wlr_surface *wlr_surface) {
	struct wlr_xdg_surface *xdg_surface;
	if ((xdg_surface = wlr_xdg_surface_try_from_wlr_surface(wlr_surface))) {
		return view_from_wlr_xdg_surface(xdg_surface);
	}
#if HAVE_XWAYLAND
	struct wlr_xwayland_surface *xsurface;
	if ((xsurface = wlr_xwayland_surface_try_from_wlr_surface(wlr_surface))) {
		return view_from_wlr_xwayland_surface(xsurface);
	}
#endif
	struct wlr_subsurface *subsurface;
	if ((subsurface = wlr_subsurface_try_from_wlr_surface(wlr_surface))) {
		return view_from_wlr_surface(subsurface->parent);
	}
	if (wlr_layer_surface_v1_try_from_wlr_surface(wlr_surface) != NULL) {
		return NULL;
	}

	const char *role = wlr_surface->role ? wlr_surface->role->name : NULL;
	wsm_log(WSM_DEBUG, "Surface of unknown type (role %s): %p",
		role, wlr_surface);
	return NULL;
}

static char *escape_pango_markup(const char *buffer) {
	size_t length = escape_markup_text(buffer, NULL);
	char *escaped_title = calloc(length + 1, sizeof(char));
	if (!escaped_title) {
		wsm_log(WSM_ERROR, "Unable to allocate char: allocation failed!");
		return NULL;
	}
	escape_markup_text(buffer, escaped_title);
	return escaped_title;
}

static size_t append_prop(char *buffer, const char *value) {
	if (!value) {
		return 0;
	}
	char *escaped_value = escape_pango_markup(value);
	lenient_strcat(buffer, escaped_value);
	size_t len = strlen(escaped_value);
	free(escaped_value);
	return len;
}

/**
 * Calculate and return the length of the formatted title.
 * If buffer is not NULL, also populate the buffer with the formatted title.
 */
static size_t parse_title_format(struct wsm_view *view, char *buffer) {
	if (!view->title_format || strcmp(view->title_format, "%title") == 0) {
		return append_prop(buffer, view_get_title(view));
	}

	size_t len = 0;
	char *format = view->title_format;
	char *next = strchr(format, '%');
	while (next) {
		lenient_strncat(buffer, format, next - format);
		len += next - format;
		format = next;

		if (strncmp(next, "%title", 6) == 0) {
			len += append_prop(buffer, view_get_title(view));
			format += 6;
		} else if (strncmp(next, "%app_id", 7) == 0) {
			len += append_prop(buffer, view_get_app_id(view));
			format += 7;
		} else if (strncmp(next, "%class", 6) == 0) {
			len += append_prop(buffer, view_get_class(view));
			format += 6;
		} else if (strncmp(next, "%instance", 9) == 0) {
			len += append_prop(buffer, view_get_instance(view));
			format += 9;
		} else if (strncmp(next, "%shell", 6) == 0) {
			len += append_prop(buffer, view_get_shell(view));
			format += 6;
		} else {
			lenient_strcat(buffer, "%");
			++format;
			++len;
		}
		next = strchr(format, '%');
	}
	lenient_strcat(buffer, format);
	len += strlen(format);

	return len;
}

void view_update_app_id(struct wsm_view *view) {
	const char *app_id = view_get_app_id(view);

	if (view->foreign_toplevel && app_id) {
		wlr_foreign_toplevel_handle_v1_set_app_id(view->foreign_toplevel, app_id);
	}
}

void view_update_title(struct wsm_view *view, bool force) {
	const char *title = view_get_title(view);

	if (!force) {
		if (title && view->window->title &&
			strcmp(title, view->window->title) == 0) {
			return;
		}
		if (!title && !view->window->title) {
			return;
		}
	}

	free(view->window->title);
	free(view->window->formatted_title);

	size_t len = parse_title_format(view, NULL);

	if (len) {
		char *buffer = calloc(len + 1, sizeof(char));
		if (!wsm_assert(buffer, "Unable to allocate title string")) {
			return;
		}
		
		parse_title_format(view, buffer);
		view->window->formatted_title = buffer;
	} else {
		view->window->formatted_title = NULL;
	}

	view->window->title = title ? strdup(title) : NULL;

	if (view->window->title_bar->title_text && len) {
		wsm_text_node_set_text(view->window->title_bar->title_text,
			view->window->formatted_title);
		window_arrange_title_bar_node(view->window);
	} else {
		window_update_title_bar(view->window);
	}

	if (view->foreign_toplevel && title) {
		wlr_foreign_toplevel_handle_v1_set_title(view->foreign_toplevel, title);
	}
}

bool view_is_visible(struct wsm_view *view) {
	if (view->window->node.destroying) {
		return false;
	}
	struct wsm_workspace *workspace = view->window->pending.workspace;
	if (!workspace && view->window->pending.fullscreen_mode != FULLSCREEN_GLOBAL) {
		return false;
	}

	if (!window_is_sticky(view->window) && workspace &&
		!workspace_is_visible(workspace)) {
		return false;
	}

	struct wsm_window *fs = global_server.scene->fullscreen_global ?
		global_server.scene->fullscreen_global : workspace->fullscreen;
	if (fs && !window_is_fullscreen(view->window) &&
		!window_is_transient_for(view->window, fs)) {
		return false;
	}
	return true;
}

void view_set_urgent(struct wsm_view *view, bool enable) {
	if (view_is_urgent(view) == enable) {
		return;
	}
	if (enable) {
		struct wsm_seat *seat = input_manager_current_seat();
		if (seat_get_focused_window(seat) == view->window) {
			return;
		}
		clock_gettime(CLOCK_MONOTONIC, &view->urgent);
		window_update_itself_and_parents(view->window);
	} else {
		view->urgent = (struct timespec){ 0 };
		if (view->urgent_timer) {
			wl_event_source_remove(view->urgent_timer);
			view->urgent_timer = NULL;
		}
	}

	if (!window_is_scratchpad_hidden(view->window)) {
		workspace_detect_urgent(view->window->pending.workspace);
	}
}

bool view_is_urgent(struct wsm_view *view) {
	return view->urgent.tv_sec || view->urgent.tv_nsec;
}

void view_remove_saved_buffer(struct wsm_view *view) {
	if (!wsm_assert(view->saved_surface_tree, "Expected a saved buffer")) {
		return;
	}

	wlr_scene_node_destroy(&view->saved_surface_tree->node);
	view->saved_surface_tree = NULL;
	wlr_scene_node_set_enabled(&view->content_tree->node, true);
}

static void view_save_buffer_iterator(struct wlr_scene_buffer *buffer,
		int sx, int sy, void *data) {
	struct wlr_scene_tree *tree = data;

	struct wlr_scene_buffer *sbuf = wlr_scene_buffer_create(tree, NULL);
	if (!sbuf) {
		wsm_log(WSM_ERROR, "Could not allocate a scene buffer when saving a surface");
		return;
	}

	wlr_scene_buffer_set_dest_size(sbuf,
		buffer->dst_width, buffer->dst_height);
	wlr_scene_buffer_set_opaque_region(sbuf, &buffer->opaque_region);
	wlr_scene_buffer_set_source_box(sbuf, &buffer->src_box);
	wlr_scene_node_set_position(&sbuf->node, sx, sy);
	wlr_scene_buffer_set_transform(sbuf, buffer->transform);
	wlr_scene_buffer_set_buffer(sbuf, buffer->buffer);
}

void view_save_buffer(struct wsm_view *view) {
	if (!wsm_assert(!view->saved_surface_tree, "Didn't expect saved buffer")) {
		view_remove_saved_buffer(view);
	}

	view->saved_surface_tree = wlr_scene_tree_create(view->scene_tree);
	if (!view->saved_surface_tree) {
		wsm_log(WSM_ERROR, "Could not allocate a scene tree node when saving a surface");
		return;
	}

	wlr_scene_node_set_enabled(&view->saved_surface_tree->node, false);
	wlr_scene_node_for_each_buffer(&view->content_tree->node,
		view_save_buffer_iterator, view->saved_surface_tree);
	wlr_scene_node_set_enabled(&view->content_tree->node, false);
	wlr_scene_node_set_enabled(&view->saved_surface_tree->node, true);
}

bool view_is_transient_for(struct wsm_view *child, struct wsm_view *ancestor) {
	return child->impl->is_transient_for &&
			child->impl->is_transient_for(child, ancestor);
}

static void send_frame_done_iterator(struct wlr_scene_buffer *scene_buffer,
		int x, int y, void *data) {
	struct timespec *when = data;
	struct wlr_scene_surface *scene_surface =
		wlr_scene_surface_try_from_buffer(scene_buffer);
	if (scene_surface) {
		wlr_scene_surface_send_frame_done(scene_surface, when);
	}
}

void view_send_frame_done(struct wsm_view *view) {
	struct timespec when;
	clock_gettime(CLOCK_MONOTONIC, &when);

	struct wlr_scene_node *node;
	wl_list_for_each(node, &view->content_tree->children, link) {
		wlr_scene_node_for_each_buffer(node, send_frame_done_iterator, &when);
	}
}
