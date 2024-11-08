#include "wsm_config.h"
#include "wsm_common.h"

struct wsm_config global_config = {0};

#define FOCUSED_BORDER 0xE0DFDEFF
#define UNFOCUSED_BORDER 0xEFF0F1FF

void wsm_config_init() {
	global_config.input_configs = wsm_list_create();
	global_config.input_type_configs = wsm_list_create();
	global_config.reloading = false;

	global_config.xwayland = XWAYLAND_MODE_LAZY;

	global_config.titlebar_h_padding = 5;
	global_config.titlebar_v_padding = 4;
	global_config.title_align = ALIGN_CENTER;
	global_config.tiling_drag = true;
	global_config.tiling_drag_threshold = 9;

	global_config.border = B_NORMAL;
	global_config.floating_border = B_NORMAL;
	global_config.floating_border_thickness = 2;
	global_config.sensing_border_thickness = 8;

	global_config.floating_maximum_width = 0;
	global_config.floating_maximum_height = 0;
	global_config.floating_minimum_width = 75;
	global_config.floating_minimum_height = 50;

	global_config.font_height = 20;
	global_config.urgent_timeout = 500;
	global_config.focus_on_window_activation = FOWA_URGENT;
	global_config.popup_during_fullscreen = POPUP_SMART;

	color_to_rgba(global_config.border_colors.focused.border, FOCUSED_BORDER);
	color_to_rgba(global_config.border_colors.focused.background, FOCUSED_BORDER);
	color_to_rgba(global_config.border_colors.focused.text, 0x000000FF);
	color_to_rgba(global_config.border_colors.focused.indicator, 0x2E9EF4FF);
	color_to_rgba(global_config.border_colors.focused.child_border, FOCUSED_BORDER);

	color_to_rgba(global_config.border_colors.focused_inactive.border, 0x333333FF);
	color_to_rgba(global_config.border_colors.focused_inactive.background, 0x5F676AFF);
	color_to_rgba(global_config.border_colors.focused_inactive.text, 0xFFFFFFFF);
	color_to_rgba(global_config.border_colors.focused_inactive.indicator, 0x484E50FF);
	color_to_rgba(global_config.border_colors.focused_inactive.child_border, 0x5F676AFF);

	color_to_rgba(global_config.border_colors.unfocused.border, UNFOCUSED_BORDER);
	color_to_rgba(global_config.border_colors.unfocused.background, UNFOCUSED_BORDER);
	color_to_rgba(global_config.border_colors.unfocused.text, 0x000000E1);
	color_to_rgba(global_config.border_colors.unfocused.indicator, 0x292D2E7D);
	color_to_rgba(global_config.border_colors.unfocused.child_border, UNFOCUSED_BORDER);

	color_to_rgba(global_config.border_colors.urgent.border, 0x2F343AFF);
	color_to_rgba(global_config.border_colors.urgent.background, 0x900000FF);
	color_to_rgba(global_config.border_colors.urgent.text, 0xFFFFFFFF);
	color_to_rgba(global_config.border_colors.urgent.indicator, 0x900000FF);
	color_to_rgba(global_config.border_colors.urgent.child_border, 0x900000FF);

	color_to_rgba(global_config.border_colors.placeholder.border, 0x000000FF);
	color_to_rgba(global_config.border_colors.placeholder.background, 0x0C0C0CFF);
	color_to_rgba(global_config.border_colors.placeholder.text, 0xFFFFFFFF);
	color_to_rgba(global_config.border_colors.placeholder.indicator, 0x000000FF);
	color_to_rgba(global_config.border_colors.placeholder.child_border, 0x0C0C0CFF);

	color_to_rgba(global_config.text_background_color, 0x00000000);
	color_to_rgba(global_config.sensing_border_color, 0x00000000);

	global_config.primary_selection = true;
}
