#include "wsm_xwayland_unmanaged.h"

#if HAVE_XWAYLAND
#include "wsm_server.h"
#include "wsm_log.h"
#include "wsm_view.h"
#include "wsm_scene.h"
#include "wsm_seat.h"
#include "wsm_arrange.h"
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

static void unmanaged_handle_unmap(struct wl_listener *listener, void *data) {
	struct wsm_xwayland_unmanaged *surface =
		wl_container_of(listener, surface, unmap);
	wsm_xwayland_unmanaged_unmap(surface);
}

static void unmanaged_handle_request_configure(struct wl_listener *listener,
	void *data) {
	struct wsm_xwayland_unmanaged *surface =
		wl_container_of(listener, surface, request_configure);
	struct wlr_xwayland_surface *xsurface = surface->wlr_xwayland_surface;
	struct wlr_xwayland_surface_configure_event *ev = data;
	wlr_xwayland_surface_configure(xsurface, ev->x, ev->y,
		ev->width, ev->height);
}

static void unmanaged_handle_associate(struct wl_listener *listener, void *data) {
	struct wsm_xwayland_unmanaged *surface =
		wl_container_of(listener, surface, associate);
	wsm_xwayland_unmanaged_associate(surface);
}

static void unmanaged_handle_dissociate(struct wl_listener *listener, void *data) {
	struct wsm_xwayland_unmanaged *surface =
		wl_container_of(listener, surface, dissociate);
	wsm_xwayland_unmanaged_dissociate(surface);
}

static void unmanaged_handle_destroy(struct wl_listener *listener, void *data) {
	struct wsm_xwayland_unmanaged *surface =
		wl_container_of(listener, surface, destroy);
	wsm_xwayland_unmanaged_destroy(surface);
}

static void unmanaged_handle_override_redirect(struct wl_listener *listener, void *data) {
	struct wsm_xwayland_unmanaged *surface =
		wl_container_of(listener, surface, override_redirect);
	struct wlr_xwayland_surface *xsurface = surface->wlr_xwayland_surface;

	bool associated = xsurface->surface != NULL;
	bool mapped = associated && xsurface->surface->mapped;
	if (mapped) {
		unmanaged_handle_unmap(&surface->unmap, NULL);
	}
	if (associated) {
		unmanaged_handle_dissociate(&surface->dissociate, NULL);
	}

	unmanaged_handle_destroy(&surface->destroy, NULL);
	xsurface->data = NULL;

	struct wsm_xwayland_view *xwayland_view = create_xwayland_view(xsurface);
	if (associated) {
		wsm_xwayland_associate(xwayland_view);
	}
	if (mapped) {
		wsm_xwayland_map(xwayland_view);
	}
}

static void unmanaged_handle_request_activate(struct wl_listener *listener, void *data) {
	struct wsm_xwayland_unmanaged *surface =
		wl_container_of(listener, surface, request_activate);
	struct wlr_xwayland_surface *xsurface = surface->wlr_xwayland_surface;
	if (xsurface->surface == NULL || !xsurface->surface->mapped) {
		return;
	}
	struct wsm_seat *seat = input_manager_current_seat();
	struct wsm_container *focus = seat_get_focused_container(seat);
	if (focus && focus->view && focus->view->pid != xsurface->pid) {
		return;
	}

	seat_set_focus_surface(seat, xsurface->surface, false);
}

static void unmanaged_handle_set_geometry(struct wl_listener *listener, void *data) {
	struct wsm_xwayland_unmanaged *surface =
		wl_container_of(listener, surface, set_geometry);
	struct wlr_xwayland_surface *xsurface = surface->wlr_xwayland_surface;

	wlr_scene_node_set_position(&surface->surface_scene->buffer->node, xsurface->x, xsurface->y);
}

static void unmanaged_handle_map(struct wl_listener *listener, void *data) {
	struct wsm_xwayland_unmanaged *surface =
		wl_container_of(listener, surface, map);
		wsm_xwayland_unmanaged_map(surface);
}

