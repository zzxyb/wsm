/*
 * Copyright © 2024 YaoBing Xiao
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial
 * portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT.  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef WSM_TITLEBAR_H
#define WSM_TITLEBAR_H

#include <stdbool.h>

#include <wayland-server-core.h>

struct wlr_scene_rect;
struct wlr_scene_tree;

struct wsm_text_node;
struct wsm_image_node;
struct wsm_image_button;

enum wsm_titlebar_state {
    WSM_ACTIVE = 0,
    WSM_INACTIVE,
    WSM_FOCUS_IN,
    WSM_FOCUS_OUT,
};

enum ssd_part_type {
    SSD_NONE = 0,
    SSD_BUTTON_CLOSE,
    SSD_BUTTON_MAXIMIZE,
    SSD_BUTTON_MINIMIZE,
    SSD_BUTTON_ICONIFY,
    SSD_PART_TITLEBAR,
    SSD_PART_TITLE,
    SSD_PART_TOP_LEFT,
    SSD_PART_TOP_RIGHT,
    SSD_PART_BOTTOM_RIGHT,
    SSD_PART_BOTTOM_LEFT,
    SSD_PART_TOP,
    SSD_PART_RIGHT,
    SSD_PART_BOTTOM,
    SSD_PART_LEFT,
    SSD_CLIENT,
    SSD_FRAME,
    SSD_ROOT,
    SSD_MENU,
    SSD_OSD,
    SSD_LAYER_SURFACE,
    SSD_LAYER_SUBSURFACE,
    SSD_UNMANAGED,
    SSD_END_MARKER
};

struct wsm_titlebar {
    struct wlr_scene_tree *tree;

    struct wlr_scene_tree *border;
    struct wlr_scene_tree *background;
    struct wsm_image_node *icon;
    struct wsm_text_node *title_text;

    struct wsm_image_button *min_button;
    struct wsm_image_button *max_button;
    struct wsm_image_button *close_button;
    bool active;

    struct {
        struct wl_signal double_click;
        struct wl_signal request_state;
    } events;
};

struct wsm_titlebar_event_request_state {
    struct wsm_titlebar *titlebar;
    enum wsm_titlebar_state state;
};

struct wsm_titlebar* wsm_titlebar_create();
void wsm_titlebar_destroy(struct wsm_titlebar *titlebar);

#endif
