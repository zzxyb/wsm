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

#include "wsm_cursor.h"
#include "wsm_server.h"
#include "wsm_seat.h"
#include "wsm_log.h"
#include "wsm_view.h"
#include "wsm_tablet.h"
#include "wsm_common.h"
#include "wsm_scene.h"
#include "wsm_output.h"
#include "wsm_container.h"
#include "wsm_workspace.h"
#include "wsm_output_manager.h"
#include "wsm_input_manager.h"
#include "wsm_seatop_default.h"
#include "node/wsm_node_descriptor.h"

#include <stdlib.h>
#include <linux/input-event-codes.h>

#include <wlr/util/region.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_touch.h>
#include <wlr/types/wlr_tablet_v2.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_xcursor_manager.h>
#include <wlr/types/wlr_relative_pointer_v1.h>
#include <wlr/types/wlr_cursor_shape_v1.h>
#include <wlr/types/wlr_pointer_constraints_v1.h>

#define WLR_CURSOR_SHAPE_V1_VERSION 1

static void cursor_hide(struct wsm_cursor *cursor) {
    wlr_cursor_unset_image(cursor->wlr_cursor);
    cursor->hidden = true;
    wlr_seat_pointer_notify_clear_focus(cursor->wsm_seat->wlr_seat);
}

static int hide_notify(void *data) {
    struct wsm_cursor *cursor = data;
    cursor_hide(cursor);
    return 1;
}

static void handle_request_cursor(struct wl_listener *listener, void *data) {
    struct wsm_cursor *cursor = wl_container_of(listener, cursor, request_cursor);
    struct wsm_seat *seat = cursor->wsm_seat;

    if (!seatop_allows_set_cursor(seat)) {
        return;
    }

    struct wlr_seat_pointer_request_set_cursor_event *event = data;
    struct wl_client *focused_client = NULL;
    struct wlr_surface *focused_surface =
        seat->wlr_seat->pointer_state.focused_surface;
    if (focused_surface != NULL) {
        focused_client = wl_resource_get_client(focused_surface->resource);
    }

    if (focused_client == NULL ||
        event->seat_client->client != focused_client) {
        wsm_log(WSM_ERROR, "denying request to set cursor from unfocused client");
        return;
    }

    cursor_set_image_surface(cursor, event->surface, event->hotspot_x,
                             event->hotspot_y, focused_client);
}

static void handle_request_set_shape(struct wl_listener *listener, void *data) {
    struct wlr_cursor_shape_manager_v1_request_set_shape_event *event = data;
    const char *shape_name = wlr_cursor_shape_v1_name(event->shape);
    struct wsm_cursor *cursor = wl_container_of(listener, cursor, request_set_shape);
    struct wsm_seat *seat = cursor->wsm_seat;

    if (!seatop_allows_set_cursor(seat)) {
        return;
    }

    struct wl_client *focused_client = NULL;
    struct wlr_surface *focused_surface = seat->wlr_seat->pointer_state.focused_surface;
    if (focused_surface != NULL) {
        focused_client = wl_resource_get_client(focused_surface->resource);
    }

    if (focused_client == NULL || event->seat_client->client != focused_client) {
        wsm_log(WSM_ERROR, "denying request to set cursor from unfocused client");
        return;
    }

    wsm_log(WSM_DEBUG, "set xcursor to shape %s", shape_name);
    cursor_set_image(cursor, wlr_cursor_shape_v1_name(event->shape), focused_client);
}

static void handle_image_surface_destroy(struct wl_listener *listener,
                                         void *data) {
    struct wsm_cursor *cursor =
        wl_container_of(listener, cursor, image_surface_destroy);
    cursor_set_image(cursor, NULL, cursor->image_client);
    cursor_rebase(cursor);
}

static void handle_pointer_hold_begin(struct wl_listener *listener, void *data) {
    struct wsm_cursor *cursor = wl_container_of(
        listener, cursor, hold_begin);
    struct wlr_pointer_hold_begin_event *event = data;
    cursor_handle_activity_from_device(cursor, &event->pointer->base);
    seatop_hold_begin(cursor->wsm_seat, event);
}

static void handle_pointer_hold_end(struct wl_listener *listener, void *data) {
    struct wsm_cursor *cursor = wl_container_of(
        listener, cursor, hold_end);
    struct wlr_pointer_hold_end_event *event = data;
    cursor_handle_activity_from_device(cursor, &event->pointer->base);
    seatop_hold_end(cursor->wsm_seat, event);
}

static void handle_pointer_pinch_begin(struct wl_listener *listener, void *data) {
    struct wsm_cursor *cursor = wl_container_of(
        listener, cursor, pinch_begin);
    struct wlr_pointer_pinch_begin_event *event = data;
    cursor_handle_activity_from_device(cursor, &event->pointer->base);
    seatop_pinch_begin(cursor->wsm_seat, event);
}

static void handle_pointer_pinch_update(struct wl_listener *listener, void *data) {
    struct wsm_cursor *cursor = wl_container_of(
        listener, cursor, pinch_update);
    struct wlr_pointer_pinch_update_event *event = data;
    cursor_handle_activity_from_device(cursor, &event->pointer->base);
    seatop_pinch_update(cursor->wsm_seat, event);
}

static void handle_pointer_pinch_end(struct wl_listener *listener, void *data) {
    struct wsm_cursor *cursor = wl_container_of(
        listener, cursor, pinch_end);
    struct wlr_pointer_pinch_end_event *event = data;
    cursor_handle_activity_from_device(cursor, &event->pointer->base);
    seatop_pinch_end(cursor->wsm_seat, event);
}

