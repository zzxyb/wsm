#ifndef WSM_INPUT_CONFIG_H
#define WSM_INPUT_CONFIG_H

#include <stdbool.h>

#include <wayland-server-core.h>

#include <wlr/util/box.h>

struct wsm_list;

struct xkb_rule_names;

/**
 * @brief Enumeration of numlock states
 */
enum wsm_numlock_state {
	WSM_NUMLOCK_ENABLE, /**< Numlock is enabled */
	WSM_NUMLOCK_DISABLE, /**< Numlock is disabled */
	WSM_NUMLOCK_KEEP, /**< Keep the current state of numlock */
};

/**
 * @brief Enumeration of actions for holding keys
 */
enum wsm_hold_key_action {
	WSM_REPEAT_KEY, /**< Repeat the key action */
	WSM_NOTHING, /**< Do nothing */
};

/**
 * @brief Enumeration of mouse handling modes
 */
enum wsm_mouse_handled_mode {
	WSM_LEFT_HANDLED_MODE, /**< Left mouse button handling mode */
	WSM_RIGHT_HANDLED_MODE, /**< Right mouse button handling mode */
};

/**
 * @brief Enumeration of pointer acceleration profiles
 */
enum wsm_pointer_acceleration_profile {
	WSM_POINTER_ACCELERATION_CONSTANT, /**< Constant pointer acceleration */
	WSM_POINTER_DEPEND_MOUSE_SPEED, /**< Pointer acceleration depends on mouse speed */
};

/**
 * @brief Structure representing a safe area for touchpad
 */
struct wsm_touchpad_safe_area {
	int top; /**< Top safe area */
	int right; /**< Right safe area */
	int bottom; /**< Bottom safe area */
	int left; /**< Left safe area */
};

/**
 * @brief Structure representing the state of a keyboard
 */
struct wsm_keyboard_state {
	struct wl_list link; /**< Link for managing a list of keyboard states */
	int delay; /**< Delay for key repeat */
	int speed; /**< Speed of key repeat */
	enum wsm_numlock_state numlock_mode; /**< Current numlock state */
	enum wsm_hold_key_action hold_key_action; /**< Action for holding keys */
	bool enable; /**< Flag indicating if the keyboard is enabled */
};

/**
 * @brief Structure representing the state of a mouse
 */
struct wsm_mouse_state {
	struct wl_list link; /**< Link for managing a list of mouse states */
	int speed; /**< Speed of the mouse */
	int scroll_factor; /**< Scroll factor for the mouse */
	enum wsm_pointer_acceleration_profile acceleration_profile; /**< Pointer acceleration profile */
	enum wsm_mouse_handled_mode handled_mode; /**< Mode for handling mouse actions */
	bool enable; /**< Flag indicating if the mouse is enabled */
	bool middle_btn_click_emulation; /**< Flag for middle button click emulation (click left and right buttons) */
	bool natural_scoll; /**< Flag for natural scrolling */
};

/**
 * @brief Structure representing the state of a touchpad
 */
struct wsm_touchpad_state {
	struct wl_list link; /**< Link for managing a list of touchpad states */
	struct wsm_touchpad_safe_area safe_area; /**< Safe area for the touchpad */
	int speed; /**< Speed of the touchpad */
	int scroll_factor; /**< Scroll factor for the touchpad */
	enum wsm_pointer_acceleration_profile acceleration_profile; /**< Pointer acceleration profile */
	enum wsm_mouse_handled_mode handled_mode; /**< Mode for handling touchpad actions */
	bool enable; /**< Flag indicating if the touchpad is enabled */
	bool disable_while_typing; /**< Flag to disable touchpad while typing */
	bool middle_btn_emulation; /**< Flag for middle button emulation */
	bool tap_to_click; /**< Flag for tap-to-click functionality */
	bool tap_and_drag; /**< Flag for tap-and-drag functionality */
	bool tap_drag_lock; /**< Flag for tap drag lock functionality */
	bool right_btn_click_emulation; /**< Flag for right button click emulation (two-finger tap) */
	bool tow_finger_scroll; /**< Flag for two-finger scrolling */
	bool natural_scoll; /**< Flag for natural scrolling */
	bool middle_btn_click_emulation; /**< Flag for middle button click emulation (click bottom right corner) */
};

/**
 * @brief Structure representing a calibration matrix
 */
struct calibration_matrix {
	float matrix[6]; /**< Calibration matrix values */
	bool configured; /**< Flag indicating if the matrix is configured */
};

/**
 * @brief Structure representing a region mapped from input configuration
 */
