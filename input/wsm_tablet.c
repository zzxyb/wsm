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

#include "wsm_log.h"
#include "wsm_seat.h"
#include "wsm_server.h"
#include "wsm_cursor.h"
#include "wsm_tablet.h"

#include <stdlib.h>

#include <wlr/config.h>
#include <wlr/backend/libinput.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_tablet_v2.h>
#include <wlr/types/wlr_tablet_tool.h>
#include <wlr/types/wlr_tablet_pad.h>

static void handle_pad_tablet_destroy(struct wl_listener *listener, void *data) {
	struct wsm_tablet_pad *pad =
			wl_container_of(listener, pad, tablet_destroy);
	
	pad->tablet = NULL;
	
	wl_list_remove(&pad->tablet_destroy.link);
	wl_list_init(&pad->tablet_destroy.link);
}

static void attach_tablet_pad(struct wsm_tablet_pad *tablet_pad,
							  struct wsm_tablet *tablet) {
	wsm_log(WSM_DEBUG, "Attaching tablet pad \"%s\" to tablet tool \"%s\"",
			tablet_pad->seat_device->input_device->wlr_device->name,
			tablet->seat_device->input_device->wlr_device->name);
	
	tablet_pad->tablet = tablet;
	
	wl_list_remove(&tablet_pad->tablet_destroy.link);
	tablet_pad->tablet_destroy.notify = handle_pad_tablet_destroy;
	wl_signal_add(&tablet->seat_device->input_device->wlr_device->events.destroy,
				  &tablet_pad->tablet_destroy);
}

struct wsm_tablet *wsm_tablet_create(struct wsm_seat *seat,
									 struct wsm_seat_device *device) {
	struct wsm_tablet *tablet =
			calloc(1, sizeof(struct wsm_tablet));
	if (!wsm_assert(tablet, "Could not create wsm_tablet for seat: allocation failed!")) {
		return NULL;
	}
	
	tablet->tablet_manager_v2 = wlr_tablet_v2_create(global_server.wl_display);
	wl_list_insert(&seat->wsm_cursor->tablets, &tablet->link);
	
	device->tablet = tablet;
	tablet->seat_device = device;
	
	return tablet;
}

void wsm_configure_tablet(struct wsm_tablet *tablet) {
	struct wlr_input_device *device =
			tablet->seat_device->input_device->wlr_device;
	struct wsm_seat *seat = tablet->seat_device->wsm_seat;
	
		   // seat_configure_xcursor(seat);
	
	if (!tablet->tablet_v2) {
		tablet->tablet_v2 =
				wlr_tablet_create(tablet->tablet_manager_v2, seat->wlr_seat, device);
	}

#if WLR_HAS_LIBINPUT_BACKEND
	if (!wlr_input_device_is_libinput(device)) {
		return;
	}

//     struct libinput_device_group *group =
//         libinput_device_get_device_group(wlr_libinput_get_device_handle(device));
//     struct wsm_tablet_pad *tablet_pad;
//     wl_list_for_each(tablet_pad, &seat->cursor->tablet_pads, link) {
//         struct wlr_input_device *pad_device =
//             tablet_pad->seat_device->input_device->wlr_device;
//         if (!wlr_input_device_is_libinput(pad_device)) {
//             continue;
//         }

//         struct libinput_device_group *pad_group =
//             libinput_device_get_device_group(wlr_libinput_get_device_handle(pad_device));

//         if (pad_group == group) {
//             attach_tablet_pad(tablet_pad, tablet);
//             break;
//         }
//     }
#endif
}

void wsm_tablet_destroy(struct wsm_tablet *tablet) {
	if (!tablet) {
		wsm_log(WSM_ERROR, "wsm_tablet is NULL!");
		return;
	}
	wl_list_remove(&tablet->link);
	free(tablet);
}

