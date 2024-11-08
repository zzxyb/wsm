#ifndef WSM_DECORATION_MANAGER_H
#define WSM_DECORATION_MANAGER_H

#include <wayland-server-core.h>

struct wsm_server;
struct wlr_server_decoration_manager;

/**
 * @brief Structure representing the server decoration manager in the WSM
 */
struct wsm_server_decoration_manager {
	struct wl_listener server_decoration; /**< Listener for server decoration events */

	struct wl_list decorations; /**< List of decorations managed by the server decoration manager */

	struct wlr_server_decoration_manager *server_decoration_manager_wlr; /**< Pointer to the WLR server decoration manager */
};

/**
 * @brief Initializes the decoration manager
 */
void decoration_manager_init();

/**
 * @brief Creates a new wsm_server_decoration_manager instance
 * @param server Pointer to the WSM server associated with the decoration manager
 * @return Pointer to the newly created wsm_server_decoration_manager instance
 */
struct wsm_server_decoration_manager *wsm_server_decoration_manager_create(const struct wsm_server* server);

#endif
