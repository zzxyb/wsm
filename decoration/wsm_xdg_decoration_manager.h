#ifndef WSM_XDG_DECORATION_MANAGER_H
#define WSM_XDG_DECORATION_MANAGER_H

#include <wayland-server-core.h>

struct wlr_xdg_decoration_manager_v1;

struct wsm_server;

/**
 * @brief Structure representing the XDG decoration manager in the WSM
 */
struct wsm_xdg_decoration_manager {
	struct wl_listener xdg_decoration; /**< Listener for XDG decoration events */
	struct wl_list xdg_decorations; /**< List of XDG decorations managed by the manager */
	struct wlr_xdg_decoration_manager_v1 *wlr_xdg_decoration_manager; /**< Pointer to the WLR XDG decoration manager */
};

/**
 * @brief Creates a new XDG decoration manager instance
 * @param server Pointer to the WSM server associated with the decoration manager
 * @return Pointer to the newly created wsm_xdg_decoration_manager instance
 */
struct wsm_xdg_decoration_manager *xdg_decoration_manager_create(const struct wsm_server* server);

#endif