static void handle_pointer_swipe_begin(struct wl_listener *listener, void *data) {
    struct wsm_cursor *cursor = wl_container_of(
        listener, cursor, swipe_begin);
    struct wlr_pointer_swipe_begin_event *event = data;
    cursor_handle_activity_from_device(cursor, &event->pointer->base);
    seatop_swipe_begin(cursor->wsm_seat, event);
}

static void handle_pointer_swipe_update(struct wl_listener *listener, void *data) {
    struct wsm_cursor *cursor = wl_container_of(
        listener, cursor, swipe_update);
    struct wlr_pointer_swipe_update_event *event = data;
    cursor_handle_activity_from_device(cursor, &event->pointer->base);
    seatop_swipe_update(cursor->wsm_seat, event);
}

static void handle_pointer_swipe_end(struct wl_listener *listener, void *data) {
    struct wsm_cursor *cursor = wl_container_of(
        listener, cursor, swipe_end);
    struct wlr_pointer_swipe_end_event *event = data;
    cursor_handle_activity_from_device(cursor, &event->pointer->base);
    seatop_swipe_end(cursor->wsm_seat, event);
}

void pointer_motion(struct wsm_cursor *cursor, uint32_t time_msec,
                    struct wlr_input_device *device, double dx, double dy,
                    double dx_unaccel, double dy_unaccel) {
    wlr_relative_pointer_manager_v1_send_relative_motion(
        global_server.wlr_relative_pointer_manager,
        cursor->wsm_seat->wlr_seat, (uint64_t)time_msec * 1000,
        dx, dy, dx_unaccel, dy_unaccel);

    // Only apply pointer constraints to real pointer input.
    if (cursor->active_constraint && device->type == WLR_INPUT_DEVICE_POINTER) {
        struct wlr_surface *surface = NULL;
        double sx, sy;
        node_at_coords(cursor->wsm_seat,
                       cursor->wlr_cursor->x, cursor->wlr_cursor->y, &surface, &sx, &sy);

        if (cursor->active_constraint->surface != surface) {
            return;
        }

        double sx_confined, sy_confined;
        if (!wlr_region_confine(&cursor->confine, sx, sy, sx + dx, sy + dy,
                                &sx_confined, &sy_confined)) {
            return;
        }

        dx = sx_confined - sx;
        dy = sy_confined - sy;
    }

    wlr_cursor_move(cursor->wlr_cursor, device, dx, dy);

    seatop_pointer_motion(cursor->wsm_seat, time_msec);
}

static void handle_pointer_motion_relative(
    struct wl_listener *listener, void *data) {
    struct wsm_cursor *cursor = wl_container_of(listener, cursor, motion);
    struct wlr_pointer_motion_event *e = data;
    cursor_handle_activity_from_device(cursor, &e->pointer->base);

    pointer_motion(cursor, e->time_msec, &e->pointer->base, e->delta_x,
                   e->delta_y, e->unaccel_dx, e->unaccel_dy);
}

static void handle_pointer_motion_absolute(
    struct wl_listener *listener, void *data) {
    struct wsm_cursor *cursor =
        wl_container_of(listener, cursor, motion_absolute);
    struct wlr_pointer_motion_absolute_event *event = data;
    cursor_handle_activity_from_device(cursor, &event->pointer->base);

    double lx, ly;
    wlr_cursor_absolute_to_layout_coords(cursor->wlr_cursor, &event->pointer->base,
                                         event->x, event->y, &lx, &ly);

    double dx = lx - cursor->wlr_cursor->x;
    double dy = ly - cursor->wlr_cursor->y;

    pointer_motion(cursor, event->time_msec, &event->pointer->base, dx, dy,
                   dx, dy);
}

static void handle_pointer_button(struct wl_listener *listener, void *data) {
    struct wsm_cursor *cursor = wl_container_of(listener, cursor, button);
    struct wlr_pointer_button_event *event = data;

    if (event->state == WL_POINTER_BUTTON_STATE_PRESSED) {
        cursor->pressed_button_count++;
    } else {
        if (cursor->pressed_button_count > 0) {
            cursor->pressed_button_count--;
        } else {
            wsm_log(WSM_ERROR, "Pressed button count was wrong");
        }
    }

    cursor_handle_activity_from_device(cursor, &event->pointer->base);
    dispatch_cursor_button(cursor, &event->pointer->base,
                           event->time_msec, event->button, event->state);
}

static void handle_pointer_axis(struct wl_listener *listener, void *data) {
    struct wsm_cursor *cursor = wl_container_of(listener, cursor, axis);
    struct wlr_pointer_axis_event *event = data;
    cursor_handle_activity_from_device(cursor, &event->pointer->base);
    dispatch_cursor_axis(cursor, event);
}

static void handle_pointer_frame(struct wl_listener *listener, void *data) {
    struct wsm_cursor *cursor = wl_container_of(listener, cursor, frame);
    wlr_seat_pointer_notify_frame(cursor->wsm_seat->wlr_seat);
}

static void handle_touch_down(struct wl_listener *listener, void *data) {
    struct wsm_cursor *cursor = wl_container_of(listener, cursor, touch_down);
    struct wlr_touch_down_event *event = data;
    cursor_handle_activity_from_device(cursor, &event->touch->base);
    cursor_hide(cursor);

    struct wsm_seat *seat = cursor->wsm_seat;

    double lx, ly;
    wlr_cursor_absolute_to_layout_coords(cursor->wlr_cursor, &event->touch->base,
                                         event->x, event->y, &lx, &ly);

    seat->touch_id = event->touch_id;
    seat->touch_x = lx;
    seat->touch_y = ly;

    seatop_touch_down(seat, event, lx, ly);
}

