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

#include "wsm_config.h"
#include "wsm_common.h"

struct wsm_config global_config = {0};

#define FOCUSED_BORDER 0xE0DFDEFF
#define UNFOCUSED_BORDER 0xEFF0F1FF

void wsm_config_init() {
    global_config.input_configs = create_list();
    global_config.input_type_configs = create_list();
    global_config.reloading = false;

    global_config.xwayland = XWAYLAND_MODE_LAZY;

    // titlebar
    global_config.titlebar_border_thickness = 1;
    global_config.titlebar_h_padding = 5;
    global_config.titlebar_v_padding = 4;
    global_config.title_align = ALIGN_CENTER;
    global_config.tiling_drag = true;
    global_config.tiling_drag_threshold = 9;

    // borders
    global_config.border = B_NORMAL;
    global_config.floating_border = B_NORMAL;
    global_config.border_thickness = 2;
    global_config.floating_border_thickness = 2;

    // floating view
    global_config.floating_maximum_width = 0;
    global_config.floating_maximum_height = 0;
    global_config.floating_minimum_width = 75;
    global_config.floating_minimum_height = 50;

    // font
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

    color_to_rgba(global_config.border_colors.background, 0xFFFFFFFF);

    global_config.primary_selection = true;
}
