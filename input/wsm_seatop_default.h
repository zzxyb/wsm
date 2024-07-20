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

#ifndef WSM_SEATOP_DEFAULT_H
#define WSM_SEATOP_DEFAULT_H

#include "gesture/wsm_gesture.h"

#include <stdint.h>

#include <xkbcommon/xkbcommon.h>

#include <wlr/util/edges.h>

#define WSM_CURSOR_PRESSED_BUTTONS_CAP 32

struct wlr_scene_node;

struct wsm_list;
struct wsm_seat;
struct wsm_container;
struct wlr_surface;
struct wsm_cursor;

struct wsm_seatop_event {
    struct wlr_scene_node *previous_node;
    uint32_t pressed_buttons[WSM_CURSOR_PRESSED_BUTTONS_CAP];
    size_t pressed_button_count;
    struct wsm_gesture_tracker gestures;
};

void seatop_begin_default(struct wsm_seat *seat);
enum wlr_edges find_resize_edge(struct wsm_container *cont,
                                struct wlr_surface *surface, struct wsm_cursor *cursor);

#endif