static void handle_touch_motion(struct wl_listener *listener, void *data) {
    struct wsm_cursor *cursor =
        wl_container_of(listener, cursor, touch_motion);
    struct wlr_touch_motion_event *event = data;
    cursor_handle_activity_from_device(cursor, &event->touch->base);

    struct wsm_seat *seat = cursor->wsm_seat;

    double lx, ly;
    wlr_cursor_absolute_to_layout_coords(cursor->wlr_cursor, &event->touch->base,
                                         event->x, event->y, &lx, &ly);

    if (seat->touch_id == event->touch_id) {
        seat->touch_x = lx;
        seat->touch_y = ly;
        drag_icons_update_position(seat);
    }

    if (cursor->simulating_pointer_from_touch) {
        if (seat->touch_id == cursor->pointer_touch_id) {
            double dx, dy;
            dx = lx - cursor->wlr_cursor->x;
            dy = ly - cursor->wlr_cursor->y;
            pointer_motion(cursor, event->time_msec, &event->touch->base,
                           dx, dy, dx, dy);
        }
    } else {
        seatop_touch_motion(seat, event, lx, ly);
    }
}

static void handle_touch_up(struct wl_listener *listener, void *data) {
    struct wsm_cursor *cursor = wl_container_of(listener, cursor, touch_up);
    struct wlr_touch_up_event *event = data;
    cursor_handle_activity_from_device(cursor, &event->touch->base);

    if (cursor->simulating_pointer_from_touch) {
        if (cursor->pointer_touch_id == cursor->wsm_seat->touch_id) {
            cursor->pointer_touch_up = true;
            dispatch_cursor_button(cursor, &event->touch->base,
                                   event->time_msec, BTN_LEFT, WL_POINTER_BUTTON_STATE_RELEASED);
        }
    } else {
        struct wsm_seat *seat = cursor->wsm_seat;
        seatop_touch_up(seat, event);
    }
}

static void handle_touch_frame(struct wl_listener *listener, void *data) {
    struct wsm_cursor *cursor =
        wl_container_of(listener, cursor, touch_frame);

    struct wlr_seat *wlr_seat = cursor->wsm_seat->wlr_seat;

    if (cursor->simulating_pointer_from_touch) {
        wlr_seat_pointer_notify_frame(wlr_seat);

        if (cursor->pointer_touch_up) {
            cursor->pointer_touch_up = false;
            cursor->simulating_pointer_from_touch = false;
        }
    } else {
        wlr_seat_touch_notify_frame(wlr_seat);
    }
}

static void handle_tablet_tool_position(struct wsm_cursor *cursor,
                                        struct wsm_tablet_tool *tool,
                                        bool change_x, bool change_y,
                                        double x, double y, double dx, double dy,
                                        int32_t time_msec) {

    if (!change_x && !change_y) {
        return;
    }

    struct wsm_tablet *tablet = tool->tablet;
    struct wsm_input_device *input_device = tablet->seat_device->input_device;

    switch (tool->mode) {
    case WSM_TABLET_TOOL_MODE_ABSOLUTE:
        wlr_cursor_warp_absolute(cursor->wlr_cursor, input_device->wlr_device,
                                 change_x ? x : NAN, change_y ? y : NAN);
        break;
    case WSM_TABLET_TOOL_MODE_RELATIVE:
        wlr_cursor_move(cursor->wlr_cursor, input_device->wlr_device, dx, dy);
        break;
    }

    double sx, sy;
    struct wlr_surface *surface = NULL;
    struct wsm_seat *seat = cursor->wsm_seat;
    wlr_scene_node_at(
        &global_server.wsm_scene->root_scene->tree.node, cursor->wlr_cursor->x, cursor->wlr_cursor->y, &sx, &sy);

    if (!cursor->simulating_pointer_from_tool_tip &&
        ((surface && wlr_surface_accepts_tablet_v2(tablet->tablet_v2, surface)) ||
         wlr_tablet_tool_v2_has_implicit_grab(tool->tablet_v2_tool))) {
        seatop_tablet_tool_motion(seat, tool, time_msec);
    } else {
        wlr_tablet_v2_tablet_tool_notify_proximity_out(tool->tablet_v2_tool);
        pointer_motion(cursor, time_msec, input_device->wlr_device, dx, dy, dx, dy);
    }
}

