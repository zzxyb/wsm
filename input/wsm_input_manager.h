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

#ifndef WSM_INPUT_MANAGER_H
#define WSM_INPUT_MANAGER_H

#include <stdbool.h>

#include <libinput.h>

#include <wlr/types/wlr_keyboard_shortcuts_inhibit_v1.h>
#include <wlr/types/wlr_virtual_keyboard_v1.h>
#include <wlr/types/wlr_virtual_pointer_v1.h>

struct wlr_seat;
struct wlr_pointer_gestures_v1;

struct wsm_seat;
struct wsm_node;
struct wsm_server;

struct wsm_input_manager {
    struct wl_list devices;
    struct wl_list seats;

    // struct wlr_input_inhibit_manager *inhibit;
    struct wlr_keyboard_shortcuts_inhibit_manager_v1 *keyboard_shortcuts_inhibit;
    struct wlr_virtual_keyboard_manager_v1 *virtual_keyboard;
    struct wlr_virtual_pointer_manager_v1 *virtual_pointer;
    struct wlr_pointer_gestures_v1 *pointer_gestures;

    struct wl_listener new_input;
    struct wl_listener inhibit_activate;
    struct wl_listener inhibit_deactivate;
    struct wl_listener keyboard_shortcuts_inhibit_new_inhibitor;
    struct wl_listener virtual_keyboard_new;
    struct wl_listener virtual_pointer_new;
};

struct wsm_input_manager *wsm_input_manager_create(const struct wsm_server* server);
struct wsm_seat *input_manager_get_default_seat();
struct wsm_seat *input_manager_current_seat(void);
struct wsm_seat *input_manager_get_seat(const char *seat_name, bool create);
struct wsm_seat *input_manager_seat_from_wlr_seat(struct wlr_seat *wlr_seat);
char *input_device_get_identifier(struct wlr_input_device *device);
void input_manager_configure_xcursor(void);
void input_manager_set_focus(struct wsm_node *node);
void input_manager_configure_all_input_mappings(void);

#endif
