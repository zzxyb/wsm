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

#define _POSIX_C_SOURCE 200809L
#include "wsm_seatop.h"
#include "wsm_seat.h"
#include "wsm_log.h"
#include "wsm_server.h"
#include "wsm_scene.h"
#include "wsm_cursor.h"

#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/util/edges.h>

static void handle_button(struct wsm_seat *seat, uint32_t time_msec,
                          struct wlr_input_device *device, uint32_t button,
                          enum wlr_button_state state) {

}

static void handle_pointer_motion(struct wsm_seat *seat, uint32_t time_msec) {
    struct wsm_seatop_event *e = seat->seatop_data;
    struct wsm_cursor *cursor = seat->wsm_cursor;

    struct wlr_surface *surface = NULL;
    double sx, sy;
    struct wlr_scene_node *node = wlr_scene_node_at(
     &global_server.wsm_scene->wlr_scene->tree.node, cursor->wlr_cursor->x, cursor->wlr_cursor->y, &sx, &sy);

    if (node != NULL) {
        struct wlr_scene_buffer *scene_buffer = wlr_scene_buffer_from_node(node);
        struct wlr_scene_surface *scene_surface =
            wlr_scene_surface_try_from_buffer(scene_buffer);
        surface = scene_surface->surface;
    }
    if (surface) {
            wlr_seat_pointer_notify_enter(seat->wlr_seat, surface, sx, sy);
            wlr_seat_pointer_notify_motion(seat->wlr_seat, time_msec, sx, sy);
    } else {
        // cursor_update_image(cursor, node);
        wlr_seat_pointer_notify_clear_focus(seat->wlr_seat);
    }

    drag_icons_update_position(seat);
    e->previous_node = node;
}

static void handle_pointer_axis(struct wsm_seat *seat,
                                struct wlr_pointer_axis_event *event) {

}

static void handle_tablet_tool_tip(struct wsm_seat *seat,
                                   struct wsm_tablet_tool *tool, uint32_t time_msec,
                                   enum wlr_tablet_tool_tip_state state) {

}

static void handle_tablet_tool_motion(struct wsm_seat *seat,
                                      struct wsm_tablet_tool *tool, uint32_t time_msec) {

}

static void handle_hold_begin(struct wsm_seat *seat,
                              struct wlr_pointer_hold_begin_event *event) {

}

static void handle_hold_end(struct wsm_seat *seat,
                            struct wlr_pointer_hold_end_event *event) {

}

static void handle_pinch_begin(struct wsm_seat *seat,
                               struct wlr_pointer_pinch_begin_event *event) {

}

static void handle_pinch_update(struct wsm_seat *seat,
                                struct wlr_pointer_pinch_update_event *event) {

}
static void handle_pinch_end(struct wsm_seat *seat,
                             struct wlr_pointer_pinch_end_event *event) {

}

static void handle_swipe_begin(struct wsm_seat *seat,
                               struct wlr_pointer_swipe_begin_event *event) {

}

static void handle_swipe_update(struct wsm_seat *seat,
                                struct wlr_pointer_swipe_update_event *event) {

}

static void handle_swipe_end(struct wsm_seat *seat,
                             struct wlr_pointer_swipe_end_event *event) {

}

static void handle_touch_down(struct wsm_seat *seat,
                              struct wlr_touch_down_event *event, double lx, double ly) {

}

static void handle_rebase(struct wsm_seat *seat, uint32_t time_msec) {

}

static const struct wsm_seatop_impl seatop_impl = {
    .button = handle_button,
    .pointer_motion = handle_pointer_motion,
    .pointer_axis = handle_pointer_axis,
    .tablet_tool_tip = handle_tablet_tool_tip,
    .tablet_tool_motion = handle_tablet_tool_motion,
    .hold_begin = handle_hold_begin,
    .hold_end = handle_hold_end,
    .pinch_begin = handle_pinch_begin,
    .pinch_update = handle_pinch_update,
    .pinch_end = handle_pinch_end,
    .swipe_begin = handle_swipe_begin,
    .swipe_update = handle_swipe_update,
    .swipe_end = handle_swipe_end,
    .touch_down = handle_touch_down,
    .rebase = handle_rebase,
    .allow_set_cursor = true,
};

void wsm_seatop_begin(struct wsm_seat *seat) {
    seatop_end(seat);

    struct wsm_seatop_event *e =
        calloc(1, sizeof(struct wsm_seatop_event));
    wsm_assert(e, "Could not create wsm_seatop_event: allocation failed!");

    seat->seatop_impl = &seatop_impl;
    seat->seatop_data = e;
    seatop_rebase(seat, 0);
}
