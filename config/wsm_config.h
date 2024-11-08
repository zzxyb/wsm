#ifndef WSM_CONFIG_H
#define WSM_CONFIG_H

#include "wsm_container.h"

extern struct wsm_config global_config;

/**
 * @brief Enumeration of XWayland modes
 */
enum xwayland_mode {
	XWAYLAND_MODE_DISABLED, /**< XWayland is disabled */
	XWAYLAND_MODE_LAZY, /**< XWayland starts lazily */
	XWAYLAND_MODE_IMMEDIATE, /**< XWayland starts immediately */
};

/**
 * @brief Enumeration of popup behavior during fullscreen
 */
enum wsm_popup_during_fullscreen {
	POPUP_SMART, /**< Smart behavior for popups */
	POPUP_IGNORE, /**< Ignore popups during fullscreen */
	POPUP_LEAVE, /**< Leave popups visible during fullscreen */
};

/**
 * @brief Enumeration of focus follows mouse modes
 */
enum focus_follows_mouse_mode {
	FOLLOWS_NO, /**< Do not follow mouse focus */
	FOLLOWS_YES, /**< Follow mouse focus */
	FOLLOWS_ALWAYS, /**< Always follow mouse focus */
};

/**
 * @brief Enumeration of focus on window activation modes
 */
enum wsm_fowa {
	FOWA_SMART, /**< Smart focus on window activation */
	FOWA_URGENT, /**< Urgent focus on window activation */
	FOWA_FOCUS, /**< Focus on window activation */
	FOWA_NONE, /**< No focus on window activation */
};

/**
 * @brief Enumeration of seat keyboard grouping types
 */
enum seat_keyboard_grouping {
	KEYBOARD_GROUP_DEFAULT, /**< Default keyboard grouping (currently smart) */
	KEYBOARD_GROUP_NONE, /**< No keyboard grouping */
	KEYBOARD_GROUP_SMART, /**< Smart keyboard grouping (keymap and repeat info) */
};

/**
 * @brief Structure representing the configuration settings for the WSM
 */
struct wsm_config {
	struct {
		struct border_colors focused; /**< Colors for focused borders */
		struct border_colors focused_inactive; /**< Colors for inactive focused borders */
		struct border_colors focused_tab_title; /**< Colors for focused tab titles */
		struct border_colors unfocused; /**< Colors for unfocused borders */
		struct border_colors urgent; /**< Colors for urgent borders */
		struct border_colors placeholder; /**< Colors for placeholder borders */
	} border_colors; /**< Border color settings */

	float text_background_color[4]; /**< Background color for text (RGBA) */
	float sensing_border_color[4]; /**< Color for sensing borders (RGBA) */

	struct wsm_list *input_configs; /**< List of input configurations */
	struct wsm_list *input_type_configs; /**< List of input type configurations */

	size_t urgent_timeout; /**< Timeout for urgent notifications */
	enum wsm_container_border border; /**< Border style for containers */
	enum wsm_container_border floating_border; /**< Border style for floating containers */
	int floating_border_thickness; /**< Thickness of the floating border */
	int sensing_border_thickness; /**< Thickness of the sensing border (in pixels) */

	int32_t floating_maximum_width; /**< Maximum width for floating windows */
	int32_t floating_maximum_height; /**< Maximum height for floating windows */
	int32_t floating_minimum_width; /**< Minimum width for floating windows */
	int32_t floating_minimum_height; /**< Minimum height for floating windows */

	int font_height; /**< Height of the font */
	int font_baseline; /**< Baseline of the font */
	int titlebar_h_padding; /**< Horizontal padding for the titlebar */
	int titlebar_v_padding; /**< Vertical padding for the titlebar */

	enum alignment title_align; /**< Alignment for the title */

	int tiling_drag_threshold; /**< Threshold for tiling drag operations */

	enum xwayland_mode xwayland; /**< Current XWayland mode */

	enum wsm_fowa focus_on_window_activation; /**< Focus behavior on window activation */
	enum wsm_popup_during_fullscreen popup_during_fullscreen; /**< Popup behavior during fullscreen */

	bool primary_selection; /**< Flag for primary selection support */
	bool tiling_drag; /**< Flag for tiling drag support */
	bool pango_markup; /**< Flag for Pango markup support */
	bool reloading; /**< Flag indicating if the configuration is reloading */
};

/**
 * @brief Initializes the WSM configuration settings
 */
void wsm_config_init();

#endif
