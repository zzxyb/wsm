#ifndef XWL_WSM_XWAYLAND_WINDOW_H
#define XWL_WSM_XWAYLAND_WINDOW_H

#include "wsm/window/wsm_window.h"

#include <stdint.h>

#include <wayland-server-core.h>

struct wlr_xdg_toplevel;

struct wsm_xdg_window {
	struct wsm_window base;

	struct wl_listener commit; /**< Listener for commit events */
	struct wl_listener request_move; /**< Listener for move requests */
	struct wl_listener request_resize; /**< Listener for resize requests */
	struct wl_listener request_maximize; /**< Listener for maximize requests */
	struct wl_listener request_fullscreen; /**< Listener for fullscreen requests */
	struct wl_listener set_title; /**< Listener for title set events */
	struct wl_listener set_app_id; /**< Listener for application ID set events */
	struct wl_listener new_popup; /**< Listener for new popup events */
	struct wl_listener map; /**< Listener for map events */
	struct wl_listener unmap; /**< Listener for unmap events */
	struct wl_listener destroy; /**< Listener for destroy events */

	struct wlr_xdg_toplevel *wlr_xdg_toplevel;
};

#endif // XWL_WSM_XWAYLAND_WINDOW_H
