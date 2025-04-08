#include "wsm_xwayland.h"

#if HAVE_XWAYLAND
#include "wsm_server.h"
#include "wsm_log.h"
#include "wsm_view.h"
#include "wsm_output.h"
#include "wsm_scene.h"
#include "wsm_seat.h"
#include "wsm_config.h"
#include "wsm_arrange.h"
#include "wsm_desktop.h"
#include "wsm_workspace.h"
#include "wsm_transaction.h"
#include "wsm_output_manager.h"
#include "wsm_input_manager.h"
#include "node/wsm_node_descriptor.h"
#include "wsm_seatop_move_floating.h"
#include "wsm_seatop_resize_floating.h"
#include "wsm_xwayland_unmanaged.h"

#include <stdlib.h>
#include <assert.h>
#include <float.h>

#include <wayland-util.h>

#include <xcb/xcb.h>
#include <xcb/xcb_icccm.h>

#include <wlr/xwayland.h>
#include <wlr/xwayland/xwayland.h>
#include <wlr/xwayland/server.h>
#include <wlr/xwayland/shell.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_xcursor_manager.h>
#include <wlr/types/wlr_xdg_activation_v1.h>

#define X11_ICON_NAME "xorg"

static const char *atom_map[ATOM_LAST] = {
	[NET_WM_WINDOW_TYPE_NORMAL] = "_NET_WM_WINDOW_TYPE_NORMAL",
	[NET_WM_WINDOW_TYPE_DIALOG] = "_NET_WM_WINDOW_TYPE_DIALOG",
	[NET_WM_WINDOW_TYPE_UTILITY] = "_NET_WM_WINDOW_TYPE_UTILITY",
	[NET_WM_WINDOW_TYPE_TOOLBAR] = "_NET_WM_WINDOW_TYPE_TOOLBAR",
	[NET_WM_WINDOW_TYPE_SPLASH] = "_NET_WM_WINDOW_TYPE_SPLASH",
	[NET_WM_WINDOW_TYPE_MENU] = "_NET_WM_WINDOW_TYPE_MENU",
	[NET_WM_WINDOW_TYPE_DROPDOWN_MENU] = "_NET_WM_WINDOW_TYPE_DROPDOWN_MENU",
	[NET_WM_WINDOW_TYPE_POPUP_MENU] = "_NET_WM_WINDOW_TYPE_POPUP_MENU",
	[NET_WM_WINDOW_TYPE_TOOLTIP] = "_NET_WM_WINDOW_TYPE_TOOLTIP",
	[NET_WM_WINDOW_TYPE_NOTIFICATION] = "_NET_WM_WINDOW_TYPE_NOTIFICATION",
	[NET_WM_STATE_MODAL] = "_NET_WM_STATE_MODAL",
	[NET_WM_ICON] = "_NET_WM_ICON",
	[NET_WM_WINDOW_OPACITY] = "_NET_WM_WINDOW_OPACITY",
	[GTK_APPLICATION_ID] = "_GTK_APPLICATION_ID",
	[UTF8_STRING] = "UTF8_STRING",
};

static const long MAX_PROP_SIZE = 100000;

static void unmanaged_handle_map(struct wl_listener *listener, void *data) {
	struct wsm_xwayland_unmanaged *surface =
		wl_container_of(listener, surface, map);
		wsm_xwayland_unmanaged_map(surface);
}

static void unmanaged_handle_associate(struct wl_listener *listener, void *data) {
	struct wsm_xwayland_unmanaged *surface =
		wl_container_of(listener, surface, associate);
	wsm_xwayland_unmanaged_associate(surface);
}

static struct wsm_xwayland_view *xwayland_view_from_view(
		struct wsm_view *view) {
	if (!wsm_assert(view->type == WSM_VIEW_XWAYLAND,
			"Expected xwayland view")) {
		return NULL;
	}
	return (struct wsm_xwayland_view *)view;
}

static char *get_xwayland_surface_app_id(xcb_get_property_reply_t *reply) {
	const struct wsm_xwayland *xwayland = &global_server.xwayland;
	if (reply->type != XCB_ATOM_STRING &&
			reply->type != xwayland->atoms[UTF8_STRING]) {
		return NULL;
	}

	size_t len = xcb_get_property_value_length(reply);
	char *class = xcb_get_property_value(reply);

	if (len > 0) {
		return strndup(class, len);
	} else {
		return NULL;
	}
}