struct wsm_xwayland_unmanaged *create_wsm_xwayland_unmanaged(
		struct wlr_xwayland_surface *xsurface) {
	struct wsm_xwayland_unmanaged *surface =
		calloc(1, sizeof(struct wsm_xwayland_unmanaged));
	if (!surface) {
		wsm_log(WSM_ERROR, "Could not create wsm_xwayland_unmanaged: allocation failed!");
		return NULL;
	}

	surface->wlr_xwayland_surface = xsurface;

	wl_signal_add(&xsurface->events.request_configure,
		&surface->request_configure);
	surface->request_configure.notify = unmanaged_handle_request_configure;
	wl_signal_add(&xsurface->events.associate, &surface->associate);
	surface->associate.notify = unmanaged_handle_associate;
	wl_signal_add(&xsurface->events.dissociate, &surface->dissociate);
	surface->dissociate.notify = unmanaged_handle_dissociate;
	wl_signal_add(&xsurface->events.destroy, &surface->destroy);
	surface->destroy.notify = unmanaged_handle_destroy;
	wl_signal_add(&xsurface->events.set_override_redirect, &surface->override_redirect);
	surface->override_redirect.notify = unmanaged_handle_override_redirect;
	wl_signal_add(&xsurface->events.request_activate, &surface->request_activate);
	surface->request_activate.notify = unmanaged_handle_request_activate;

	return surface;
}

void wsm_xwayland_unmanaged_destroy(struct wsm_xwayland_unmanaged *surface) {
	wl_list_remove(&surface->request_configure.link);
	wl_list_remove(&surface->associate.link);
	wl_list_remove(&surface->dissociate.link);
	wl_list_remove(&surface->destroy.link);
	wl_list_remove(&surface->override_redirect.link);
	wl_list_remove(&surface->request_activate.link);
	free(surface);
}

void wsm_xwayland_unmanaged_associate(struct wsm_xwayland_unmanaged *surface) {
	struct wlr_xwayland_surface *xsurface = surface->wlr_xwayland_surface;
	wl_signal_add(&xsurface->surface->events.map, &surface->map);
	surface->map.notify = unmanaged_handle_map;
	wl_signal_add(&xsurface->surface->events.unmap, &surface->unmap);
	surface->unmap.notify = unmanaged_handle_unmap;
}

void wsm_xwayland_unmanaged_dissociate(struct wsm_xwayland_unmanaged *surface) {
	wl_list_remove(&surface->map.link);
	wl_list_remove(&surface->unmap.link);
}

void wsm_xwayland_unmanaged_unmap(struct wsm_xwayland_unmanaged *surface) {
	struct wlr_xwayland_surface *xsurface = surface->wlr_xwayland_surface;
	if (surface->surface_scene) {
		wl_list_remove(&surface->set_geometry.link);
		
		wlr_scene_node_destroy(&surface->surface_scene->buffer->node);
		surface->surface_scene = NULL;
	}

	struct wsm_seat *seat = input_manager_current_seat();
	if (seat->seat->keyboard_state.focused_surface == xsurface->surface) {
		// This simply returns focus to the parent surface if there's one available.
		// This seems to handle JetBrains issues.
		if (xsurface->parent && xsurface->parent->surface
				&& wlr_xwayland_or_surface_wants_focus(xsurface->parent)) {
			seat_set_focus_surface(seat, xsurface->parent->surface, false);
			return;
		}

		// Restore focus
		struct wsm_node *previous = seat_get_focus_inactive(seat, &global_server.scene->node);
		if (previous) {
			// Hack to get seat to re-focus the return value of get_focus
			seat_set_focus(seat, NULL);
			seat_set_focus(seat, previous);
		}
	}
}

void wsm_xwayland_unmanaged_map(struct wsm_xwayland_unmanaged *surface) {
		struct wlr_xwayland_surface *xsurface = surface->wlr_xwayland_surface;

	surface->surface_scene = wlr_scene_surface_create(global_server.scene->layers.unmanaged,
		xsurface->surface);

	if (surface->surface_scene) {
		wsm_scene_descriptor_assign(&surface->surface_scene->buffer->node,
			WSM_SCENE_DESC_XWAYLAND_UNMANAGED, surface);
		wlr_scene_node_set_position(&surface->surface_scene->buffer->node,
			xsurface->x, xsurface->y);

		wl_signal_add(&xsurface->events.set_geometry, &surface->set_geometry);
		surface->set_geometry.notify = unmanaged_handle_set_geometry;
	}

	if (wlr_xwayland_or_surface_wants_focus(xsurface)) {
		struct wsm_seat *seat = input_manager_current_seat();
		struct wlr_xwayland *xwayland = global_server.xwayland.xwayland_wlr;
		wlr_xwayland_set_seat(xwayland, seat->seat);
		seat_set_focus_surface(seat, xsurface->surface, false);
	}
}

#endif