static void handle_tool_axis(struct wl_listener *listener, void *data) {
    struct wsm_cursor *cursor = wl_container_of(listener, cursor, tool_axis);
    struct wlr_tablet_tool_axis_event *event = data;
    cursor_handle_activity_from_device(cursor, &event->tablet->base);

    struct wsm_tablet_tool *wsm_tool = event->tool->data;
    if (!wsm_tool) {
        wsm_log(WSM_DEBUG, "tool axis before proximity");
        return;
    }

    handle_tablet_tool_position(cursor, wsm_tool,
                                event->updated_axes & WLR_TABLET_TOOL_AXIS_X,
                                event->updated_axes & WLR_TABLET_TOOL_AXIS_Y,
                                event->x, event->y, event->dx, event->dy, event->time_msec);

    if (event->updated_axes & WLR_TABLET_TOOL_AXIS_PRESSURE) {
        wlr_tablet_v2_tablet_tool_notify_pressure(
            wsm_tool->tablet_v2_tool, event->pressure);
    }

    if (event->updated_axes & WLR_TABLET_TOOL_AXIS_DISTANCE) {
        wlr_tablet_v2_tablet_tool_notify_distance(
            wsm_tool->tablet_v2_tool, event->distance);
    }

    if (event->updated_axes & WLR_TABLET_TOOL_AXIS_TILT_X) {
        wsm_tool->tilt_x = event->tilt_x;
    }

    if (event->updated_axes & WLR_TABLET_TOOL_AXIS_TILT_Y) {
        wsm_tool->tilt_y = event->tilt_y;
    }

    if (event->updated_axes & (WLR_TABLET_TOOL_AXIS_TILT_X | WLR_TABLET_TOOL_AXIS_TILT_Y)) {
        wlr_tablet_v2_tablet_tool_notify_tilt(
            wsm_tool->tablet_v2_tool,
            wsm_tool->tilt_x, wsm_tool->tilt_y);
    }

    if (event->updated_axes & WLR_TABLET_TOOL_AXIS_ROTATION) {
        wlr_tablet_v2_tablet_tool_notify_rotation(
            wsm_tool->tablet_v2_tool, event->rotation);
    }

    if (event->updated_axes & WLR_TABLET_TOOL_AXIS_SLIDER) {
        wlr_tablet_v2_tablet_tool_notify_slider(
            wsm_tool->tablet_v2_tool, event->slider);
    }

    if (event->updated_axes & WLR_TABLET_TOOL_AXIS_WHEEL) {
        wlr_tablet_v2_tablet_tool_notify_wheel(
            wsm_tool->tablet_v2_tool, event->wheel_delta, 0);
    }
}

static void handle_tool_tip(struct wl_listener *listener, void *data) {
    struct wsm_cursor *cursor = wl_container_of(listener, cursor, tool_tip);
    struct wlr_tablet_tool_tip_event *event = data;
    cursor_handle_activity_from_device(cursor, &event->tablet->base);

    struct wsm_tablet_tool *wsm_tool = event->tool->data;
    struct wlr_tablet_v2_tablet *tablet_v2 = wsm_tool->tablet->tablet_v2;
    struct wsm_seat *seat = cursor->wsm_seat;


    double sx, sy;
    struct wlr_surface *surface = NULL;
    wlr_scene_node_at(
        &global_server.wsm_scene->root_scene->tree.node, cursor->wlr_cursor->x, cursor->wlr_cursor->y, &sx, &sy);

    if (cursor->simulating_pointer_from_tool_tip &&
        event->state == WLR_TABLET_TOOL_TIP_UP) {
        cursor->simulating_pointer_from_tool_tip = false;
        dispatch_cursor_button(cursor, &event->tablet->base, event->time_msec,
                               BTN_LEFT, WL_POINTER_BUTTON_STATE_RELEASED);
        wlr_seat_pointer_notify_frame(seat->wlr_seat);
    } else if (!surface || !wlr_surface_accepts_tablet_v2(tablet_v2, surface)) {
        if (event->state == WLR_TABLET_TOOL_TIP_UP) {
            seatop_tablet_tool_tip(seat, wsm_tool, event->time_msec,
                                   WLR_TABLET_TOOL_TIP_UP);
        } else {
            cursor->simulating_pointer_from_tool_tip = true;
            dispatch_cursor_button(cursor, &event->tablet->base,
                                   event->time_msec, BTN_LEFT, WL_POINTER_BUTTON_STATE_PRESSED);
            wlr_seat_pointer_notify_frame(seat->wlr_seat);
        }
    } else {
        seatop_tablet_tool_tip(seat, wsm_tool, event->time_msec, event->state);
    }
}

static struct wsm_tablet *get_tablet_for_device(struct wsm_cursor *cursor,
                                                 struct wlr_input_device *device) {
    struct wsm_tablet *tablet;
    wl_list_for_each(tablet, &cursor->tablets, link) {
        if (tablet->seat_device->input_device->wlr_device == device) {
            return tablet;
        }
    }
    return NULL;
}

static void handle_tool_proximity(struct wl_listener *listener, void *data) {
    struct wsm_cursor *cursor =
        wl_container_of(listener, cursor, tool_proximity);
    struct wlr_tablet_tool_proximity_event *event = data;
    cursor_handle_activity_from_device(cursor, &event->tablet->base);

    struct wlr_tablet_tool *tool = event->tool;
    if (!tool->data) {
        struct wsm_tablet *tablet = get_tablet_for_device(cursor,
                                                           &event->tablet->base);
        if (!tablet) {
            wsm_log(WSM_ERROR, "no tablet for tablet tool");
            return;
        }
        wsm_tablet_tool_configure(tablet, tool);
    }

    struct wsm_tablet_tool *wsm_tool = tool->data;
    if (!wsm_tool) {
        wsm_log(WSM_ERROR, "tablet tool not initialized");
        return;
    }

    if (event->state == WLR_TABLET_TOOL_PROXIMITY_OUT) {
        wlr_tablet_v2_tablet_tool_notify_proximity_out(wsm_tool->tablet_v2_tool);
        return;
    }

    handle_tablet_tool_position(cursor, wsm_tool, true, true, event->x, event->y,
                                0, 0, event->time_msec);
}

static void handle_tool_button(struct wl_listener *listener, void *data) {
    struct wsm_cursor *cursor = wl_container_of(listener, cursor, tool_button);
    struct wlr_tablet_tool_button_event *event = data;
    cursor_handle_activity_from_device(cursor, &event->tablet->base);

    struct wsm_tablet_tool *wsm_tool = event->tool->data;
    if (!wsm_tool) {
        wsm_log(WSM_DEBUG, "tool button before proximity");
        return;
    }

    wlr_tablet_v2_tablet_tool_notify_button(wsm_tool->tablet_v2_tool,
                                            event->button, (enum zwp_tablet_pad_v2_button_state)event->state);

    switch (event->state) {
    case WL_POINTER_BUTTON_STATE_PRESSED:
        cursor->tool_buttons++;
        break;
    case WLR_BUTTON_RELEASED:
        if (cursor->tool_buttons == 0) {
            wsm_log(WSM_ERROR, "inconsistent tablet tool button events");
        } else {
            cursor->tool_buttons--;
        }
        break;
    }
}

