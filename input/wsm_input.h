#ifndef WSM_INPUT_H
#define WSM_INPUT_H

#include <stdbool.h>

#include <libinput.h>

#include <wayland-server-core.h>

struct libinput_device;

struct wlr_input_device;
struct wlr_keyboard_shortcuts_inhibit_manager_v1;
struct wlr_pointer_gestures_v1;
struct wlr_seat;
struct wlr_virtual_keyboard_manager_v1;
struct wlr_virtual_pointer_manager_v1;

struct input_config;
struct wsm_input_device_impl;
struct wsm_node;
struct wsm_seat;
struct wsm_server;

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
 * @brief Structure representing an input device in the WSM
 */
struct wsm_input_device {
	struct wl_listener device_destroy; /**< Listener for handling device destruction events */
	struct wl_list link; /**< Link for managing a list of input devices */

	char *identifier; /**< Identifier for the input device */
	struct wlr_input_device *input_device_wlr; /**< Pointer to the associated WLR input device */
	const struct wsm_input_device_impl *input_device_impl_wsm; /**< Pointer to the implementation of the input device */
	bool is_virtual; /**< Flag indicating if the input device is virtual */
};

/**
 * @brief Creates a new wsm_input_manager instance
 * @param server Pointer to the WSM server that will manage input
 * @return Pointer to the newly created wsm_input_manager instance
 */
struct wsm_input_manager *wsm_input_manager_create(const struct wsm_server *server);

/**
 * @brief Destroys the specified input manager instance
 * @param input_manager Pointer to the input manager instance
 */
void wsm_input_manager_destroy(struct wsm_input_manager *input_manager);

/**
 * @brief Detaches input manager listeners from the backend
 * @param input_manager Pointer to the input manager instance
 */
void wsm_input_manager_detach_backend(struct wsm_input_manager *input_manager);

/**
 * @brief Retrieves the default seat from the input manager
 * @return Pointer to the default wsm_seat
 */
struct wsm_seat *input_manager_get_default_seat(void);

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

/**
 * @brief Structure representing the implementation of input device functionalities
 */
struct wsm_input_device_impl {
	/**
	 * @brief Sets the send events mode for the specified device
	 * @param device Pointer to the libinput_device
	 * @param mode Mode for sending events
	 * @return Status of the configuration
	 */
	enum libinput_config_status (*set_send_events)
		(struct libinput_device *device, uint32_t mode);

	/**
	 * @brief Sets the tap state for the specified device
	 * @param device Pointer to the libinput_device
	 * @param tap Tap state to set
	 * @return Status of the configuration
	 */
	enum libinput_config_status (*set_tap)
		(struct libinput_device *device, enum libinput_config_tap_state tap);

	/**
	 * @brief Sets the tap button map for the specified device
	 * @param device Pointer to the libinput_device
	 * @param map Button map to set
	 * @return Status of the configuration
	 */
	enum libinput_config_status (*set_tap_button_map)
		(struct libinput_device *device, enum libinput_config_tap_button_map map);

	/**
	 * @brief Sets the tap drag state for the specified device
	 * @param device Pointer to the libinput_device
	 * @param drag Drag state to set
	 * @return Status of the configuration
	 */
	enum libinput_config_status (*set_tap_drag)
		(struct libinput_device *device, enum libinput_config_drag_state drag);

	/**
	 * @brief Sets the tap drag lock state for the specified device
	 * @param device Pointer to the libinput_device
	 * @param lock Lock state to set
	 * @return Status of the configuration
	 */
	enum libinput_config_status (*set_tap_drag_lock)
		(struct libinput_device *device, enum libinput_config_drag_lock_state lock);

	/**
	 * @brief Sets the acceleration speed for the specified device
	 * @param device Pointer to the libinput_device
	 * @param speed Speed to set
	 * @return Status of the configuration
	 */
	enum libinput_config_status (*set_accel_speed)(struct libinput_device *device, double speed);

	/**
	 * @brief Sets the rotation angle for the specified device
	 * @param device Pointer to the libinput_device
	 * @param angle Angle to set
	 * @return Status of the configuration
	 */
	enum libinput_config_status (*set_rotation_angle)(struct libinput_device *device, double angle);