static const char *get_string_prop(struct wsm_view *view, enum wsm_view_prop prop) {
	if (xwayland_view_from_view(view) == NULL) {
		return NULL;
	}

	switch (prop) {
	case VIEW_PROP_TITLE:
		return view->wlr_xwayland_surface->title;
	case VIEW_PROP_CLASS:
		return view->wlr_xwayland_surface->class;
	case VIEW_PROP_APP_ID:;
		if (view->app_id) {
			return view->app_id;
		}
		struct wsm_xwayland *xwayland = &global_server.xwayland;
		xcb_get_property_cookie_t cookies = xcb_get_property(xwayland->xcb_conn, 0, view->wlr_xwayland_surface->window_id,
			 xwayland->atoms[GTK_APPLICATION_ID], xwayland->atoms[UTF8_STRING], 0, MAX_PROP_SIZE);
		xcb_get_property_reply_t *reply =
			xcb_get_property_reply(xwayland->xcb_conn, cookies, NULL);
		if (reply->type != XCB_ATOM_STRING &&
			reply->type != xwayland->atoms[UTF8_STRING]) {
			char *new_text = strdup(view->wlr_xwayland_surface->instance);
			if (new_text) {
				free(view->app_id);
				view->app_id = new_text;

				view->app_icon_path = find_app_icon_frome_app_id(
					global_server.desktop_interface, view->app_id);
				if (!view->app_icon_path) {
					view->app_icon_path = find_icon_file_frome_theme(
						global_server.desktop_interface, X11_ICON_NAME);
				}
			} else {
				view->app_icon_path = find_icon_file_frome_theme(
					global_server.desktop_interface, X11_ICON_NAME);
			}
		} else {
			char *new_text = get_xwayland_surface_app_id(reply);
			if (new_text) {
				free(view->app_id);
				view->app_id = new_text;

				view->app_icon_path = find_app_icon_frome_app_id(
					global_server.desktop_interface, view->app_id);
				if (!view->app_icon_path) {
					view->app_icon_path = find_icon_file_frome_theme(
						global_server.desktop_interface, X11_ICON_NAME);
				}
			} else {
				view->app_icon_path = find_icon_file_frome_theme(
					global_server.desktop_interface, X11_ICON_NAME);
			}
		}
		return view->app_id;
	case VIEW_PROP_INSTANCE:
		return view->wlr_xwayland_surface->instance;
	case VIEW_PROP_WINDOW_ROLE:
		return view->wlr_xwayland_surface->role;
	default:
		return NULL;
	}
}

static uint32_t get_int_prop(struct wsm_view *view, enum wsm_view_prop prop) {
	if (xwayland_view_from_view(view) == NULL) {
		return 0;
	}

	switch (prop) {
	case VIEW_PROP_X11_WINDOW_ID:
		return view->wlr_xwayland_surface->window_id;
	case VIEW_PROP_X11_PARENT_ID:
		if (view->wlr_xwayland_surface->parent) {
			return view->wlr_xwayland_surface->parent->window_id;
		}
		return 0;
	case VIEW_PROP_WINDOW_TYPE:
		if (view->wlr_xwayland_surface->window_type_len == 0) {
			return 0;
		}
		return view->wlr_xwayland_surface->window_type[0];
	default:
		return 0;
	}
}

static uint32_t configure(struct wsm_view *view, double lx, double ly,
		int width, int height) {
	const struct wsm_xwayland_view *xwayland_view = xwayland_view_from_view(view);
	if (xwayland_view == NULL) {
		return 0;
	}
	struct wlr_xwayland_surface *xsurface = view->wlr_xwayland_surface;

	wlr_xwayland_surface_configure(xsurface, lx, ly, width, height);

	// xwayland doesn't give us a serial for the configure
	return 0;
}

static void set_activated(struct wsm_view *view, bool activated) {
	if (xwayland_view_from_view(view) == NULL) {
		return;
	}

	struct wlr_xwayland_surface *surface = view->wlr_xwayland_surface;

	if (activated && surface->minimized) {
		wlr_xwayland_surface_set_minimized(surface, false);
	}

	wlr_xwayland_surface_activate(surface, activated);
	wlr_xwayland_surface_restack(surface, NULL, XCB_STACK_MODE_ABOVE);
}

