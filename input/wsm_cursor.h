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

#ifndef WSM_CURSOR_H
#define WSM_CURSOR_H

#include <stdbool.h>

#include <pixman.h>

#include <wayland-util.h>
#include <wayland-server-core.h>

#include <wlr/types/wlr_input_device.h>

struct wlr_cursor;
struct wsm_seat;
struct wlr_surface;
struct wlr_input_device;
struct wlr_xcursor_manager;
struct wlr_pointer_constraint_v1;
struct wlr_pointer_gestures_v1;
struct wlr_pointer_axis_event;

enum wsm_cursor_mode {
    WSM_CURSOR_PASSTHROUGH,
    WSM_CURSOR_MOVE,
    WSM_CURSOR_RESIZE,
};

enum seat_config_hide_cursor_when_typing {
    HIDE_WHEN_TYPING_DEFAULT,
    HIDE_WHEN_TYPING_ENABLE,
    HIDE_WHEN_TYPING_DISABLE,
};

struct wsm_cursor {
    struct wsm_seat *wsm_seat;
    struct wlr_cursor *wlr_cursor;
    struct {
        double x, y;
        // node
    } previous;
    struct wlr_xcursor_manager *xcursor_manager;
    struct wl_list tablets;
    struct wl_list tablet_pads;

    const char *image;
    struct wl_client *image_client;
    struct wlr_surface *image_surface;
    int hotspot_x, hotspot_y;

    struct wlr_pointer_constraint_v1 *active_constraint;
    pixman_region32_t confine; // invalid if active_constraint == NULL
    bool active_confine_requires_warp;

    struct wlr_pointer_gestures_v1 *pointer_gestures;
    struct wl_listener hold_begin;
    struct wl_listener hold_end;
    struct wl_listener pinch_begin;
    struct wl_listener pinch_update;
    struct wl_listener pinch_end;
    struct wl_listener swipe_begin;
    struct wl_listener swipe_update;
    struct wl_listener swipe_end;

    struct wl_listener motion;
    struct wl_listener motion_absolute;
    struct wl_listener button;
    struct wl_listener axis;
    struct wl_listener frame;

    struct wl_listener touch_down;
    struct wl_listener touch_up;
    struct wl_listener touch_cancel;
    struct wl_listener touch_motion;
    struct wl_listener touch_frame;
    bool simulating_pointer_from_touch;
    bool pointer_touch_up;
    int32_t pointer_touch_id;

    struct wl_listener tool_axis;
    struct wl_listener tool_tip;
    struct wl_listener tool_proximity;
    struct wl_listener tool_button;
    bool simulating_pointer_from_tool_tip;
    bool simulating_pointer_from_tool_button;
    uint32_t tool_buttons;

    struct wl_listener request_set_cursor;
    struct wl_listener image_surface_destroy;

    struct wl_listener constraint_commit;

    struct wl_event_source *hide_source;
    bool hidden;
    enum seat_config_hide_cursor_when_typing hide_when_typing;

    size_t pressed_button_count;
};

struct wsm_cursor *wsm_cursor_create(struct wsm_seat *seat);
void wsm_cursor_destroy(struct wsm_cursor *cursor);
void cursor_set_image(struct wsm_cursor *cursor, const char *image,
                      struct wl_client *client);
void cursor_set_image_surface(struct wsm_cursor *cursor, struct wlr_surface *surface,
                              int32_t hotspot_x, int32_t hotspot_y, struct wl_client *client);
void cursor_rebase(struct wsm_cursor *cursor);
void seatop_rebase(struct wsm_seat *seat, uint32_t time_msec);
void cursor_handle_activity_from_device(struct wsm_cursor *cursor,
                                        struct wlr_input_device *device);
void cursor_handle_activity_from_idle_source(struct wsm_cursor *cursor,
                                             enum wlr_input_device_type idle_source);
void cursor_unhide(struct wsm_cursor *cursor);
void pointer_motion(struct wsm_cursor *cursor, uint32_t time_msec,
                    struct wlr_input_device *device, double dx, double dy,
                    double dx_unaccel, double dy_unaccel);
void dispatch_cursor_button(struct wsm_cursor *cursor,
                            struct wlr_input_device *device, uint32_t time_msec, uint32_t button,
                            enum wlr_button_state state);
void dispatch_cursor_axis(struct wsm_cursor *cursor,
                          struct wlr_pointer_axis_event *event);
void cursor_update_image(struct wsm_seat *seat);

#endif