	/**
	 * @brief Sets the acceleration profile for the specified device
	 * @param device Pointer to the libinput_device
	 * @param profile Profile to set
	 * @return Status of the configuration
	 */
	enum libinput_config_status (*set_accel_profile)
		(struct libinput_device *device, enum libinput_config_accel_profile profile);

	/**
	 * @brief Sets the natural scroll state for the specified device
	 * @param d Pointer to the libinput_device
	 * @param n Natural scroll state to set
	 * @return Status of the configuration
	 */
	enum libinput_config_status (*set_natural_scroll)(struct libinput_device *d, bool n);

	/**
	 * @brief Sets the left-handed mode for the specified device
	 * @param device Pointer to the libinput_device
	 * @param left Boolean indicating if left-handed mode should be enabled
	 * @return Status of the configuration
	 */
	enum libinput_config_status (*set_left_handed)(struct libinput_device *device, bool left);

	/**
	 * @brief Sets the click method for the specified device
	 * @param device Pointer to the libinput_device
	 * @param method Click method to set
	 * @return Status of the configuration
	 */
	enum libinput_config_status (*set_click_method)
		(struct libinput_device *device, enum libinput_config_click_method method);

	/**
	 * @brief Sets the middle emulation state for the specified device
	 * @param dev Pointer to the libinput_device
	 * @param mid Middle emulation state to set
	 * @return Status of the configuration
	 */
	enum libinput_config_status (*set_middle_emulation)
		(struct libinput_device *dev, enum libinput_config_middle_emulation_state mid);

	/**
	 * @brief Sets the scroll method for the specified device
	 * @param device Pointer to the libinput_device
	 * @param method Scroll method to set
	 * @return Status of the configuration
	 */
	enum libinput_config_status (*set_scroll_method)
		(struct libinput_device *device, enum libinput_config_scroll_method method);

	/**
	 * @brief Sets the scroll button for the specified device
	 * @param dev Pointer to the libinput_device
	 * @param button Button to set
	 * @return Status of the configuration
	 */
	enum libinput_config_status (*set_scroll_button)(struct libinput_device *dev, uint32_t button);

	/**
	 * @brief Sets the scroll button lock state for the specified device
	 * @param dev Pointer to the libinput_device
	 * @param lock Lock state to set
	 * @return Status of the configuration
	 */
	enum libinput_config_status (*set_scroll_button_lock)
		(struct libinput_device *dev, enum libinput_config_scroll_button_lock_state lock);

	/**
	 * @brief Sets the DWT state for the specified device
	 * @param device Pointer to the libinput_device
	 * @param dwt DWT state to set
	 * @return Status of the configuration
	 */
	enum libinput_config_status (*set_dwt)(struct libinput_device *device, bool dwt);

	/**
	 * @brief Sets the DWTP state for the specified device
	 * @param device Pointer to the libinput_device
	 * @param dwtp DWTP state to set
	 * @return Status of the configuration
	 */
	enum libinput_config_status (*set_dwtp)(struct libinput_device *device, bool dwtp);

	/**
	 * @brief Sets the calibration matrix for the specified device
	 * @param dev Pointer to the libinput_device
	 * @param mat Calibration matrix to set
	 * @return Status of the configuration
	 */
	enum libinput_config_status (*set_calibration_matrix)(struct libinput_device *dev, float mat[6]);
};

/**
 * @brief Creates a new wsm_input_device instance
 * @return Pointer to the newly created wsm_input_device instance
 */
struct wsm_input_device* wsm_input_device_create();

/**
 * @brief Retrieves the wsm_input_device associated with the specified WLR input device
 * @param device Pointer to the WLR input device
 * @return Pointer to the associated wsm_input_device instance
 */
struct wsm_input_device *input_wsm_device_from_wlr(struct wlr_input_device *device);

/**
 * @brief Destroys the specified wsm_input_device instance
 * @param wlr_device Pointer to the WLR input device to be destroyed
 */
void wsm_input_device_destroy(struct wlr_input_device *wlr_device);

/**
 * @brief Configures the send events for the specified libinput device
 * @param input_device Pointer to the wsm_input_device to configure
 */
void wsm_input_configure_libinput_device_send_events(struct wsm_input_device *input_device);

#endif
