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

#ifndef WSM_SEAT_H
#define WSM_SEAT_H

#include "wsm_input.h"

#include <stdbool.h>

#include <pixman.h>

#include <wayland-util.h>
#include <wayland-server-core.h>

#include <wlr/types/wlr_tablet_tool.h>
#include <wlr/types/wlr_input_device.h>

struct wlr_seat;
struct wlr_layer_surface_v1;
struct wlr_pointer_axis_event;
struct wlr_pointer_hold_end_event;
struct wlr_pointer_pinch_end_event;
struct wlr_pointer_swipe_end_event;
struct wlr_pointer_hold_begin_event;
struct wlr_pointer_pinch_begin_event;
struct wlr_pointer_swipe_begin_event;
struct wlr_pointer_swipe_update_event;
struct wlr_pointer_pinch_update_event;
struct wlr_touch_motion_event;
struct wlr_touch_up_event;
struct wlr_touch_down_event;
struct wlr_touch_cancel_event;

struct wsm_cursor;
struct wsm_output;
struct wsm_keyboard;
struct wsm_switch;
struct wsm_tablet;
struct wsm_tablet_pad;
struct wsm_input_device;
struct wsm_tablet_tool;
struct wsm_input_method_relay;

struct wsm_seat {
    struct wlr_seat *wlr_seat;
    struct wsm_cursor *wsm_cursor;

    bool has_focus;
    struct wl_list focus_stack;

    struct wlr_layer_surface_v1 *focused_layer;

    struct wl_client *exclusive_client;

    // Last touch point
    int32_t touch_id;
    double touch_x, touch_y;

    // Seat operations (drag and resize)
    const struct wsm_seatop_impl *seatop_impl;
    void *seatop_data;

    uint32_t last_button_serial;

    uint32_t idle_inhibit_sources, idle_wake_sources;

    struct wl_list deferred_bindings;

    struct wsm_input_method_relay *im_relay;

    struct wl_listener focus_destroy;
    struct wl_listener new_node;
    struct wl_listener request_start_drag;
    struct wl_listener start_drag;
    struct wl_listener request_set_selection;
    struct wl_listener request_set_primary_selection;

    struct wl_list devices;
    struct wl_list keyboard_groups;
    struct wl_list keyboard_shortcuts_inhibitors;

    struct wl_list link;
};

struct wsm_seatop_impl {
	void (*button)(struct wsm_seat *seat, uint32_t time_msec,
			struct wlr_input_device *device, uint32_t button,
			enum wlr_button_state state);
	void (*pointer_motion)(struct wsm_seat *seat, uint32_t time_msec);
	void (*pointer_axis)(struct wsm_seat *seat,
			struct wlr_pointer_axis_event *event);
	void (*hold_begin)(struct wsm_seat *seat,
			struct wlr_pointer_hold_begin_event *event);
	void (*hold_end)(struct wsm_seat *seat,
			struct wlr_pointer_hold_end_event *event);
	void (*pinch_begin)(struct wsm_seat *seat,
			struct wlr_pointer_pinch_begin_event *event);
	void (*pinch_update)(struct wsm_seat *seat,
			struct wlr_pointer_pinch_update_event *event);
	void (*pinch_end)(struct wsm_seat *seat,
			struct wlr_pointer_pinch_end_event *event);
	void (*swipe_begin)(struct wsm_seat *seat,
			struct wlr_pointer_swipe_begin_event *event);
	void (*swipe_update)(struct wsm_seat *seat,
			struct wlr_pointer_swipe_update_event *event);
	void (*swipe_end)(struct wsm_seat *seat,
			struct wlr_pointer_swipe_end_event *event);
    void (*rebase)(struct wsm_seat *seat, uint32_t time_msec);
    void (*touch_motion)(struct wsm_seat *seat,
                         struct wlr_touch_motion_event *event, double lx, double ly);
    void (*touch_up)(struct wsm_seat *seat,
                     struct wlr_touch_up_event *event);
    void (*touch_down)(struct wsm_seat *seat,
                       struct wlr_touch_down_event *event, double lx, double ly);
    void (*touch_cancel)(struct wsm_seat *seat,
                         struct wlr_touch_cancel_event *event);
    void (*tablet_tool_motion)(struct wsm_seat *seat,
                               struct wsm_tablet_tool *tool, uint32_t time_msec);
    void (*tablet_tool_tip)(struct wsm_seat *seat, struct wsm_tablet_tool *tool,
                            uint32_t time_msec, enum wlr_tablet_tool_tip_state state);
    void (*end)(struct wsm_seat *seat);
    void (*render)(struct wsm_seat *seat, struct wsm_output *output,
			pixman_region32_t *damage);
	bool allow_set_cursor;
};