static void set_tiled(struct wsm_view *view, bool tiled) {
	if (xwayland_view_from_view(view) == NULL) {
		return;
	}

	struct wlr_xwayland_surface *surface = view->wlr_xwayland_surface;
	wlr_xwayland_surface_set_maximized(surface, tiled);
}

static void set_fullscreen(struct wsm_view *view, bool fullscreen) {
	if (xwayland_view_from_view(view) == NULL) {
		return;
	}

	struct wlr_xwayland_surface *surface = view->wlr_xwayland_surface;
	wlr_xwayland_surface_set_fullscreen(surface, fullscreen);
}

static bool wants_floating(struct wsm_view *view) {
	if (xwayland_view_from_view(view) == NULL) {
		return false;
	}

	struct wlr_xwayland_surface *surface = view->wlr_xwayland_surface;
	const struct wsm_xwayland *xwayland = &global_server.xwayland;

	if (surface->modal) {
		return true;
	}

	for (size_t i = 0; i < surface->window_type_len; ++i) {
		xcb_atom_t type = surface->window_type[i];
		if (type == xwayland->atoms[NET_WM_WINDOW_TYPE_DIALOG] ||
			type == xwayland->atoms[NET_WM_WINDOW_TYPE_UTILITY] ||
			type == xwayland->atoms[NET_WM_WINDOW_TYPE_TOOLBAR] ||
			type == xwayland->atoms[NET_WM_WINDOW_TYPE_SPLASH]) {
			return true;
		}
	}

	const xcb_size_hints_t *size_hints = surface->size_hints;
	if (size_hints != NULL &&
		size_hints->min_width > 0 && size_hints->min_height > 0 &&
		(size_hints->max_width == size_hints->min_width ||
		 size_hints->max_height == size_hints->min_height)) {
		return true;
	}

	return false;
}

static void handle_set_decorations(struct wl_listener *listener, void *data) {
	struct wsm_xwayland_view *xwayland_view =
		wl_container_of(listener, xwayland_view, set_decorations);
	struct wsm_view *view = &xwayland_view->view;
	const struct wlr_xwayland_surface *xsurface = view->wlr_xwayland_surface;

	bool csd = xsurface->decorations != WLR_XWAYLAND_SURFACE_DECORATIONS_ALL;
	view_update_csd_from_client(view, csd);
}

static bool is_transient_for(struct wsm_view *child,
		struct wsm_view *ancestor) {
	if (xwayland_view_from_view(child) == NULL) {
		return false;
	}

	const struct wlr_xwayland_surface *surface = child->wlr_xwayland_surface;
	while (surface) {
		if (surface->parent == ancestor->wlr_xwayland_surface) {
			return true;
		}
		surface = surface->parent;
	}

	return false;
}

static void _maximize(struct wsm_view *view, bool maximize) {
	if (xwayland_view_from_view(view) == NULL) {
		return;
	}
	wlr_xwayland_surface_set_maximized(view->wlr_xwayland_surface, maximize);
}

static void _minimize(struct wsm_view *view, bool minimize) {
	if (xwayland_view_from_view(view) == NULL) {
		return;
	}

	wlr_xwayland_surface_set_minimized(view->wlr_xwayland_surface, minimize);
}

static void _close(struct wsm_view *view) {
	if (xwayland_view_from_view(view) == NULL) {
		return;
	}

	wlr_xwayland_surface_close(view->wlr_xwayland_surface);
}

static void destroy(struct wsm_view *view) {
	struct wsm_xwayland_view *xwayland_view = xwayland_view_from_view(view);
	if (xwayland_view == NULL) {
		return;
	}

	free(xwayland_view);
}

static void get_constraints(struct wsm_view *view, double *min_width,
		double *max_width, double *min_height, double *max_height) {
	const struct wlr_xwayland_surface *surface = view->wlr_xwayland_surface;
	xcb_size_hints_t *size_hints = surface->size_hints;

	if (size_hints == NULL) {
		*min_width = DBL_MIN;
		*max_width = DBL_MAX;
		*min_height = DBL_MIN;
		*max_height = DBL_MAX;
		return;
	}

	*min_width = size_hints->min_width > 0 ? size_hints->min_width : DBL_MIN;
	*max_width = size_hints->max_width > 0 ? size_hints->max_width : DBL_MAX;
	*min_height = size_hints->min_height > 0 ? size_hints->min_height : DBL_MIN;
	*max_height = size_hints->max_height > 0 ? size_hints->max_height : DBL_MAX;
}

