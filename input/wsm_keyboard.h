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

#ifndef WSM_KEYBOARD_H
#define WSM_KEYBOARD_H

#include <wayland-server-core.h>

#include <xkbcommon/xkbcommon.h>

#define WSM_KEYBOARD_PRESSED_KEYS_CAP 32

struct wlr_keyboard;
struct wlr_keyboard_group;
struct wlr_keyboard_shortcuts_inhibitor_v1;

struct xkb_keymap;

struct wsm_seat;
struct wsm_list;
struct wsm_seat_device;

enum binding_input_type {
    BINDING_KEYCODE,
    BINDING_KEYSYM,
    BINDING_MOUSECODE,
    BINDING_MOUSESYM,
    BINDING_SWITCH, // dummy, only used to call seat_execute_command
    BINDING_GESTURE // dummy, only used to call seat_execute_command
};

enum binding_flags {
    BINDING_RELEASE = 1 << 0,
    BINDING_LOCKED = 1 << 1, // keyboard only
    BINDING_BORDER = 1 << 2, // mouse only; trigger on container border
    BINDING_CONTENTS = 1 << 3, // mouse only; trigger on container contents
    BINDING_TITLEBAR = 1 << 4, // mouse only; trigger on container titlebar
    BINDING_CODE = 1 << 5, // keyboard only; convert keysyms into keycodes
    BINDING_RELOAD = 1 << 6, // switch only; (re)trigger binding on reload
    BINDING_INHIBITED = 1 << 7, // keyboard only: ignore shortcut inhibitor
    BINDING_NOREPEAT = 1 << 8, // keyboard only; do not trigger when repeating a held key
    BINDING_EXACT = 1 << 9, // gesture only; only trigger on exact match
};

struct wsm_binding {
    enum binding_input_type type;
    int order;
    char *input;
    uint32_t flags;
    struct wsm_list *keys; // sorted in ascending order
    struct wsm_list *syms; // sorted in ascending order; NULL if BINDING_CODE is not set
    uint32_t modifiers;
    xkb_layout_index_t group;
    char *command;
};

struct wsm_shortcut_state {
    uint32_t pressed_keys[WSM_KEYBOARD_PRESSED_KEYS_CAP];
    uint32_t pressed_keycodes[WSM_KEYBOARD_PRESSED_KEYS_CAP];
    uint32_t last_keycode;
    uint32_t last_raw_modifiers;
    size_t npressed;
    uint32_t current_key;
};

struct wsm_keyboard {
    struct wsm_seat_device *seat_device;
    struct wlr_keyboard *wlr;

    struct xkb_keymap *keymap;
    xkb_layout_index_t effective_layout;

    int32_t repeat_rate;
    int32_t repeat_delay;

    struct wl_listener keyboard_key;
    struct wl_listener keyboard_modifiers;

    struct wsm_shortcut_state state_keysyms_translated;
    struct wsm_shortcut_state state_keysyms_raw;
    struct wsm_shortcut_state state_keycodes;
    struct wsm_shortcut_state state_pressed_sent;
    struct wsm_binding *held_binding;

    struct wl_event_source *key_repeat_source;
    struct wsm_binding *repeat_binding;
};

struct wsm_keyboard_group {
    struct wlr_keyboard_group *wlr_group;
    struct wsm_seat_device *seat_device;
    struct wl_listener keyboard_key;
    struct wl_listener keyboard_modifiers;
    struct wl_listener enter;
    struct wl_listener leave;
    struct wl_list link;
};

struct wsm_keyboard_shortcuts_inhibitor {
    struct wlr_keyboard_shortcuts_inhibitor_v1 *inhibitor;

    struct wl_listener destroy;

    struct wl_list link; // wsm_seat::keyboard_shortcuts_inhibitors
};

struct wsm_keyboard *wsm_keyboard_create(struct wsm_seat *seat,
                                           struct wsm_seat_device *device);
void wsm_keyboard_configure(struct wsm_keyboard *keyboard);
void wsm_keyboard_destroy(struct wsm_keyboard *keyboard);
void wsm_keyboard_disarm_key_repeat(struct wsm_keyboard *keyboard);
struct wsm_keyboard *wsm_keyboard_for_wlr_keyboard(
    struct wsm_seat *seat, struct wlr_keyboard *wlr_keyboard);
uint32_t get_modifier_mask_by_name(const char *name);
const char *get_modifier_name_by_mask(uint32_t modifier);
int get_modifier_names(const char **names, uint32_t modifier_masks);
struct wsm_keyboard_shortcuts_inhibitor *
keyboard_shortcuts_inhibitor_get_for_focused_surface(
    const struct wsm_seat *seat);

#endif
