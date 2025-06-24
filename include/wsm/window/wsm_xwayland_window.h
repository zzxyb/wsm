#ifndef XWL_WSM_XWAYLAND_WINDOW_H
#define XWL_WSM_XWAYLAND_WINDOW_H

#include "wsm/window/wsm_window.h"

#include <stdint.h>

#include <wayland-server-core.h>

struct wlr_xwayland_surface;

struct wsm_xwayland_window {
	struct wsm_window base;

	struct wl_listener commit; /**< Listener for commit events */
	struct wl_listener request_move; /**< Listener for move requests */
	struct wl_listener request_resize; /**< Listener for resize requests */
	struct wl_listener request_maximize; /**< Listener for maximize requests */
	struct wl_listener request_minimize; /**< Listener for minimize requests */
	struct wl_listener request_configure; /**< Listener for configure requests */
	struct wl_listener request_fullscreen; /**< Listener for fullscreen requests */
	struct wl_listener request_activate; /**< Listener for activate requests */
	struct wl_listener set_title; /**< Listener for title set events */
	struct wl_listener set_class; /**< Listener for class set events */
	struct wl_listener set_role; /**< Listener for role set events */
	struct wl_listener set_startup_id; /**< Listener for startup ID set events */
	struct wl_listener set_window_type; /**< Listener for window type set events */
	struct wl_listener set_hints; /**< Listener for hints set events */
	struct wl_listener set_decorations; /**< Listener for decorations set events */
	struct wl_listener associate; /**< Listener for associate events */
	struct wl_listener dissociate; /**< Listener for dissociate events */
	struct wl_listener map; /**< Listener for map events */
	struct wl_listener unmap; /**< Listener for unmap events */
	struct wl_listener destroy; /**< Listener for destroy events */
	struct wl_listener override_redirect; /**< Listener for override redirect events */
	struct wl_listener surface_tree_destroy; /**< Listener for surface tree destroy events */

	struct wlr_scene_tree *surface_tree; /**< Scene tree for the surface */
	struct wlr_xwayland_surface *wlr_xwayland_surface;
};

uint32_t wsm_xwayland_window_get_x11_window_id(struct wsm_xwayland_window *window);
uint32_t wsm_xwayland_window_get_x11_parent_id(struct wsm_xwayland_window *window);

#endif // XWL_WSM_XWAYLAND_WINDOW_H