static void handle_tablet_tool_set_cursor(struct wl_listener *listener, void *data) {
	struct wsm_tablet_tool *tool =
			wl_container_of(listener, tool, set_cursor);
	struct wlr_tablet_v2_event_cursor *event = data;
	
	struct wsm_cursor *cursor = tool->seat->wsm_cursor;
	if (!seatop_allows_set_cursor(cursor->wsm_seat)) {
		return;
	}
	
	struct wl_client *focused_client = NULL;
	struct wlr_surface *focused_surface = tool->tablet_v2_tool->focused_surface;
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

static void handle_tablet_tool_destroy(struct wl_listener *listener, void *data) {
	struct wsm_tablet_tool *tool =
			wl_container_of(listener, tool, tool_destroy);
	
	wl_list_remove(&tool->tool_destroy.link);
	wl_list_remove(&tool->set_cursor.link);
	
	free(tool);
}

void wsm_tablet_tool_configure(struct wsm_tablet *tablet,
							   struct wlr_tablet_tool *wlr_tool) {
	struct wsm_tablet_tool *tool =
			calloc(1, sizeof(struct wsm_tablet_tool));
	if (!wsm_assert(tool, "Could not create wsm tablet tool for tablet: allocation failed!")) {
		return;
	}
	
	switch (wlr_tool->type) {
	case WLR_TABLET_TOOL_TYPE_LENS:
	case WLR_TABLET_TOOL_TYPE_MOUSE:
		tool->mode = WSM_TABLET_TOOL_MODE_RELATIVE;
		break;
	default:
		tool->mode = WSM_TABLET_TOOL_MODE_ABSOLUTE;
	}
	
	tool->seat = tablet->seat_device->wsm_seat;
	tool->tablet = tablet;
	tool->tablet_v2_tool =
			wlr_tablet_tool_create(tablet->tablet_manager_v2,
								   tablet->seat_device->wsm_seat->wlr_seat, wlr_tool);
	
	tool->tool_destroy.notify = handle_tablet_tool_destroy;
	wl_signal_add(&wlr_tool->events.destroy, &tool->tool_destroy);
	
	tool->set_cursor.notify = handle_tablet_tool_set_cursor;
	wl_signal_add(&tool->tablet_v2_tool->events.set_cursor,
				  &tool->set_cursor);
	
	wlr_tool->data = tool;
}

static void handle_tablet_pad_attach(struct wl_listener *listener,
									 void *data) {
	struct wsm_tablet_pad *pad = wl_container_of(listener, pad, attach);
	struct wlr_tablet_tool *wlr_tool = data;
	struct wsm_tablet_tool *tool = wlr_tool->data;
	
	if (!tool) {
		return;
	}
	
	attach_tablet_pad(pad, tool->tablet);
}

static void handle_tablet_pad_ring(struct wl_listener *listener, void *data) {
	struct wsm_tablet_pad *pad = wl_container_of(listener, pad, ring);
	struct wlr_tablet_pad_ring_event *event = data;
	
	if (!pad->current_surface) {
		return;
	}
	
	wlr_tablet_v2_tablet_pad_notify_ring(pad->tablet_v2_pad,
										 event->ring, event->position,
										 event->source == WLR_TABLET_PAD_RING_SOURCE_FINGER,
										 event->time_msec);
}

static void handle_tablet_pad_strip(struct wl_listener *listener, void *data) {
	struct wsm_tablet_pad *pad = wl_container_of(listener, pad, strip);
	struct wlr_tablet_pad_strip_event *event = data;
	
	if (!pad->current_surface) {
		return;
	}
	
	wlr_tablet_v2_tablet_pad_notify_strip(pad->tablet_v2_pad,
										  event->strip, event->position,
										  event->source == WLR_TABLET_PAD_STRIP_SOURCE_FINGER,
										  event->time_msec);
}

static void handle_tablet_pad_button(struct wl_listener *listener, void *data) {
	struct wsm_tablet_pad *pad = wl_container_of(listener, pad, button);
	struct wlr_tablet_pad_button_event *event = data;
	
	if (!pad->current_surface) {
		return;
	}
	
	wlr_tablet_v2_tablet_pad_notify_mode(pad->tablet_v2_pad,
										 event->group, event->mode, event->time_msec);
	
	wlr_tablet_v2_tablet_pad_notify_button(pad->tablet_v2_pad,
										   event->button, event->time_msec,
										   (enum zwp_tablet_pad_v2_button_state)event->state);
}

struct wsm_tablet_pad *wsm_tablet_pad_create(struct wsm_seat *seat,
											 struct wsm_seat_device *device) {
	struct wsm_tablet_pad *tablet_pad =
			calloc(1, sizeof(struct wsm_tablet_pad));
	if (!wsm_assert(tablet_pad, "Could not create wsm tablet: allocation failed!")) {
		return NULL;
	}
	
	tablet_pad->wlr = wlr_tablet_pad_from_input_device(device->input_device->wlr_device);
	tablet_pad->seat_device = device;
	wl_list_init(&tablet_pad->attach.link);
	wl_list_init(&tablet_pad->button.link);
	wl_list_init(&tablet_pad->strip.link);
	wl_list_init(&tablet_pad->ring.link);
	wl_list_init(&tablet_pad->surface_destroy.link);
	wl_list_init(&tablet_pad->tablet_destroy.link);
	
	wl_list_insert(&seat->wsm_cursor->tablet_pads, &tablet_pad->link);
	
	return tablet_pad;
}

void wsm_configure_tablet_pad(struct wsm_tablet_pad *tablet_pad) {
	struct wlr_input_device *wlr_device =
			tablet_pad->seat_device->input_device->wlr_device;
	struct wsm_seat *seat = tablet_pad->seat_device->wsm_seat;
	
	if (!tablet_pad->tablet_v2_pad) {
		tablet_pad->tablet_v2_pad =
				wlr_tablet_pad_create(tablet_pad->tablet->tablet_manager_v2, seat->wlr_seat, wlr_device);
	}
	
	wl_list_remove(&tablet_pad->attach.link);
	tablet_pad->attach.notify = handle_tablet_pad_attach;
	wl_signal_add(&tablet_pad->wlr->events.attach_tablet,
				  &tablet_pad->attach);
	
	wl_list_remove(&tablet_pad->button.link);
	tablet_pad->button.notify = handle_tablet_pad_button;
	wl_signal_add(&tablet_pad->wlr->events.button, &tablet_pad->button);
	
	wl_list_remove(&tablet_pad->strip.link);
	tablet_pad->strip.notify = handle_tablet_pad_strip;
	wl_signal_add(&tablet_pad->wlr->events.strip, &tablet_pad->strip);
	
	wl_list_remove(&tablet_pad->ring.link);
	tablet_pad->ring.notify = handle_tablet_pad_ring;
	wl_signal_add(&tablet_pad->wlr->events.ring, &tablet_pad->ring);

#if WLR_HAS_LIBINPUT_BACKEND
	/* Search for a sibling tablet */
	if (!wlr_input_device_is_libinput(wlr_device)) {
		/* We can only do this on libinput devices */
		return;
	}
	
	struct libinput_device_group *group =
			libinput_device_get_device_group(wlr_libinput_get_device_handle(wlr_device));
	struct wsm_tablet *tool;
	wl_list_for_each(tool, &seat->wsm_cursor->tablets, link) {
		struct wlr_input_device *tablet =
				tool->seat_device->input_device->wlr_device;
		if (!wlr_input_device_is_libinput(tablet)) {
			continue;
		}
		
		struct libinput_device_group *tablet_group =
				libinput_device_get_device_group(wlr_libinput_get_device_handle(tablet));
		
		if (tablet_group == group) {
			attach_tablet_pad(tablet_pad, tool);
			break;
		}
	}
#endif
}

void wsm_tablet_pad_destroy(struct wsm_tablet_pad *tablet_pad) {
	if (!tablet_pad) {
		return;
	}
	
	wl_list_remove(&tablet_pad->link);
	wl_list_remove(&tablet_pad->attach.link);
	wl_list_remove(&tablet_pad->button.link);
	wl_list_remove(&tablet_pad->strip.link);
	wl_list_remove(&tablet_pad->ring.link);
	wl_list_remove(&tablet_pad->surface_destroy.link);
	wl_list_remove(&tablet_pad->tablet_destroy.link);
	
	free(tablet_pad);
}

static void handle_pad_tablet_surface_destroy(struct wl_listener *listener,
											  void *data) {
	struct wsm_tablet_pad *tablet_pad =
			wl_container_of(listener, tablet_pad, surface_destroy);
	
	wsm_tablet_pad_set_focus(tablet_pad, NULL);
}

void wsm_tablet_pad_set_focus(struct wsm_tablet_pad *tablet_pad,
							  struct wlr_surface *surface) {
	if (!tablet_pad || !tablet_pad->tablet) {
		return;
	}
	
	if (surface == tablet_pad->current_surface) {
		return;
	}
	
	/* Leave current surface */
	if (tablet_pad->current_surface) {
		wlr_tablet_v2_tablet_pad_notify_leave(tablet_pad->tablet_v2_pad,
											  tablet_pad->current_surface);
		wl_list_remove(&tablet_pad->surface_destroy.link);
		wl_list_init(&tablet_pad->surface_destroy.link);
		tablet_pad->current_surface = NULL;
	}
	
	if (surface == NULL ||
		!wlr_surface_accepts_tablet_v2(tablet_pad->tablet->tablet_v2, surface)) {
		return;
	}
	
	wlr_tablet_v2_tablet_pad_notify_enter(tablet_pad->tablet_v2_pad,
										  tablet_pad->tablet->tablet_v2, surface);
	
	tablet_pad->current_surface = surface;
	tablet_pad->surface_destroy.notify = handle_pad_tablet_surface_destroy;
	wl_signal_add(&surface->events.destroy, &tablet_pad->surface_destroy);
}
