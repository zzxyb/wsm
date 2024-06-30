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

struct wsm_config {
    // border colors
    struct {
        struct border_colors focused;
        struct border_colors focused_inactive;
        struct border_colors focused_tab_title;
        struct border_colors unfocused;
        struct border_colors urgent;
        struct border_colors placeholder;
        float background[4];
    } border_colors;

    enum wsm_container_border border;
    enum wsm_container_border floating_border;
    int border_thickness;
    int floating_border_thickness;

    // floating view
    int32_t floating_maximum_width;
    int32_t floating_maximum_height;
    int32_t floating_minimum_width;
    int32_t floating_minimum_height;

    int font_height;
    int font_baseline;
    bool pango_markup;
    int titlebar_border_thickness;
    int titlebar_h_padding;
    int titlebar_v_padding;

    enum alignment title_align;
    bool tiling_drag;
    int tiling_drag_threshold;

    enum xwayland_mode xwayland;

    size_t urgent_timeout;
    enum wsm_fowa focus_on_window_activation;
    enum wsm_popup_during_fullscreen popup_during_fullscreen;

    bool primary_selection;
};

void wsm_config_init();

#endif