static const struct wsm_view_impl view_impl = {
	.get_constraints = get_constraints,
	.get_string_prop = get_string_prop,
	.get_int_prop = get_int_prop,
	.configure = configure,
	.set_activated = set_activated,
	.set_tiled = set_tiled,
	.set_fullscreen = set_fullscreen,
	.wants_floating = wants_floating,
	.is_transient_for = is_transient_for,
	.maximize = _maximize,
	.minimize = _minimize,
	.close = _close,
	.destroy = destroy,
};

static void handle_commit(struct wl_listener *listener, void *data) {
	struct wsm_xwayland_view *xwayland_view =
		wl_container_of(listener, xwayland_view, commit);
	struct wsm_view *view = &xwayland_view->view;
	struct wlr_xwayland_surface *xsurface = view->wlr_xwayland_surface;
	struct wlr_surface_state *state = &xsurface->surface->current;

	struct wlr_box new_geo = {0};
	new_geo.width = state->width;
	new_geo.height = state->height;

	bool new_size = new_geo.width != view->geometry.width ||
		new_geo.height != view->geometry.height;

	if (new_size) {
		// The client changed its surface size in this commit. For floating
		// containers, we resize the container to match. For tiling containers,
		// we only recenter the surface.
		memcpy(&view->geometry, &new_geo, sizeof(struct wlr_box));
		if (container_is_floating(view->container)) {
			view_update_size(view);
			transaction_commit_dirty_client();
		}

		view_center_and_clip_surface(view);
	}

	if (view->container->node.instruction) {
		bool successful = transaction_notify_view_ready_by_geometry(view,
			xsurface->x, xsurface->y, state->width, state->height);

		if (view->saved_surface_tree && !successful) {
			view_send_frame_done(view);
		}
	}
}

static void handle_destroy(struct wl_listener *listener, void *data) {
	struct wsm_xwayland_view *xwayland_view =
		wl_container_of(listener, xwayland_view, destroy);
	struct wsm_view *view = &xwayland_view->view;

	if (view->surface) {
		view_unmap(view);
		wl_list_remove(&xwayland_view->commit.link);
	}

	xwayland_view->view.wlr_xwayland_surface = NULL;

	wl_list_remove(&xwayland_view->destroy.link);
	wl_list_remove(&xwayland_view->request_configure.link);
	wl_list_remove(&xwayland_view->request_fullscreen.link);
	wl_list_remove(&xwayland_view->request_minimize.link);
	wl_list_remove(&xwayland_view->request_move.link);
	wl_list_remove(&xwayland_view->request_resize.link);
	wl_list_remove(&xwayland_view->request_activate.link);
	wl_list_remove(&xwayland_view->set_title.link);
	wl_list_remove(&xwayland_view->set_class.link);
	wl_list_remove(&xwayland_view->set_role.link);
	wl_list_remove(&xwayland_view->set_startup_id.link);
	wl_list_remove(&xwayland_view->set_window_type.link);
	wl_list_remove(&xwayland_view->set_hints.link);
	wl_list_remove(&xwayland_view->set_decorations.link);
	wl_list_remove(&xwayland_view->associate.link);
	wl_list_remove(&xwayland_view->dissociate.link);
	wl_list_remove(&xwayland_view->override_redirect.link);

	view_begin_destroy(&xwayland_view->view);
}

static void handle_unmap(struct wl_listener *listener, void *data) {
	struct wsm_xwayland_view *xwayland_view =
		wl_container_of(listener, xwayland_view, unmap);
	struct wsm_view *view = &xwayland_view->view;

	if (!wsm_assert(view->surface, "Cannot unmap unmapped view")) {
		return;
	}

	wl_list_remove(&xwayland_view->commit.link);
	wl_list_remove(&xwayland_view->surface_tree_destroy.link);

	if (xwayland_view->surface_tree) {
		wlr_scene_node_destroy(&xwayland_view->surface_tree->node);
		xwayland_view->surface_tree = NULL;
	}

	view_unmap(view);
}

static void handle_surface_tree_destroy(struct wl_listener *listener, void *data) {
	struct wsm_xwayland_view *xwayland_view = wl_container_of(listener, xwayland_view,
		surface_tree_destroy);
	xwayland_view->surface_tree = NULL;
}

