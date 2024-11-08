#ifndef WSM_INPUT_MANAGER_H
#define WSM_INPUT_MANAGER_H

#include <stdbool.h>

#include <libinput.h>

#include <wlr/types/wlr_keyboard_shortcuts_inhibit_v1.h>
#include <wlr/types/wlr_virtual_keyboard_v1.h>
#include <wlr/types/wlr_virtual_pointer_v1.h>

struct wlr_seat;
struct wlr_pointer_gestures_v1;

struct wsm_seat;
struct wsm_node;
struct wsm_server;
struct input_config;
struct wsm_input_device;

/**
 * @brief Structure representing the input manager in the WSM
 */
struct wsm_input_manager {
	struct wl_listener new_input; /**< Listener for handling new input events */
	struct wl_listener inhibit_activate; /**< Listener for handling inhibit activation events */
	struct wl_listener inhibit_deactivate; /**< Listener for handling inhibit deactivation events */
	struct wl_listener keyboard_shortcuts_inhibit_new_inhibitor; /**< Listener for new keyboard shortcuts inhibitors */
	struct wl_listener virtual_keyboard_new; /**< Listener for new virtual keyboard events */
	struct wl_listener virtual_pointer_new; /**< Listener for new virtual pointer events */

	struct wl_list devices; /**< List of input devices managed by the input manager */
	struct wl_list seats; /**< List of seats managed by the input manager */

	struct wlr_keyboard_shortcuts_inhibit_manager_v1 *keyboard_shortcuts_inhibit_wlr; /**< Pointer to the WLR keyboard shortcuts inhibit manager */
	struct wlr_virtual_keyboard_manager_v1 *virtual_keyboard_manager_wlr; /**< Pointer to the WLR virtual keyboard manager */
	struct wlr_virtual_pointer_manager_v1 *virtual_pointer_manager_wlr; /**< Pointer to the WLR virtual pointer manager */
	struct wlr_pointer_gestures_v1 *pointer_gestures_wlr; /**< Pointer to the WLR pointer gestures manager */
};

/**
 * @brief Creates a new wsm_input_manager instance
 * @param server Pointer to the WSM server that will manage input
 * @return Pointer to the newly created wsm_input_manager instance
 */
struct wsm_input_manager *wsm_input_manager_create(const struct wsm_server* server);

/**
 * @brief Retrieves the default seat from the input manager
 * @return Pointer to the default wsm_seat
 */
struct wsm_seat *input_manager_get_default_seat();

/**
 * @brief Retrieves the current seat from the input manager
 * @return Pointer to the current wsm_seat
 */
struct wsm_seat *input_manager_current_seat(void);

/**
 * @brief Retrieves a seat by name from the input manager
 * @param seat_name Name of the seat to retrieve
 * @param create Boolean indicating if a new seat should be created if it doesn't exist
 * @return Pointer to the requested wsm_seat
 */
struct wsm_seat *input_manager_get_seat(const char *seat_name, bool create);

/**
 * @brief Retrieves a wsm_seat from a WLR seat
 * @param wlr_seat Pointer to the WLR seat
 * @return Pointer to the corresponding wsm_seat
 */
struct wsm_seat *input_manager_seat_from_wlr_seat(struct wlr_seat *wlr_seat);

/**
 * @brief Retrieves the identifier for the specified input device
 * @param device Pointer to the WLR input device
 * @return Pointer to the identifier string for the device
 */
char *input_device_get_identifier(struct wlr_input_device *device);

/**
 * @brief Configures the X cursor for the input manager
 */
void input_manager_configure_xcursor(void);

/**
 * @brief Sets focus to the specified node
 * @param node Pointer to the wsm_node to focus
 */
void input_manager_set_focus(struct wsm_node *node);

/**
 * @brief Configures all input mappings for the input manager
 */
void input_manager_configure_all_input_mappings(void);

/**
 * @brief Retrieves the configuration for the specified input device
 * @param device Pointer to the wsm_input_device whose configuration is to be retrieved
 * @return Pointer to the input_config associated with the device
 */
struct input_config *input_device_get_config(struct wsm_input_device *device);

/**
 * @brief Retrieves the type of the specified input device
 * @param device Pointer to the wsm_input_device whose type is to be retrieved
 * @return Pointer to the string representing the type of the device
 */
const char *input_device_get_type(struct wsm_input_device *device);

#endif
