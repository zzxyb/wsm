/** \file
 *
 *  \brief premits client to inhibit the idle behaviror such as
 *  screenblanking, locking and screensaving.
 *
 *  \warning Implemented based on wlr_idle_inhibit.
 */

#ifndef WSM_IDLE_INHIBIT_V1_H
#define WSM_IDLE_INHIBIT_V1_H

#include <wayland-server-core.h>

struct wlr_idle_inhibit_manager_v1;
struct wlr_idle_inhibitor_v1;

struct wsm_view;

/**
 * @brief Enumeration of idle inhibit modes
 */
enum wsm_idle_inhibit_mode {
	INHIBIT_IDLE_APPLICATION,  /**< Application set inhibitor (when visible) */
	INHIBIT_IDLE_FOCUS,  /**< User set inhibitor when focused */
	INHIBIT_IDLE_FULLSCREEN,  /**< User set inhibitor when fullscreen + visible */
	INHIBIT_IDLE_OPEN,  /**< User set inhibitor while open */
	INHIBIT_IDLE_VISIBLE  /**< User set inhibitor when visible */
};

/**
 * @brief Structure representing the idle inhibit manager
 */
struct wsm_idle_inhibit_manager_v1 {
	struct wl_listener new_idle_inhibitor_v1; /**< Listener for new idle inhibitor events */
	struct wl_list inhibitors; /**< List of active inhibitors */
	struct wlr_idle_inhibit_manager_v1 *idle_inhibit_manager_wlr; /**< Pointer to the WLR idle inhibit manager */
};

/**
 * @brief Structure representing an idle inhibitor
 */
struct wsm_idle_inhibitor_v1 {
	struct wl_listener destroy; /**< Listener for destruction events */
	struct wl_list link; /**< Link for managing a list of idle inhibitors */
	struct wlr_idle_inhibitor_v1 *idle_inhibitor_wlr; /**< Pointer to the WLR idle inhibitor instance */
	struct wsm_view *view; /**< Pointer to the associated WSM view */
	enum wsm_idle_inhibit_mode mode; /**< Current mode of the idle inhibitor */
};

/**
 * @brief Checks if the specified idle inhibitor is active
 * @param inhibitor Pointer to the wsm_idle_inhibitor_v1 instance to check
 * @return true if the inhibitor is active, false otherwise
 */
bool wsm_idle_inhibit_v1_is_active(
		struct wsm_idle_inhibitor_v1 *inhibitor);

/**
 * @brief Checks the active state of all idle inhibitors
 */
void wsm_idle_inhibit_v1_check_active(void);

/**
 * @brief Registers a user-defined idle inhibitor for the specified view
 * @param view Pointer to the wsm_view instance to register the inhibitor for
 * @param mode The mode of the idle inhibitor
 */
void wsm_idle_inhibit_v1_user_inhibitor_register(struct wsm_view *view,
	enum wsm_idle_inhibit_mode mode);

/**
 * @brief Retrieves the user-defined idle inhibitor for the specified view
 * @param view Pointer to the wsm_view instance to retrieve the inhibitor for
 * @return Pointer to the associated wsm_idle_inhibitor_v1 instance, or NULL if not found
 */
struct wsm_idle_inhibitor_v1 *wsm_idle_inhibit_v1_user_inhibitor_for_view(
	struct wsm_view *view);

/**
 * @brief Retrieves the application idle inhibitor for the specified view
 * @param view Pointer to the wsm_view instance to retrieve the inhibitor for
 * @return Pointer to the associated wsm_idle_inhibitor_v1 instance, or NULL if not found
 */
struct wsm_idle_inhibitor_v1 *wsm_idle_inhibit_v1_application_inhibitor_for_view(
	struct wsm_view *view);

/**
 * @brief Destroys the specified user-defined idle inhibitor
 * @param inhibitor Pointer to the wsm_idle_inhibitor_v1 instance to destroy
 */
void wsm_idle_inhibit_v1_user_inhibitor_destroy(
	struct wsm_idle_inhibitor_v1 *inhibitor);

/**
 * @brief Initializes the idle inhibit manager
 * @return true if initialization was successful, false otherwise
 */
bool wsm_idle_inhibit_manager_v1_init(void);

#endif
