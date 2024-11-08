#ifndef WSM_INPUT_TEXT_INPUT_H
#define WSM_INPUT_TEXT_INPUT_H

#include "wsm_view.h"

#include <wayland-server-core.h>

struct wlr_surface;
struct wlr_input_method_v2;
struct wlr_text_input_v3;
struct wlr_text_input_manager_v3;
struct wlr_input_method_manager_v2;
struct wlr_input_popup_surface_v2;

struct wsm_seat;

/**
 * @brief Structure representing the input method relay in the WSM
 */
struct wsm_input_method_relay {
	struct wl_listener input_method_new; /**< Listener for new input method events */
	struct wl_listener input_method_commit; /**< Listener for input method commit events */
	struct wl_listener input_method_new_popup_surface; /**< Listener for new popup surface events */
	struct wl_listener input_method_grab_keyboard; /**< Listener for keyboard grab events */
	struct wl_listener input_method_destroy; /**< Listener for input method destruction events */

	struct wl_listener input_method_keyboard_grab_destroy; /**< Listener for keyboard grab destruction events */

	struct wl_listener text_input_new; /**< Listener for new text input events */

	struct wl_list text_inputs; /**< List of text inputs (linked via wsm_text_input::link) */
	struct wl_list input_popups; /**< List of input popups (linked via wsm_input_popup::link) */
	struct wlr_input_method_v2 *input_method; /**< Pointer to the WLR input method (optional) */

	struct wsm_seat *seat; /**< Pointer to the associated WSM seat */
};

/**
 * @brief Structure representing an input popup in the WSM
 */
struct wsm_input_popup {
	struct wl_listener popup_destroy; /**< Listener for popup destruction events */
	struct wl_listener popup_surface_commit; /**< Listener for popup surface commit events */

	struct wl_listener focused_surface_unmap; /**< Listener for focused surface unmap events */

	struct wl_list link; /**< Link for managing a list of input popups */
	struct wsm_popup_desc desc; /**< Description of the popup */
	struct wsm_input_method_relay *relay; /**< Pointer to the associated input method relay */

	struct wlr_scene_tree *scene_tree; /**< Pointer to the scene tree for the popup */
	struct wlr_input_popup_surface_v2 *popup_surface; /**< Pointer to the WLR input popup surface */
};

/**
 * @brief Structure representing version 3 of text input in the WSM
 */
struct wsm_text_input_v3 {
	struct wl_listener pending_focused_surface_destroy; /**< Listener for pending focused surface destruction events */

	struct wl_listener text_input_enable; /**< Listener for enabling text input */
	struct wl_listener text_input_commit; /**< Listener for committing text input */
	struct wl_listener text_input_disable; /**< Listener for disabling text input */
	struct wl_listener text_input_destroy; /**< Listener for destroying text input */

	struct wl_list link; /**< Link for managing a list of text inputs */

	struct wsm_input_method_relay *relay; /**< Pointer to the associated input method relay */

	struct wlr_text_input_v3 *text_input_v3_wlr; /**< Pointer to the WLR version 3 text input instance */
	struct wlr_surface *pending_focused_surface; /**< Pointer to the pending focused surface */
};

/**
 * @brief Initializes the input method relay for the specified seat
 * @param seat Pointer to the WSM seat associated with the relay
 * @param relay Pointer to the wsm_input_method_relay instance to initialize
 */
void wsm_input_method_relay_init(struct wsm_seat *seat,
	struct wsm_input_method_relay *relay);

/**
 * @brief Finishes the input method relay, cleaning up resources
 * @param relay Pointer to the wsm_input_method_relay instance to finish
 */
void wsm_input_method_relay_finish(struct wsm_input_method_relay *relay);

/**
 * @brief Sets focus for the input method relay to the specified surface
 * @param relay Pointer to the wsm_input_method_relay instance
 * @param surface Pointer to the WLR surface to focus on
 */
void wsm_input_method_relay_set_focus(struct wsm_input_method_relay *relay,
	struct wlr_surface *surface);

/**
 * @brief Creates a new version 3 text input instance
 * @param relay Pointer to the wsm_input_method_relay associated with the text input
 * @param text_input Pointer to the WLR version 3 text input instance
 * @return Pointer to the newly created wsm_text_input_v3 instance
 */
struct wsm_text_input_v3 *wsm_text_input_v3_create(
	struct wsm_input_method_relay *relay,
	struct wlr_text_input_v3 *text_input);

#endif
