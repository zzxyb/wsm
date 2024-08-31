/*
MIT License

Copyright (c) 2024 YaoBing Xiao

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

#ifndef WSM_INPUT_CONFIG_H
#define WSM_INPUT_CONFIG_H

#include <stdbool.h>

#include <wayland-server-core.h>

#include <wlr/util/box.h>

struct wsm_list;

struct xkb_rule_names;

enum wsm_numlock_state {
	WSM_NUMLOCK_ENABLE,
	WSM_NUMLOCK_DISABLE,
	WSM_NUMLOCK_KEEP,
};

enum wsm_hold_key_action {
	WSM_REPEAT_KEY,
	WSM_NOTHING,
};

enum wsm_mouse_handled_mode {
	WSM_LEFT_HANDLED_MODE,
	WSM_RIGHT_HANDLED_MODE,
};

enum wsm_pointer_acceleration_profile {
	WSM_POINTER_ACCELERATION_CONSTANT,
	WSM_POINTER_DEPEND_MOUSE_SPEED,
};

struct wsm_touchpad_safe_area {
	int top;
	int right;
	int bottom;
	int left;
};

struct wsm_keyboard_state {
	struct wl_list link;
	bool enable;
	int delay;
	int speed;
	enum wsm_numlock_state numlock_mode;
	enum wsm_hold_key_action hold_key_action;
};

struct wsm_mouse_state {
	struct wl_list link;
	bool enable;
	enum wsm_mouse_handled_mode handled_mode;
	bool middle_btn_click_emulation; // click left btn and right btn to simulate midle btn
	int speed;
	enum wsm_pointer_acceleration_profile acceleration_profile;
	bool natural_scoll;
	int scroll_factor;
};

struct wsm_touchpad_state {
	struct wl_list link;
	bool enable;
	bool disable_while_typing;
	enum wsm_mouse_handled_mode handled_mode;
	bool middle_btn_emulation;
	int speed;
	enum wsm_pointer_acceleration_profile acceleration_profile;
	bool tap_to_click;
	bool tap_and_drag;
	bool tap_drag_lock;
	bool right_btn_click_emulation; // two finger tap trigger emulation right btn click
	bool tow_finger_scroll;
	bool natural_scoll;
	int scroll_factor;
	bool middle_btn_click_emulation; // Click the bottom right corner of the touchpad to trigger emulation right btn click
	struct wsm_touchpad_safe_area safe_area;
};

struct calibration_matrix {
	bool configured;
	float matrix[6];
};

struct input_config_mapped_from_region {
	double x1, y1;
	double x2, y2;
	bool mm;
};

enum input_config_mapped_to {
	MAPPED_TO_DEFAULT,
	MAPPED_TO_OUTPUT,
	MAPPED_TO_REGION,
};

struct input_config {
	char *identifier;
	const char *input_type;
	
	int accel_profile;
	struct calibration_matrix calibration_matrix;
	int click_method;
	int clickfinger_button_map;
	int drag;
	int drag_lock;
	int dwt;
	int dwtp;
	int left_handed;
	int middle_emulation;
	int natural_scroll;
	float pointer_accel;
	float rotation_angle;
	float scroll_factor;
	int repeat_delay;
	int repeat_rate;
	int scroll_button;
	int scroll_button_lock;
	int scroll_method;
	int send_events;
	int tap;
	int tap_button_map;
	
	char *xkb_layout;
	char *xkb_model;
	char *xkb_options;
	char *xkb_rules;
	char *xkb_variant;
	char *xkb_file;
	
	bool xkb_file_is_set;
	
	int xkb_numlock;
	int xkb_capslock;
	
	struct input_config_mapped_from_region *mapped_from_region;
	
	enum input_config_mapped_to mapped_to;
	char *mapped_to_output;
	struct wlr_box *mapped_to_region;
	
	struct wsm_list *tools;
	
	bool capturable;
	struct wlr_box region;
};

struct wsm_graphics_tablet_state {
	struct wl_list link;
	bool enable;
};

struct wsm_touchscreen_config {
	struct wl_list link;
	bool enable;
};

struct wsm_inputs_config {
	struct wl_list keyboards_state;
	struct wl_list mouses_state;
	struct wl_list touchpads_state;
	struct wl_list graphics_tablets_state;
	struct wl_list touchscreens_state;
};

bool wsm_inputs_config_load(struct wsm_inputs_config *configs);
void input_config_fill_rule_names(struct input_config *ic,
								  struct xkb_rule_names *rules);

#endif
