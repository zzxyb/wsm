#ifndef WSM_CONFIG_H
#define WSM_CONFIG_H

#include "wsm_container.h"

extern struct wsm_config global_config;

enum xwayland_mode {
	XWAYLAND_MODE_DISABLED,
	XWAYLAND_MODE_LAZY,
	XWAYLAND_MODE_IMMEDIATE,
};

enum wsm_popup_during_fullscreen {
	POPUP_SMART,
	POPUP_IGNORE,
	POPUP_LEAVE,
};

enum focus_follows_mouse_mode {
	FOLLOWS_NO,
	FOLLOWS_YES,
	FOLLOWS_ALWAYS,
};

enum wsm_fowa {
	FOWA_SMART,
	FOWA_URGENT,
	FOWA_FOCUS,
	FOWA_NONE,
};

enum seat_keyboard_grouping {
	KEYBOARD_GROUP_DEFAULT, // the default is currently smart
	KEYBOARD_GROUP_NONE,
	KEYBOARD_GROUP_SMART, // keymap and repeat info
};

struct wsm_config {
	struct {
		struct border_colors focused;
		struct border_colors focused_inactive;
		struct border_colors focused_tab_title;
		struct border_colors unfocused;
		struct border_colors urgent;
		struct border_colors placeholder;
	} border_colors;

	float text_background_color[4];
	float sensing_border_color[4];

	struct wsm_list *input_configs;
	struct wsm_list *input_type_configs;

	size_t urgent_timeout;
	enum wsm_container_border border;
	enum wsm_container_border floating_border;
	int floating_border_thickness;
	int sensing_border_thickness; // pixels to expand based on the current border

	int32_t floating_maximum_width;
	int32_t floating_maximum_height;
	int32_t floating_minimum_width;
	int32_t floating_minimum_height;

	int font_height;
	int font_baseline;
	int titlebar_h_padding;
	int titlebar_v_padding;

	enum alignment title_align;

	int tiling_drag_threshold;

	enum xwayland_mode xwayland;

	enum wsm_fowa focus_on_window_activation;
	enum wsm_popup_during_fullscreen popup_during_fullscreen;

	bool primary_selection;
	bool tiling_drag;
	bool pango_markup;
	bool reloading;
};

void wsm_config_init();

#endif