struct input_config_mapped_from_region {
	double x1, y1; /**< Coordinates of the first corner */
	double x2, y2; /**< Coordinates of the second corner */
	bool mm; /**< Flag indicating if the region is in millimeters */
};

/**
 * @brief Enumeration of input configuration mapping types
 */
enum input_config_mapped_to {
	MAPPED_TO_DEFAULT, /**< Mapped to default */
	MAPPED_TO_OUTPUT, /**< Mapped to output */
	MAPPED_TO_REGION, /**< Mapped to a specific region */
};

/**
 * @brief Structure representing input configuration settings
 */
struct input_config {
	struct calibration_matrix calibration_matrix; /**< Calibration matrix for the input */
	struct wlr_box region; /**< Region for the input configuration */
	struct wsm_list *tools; /**< List of tools associated with the input */
	struct input_config_mapped_from_region *mapped_from_region; /**< Region mapped from input configuration */
	struct wlr_box *mapped_to_region; /**< Region to which the input is mapped */
	char *identifier; /**< Identifier for the input configuration */
	const char *input_type; /**< Type of the input (e.g., keyboard, mouse) */
	char *xkb_layout; /**< XKB layout for the input */
	char *xkb_model; /**< XKB model for the input */
	char *xkb_options; /**< XKB options for the input */
	char *xkb_rules; /**< XKB rules for the input */
	char *xkb_variant; /**< XKB variant for the input */
	char *xkb_file; /**< XKB file for the input */
	char *mapped_to_output; /**< Output to which the input is mapped */

	int accel_profile; /**< Acceleration profile for the input */
	int click_method; /**< Click method for the input */
	int clickfinger_button_map; /**< Button map for clickfinger */
	int drag; /**< Drag setting for the input */
	int drag_lock; /**< Drag lock setting for the input */
	int dwt; /**< DWT setting for the input */
	int dwtp; /**< DWTP setting for the input */
	int left_handed; /**< Flag for left-handed configuration */
	int middle_emulation; /**< Flag for middle button emulation */
	int natural_scroll; /**< Flag for natural scrolling */
	float pointer_accel; /**< Pointer acceleration value */
	float rotation_angle; /**< Rotation angle for the input */
	float scroll_factor; /**< Scroll factor for the input */
	int repeat_delay; /**< Delay for key repeat */
	int repeat_rate; /**< Rate for key repeat */
	int scroll_button; /**< Button used for scrolling */
	int scroll_button_lock; /**< Lock for the scroll button */
	int scroll_method; /**< Method used for scrolling */
	int send_events; /**< Events to send for the input */
	int tap; /**< Tap setting for the input */
	int tap_button_map; /**< Button map for tap actions */

	int xkb_numlock; /**< XKB numlock setting */
	int xkb_capslock; /**< XKB capslock setting */

	enum input_config_mapped_to mapped_to; /**< Mapping type for the input configuration */

	bool capturable; /**< Flag indicating if the input is capturable */
	bool xkb_file_is_set; /**< Flag indicating if the XKB file is set */
};

/**
 * @brief Structure representing the state of a graphics tablet
 */
struct wsm_graphics_tablet_state {
	struct wl_list link; /**< Link for managing a list of graphics tablet states */
	bool enable; /**< Flag indicating if the graphics tablet is enabled */
};

/**
 * @brief Structure representing the configuration for a touchscreen
 */
struct wsm_touchscreen_config {
	struct wl_list link; /**< Link for managing a list of touchscreen configurations */
	bool enable; /**< Flag indicating if the touchscreen is enabled */
};

/**
 * @brief Structure representing the configuration for input devices
 */
struct wsm_inputs_config {
	struct wl_list keyboards_state; /**< List of keyboard states */
	struct wl_list mouses_state; /**< List of mouse states */
	struct wl_list touchpads_state; /**< List of touchpad states */
	struct wl_list graphics_tablets_state; /**< List of graphics tablet states */
	struct wl_list touchscreens_state; /**< List of touchscreen states */
};

/**
 * @brief Loads the input configurations
 * @param configs Pointer to the wsm_inputs_config instance to load configurations into
 * @return true if loading was successful, false otherwise
 */
bool wsm_inputs_config_load(struct wsm_inputs_config *configs);

/**
 * @brief Fills the rule names for the specified input configuration
 * @param ic Pointer to the input_config instance to fill rule names for
 * @param rules Pointer to the xkb_rule_names instance to fill
 */
void input_config_fill_rule_names(struct input_config *ic,
	struct xkb_rule_names *rules);

#endif