struct wsm_seat_device {
    struct wsm_seat *wsm_seat;
    struct wsm_input_device *input_device;
    struct wsm_keyboard *keyboard;
    struct wsm_switch *switch_device;
    struct wsm_tablet *tablet;
    struct wsm_tablet_pad *tablet_pad;
    struct wl_list link;
};

struct wsm_seat *seat_create(const char *seat_name);
void seat_destroy(struct wsm_seat *seat);
void seat_add_device(struct wsm_seat *seat,
                     struct wsm_input_device *device);
void seat_configure_device(struct wsm_seat *seat,
                           struct wsm_input_device *device);
void seat_reset_device(struct wsm_seat *seat,
                       struct wsm_input_device *input_device);
void seat_remove_device(struct wsm_seat *seat,
                        struct wsm_input_device *device);
void seat_idle_notify_activity(struct wsm_seat *seat,
                               enum wlr_input_device_type source);
void seatop_hold_begin(struct wsm_seat *seat,
                       struct wlr_pointer_hold_begin_event *event);
void seatop_hold_end(struct wsm_seat *seat,
                     struct wlr_pointer_hold_end_event *event);
void seatop_pinch_begin(struct wsm_seat *seat,
                        struct wlr_pointer_pinch_begin_event *event);
void seatop_pinch_update(struct wsm_seat *seat,
                         struct wlr_pointer_pinch_update_event *event);
void seatop_pinch_end(struct wsm_seat *seat,
                      struct wlr_pointer_pinch_end_event *event);
void seatop_swipe_begin(struct wsm_seat *seat,
                        struct wlr_pointer_swipe_begin_event *event);
void seatop_swipe_update(struct wsm_seat *seat,
                         struct wlr_pointer_swipe_update_event *event);
void seatop_swipe_end(struct wsm_seat *seat,
                      struct wlr_pointer_swipe_end_event *event);
void seatop_pointer_motion(struct wsm_seat *seat, uint32_t time_msec);
void seatop_pointer_axis(struct wsm_seat *seat,
                         struct wlr_pointer_axis_event *event);
void seatop_button(struct wsm_seat *seat, uint32_t time_msec,
                   struct wlr_input_device *device, uint32_t button,
                   enum wlr_button_state state);
void seatop_touch_motion(struct wsm_seat *seat,
                         struct wlr_touch_motion_event *event, double lx, double ly);
void seatop_touch_up(struct wsm_seat *seat,
                     struct wlr_touch_up_event *event);
void seatop_touch_down(struct wsm_seat *seat,
                       struct wlr_touch_down_event *event, double lx, double ly);
void seatop_touch_cancel(struct wsm_seat *seat,
                         struct wlr_touch_cancel_event *event);
void seatop_tablet_tool_motion(struct wsm_seat *seat,
                               struct wsm_tablet_tool *tool, uint32_t time_msec);
bool seatop_allows_set_cursor(struct wsm_seat *seat);
void seat_add_device(struct wsm_seat *seat,
                     struct wsm_input_device *device);

#endif