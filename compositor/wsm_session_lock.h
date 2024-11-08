#ifndef WSM_SESSION_LOCK_H
#define WSM_SESSION_LOCK_H

#include <stdbool.h>

#include <wayland-server-core.h>

struct wlr_surface;
struct wlr_session_lock_v1;

struct wsm_output;

/**
 * @brief Structure representing a session lock in the WSM
 */
struct wsm_session_lock {
	struct wl_listener new_surface; /**< Listener for new surface events */
	struct wl_listener unlock; /**< Listener for unlock events */
	struct wl_listener destroy; /**< Listener for destruction events */

	struct wl_list outputs; /**< List of outputs associated with the session lock (struct wsm_session_lock_output) */

	struct wlr_session_lock_v1 *session_lock_wlr; /**< Pointer to the WLR session lock instance */
	struct wlr_surface *focused; /**< Pointer to the currently focused surface */
	bool abandoned; /**< Flag indicating if the session lock has been abandoned */
};

/**
 * @brief Initializes the session lock system
 */
void wsm_session_lock_init(void);

/**
 * @brief Adds an output to the specified session lock
 * @param lock Pointer to the wsm_session_lock instance
 * @param output Pointer to the wsm_output instance to add
 */
void wsm_session_lock_add_output(struct wsm_session_lock *lock,
	struct wsm_output *output);

/**
 * @brief Checks if the specified session lock has the given surface
 * @param lock Pointer to the wsm_session_lock instance
 * @param surface Pointer to the WLR surface to check
 * @return true if the session lock has the surface, false otherwise
 */
bool wsm_session_lock_has_surface(struct wsm_session_lock *lock,
	struct wlr_surface *surface);

#endif
