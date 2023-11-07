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

struct wsm_text_node;
struct wsm_image_node;
struct wsm_image_button;

enum wsm_titlebar_state {
    WSM_ACTIVE = 0,
    WSM_INACTIVE,
    WSM_FOCUS_IN,
    WSM_FOCUS_OUT,
};

struct wsm_titlebar {
    struct wlr_scene_rect *background;
    struct wsm_image_node *icon;
    struct wsm_text_node *title;
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
void wsm_output_destroy(struct wsm_titlebar *titlebar);

#endif