void wsm_xwayland_map(struct wsm_xwayland_view *xwayland_view) {
	struct wsm_view *view = &xwayland_view->view;
	struct wlr_xwayland_surface *xsurface = view->wlr_xwayland_surface;

	view->natural_width = xsurface->width;
	view->natural_height = xsurface->height;

	wl_signal_add(&xsurface->surface->events.commit, &xwayland_view->commit);
	xwayland_view->commit.notify = handle_commit;

	view_map(view, xsurface->surface, xsurface->fullscreen, NULL, false);

	xwayland_view->surface_tree = wlr_scene_subsurface_tree_create(
		xwayland_view->view.content_tree, xsurface->surface);

	if (xwayland_view->surface_tree) {
		xwayland_view->surface_tree_destroy.notify = handle_surface_tree_destroy;
		wl_signal_add(&xwayland_view->surface_tree->node.events.destroy,
			 &xwayland_view->surface_tree_destroy);
	}

	transaction_commit_dirty();
}

static void handle_map(struct wl_listener *listener, void *data) {
	struct wsm_xwayland_view *xwayland_view =
		wl_container_of(listener, xwayland_view, map);
	wsm_xwayland_map(xwayland_view);
}

static void handle_dissociate(struct wl_listener *listener, void *data);

static void handle_override_redirect(struct wl_listener *listener, void *data) {
	struct wsm_xwayland_view *xwayland_view =
		wl_container_of(listener, xwayland_view, override_redirect);
	struct wsm_view *view = &xwayland_view->view;
	struct wlr_xwayland_surface *xsurface = view->wlr_xwayland_surface;

	bool associated = xsurface->surface != NULL;
	bool mapped = associated && xsurface->surface->mapped;
	if (mapped) {
		handle_unmap(&xwayland_view->unmap, NULL);
	}

	if (associated) {
		handle_dissociate(&xwayland_view->dissociate, NULL);
	}

	handle_destroy(&xwayland_view->destroy, view);
	xsurface->data = NULL;

	struct wsm_xwayland_unmanaged *unmanaged = create_wsm_xwayland_unmanaged(xsurface);
	assert(unmanaged);
	if (associated) {
		unmanaged_handle_associate(&unmanaged->associate, NULL);
	}

	if (mapped) {
		unmanaged_handle_map(&unmanaged->map, xsurface);
	}
}

static void handle_request_configure(struct wl_listener *listener, void *data) {
	struct wsm_xwayland_view *xwayland_view =
		wl_container_of(listener, xwayland_view, request_configure);
	struct wlr_xwayland_surface_configure_event *ev = data;
	struct wsm_view *view = &xwayland_view->view;
	struct wlr_xwayland_surface *xsurface = view->wlr_xwayland_surface;
	if (xsurface->surface == NULL || !xsurface->surface->mapped) {
		wlr_xwayland_surface_configure(xsurface, ev->x, ev->y,
			ev->width, ev->height);
		return;
	}

	if (container_is_floating(view->container)) {
		// Respect minimum and maximum sizes
		view->natural_width = ev->width;
		view->natural_height = ev->height;
		container_floating_resize_and_center(view->container);

		configure(view, view->container->pending.content_x,
			 view->container->pending.content_y,
			view->container->pending.content_width,
			view->container->pending.content_height);
		node_set_dirty(&view->container->node);
	} else {
		configure(view, view->container->current.content_x,
			view->container->current.content_y,
			view->container->current.content_width,
			view->container->current.content_height);
	}
}

static void handle_request_fullscreen(struct wl_listener *listener, void *data) {
	struct wsm_xwayland_view *xwayland_view =
		wl_container_of(listener, xwayland_view, request_fullscreen);
	struct wsm_view *view = &xwayland_view->view;
	struct wlr_xwayland_surface *xsurface = view->wlr_xwayland_surface;
	if (xsurface->surface == NULL || !xsurface->surface->mapped) {
		return;
	}

	container_set_fullscreen(view->container, xsurface->fullscreen);
	arrange_root_auto();
	transaction_commit_dirty();
}

