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
#include "wsm_log.h"
#include "wsm_server.h"
#include "wsm_seat.h"
#include "wsm_common.h"
#include "wsm_input_manager.h"

#include <ctype.h>
#include <string.h>

#include <wlr/config.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_virtual_keyboard_v1.h>
#include <wlr/types/wlr_virtual_pointer_v1.h>
#include <wlr/types/wlr_input_inhibitor.h>

#define DEFAULT_SEAT "seat0"

static void handle_device_destroy(struct wl_listener *listener, void *data) {
    struct wlr_input_device *device = data;
    wsm_input_device_destroy(device);
}

static void handle_new_input(struct wl_listener *listener, void *data) {
    struct wsm_input_manager *input_manager =
        wl_container_of(listener, input_manager, new_input);
    struct wlr_input_device *device = data;

    struct wsm_input_device *input_device = wsm_input_device_create();
    device->data = input_device;

    input_device->wlr_device = device;
    input_device->identifier = input_device_get_identifier(device);
    wl_list_insert(&input_manager->devices, &input_device->link);

    struct wsm_seat *seat = NULL;
    wl_list_for_each(seat, &input_manager->seats, link) {
            seat_add_device(seat, input_device);
    }

    wsm_log(WSM_DEBUG, "adding device: '%s'",
            input_device->identifier);

    input_device->device_destroy.notify = handle_device_destroy;
    wl_signal_add(&device->events.destroy, &input_device->device_destroy);
}

static void handle_new_virtual_keyboard(struct wl_listener *listener, void *data) {

}

static void handle_new_virtual_pointer(struct wl_listener *listener, void *data) {

}

static void handle_inhibit_activate(struct wl_listener *listener, void *data) {

}

static void handle_inhibit_deactivate(struct wl_listener *listener, void *data) {

}

static void handle_keyboard_shortcuts_inhibit_new_inhibitor(
    struct wl_listener *listener, void *data) {

}


struct wsm_input_manager *wsm_input_manager_create(const struct wsm_server* server) {
    struct wsm_input_manager *input_manager = calloc(1, sizeof(struct wsm_input_manager));
    if (!wsm_assert(input_manager, "Could not create wsm_input_manager: allocation failed!")) {
        return NULL;
    }

    wl_list_init(&input_manager->devices);
    wl_list_init(&input_manager->seats);

    input_manager->new_input.notify = handle_new_input;
    wl_signal_add(&server->backend->events.new_input, &input_manager->new_input);

    input_manager->virtual_keyboard =
        wlr_virtual_keyboard_manager_v1_create(server->wl_display);
    input_manager->virtual_keyboard_new.notify = handle_new_virtual_keyboard;
    wl_signal_add(&input_manager->virtual_keyboard->events.new_virtual_keyboard,
                  &input_manager->virtual_keyboard_new);

    input_manager->virtual_pointer =
        wlr_virtual_pointer_manager_v1_create(server->wl_display);
    input_manager->virtual_pointer_new.notify = handle_new_virtual_pointer;
    wl_signal_add(&input_manager->virtual_pointer->events.new_virtual_pointer,
                  &input_manager->virtual_pointer_new);

    input_manager->inhibit =
        wlr_input_inhibit_manager_create(server->wl_display);
    input_manager->inhibit_activate.notify = handle_inhibit_activate;
    wl_signal_add(&input_manager->inhibit->events.activate,
                  &input_manager->inhibit_activate);
    input_manager->inhibit_deactivate.notify = handle_inhibit_deactivate;
    wl_signal_add(&input_manager->inhibit->events.deactivate,
                  &input_manager->inhibit_deactivate);

    input_manager->keyboard_shortcuts_inhibit =
        wlr_keyboard_shortcuts_inhibit_v1_create(server->wl_display);
    input_manager->keyboard_shortcuts_inhibit_new_inhibitor.notify =
        handle_keyboard_shortcuts_inhibit_new_inhibitor;
    wl_signal_add(&input_manager->keyboard_shortcuts_inhibit->events.new_inhibitor,
                  &input_manager->keyboard_shortcuts_inhibit_new_inhibitor);

    return input_manager;
}

struct wsm_seat *input_manager_get_default_seat() {
    return input_manager_get_seat(DEFAULT_SEAT, true);
}

struct wsm_seat *input_manager_get_seat(const char *seat_name, bool create) {
    struct wsm_seat *seat = NULL;
    wl_list_for_each(seat, &global_server.wsm_input_manager->seats, link) {
        if (strcmp(seat->wlr_seat->name, seat_name) == 0) {
            return seat;
        }
    }

    return create ? seat_create(seat_name) : NULL;
}

struct wsm_seat *input_manager_seat_from_wlr_seat(struct wlr_seat *wlr_seat) {
    struct wsm_seat *seat = NULL;

    wl_list_for_each(seat, &global_server.wsm_input_manager->seats, link) {
        if (seat->wlr_seat == wlr_seat) {
            return seat;
        }
    }

    return NULL;
}

char *input_device_get_identifier(struct wlr_input_device *device) {
    int vendor = device->vendor;
    int product = device->product;
    char *name = strdup(device->name ? device->name : "");
    strip_whitespace(name);

    char *p = name;
    for (; *p; ++p) {
        // There are in fact input devices with unprintable characters in its name
        if (*p == ' ' || !isprint(*p)) {
            *p = '_';
        }
    }

    char *identifier = format_str("%d:%d:%s", vendor, product, name);
    free(name);
    return identifier;
}