static void handle_request_pointer_set_cursor(struct wl_listener *listener,
                                              void *data) {
    struct wsm_cursor *cursor =
        wl_container_of(listener, cursor, request_set_cursor);
    if (!seatop_allows_set_cursor(cursor->wsm_seat)) {
        return;
    }
    struct wlr_seat_pointer_request_set_cursor_event *event = data;

    struct wl_client *focused_client = NULL;
    struct wlr_surface *focused_surface =
        cursor->wsm_seat->wlr_seat->pointer_state.focused_surface;
    if (focused_surface != NULL) {
        focused_client = wl_resource_get_client(focused_surface->resource);
    }

    if (focused_client == NULL ||
        event->seat_client->client != focused_client) {
        wsm_log(WSM_DEBUG, "denying request to set cursor from unfocused client");
        return;
    }

    cursor_set_image_surface(cursor, event->surface, event->hotspot_x,
                             event->hotspot_y, focused_client);
}

struct wsm_cursor *wsm_cursor_create(const struct wsm_server* server, struct wsm_seat *seat) {
    struct wsm_cursor *cursor = calloc(1, sizeof(struct wsm_cursor));
    if (!wsm_assert(cursor, "Could not create wsm_cursor: allocation failed!")) {
        return NULL;
    }

    struct wlr_cursor *wlr_cursor = wlr_cursor_create();
    if (!wsm_assert(wlr_cursor, "could not allocate wlr_cursor")) {
        return NULL;
    }
    cursor->wlr_cursor = wlr_cursor;
    cursor->previous.x = wlr_cursor->x;
    cursor->previous.y = wlr_cursor->y;

    cursor->wsm_seat = seat;
    wlr_cursor_attach_output_layout(wlr_cursor, server->wsm_scene->output_layout);

    wlr_cursor_set_xcursor(cursor->wlr_cursor, global_server.xcursor_manager, "default");

    cursor->request_cursor.notify = handle_request_cursor;
    wl_signal_add(&seat->wlr_seat->events.request_set_cursor,
                  &cursor->request_cursor);

    struct wlr_cursor_shape_manager_v1 *cursor_shape_manager =
        wlr_cursor_shape_manager_v1_create(server->wl_display,
                                           WLR_CURSOR_SHAPE_V1_VERSION);
    if (!wsm_assert(cursor_shape_manager, "Could not create wlr_cursor_shape_manager_v1: allocation failed!")) {
        free(cursor);
        return NULL;
    }
    cursor->request_set_shape.notify = handle_request_set_shape;
    wl_signal_add(&cursor_shape_manager->events.request_set_shape,
                  &cursor->request_set_shape);

    cursor->hide_source = wl_event_loop_add_timer(server->wl_event_loop,
                                                  hide_notify, cursor);

    wl_list_init(&cursor->image_surface_destroy.link);
    cursor->image_surface_destroy.notify = handle_image_surface_destroy;

    cursor->hold_begin.notify = handle_pointer_hold_begin;
    wl_signal_add(&wlr_cursor->events.hold_begin, &cursor->hold_begin);

    cursor->hold_end.notify = handle_pointer_hold_end;
    wl_signal_add(&wlr_cursor->events.hold_end, &cursor->hold_end);

    cursor->pinch_begin.notify = handle_pointer_pinch_begin;
    wl_signal_add(&wlr_cursor->events.pinch_begin, &cursor->pinch_begin);

    cursor->pinch_update.notify = handle_pointer_pinch_update;
    wl_signal_add(&wlr_cursor->events.pinch_update, &cursor->pinch_update);

    cursor->pinch_end.notify = handle_pointer_pinch_end;
    wl_signal_add(&wlr_cursor->events.pinch_end, &cursor->pinch_end);

    cursor->swipe_begin.notify = handle_pointer_swipe_begin;
    wl_signal_add(&wlr_cursor->events.swipe_begin, &cursor->swipe_begin);

    cursor->swipe_update.notify = handle_pointer_swipe_update;
    wl_signal_add(&wlr_cursor->events.swipe_update, &cursor->swipe_update);

    cursor->swipe_end.notify = handle_pointer_swipe_end;
    wl_signal_add(&wlr_cursor->events.swipe_end, &cursor->swipe_end);

    cursor->motion.notify = handle_pointer_motion_relative;
    wl_signal_add(&wlr_cursor->events.motion, &cursor->motion);

    cursor->motion_absolute.notify = handle_pointer_motion_absolute;
    wl_signal_add(&wlr_cursor->events.motion_absolute,
                  &cursor->motion_absolute);

    cursor->button.notify = handle_pointer_button;
    wl_signal_add(&wlr_cursor->events.button, &cursor->button);

    cursor->axis.notify = handle_pointer_axis;
    wl_signal_add(&wlr_cursor->events.axis, &cursor->axis);

    cursor->frame.notify = handle_pointer_frame;
    wl_signal_add(&wlr_cursor->events.frame, &cursor->frame);

    cursor->touch_down.notify = handle_touch_down;
    wl_signal_add(&wlr_cursor->events.touch_down, &cursor->touch_down);

    cursor->touch_up.notify = handle_touch_up;
    wl_signal_add(&wlr_cursor->events.touch_up, &cursor->touch_up);

