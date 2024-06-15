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
#include "wsm_seat.h"
#include "wsm_server.h"
#include "wsm_keyboard.h"
#include "wsm_text_input.h"

#include <stdlib.h>

#include <xkbcommon/xkbcommon.h>
#include <xkbcommon/xkbcommon-names.h>

#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_keyboard.h>
#include <wlr/interfaces/wlr_keyboard.h>
#include <wlr/types/wlr_keyboard.h>
#include <wlr/types/wlr_keyboard_group.h>
#include <wlr/types/wlr_input_method_v2.h>
#include <wlr/types/wlr_virtual_keyboard_v1.h>

static int handle_keyboard_repeat(void *data) {
    // struct wsm_keyboard *keyboard = data;
    // execute
    return 0;
}

struct wsm_keyboard *wsm_keyboard_create(struct wsm_seat *seat,  struct wsm_seat_device *device) {
    struct wsm_keyboard *keyboard =
        calloc(1, sizeof(struct wsm_keyboard));
    if (!wsm_assert(keyboard, "Could not create wsm_keyboard for seat: allocation failed!")) {
        return NULL;
    }

    keyboard->seat_device = device;
    keyboard->wlr = wlr_keyboard_from_input_device(device->input_device->wlr_device);
    device->keyboard = keyboard;

    wl_list_init(&keyboard->keyboard_key.link);
    wl_list_init(&keyboard->keyboard_modifiers.link);

    keyboard->key_repeat_source = wl_event_loop_add_timer(global_server.wl_event_loop,
                                                          handle_keyboard_repeat, keyboard);

    return keyboard;
}

static void handle_key_event(struct wsm_keyboard *keyboard,
                             struct wlr_keyboard_key_event *event) {

}

static void handle_keyboard_key(struct wl_listener *listener, void *data) {
    struct wsm_keyboard *keyboard =
        wl_container_of(listener, keyboard, keyboard_key);
    handle_key_event(keyboard, data);
}

static struct wlr_input_method_keyboard_grab_v2 *keyboard_get_im_grab(
    struct wsm_keyboard *keyboard) {
    struct wlr_input_method_v2 *input_method = keyboard->seat_device->
                                               wsm_seat->im_relay->input_method;
    struct wlr_virtual_keyboard_v1 *virtual_keyboard =
        wlr_input_device_get_virtual_keyboard(keyboard->seat_device->input_device->wlr_device);
    if (!input_method || !input_method->keyboard_grab || (virtual_keyboard &&
                                                          wl_resource_get_client(virtual_keyboard->resource) ==
                                                              wl_resource_get_client(input_method->keyboard_grab->resource))) {
        return NULL;
    }
    return input_method->keyboard_grab;
}

static void handle_modifier_event(struct wsm_keyboard *keyboard) {
    if (!keyboard->wlr->group) {
        struct wlr_input_method_keyboard_grab_v2 *kb_grab = keyboard_get_im_grab(keyboard);

        if (kb_grab) {
            wlr_input_method_keyboard_grab_v2_set_keyboard(kb_grab, keyboard->wlr);
            wlr_input_method_keyboard_grab_v2_send_modifiers(kb_grab,
                                                             &keyboard->wlr->modifiers);
        } else {
            struct wlr_seat *wlr_seat = keyboard->seat_device->wsm_seat->wlr_seat;
            wlr_seat_set_keyboard(wlr_seat, keyboard->wlr);
            wlr_seat_keyboard_notify_modifiers(wlr_seat,
                                               &keyboard->wlr->modifiers);
        }
    }

    if (keyboard->wlr->modifiers.group != keyboard->effective_layout) {
        keyboard->effective_layout = keyboard->wlr->modifiers.group;
    }
}

static void handle_keyboard_modifiers(struct wl_listener *listener,
                                      void *data) {
    struct wsm_keyboard *keyboard =
        wl_container_of(listener, keyboard, keyboard_modifiers);
    handle_modifier_event(keyboard);
}

void wsm_keyboard_configure(struct wsm_keyboard *keyboard) {
    // If the seat has no active keyboard, set this one
    struct wlr_seat *seat = keyboard->seat_device->wsm_seat->wlr_seat;
    struct wlr_keyboard *current_keyboard = seat->keyboard_state.keyboard;
    if (current_keyboard == NULL) {
        wlr_seat_set_keyboard(seat, keyboard->wlr);
    }

    wl_list_remove(&keyboard->keyboard_key.link);
    keyboard->keyboard_key.notify = handle_keyboard_key;
    wl_signal_add(&keyboard->wlr->events.key, &keyboard->keyboard_key);

    wl_list_remove(&keyboard->keyboard_modifiers.link);
    keyboard->keyboard_modifiers.notify = handle_keyboard_modifiers;
    wl_signal_add(&keyboard->wlr->events.modifiers,
                  &keyboard->keyboard_modifiers);
}

void wsm_keyboard_destroy(struct wsm_keyboard *keyboard) {
    if (!keyboard) {
        wsm_log(WSM_ERROR, "wsm_keyboard is NULL!");
        return;
    }
    // if (keyboard->wlr->group) {
    //     wsm_keyboard_group_remove(keyboard);
    // }
    struct wlr_seat *wlr_seat = keyboard->seat_device->wsm_seat->wlr_seat;
    if (wlr_seat_get_keyboard(wlr_seat) == keyboard->wlr) {
        wlr_seat_set_keyboard(wlr_seat, NULL);
    }
    if (keyboard->keymap) {
        xkb_keymap_unref(keyboard->keymap);
    }
    wl_list_remove(&keyboard->keyboard_key.link);
    wl_list_remove(&keyboard->keyboard_modifiers.link);
    wsm_keyboard_disarm_key_repeat(keyboard);
    wl_event_source_remove(keyboard->key_repeat_source);
    free(keyboard);
}

void wsm_keyboard_disarm_key_repeat(struct wsm_keyboard *keyboard) {
    if (!keyboard) {
        wsm_log(WSM_ERROR, "wsm_keyboard is NULL in keyboard_disarm_key_repeat!");
        return;
    }
    if (wl_event_source_timer_update(keyboard->key_repeat_source, 0) < 0) {
        wsm_log(WSM_DEBUG, "failed to disarm key repeat timer");
    }
}

struct wsm_keyboard *wsm_keyboard_for_wlr_keyboard(
    struct wsm_seat *seat, struct wlr_keyboard *wlr_keyboard) {
    struct wsm_seat_device *seat_device;
    wl_list_for_each(seat_device, &seat->devices, link) {
        struct wsm_input_device *input_device = seat_device->input_device;
        if (input_device->wlr_device->type != WLR_INPUT_DEVICE_KEYBOARD) {
            continue;
        }
        if (input_device->wlr_device == &wlr_keyboard->base) {
            return seat_device->keyboard;
        }
    }
    struct wsm_keyboard_group *group;
    wl_list_for_each(group, &seat->keyboard_groups, link) {
        struct wsm_input_device *input_device =
            group->seat_device->input_device;
        if (input_device->wlr_device == &wlr_keyboard->base) {
            return group->seat_device->keyboard;
        }
    }
    return NULL;
}
