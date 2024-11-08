/** \file
 *
 *  \brief Service core, manages global resources and initialization.
 *
 */

#ifndef WSM_XDG_DECORATION_H
#define WSM_XDG_DECORATION_H

#include <wayland-server-core.h>

struct wlr_surface;
struct wlr_xdg_toplevel_decoration_v1;

struct wsm_view;

/**
 * @brief Structure representing an XDG decoration in the WSM
 */
struct wsm_xdg_decoration {
	struct wl_listener destroy; /**< Listener for destruction events */
	struct wl_listener request_mode; /**< Listener for mode request events */

	struct wl_list link; /**< Link for managing a list of XDG decorations */

	struct wlr_xdg_toplevel_decoration_v1 *xdg_decoration_wlr; /**< Pointer to the WLR XDG toplevel decoration instance */

	struct wsm_view *view; /**< Pointer to the associated WSM view */
};

/**
 * @brief Handles XDG decoration events
 * @param listener Pointer to the wl_listener instance
 * @param data Pointer to the event data
 */
void handle_xdg_decoration(struct wl_listener *listener, void *data);

/**
 * @brief Retrieves the XDG decoration associated with the specified surface
 * @param surface Pointer to the WLR surface to retrieve the decoration from
 * @return Pointer to the associated wsm_xdg_decoration instance
 */
struct wsm_xdg_decoration *wsm_server_decoration_from_surface(
	struct wlr_surface *surface);

/**
 * @brief Sets the mode for the specified XDG decoration
 * @param deco Pointer to the wsm_xdg_decoration instance to configure
 */
void set_xdg_decoration_mode(struct wsm_xdg_decoration *deco);

#endif