    cursor->touch_motion.notify = handle_touch_motion;
    wl_signal_add(&wlr_cursor->events.touch_motion,
                  &cursor->touch_motion);

    cursor->touch_frame.notify = handle_touch_frame;
    wl_signal_add(&wlr_cursor->events.touch_frame, &cursor->touch_frame);

    cursor->tool_axis.notify = handle_tool_axis;
    wl_signal_add(&wlr_cursor->events.tablet_tool_axis,
                  &cursor->tool_axis);

    cursor->tool_tip.notify = handle_tool_tip;
    wl_signal_add(&wlr_cursor->events.tablet_tool_tip, &cursor->tool_tip);

    cursor->tool_proximity.notify = handle_tool_proximity;
    wl_signal_add(&wlr_cursor->events.tablet_tool_proximity, &cursor->tool_proximity);

    cursor->tool_button.notify = handle_tool_button;
    wl_signal_add(&wlr_cursor->events.tablet_tool_button, &cursor->tool_button);

    cursor->request_set_cursor.notify = handle_request_pointer_set_cursor;
    wl_signal_add(&seat->wlr_seat->events.request_set_cursor,
                  &cursor->request_set_cursor);

    wl_list_init(&cursor->constraint_commit.link);
    wl_list_init(&cursor->tablets);
    wl_list_init(&cursor->tablet_pads);

    cursor->wlr_cursor = wlr_cursor;

    return cursor;
}

void wsm_cursor_destroy(struct wsm_cursor *cursor) {
    if (!cursor) {
        wsm_log(WSM_ERROR, "wsm_cursor is NULL!");
        return;
    }

    wl_event_source_remove(cursor->hide_source);

    wl_list_remove(&cursor->image_surface_destroy.link);
    wl_list_remove(&cursor->hold_begin.link);
    wl_list_remove(&cursor->hold_end.link);
    wl_list_remove(&cursor->pinch_begin.link);
    wl_list_remove(&cursor->pinch_update.link);
    wl_list_remove(&cursor->pinch_end.link);
    wl_list_remove(&cursor->swipe_begin.link);
    wl_list_remove(&cursor->swipe_update.link);
    wl_list_remove(&cursor->swipe_end.link);
    wl_list_remove(&cursor->motion.link);
    wl_list_remove(&cursor->motion_absolute.link);
    wl_list_remove(&cursor->button.link);
    wl_list_remove(&cursor->axis.link);
    wl_list_remove(&cursor->frame.link);
    wl_list_remove(&cursor->touch_down.link);
    wl_list_remove(&cursor->touch_up.link);
    wl_list_remove(&cursor->touch_cancel.link);
    wl_list_remove(&cursor->touch_motion.link);
    wl_list_remove(&cursor->touch_frame.link);
    wl_list_remove(&cursor->tool_axis.link);
    wl_list_remove(&cursor->tool_tip.link);
    wl_list_remove(&cursor->tool_button.link);
    wl_list_remove(&cursor->request_set_cursor.link);

    wlr_xcursor_manager_destroy(global_server.xcursor_manager);
    global_server.xcursor_manager = NULL;

    wlr_cursor_destroy(cursor->wlr_cursor);
    free(cursor);
}

void cursor_rebase_all(void) {
    if (!global_server.wsm_scene->outputs->length) {
        return;
    }

    struct wsm_seat *seat;
    wl_list_for_each(seat, &global_server.wsm_input_manager->seats, link) {
        cursor_rebase(seat->wsm_cursor);
    }
}

static void set_image_surface(struct wsm_cursor *cursor,
                              struct wlr_surface *surface) {
    wl_list_remove(&cursor->image_surface_destroy.link);
    cursor->image_surface = surface;
    if (surface) {
        wl_signal_add(&surface->events.destroy, &cursor->image_surface_destroy);
    } else {
        wl_list_init(&cursor->image_surface_destroy.link);
    }
}

void cursor_set_image(struct wsm_cursor *cursor, const char *image,
                      struct wl_client *client) {
    if (!(cursor->wsm_seat->wlr_seat->capabilities & WL_SEAT_CAPABILITY_POINTER)) {
        return;
    }

    const char *current_image = cursor->image;
    set_image_surface(cursor, NULL);
    cursor->image = image;
    cursor->hotspot_x = cursor->hotspot_y = 0;
    cursor->image_client = client;

    if (cursor->hidden) {
        return;
    }

    if (!image) {
        wlr_cursor_unset_image(cursor->wlr_cursor);
    } else if (!current_image || strcmp(current_image, image) != 0) {
        wlr_cursor_set_xcursor(cursor->wlr_cursor, global_server.xcursor_manager, image);
    }
}

void cursor_set_image_surface(struct wsm_cursor *cursor,
                              struct wlr_surface *surface, int32_t hotspot_x, int32_t hotspot_y,
                              struct wl_client *client) {
    if (!(cursor->wsm_seat->wlr_seat->capabilities & WL_SEAT_CAPABILITY_POINTER)) {
        return;
    }

    set_image_surface(cursor, surface);
    cursor->image = NULL;
    cursor->hotspot_x = hotspot_x;
    cursor->hotspot_y = hotspot_y;
    cursor->image_client = client;

    if (cursor->hidden) {
        return;
    }

    wlr_cursor_set_surface(cursor->wlr_cursor, surface, hotspot_x, hotspot_y);
}

void cursor_rebase(struct wsm_cursor *cursor) {
    uint32_t time_msec = get_current_time_msec();
    seatop_rebase(cursor->wsm_seat, time_msec);
}

