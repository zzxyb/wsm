#ifndef WSM_DECORATION_H
#define WSM_DECORATION_H

#include <wayland-server-core.h>

struct wlr_surface;
struct wlr_server_decoration;
struct wlr_server_decoration_manager;
struct wsm_server;

/**
 * @brief Structure representing the server decoration manager in the WSM
 */
struct wsm_server_decoration_manager {
	struct wl_listener server_decoration; /**< Listener for server decoration events */

	struct wl_list decorations; /**< List of decorations managed by the server decoration manager */

	struct wlr_server_decoration_manager *server_decoration_manager_wlr; /**< Pointer to the WLR server decoration manager */
};

/**
 * @brief Structure representing a server decoration in the WSM
 */
struct wsm_server_decoration {
	struct wl_listener destroy; /**< Listener for destruction events */
	struct wl_listener mode; /**< Listener for mode change events */

	struct wl_list link; /**< Link for managing a list of server decorations */

	struct wlr_server_decoration *server_decoration_wlr; /**< Pointer to the WLR server decoration instance */
};

/**
 * @brief Handles server decoration events
 * @param listener Pointer to the wl_listener instance
 * @param data Pointer to the event data
 */
void handle_server_decoration(struct wl_listener *listener, void *data);

/**
 * @brief Creates a new wsm_server_decoration_manager instance
 * @param server Pointer to the WSM server associated with the decoration manager
 * @return Pointer to the newly created wsm_server_decoration_manager instance
 */
struct wsm_server_decoration_manager *wsm_server_decoration_manager_create(
	const struct wsm_server *server);

/**
 * @brief Retrieves the server decoration associated with the specified surface
 * @param surface Pointer to the WLR surface to retrieve the decoration from
 * @return Pointer to the associated wsm_server_decoration instance
 */
struct wsm_server_decoration *decoration_from_surface(
	struct wlr_surface *surface);

#endif