static void handle_request_minimize(struct wl_listener *listener, void *data) {
	struct wsm_xwayland_view *xwayland_view =
		wl_container_of(listener, xwayland_view, request_minimize);
	struct wsm_view *view = &xwayland_view->view;
	struct wlr_xwayland_surface *xsurface = view->wlr_xwayland_surface;
	if (xsurface->surface == NULL || !xsurface->surface->mapped) {
		return;
	}

	struct wlr_xwayland_minimize_event *e = data;
	struct wsm_seat *seat = input_manager_current_seat();
	bool focused = seat_get_focus(seat) == &view->container->node;
	wlr_xwayland_surface_set_minimized(xsurface, !focused && e->minimize);
}

static void handle_request_move(struct wl_listener *listener, void *data) {
	struct wsm_xwayland_view *xwayland_view =
		wl_container_of(listener, xwayland_view, request_move);
	struct wsm_view *view = &xwayland_view->view;
	const struct wlr_xwayland_surface *xsurface = view->wlr_xwayland_surface;
	if (xsurface->surface == NULL || !xsurface->surface->mapped) {
		return;
	}

	if (!container_is_floating(view->container) ||
		view->container->pending.fullscreen_mode) {
		return;
	}

	struct wsm_seat *seat = input_manager_current_seat();
	seatop_begin_move_floating(seat, view->container);
}

static void handle_request_resize(struct wl_listener *listener, void *data) {
	struct wsm_xwayland_view *xwayland_view =
		wl_container_of(listener, xwayland_view, request_resize);
	struct wsm_view *view = &xwayland_view->view;
	const struct wlr_xwayland_surface *xsurface = view->wlr_xwayland_surface;
	if (xsurface->surface == NULL || !xsurface->surface->mapped) {
		return;
	}

	if (!container_is_floating(view->container)) {
		return;
	}

	struct wlr_xwayland_resize_event *e = data;
	struct wsm_seat *seat = input_manager_current_seat();
	seatop_begin_resize_floating(seat, view->container, e->edges);
}

static void handle_request_activate(struct wl_listener *listener, void *data) {
	struct wsm_xwayland_view *xwayland_view =
		wl_container_of(listener, xwayland_view, request_activate);
	struct wsm_view *view = &xwayland_view->view;
	const struct wlr_xwayland_surface *xsurface = view->wlr_xwayland_surface;
	if (xsurface->surface == NULL || !xsurface->surface->mapped) {
		return;
	}

	view_request_activate(view, NULL);
	transaction_commit_dirty();
}

static void handle_set_title(struct wl_listener *listener, void *data) {
	struct wsm_xwayland_view *xwayland_view =
		wl_container_of(listener, xwayland_view, set_title);
	struct wsm_view *view = &xwayland_view->view;
	const struct wlr_xwayland_surface *xsurface = view->wlr_xwayland_surface;
	if (xsurface->surface == NULL || !xsurface->surface->mapped) {
		return;
	}

	view_update_title(view, false);
}

static void handle_set_class(struct wl_listener *listener, void *data) {
	struct wsm_xwayland_view *xwayland_view =
		wl_container_of(listener, xwayland_view, set_class);
	const struct wsm_view *view = &xwayland_view->view;
	const struct wlr_xwayland_surface *xsurface = view->wlr_xwayland_surface;
	if (xsurface->surface == NULL || !xsurface->surface->mapped) {
		return;
	}
}

static void handle_set_role(struct wl_listener *listener, void *data) {
	struct wsm_xwayland_view *xwayland_view =
		wl_container_of(listener, xwayland_view, set_role);
	const struct wsm_view *view = &xwayland_view->view;
	const struct wlr_xwayland_surface *xsurface = view->wlr_xwayland_surface;
	if (xsurface->surface == NULL || !xsurface->surface->mapped) {
		return;
	}
}

static void handle_set_startup_id(struct wl_listener *listener, void *data) {
	struct wsm_xwayland_view *xwayland_view =
		wl_container_of(listener, xwayland_view, set_startup_id);
	struct wsm_view *view = &xwayland_view->view;
	struct wlr_xwayland_surface *xsurface = view->wlr_xwayland_surface;
	if (xsurface->startup_id == NULL) {
		return;
	}

	const struct wlr_xdg_activation_token_v1 *token = wlr_xdg_activation_v1_find_token(
		global_server.xdg_activation_v1, xsurface->startup_id);
	if (token == NULL) {
		return;
	}
}