void cursor_handle_activity_from_device(struct wsm_cursor *cursor,
                                        struct wlr_input_device *device) {
    cursor_handle_activity_from_idle_source(cursor, device->type);
}

void cursor_handle_activity_from_idle_source(struct wsm_cursor *cursor,
                                             enum wlr_input_device_type idle_source) {
    seat_idle_notify_activity(cursor->wsm_seat, idle_source);
    if (idle_source != WLR_INPUT_DEVICE_TOUCH) {
        cursor_unhide(cursor);
    }
}

void cursor_unhide(struct wsm_cursor *cursor) {
    if (!cursor->hidden) {
        return;
    }

    cursor->hidden = false;
    if (cursor->image_surface) {
        cursor_set_image_surface(cursor,
                                 cursor->image_surface,
                                 cursor->hotspot_x,
                                 cursor->hotspot_y,
                                 cursor->image_client);
    } else {
        const char *image = cursor->image;
        cursor->image = NULL;
        cursor_set_image(cursor, image, cursor->image_client);
    }
    cursor_rebase(cursor);
}

void dispatch_cursor_axis(struct wsm_cursor *cursor,
                          struct wlr_pointer_axis_event *event) {
    seatop_pointer_axis(cursor->wsm_seat, event);
}

void
cursor_update_image(struct wsm_cursor *cursor, struct wsm_node *node) {
    if (node && node->type == N_CONTAINER) {
        enum wlr_edges edge = find_resize_edge(node->wsm_container, NULL, cursor);
        if (edge == WLR_EDGE_NONE) {
            cursor_set_image(cursor, "default", NULL);
        } else if (container_is_floating(node->wsm_container)) {
            cursor_set_image(cursor, wlr_xcursor_get_resize_name(edge), NULL);
        } else {
            if (edge & (WLR_EDGE_LEFT | WLR_EDGE_RIGHT)) {
                cursor_set_image(cursor, "col-resize", NULL);
            } else {
                cursor_set_image(cursor, "row-resize", NULL);
            }
        }
    } else {
        cursor_set_image(cursor, "default", NULL);
    }
}

void cursor_warp_to_container(struct wsm_cursor *cursor,
                              struct wsm_container *container, bool force) {
    if (!container) {
        return;
    }

    struct wlr_box box;
    container_get_box(container, &box);
    if (!force && wlr_box_contains_point(&box, cursor->wlr_cursor->x,
                                         cursor->wlr_cursor->y)) {
        return;
    }

    double x = container->pending.x + container->pending.width / 2.0;
    double y = container->pending.y + container->pending.height / 2.0;

    wlr_cursor_warp(cursor->wlr_cursor, NULL, x, y);
    cursor_unhide(cursor);
}

void cursor_warp_to_workspace(struct wsm_cursor *cursor,
                              struct wsm_workspace *workspace) {
    if (!workspace) {
        return;
    }

    double x = workspace->x + workspace->width / 2.0;
    double y = workspace->y + workspace->height / 2.0;

    wlr_cursor_warp(cursor->wlr_cursor, NULL, x, y);
    cursor_unhide(cursor);
}

static void check_constraint_region(struct wsm_cursor *cursor) {
    struct wlr_pointer_constraint_v1 *constraint = cursor->active_constraint;
    pixman_region32_t *region = &constraint->region;
    struct wsm_view *view = view_from_wlr_surface(constraint->surface);
    if (cursor->active_confine_requires_warp && view) {
        cursor->active_confine_requires_warp = false;

        struct wsm_container *con = view->container;

        double sx = cursor->wlr_cursor->x - con->pending.content_x + view->geometry.x;
        double sy = cursor->wlr_cursor->y - con->pending.content_y + view->geometry.y;

        if (!pixman_region32_contains_point(region,
                                            floor(sx), floor(sy), NULL)) {
            int nboxes;
            pixman_box32_t *boxes = pixman_region32_rectangles(region, &nboxes);
            if (nboxes > 0) {
                double sx = (boxes[0].x1 + boxes[0].x2) / 2.;
                double sy = (boxes[0].y1 + boxes[0].y2) / 2.;

                wlr_cursor_warp_closest(cursor->wlr_cursor, NULL,
                                        sx + con->pending.content_x - view->geometry.x,
                                        sy + con->pending.content_y - view->geometry.y);

                cursor_rebase(cursor);
            }
        }
    }

    if (constraint->type == WLR_POINTER_CONSTRAINT_V1_CONFINED) {
        pixman_region32_copy(&cursor->confine, region);
    } else {
        pixman_region32_clear(&cursor->confine);
    }
}

static void handle_constraint_commit(struct wl_listener *listener,
                                     void *data) {
    struct wsm_cursor *cursor =
        wl_container_of(listener, cursor, constraint_commit);
    struct wlr_pointer_constraint_v1 *constraint = cursor->active_constraint;
    assert(constraint->surface == data);

    check_constraint_region(cursor);
}

void wsm_cursor_constrain(struct wsm_cursor *cursor,
                           struct wlr_pointer_constraint_v1 *constraint) {
    if (cursor->active_constraint == constraint) {
        return;
    }

    wl_list_remove(&cursor->constraint_commit.link);
    if (cursor->active_constraint) {
        if (constraint == NULL) {
            warp_to_constraint_cursor_hint(cursor);
        }
        wlr_pointer_constraint_v1_send_deactivated(
            cursor->active_constraint);
    }

    cursor->active_constraint = constraint;

    if (constraint == NULL) {
        wl_list_init(&cursor->constraint_commit.link);
        return;
    }

    cursor->active_confine_requires_warp = true;
    if (pixman_region32_not_empty(&constraint->current.region)) {
        pixman_region32_intersect(&constraint->region,
                                  &constraint->surface->input_region, &constraint->current.region);
    } else {
        pixman_region32_copy(&constraint->region,
                             &constraint->surface->input_region);
    }

    check_constraint_region(cursor);

    wlr_pointer_constraint_v1_send_activated(constraint);

    cursor->constraint_commit.notify = handle_constraint_commit;
    wl_signal_add(&constraint->surface->events.commit,
                  &cursor->constraint_commit);
}

