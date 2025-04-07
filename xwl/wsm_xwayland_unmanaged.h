#ifndef WSM_XWAYLAND_UNMANAGED_H
#define WSM_XWAYLAND_UNMANAGED_H

#include "../config.h"

#include <wayland-server-core.h>

#ifdef HAVE_XWAYLAND

struct wlr_scene_surface;
struct wlr_xwayland_surface;

/**
 * @brief Structure representing an unmanaged XWayland surface
 */
struct wsm_xwayland_unmanaged {
	struct wlr_xwayland_surface *wlr_xwayland_surface; /**< Pointer to the WLR XWayland surface */

	struct wlr_scene_surface *surface_scene; /**< Scene surface for the unmanaged surface */

	struct wl_listener request_activate; /**< Listener for activate requests */
	struct wl_listener request_configure; /**< Listener for configure requests */
	struct wl_listener request_fullscreen; /**< Listener for fullscreen requests */
	struct wl_listener set_geometry; /**< Listener for geometry set events */
	struct wl_listener associate; /**< Listener for associate events */
	struct wl_listener dissociate; /**< Listener for dissociate events */
	struct wl_listener map; /**< Listener for map events */
	struct wl_listener unmap; /**< Listener for unmap events */
	struct wl_listener destroy; /**< Listener for destroy events */
	struct wl_listener override_redirect; /**< Listener for override redirect events */
};

struct wsm_xwayland_unmanaged *create_wsm_xwayland_unmanaged(
	struct wlr_xwayland_surface *xsurface);
void wsm_xwayland_unmanaged_destroy(struct wsm_xwayland_unmanaged *surface);
void wsm_xwayland_unmanaged_associate(struct wsm_xwayland_unmanaged *surface);
void wsm_xwayland_unmanaged_dissociate(struct wsm_xwayland_unmanaged *surface);
void wsm_xwayland_unmanaged_unmap(struct wsm_xwayland_unmanaged *surface);
void wsm_xwayland_unmanaged_map(struct wsm_xwayland_unmanaged *surface);

#endif

#endif