static void handle_set_window_type(struct wl_listener *listener, void *data) {
	struct wsm_xwayland_view *xwayland_view =
		wl_container_of(listener, xwayland_view, set_window_type);
	const struct wsm_view *view = &xwayland_view->view;
	const struct wlr_xwayland_surface *xsurface = view->wlr_xwayland_surface;
	if (xsurface->surface == NULL || !xsurface->surface->mapped) {
		return;
	}
}

static void handle_set_hints(struct wl_listener *listener, void *data) {
	struct wsm_xwayland_view *xwayland_view =
		wl_container_of(listener, xwayland_view, set_hints);
	struct wsm_view *view = &xwayland_view->view;
	struct wlr_xwayland_surface *xsurface = view->wlr_xwayland_surface;
	if (xsurface->surface == NULL || !xsurface->surface->mapped) {
		return;
	}

	const bool hints_urgency = xcb_icccm_wm_hints_get_urgency(xsurface->hints);
	if (!hints_urgency && view->urgent_timer) {
		return;
	}

	if (view->allow_request_urgent) {
		view_set_urgent(view, hints_urgency);
	}
}

static void handle_associate(struct wl_listener *listener, void *data) {
	struct wsm_xwayland_view *xwayland_view =
		wl_container_of(listener, xwayland_view, associate);
	wsm_xwayland_associate(xwayland_view);
}

void wsm_xwayland_associate(struct wsm_xwayland_view *xwayland_view) {
	struct wlr_xwayland_surface *xsurface =
	xwayland_view->view.wlr_xwayland_surface;
	wl_signal_add(&xsurface->surface->events.unmap, &xwayland_view->unmap);
	xwayland_view->unmap.notify = handle_unmap;
	wl_signal_add(&xsurface->surface->events.map, &xwayland_view->map);
	xwayland_view->map.notify = handle_map;
}

static void handle_dissociate(struct wl_listener *listener, void *data) {
	struct wsm_xwayland_view *xwayland_view =
		wl_container_of(listener, xwayland_view, dissociate);
	wl_list_remove(&xwayland_view->map.link);
	wl_list_remove(&xwayland_view->unmap.link);
}

struct wsm_xwayland_view *create_xwayland_view(struct wlr_xwayland_surface *xsurface) {
	wsm_log(WSM_DEBUG, "New xwayland surface title='%s' class='%s'",
		xsurface->title, xsurface->class);

	struct wsm_xwayland_view *xwayland_view =
		calloc(1, sizeof(struct wsm_xwayland_view));
	if (!xwayland_view) {
		wsm_log(WSM_ERROR, "Could not create wsm_xwayland_view: allocation failed!");
		return NULL;
	}

	if (!view_init(&xwayland_view->view, WSM_VIEW_XWAYLAND, &view_impl)) {
		free(xwayland_view);
		return NULL;
	}

	xwayland_view->view.wlr_xwayland_surface = xsurface;

	wl_signal_add(&xsurface->events.destroy, &xwayland_view->destroy);
	xwayland_view->destroy.notify = handle_destroy;

	wl_signal_add(&xsurface->events.request_configure,
		&xwayland_view->request_configure);
	xwayland_view->request_configure.notify = handle_request_configure;

	wl_signal_add(&xsurface->events.request_fullscreen,
		&xwayland_view->request_fullscreen);
	xwayland_view->request_fullscreen.notify = handle_request_fullscreen;

	wl_signal_add(&xsurface->events.request_minimize,
		&xwayland_view->request_minimize);
	xwayland_view->request_minimize.notify = handle_request_minimize;

	wl_signal_add(&xsurface->events.request_activate,
		&xwayland_view->request_activate);
	xwayland_view->request_activate.notify = handle_request_activate;

	wl_signal_add(&xsurface->events.request_move,
		&xwayland_view->request_move);
	xwayland_view->request_move.notify = handle_request_move;

	wl_signal_add(&xsurface->events.request_resize,
		&xwayland_view->request_resize);
	xwayland_view->request_resize.notify = handle_request_resize;

	wl_signal_add(&xsurface->events.set_title, &xwayland_view->set_title);
	xwayland_view->set_title.notify = handle_set_title;

	wl_signal_add(&xsurface->events.set_class, &xwayland_view->set_class);
	xwayland_view->set_class.notify = handle_set_class;

	wl_signal_add(&xsurface->events.set_role, &xwayland_view->set_role);
	xwayland_view->set_role.notify = handle_set_role;