void warp_to_constraint_cursor_hint(struct wsm_cursor *cursor) {
    struct wlr_pointer_constraint_v1 *constraint = cursor->active_constraint;

    double sx = constraint->current.cursor_hint.x;
    double sy = constraint->current.cursor_hint.y;

    struct wsm_view *view = view_from_wlr_surface(constraint->surface);
    if (!view) {
        return;
    }

    struct wsm_container *con = view->container;

    double lx = sx + con->pending.content_x - view->geometry.x;
    double ly = sy + con->pending.content_y - view->geometry.y;

    wlr_cursor_warp(cursor->wlr_cursor, NULL, lx, ly);

    wlr_seat_pointer_warp(constraint->seat, sx, sy);
}

struct wsm_node *node_at_coords(
    struct wsm_seat *seat, double lx, double ly,
    struct wlr_surface **surface, double *sx, double *sy) {
    struct wlr_scene_node *scene_node = NULL;

    struct wlr_scene_node *node;
    wl_list_for_each_reverse(node, &global_server.wsm_scene->layer_tree->children, link) {
        struct wlr_scene_tree *layer = wlr_scene_tree_from_node(node);

        bool non_interactive = wsm_scene_descriptor_try_get(&layer->node,
                                                        WSM_SCENE_DESC_NON_INTERACTIVE);
        if (non_interactive) {
            continue;
        }

        scene_node = wlr_scene_node_at(&layer->node, lx, ly, sx, sy);
        if (scene_node) {
            break;
        }
    }

    if (scene_node) {
        // determine what wlr_surface we clicked on
        if (scene_node->type == WLR_SCENE_NODE_BUFFER) {
            struct wlr_scene_buffer *scene_buffer =
                wlr_scene_buffer_from_node(scene_node);
            struct wlr_scene_surface *scene_surface =
                wlr_scene_surface_try_from_buffer(scene_buffer);

            if (scene_surface) {
                *surface = scene_surface->surface;
            }
        }

        // determine what container we clicked on
        struct wlr_scene_node *current = scene_node;
        while (true) {
            struct wsm_container *con = wsm_scene_descriptor_try_get(current,
                                                                  WSM_SCENE_DESC_CONTAINER);

            if (!con) {
                struct wsm_view *view = wsm_scene_descriptor_try_get(current,
                                                                  WSM_SCENE_DESC_VIEW);
                if (view) {
                    con = view->container;
                }
            }

            if (!con) {
                struct wsm_popup_desc *popup =
                    wsm_scene_descriptor_try_get(current, WSM_SCENE_DESC_POPUP);
                if (popup && popup->view) {
                    con = popup->view->container;
                }
            }

            if (con && (!con->view || con->view->surface)) {
                return &con->node;
            }

            if (wsm_scene_descriptor_try_get(current, WSM_SCENE_DESC_LAYER_SHELL)) {
                // We don't want to feed through the current workspace on
                // layer shells
                return NULL;
            }

#if HAVE_XWAYLAND
            if (wsm_scene_descriptor_try_get(current, WSM_SCENE_DESC_XWAYLAND_UNMANAGED)) {
                return NULL;
            }
#endif

            if (!current->parent) {
                break;
            }

            current = &current->parent->node;
        }
    }

    // if we aren't on a container, determine what workspace we are on
    struct wlr_output *wlr_output = wlr_output_layout_output_at(
        global_server.wsm_scene->output_layout, lx, ly);
    if (wlr_output == NULL) {
        return NULL;
    }

    struct wsm_output *output = wlr_output->data;
    if (!output || !output->enabled) {
        // output is being destroyed or is being enabled
        return NULL;
    }

    struct wsm_workspace *ws = output_get_active_workspace(output);
    if (!ws) {
        return NULL;
    }

    return &ws->node;
}

void dispatch_cursor_button(struct wsm_cursor *cursor,
                            struct wlr_input_device *device, uint32_t time_msec, uint32_t button,
                            enum wl_pointer_button_state state) {
    if (time_msec == 0) {
        time_msec = get_current_time_msec();
    }

    seatop_button(cursor->wsm_seat, time_msec, device, button, state);
}

void cursor_notify_key_press(struct wsm_cursor *cursor) {
    // if (cursor->hidden) {
    //     return;
    // }

    // if (cursor->hide_when_typing == HIDE_WHEN_TYPING_DEFAULT) {
    //     // No cached value, need to lookup in the seat_config
    //     const struct seat_config *seat_config = seat_get_config(cursor->seat);
    //     if (!seat_config) {
    //         seat_config = seat_get_config_by_name("*");
    //         if (!seat_config) {
    //             return;
    //         }
    //     }
    //     cursor->hide_when_typing = seat_config->hide_cursor_when_typing;
    //     // The default is currently disabled
    //     if (cursor->hide_when_typing == HIDE_WHEN_TYPING_DEFAULT) {
    //         cursor->hide_when_typing = HIDE_WHEN_TYPING_DISABLE;
    //     }
    // }

    // if (cursor->hide_when_typing == HIDE_WHEN_TYPING_ENABLE) {
    //     cursor_hide(cursor);
    // }
}