	wl_signal_add(&xsurface->events.set_startup_id,
		&xwayland_view->set_startup_id);
	xwayland_view->set_startup_id.notify = handle_set_startup_id;

	wl_signal_add(&xsurface->events.set_window_type,
		&xwayland_view->set_window_type);
	xwayland_view->set_window_type.notify = handle_set_window_type;

	wl_signal_add(&xsurface->events.set_hints, &xwayland_view->set_hints);
	xwayland_view->set_hints.notify = handle_set_hints;

	wl_signal_add(&xsurface->events.set_decorations,
		&xwayland_view->set_decorations);
	xwayland_view->set_decorations.notify = handle_set_decorations;

	wl_signal_add(&xsurface->events.associate, &xwayland_view->associate);
	xwayland_view->associate.notify = handle_associate;

	wl_signal_add(&xsurface->events.dissociate, &xwayland_view->dissociate);
	xwayland_view->dissociate.notify = handle_dissociate;

	wl_signal_add(&xsurface->events.set_override_redirect,
		&xwayland_view->override_redirect);
	xwayland_view->override_redirect.notify = handle_override_redirect;

	xsurface->data = xwayland_view;

	return xwayland_view;
}

void handle_xwayland_surface(struct wl_listener *listener, void *data) {
	struct wlr_xwayland_surface *xsurface = data;

	if (xsurface->override_redirect) {
		wsm_log(WSM_DEBUG, "New xwayland unmanaged surface");
		create_wsm_xwayland_unmanaged(xsurface);
		return;
	}

	create_xwayland_view(xsurface);
}

void handle_xwayland_ready(struct wl_listener *listener, void *data) {
	struct wsm_server *server =
		wl_container_of(listener, server, xwayland_ready);
	struct wsm_xwayland *xwayland = &server->xwayland;

	xwayland->xcb_conn = xcb_connect(NULL, NULL);
	int err = xcb_connection_has_error(xwayland->xcb_conn);
	if (err) {
		wsm_log(WSM_ERROR, "XCB connect failed: %d", err);
		return;
	}

	xcb_intern_atom_cookie_t cookies[ATOM_LAST];
	for (size_t i = 0; i < ATOM_LAST; i++) {
		cookies[i] =
			xcb_intern_atom(xwayland->xcb_conn, 0, strlen(atom_map[i]), atom_map[i]);
	}

	for (size_t i = 0; i < ATOM_LAST; i++) {
		xcb_generic_error_t *error = NULL;
		xcb_intern_atom_reply_t *reply =
			xcb_intern_atom_reply(xwayland->xcb_conn, cookies[i], &error);
		if (reply != NULL && error == NULL) {
			xwayland->atoms[i] = reply->atom;
		}

		free(reply);

		if (error != NULL) {
			wsm_log(WSM_ERROR, "could not resolve atom %s, X11 error code %d",
				atom_map[i], error->error_code);
			free(error);
			break;
		}
	}
}

/**
 * @brief xwayland_start start a xwayland server
 * @param server
 * @return true represents successed; false maybe failed to 
 * start Xwayland or wsm disabled Xwayland
 */
bool xwayland_start(struct wsm_server *server) {
	if (global_server.xwayland_enabled) {
		wsm_log(WSM_DEBUG, "Initializing Xwayland (lazy=%d)",
			global_config.xwayland == XWAYLAND_MODE_LAZY);
		server->xwayland.xwayland_wlr = wlr_xwayland_create(server->wl_display, server->wlr_compositor,
			global_config.xwayland  == XWAYLAND_MODE_LAZY);
		if (!server->xwayland.xwayland_wlr) {
			wsm_log(WSM_ERROR, "Failed to start Xwayland");
			unsetenv("DISPLAY");

			return false;
		} else {
			server->xwayland_surface.notify = handle_xwayland_surface;
			wl_signal_add(&server->xwayland.xwayland_wlr->events.new_surface,
				&server->xwayland_surface);
			server->xwayland_ready.notify = handle_xwayland_ready;
			wl_signal_add(&server->xwayland.xwayland_wlr->events.ready,
				&server->xwayland_ready);
			setenv("DISPLAY", server->xwayland.xwayland_wlr->display_name, true);
		}
	}

	return true;
}

struct wsm_view *view_from_wlr_xwayland_surface(
	const struct wlr_xwayland_surface *xsurface) {
	return xsurface->data;
}

#endif
