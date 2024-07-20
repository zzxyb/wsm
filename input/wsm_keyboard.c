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
#include "wsm_list.h"
#include "wsm_server.h"
#include "wsm_config.h"
#include "wsm_input_config.h"
#include "wsm_keyboard.h"
#include "wsm_text_input.h"
#include "wsm_input_manager.h"

#include <stdlib.h>
#include <strings.h>
#include <limits.h>

#include <xkbcommon/xkbcommon.h>
#include <xkbcommon/xkbcommon-names.h>

#include <wlr/config.h>

#if WLR_HAS_SESSION
#include <wlr/backend/session.h>
#endif

#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_keyboard.h>
#include <wlr/interfaces/wlr_keyboard.h>
#include <wlr/types/wlr_keyboard.h>
#include <wlr/types/wlr_keyboard_group.h>
#include <wlr/types/wlr_input_method_v2.h>
#include <wlr/types/wlr_virtual_keyboard_v1.h>

static struct modifier_key {
    char *name;
    uint32_t mod;
} modifiers[] = {
    { XKB_MOD_NAME_SHIFT, WLR_MODIFIER_SHIFT },
    { XKB_MOD_NAME_CAPS, WLR_MODIFIER_CAPS },
    { XKB_MOD_NAME_CTRL, WLR_MODIFIER_CTRL },
    { "Ctrl", WLR_MODIFIER_CTRL },
    { XKB_MOD_NAME_ALT, WLR_MODIFIER_ALT },
    { "Alt", WLR_MODIFIER_ALT },
    { XKB_MOD_NAME_NUM, WLR_MODIFIER_MOD2 },
    { "Mod3", WLR_MODIFIER_MOD3 },
    { XKB_MOD_NAME_LOGO, WLR_MODIFIER_LOGO },
    { "Super", WLR_MODIFIER_LOGO },
    { "Mod5", WLR_MODIFIER_MOD5 },
    };

uint32_t get_modifier_mask_by_name(const char *name) {
    int i;
    for (i = 0; i < (int)(sizeof(modifiers) / sizeof(struct modifier_key)); ++i) {
        if (strcasecmp(modifiers[i].name, name) == 0) {
            return modifiers[i].mod;
        }
    }

    return 0;
}

const char *get_modifier_name_by_mask(uint32_t modifier) {
    int i;
    for (i = 0; i < (int)(sizeof(modifiers) / sizeof(struct modifier_key)); ++i) {
        if (modifiers[i].mod == modifier) {
            return modifiers[i].name;
        }
    }

    return NULL;
}

int get_modifier_names(const char **names, uint32_t modifier_masks) {
    int length = 0;
    int i;
    for (i = 0; i < (int)(sizeof(modifiers) / sizeof(struct modifier_key)); ++i) {
        if ((modifier_masks & modifiers[i].mod) != 0) {
            names[length] = modifiers[i].name;
            ++length;
            modifier_masks ^= modifiers[i].mod;
        }
    }

    return length;
}

static bool state_erase_key(struct wsm_shortcut_state *state,
                            uint32_t keycode) {
    bool found = false;
    size_t j = 0;
    for (size_t i = 0; i < state->npressed; ++i) {
        if (i > j) {
            state->pressed_keys[j] = state->pressed_keys[i];
            state->pressed_keycodes[j] = state->pressed_keycodes[i];
        }
        if (state->pressed_keycodes[i] != keycode) {
            ++j;
        } else {
            found = true;
        }
    }
    while(state->npressed > j) {
        --state->npressed;
        state->pressed_keys[state->npressed] = 0;
        state->pressed_keycodes[state->npressed] = 0;
    }
    state->current_key = 0;
    return found;
}

static void state_add_key(struct wsm_shortcut_state *state,
                          uint32_t keycode, uint32_t key_id) {
    if (state->npressed >= WSM_KEYBOARD_PRESSED_KEYS_CAP) {
        return;
    }
    size_t i = 0;
    while (i < state->npressed && state->pressed_keys[i] < key_id) {
        ++i;
    }
    size_t j = state->npressed;
    while (j > i) {
        state->pressed_keys[j] = state->pressed_keys[j - 1];
        state->pressed_keycodes[j] = state->pressed_keycodes[j - 1];
        --j;
    }
    state->pressed_keys[i] = key_id;
    state->pressed_keycodes[i] = keycode;
    state->npressed++;
    state->current_key = key_id;
}

static bool update_shortcut_state(struct wsm_shortcut_state *state,
                                  uint32_t keycode, enum wl_keyboard_key_state keystate, uint32_t new_key,
                                  uint32_t raw_modifiers) {
    bool last_key_was_a_modifier = raw_modifiers != state->last_raw_modifiers;
    state->last_raw_modifiers = raw_modifiers;

    if (last_key_was_a_modifier && state->last_keycode) {
        // Last pressed key before this one was a modifier
        state_erase_key(state, state->last_keycode);
    }

    if (keystate == WL_KEYBOARD_KEY_STATE_PRESSED) {
        // Add current key to set; there may be duplicates
        state_add_key(state, keycode, new_key);
        state->last_keycode = keycode;
    } else {
        return state_erase_key(state, keycode);
    }

    return false;
}

// static void get_active_binding(const struct wsm_shortcut_state *state,
//                                struct wsm_list *bindings, struct wsm_binding **current_binding,
//                                uint32_t modifiers, bool release, bool locked, bool inhibited,
//                                const char *input, bool exact_input, xkb_layout_index_t group) {
//     for (int i = 0; i < bindings->length; ++i) {
//         struct wsm_binding *binding = bindings->items[i];
//         bool binding_locked = (binding->flags & BINDING_LOCKED) != 0;
//         bool binding_inhibited = (binding->flags & BINDING_INHIBITED) != 0;
//         bool binding_release = binding->flags & BINDING_RELEASE;

//         if (modifiers ^ binding->modifiers ||
//             release != binding_release ||
//             locked > binding_locked ||
//             inhibited > binding_inhibited ||
//             (binding->group != XKB_LAYOUT_INVALID &&
//              binding->group != group) ||
//             (strcmp(binding->input, input) != 0 &&
//              (strcmp(binding->input, "*") != 0 || exact_input))) {
//             continue;
//         }

//         bool match = false;
//         if (state->npressed == (size_t)binding->keys->length) {
//             match = true;
//             for (size_t j = 0; j < state->npressed; j++) {
//                 uint32_t key = *(uint32_t *)binding->keys->items[j];
//                 if (key != state->pressed_keys[j]) {
//                     match = false;
//                     break;
//                 }
//             }
//         } else if (binding->keys->length == 1) {
//             /*
//              * If no multiple-key binding has matched, try looking for
//              * single-key bindings that match the newly-pressed key.
//              */
//             match = state->current_key == *(uint32_t *)binding->keys->items[0];
//         }
//         if (!match) {
//             continue;
//         }

//         if (*current_binding) {
//             if (*current_binding == binding) {
//                 continue;
//             }

//             bool current_locked =
//                 ((*current_binding)->flags & BINDING_LOCKED) != 0;
//             bool current_inhibited =
//                 ((*current_binding)->flags & BINDING_INHIBITED) != 0;
//             bool current_input = strcmp((*current_binding)->input, input) == 0;
//             bool current_group_set =
//                 (*current_binding)->group != XKB_LAYOUT_INVALID;
//             bool binding_input = strcmp(binding->input, input) == 0;
//             bool binding_group_set = binding->group != XKB_LAYOUT_INVALID;

//             if (current_input == binding_input
//                 && current_locked == binding_locked
//                 && current_inhibited == binding_inhibited
//                 && current_group_set == binding_group_set) {
//                 wsm_log(WSM_DEBUG,
//                          "Encountered conflicting bindings %d and %d",
//                          (*current_binding)->order, binding->order);
//                 continue;
//             }

//             if (current_input && !binding_input) {
//                 continue; // Prefer the correct input
//             }

//             if (current_input == binding_input &&
//                 (*current_binding)->group == group) {
//                 continue; // Prefer correct group for matching inputs
//             }

//             if (current_input == binding_input &&
//                 current_group_set == binding_group_set &&
//                 current_locked == locked) {
//                 continue; // Prefer correct lock state for matching input+group
//             }

//             if (current_input == binding_input &&
//                 current_group_set == binding_group_set &&
//                 current_locked == binding_locked &&
//                 current_inhibited == inhibited) {
//                 // Prefer correct inhibition state for matching
//                 // input+group+locked
//                 continue;
//             }
//         }

//         *current_binding = binding;
//         if (strcmp((*current_binding)->input, input) == 0 &&
//             (((*current_binding)->flags & BINDING_LOCKED) == locked) &&
//             (((*current_binding)->flags & BINDING_INHIBITED) == inhibited) &&
//             (*current_binding)->group == group) {
//             return; // If a perfect match is found, quit searching
//         }
//     }
// }

static bool keyboard_execute_compositor_binding(struct wsm_keyboard *keyboard,
                                                const xkb_keysym_t *pressed_keysyms, uint32_t modifiers, size_t keysyms_len) {
    for (size_t i = 0; i < keysyms_len; ++i) {
        xkb_keysym_t keysym = pressed_keysyms[i];
        if (keysym >= XKB_KEY_XF86Switch_VT_1 &&
            keysym <= XKB_KEY_XF86Switch_VT_12) {
#if WLR_HAS_SESSION
            if (global_server.wlr_session) {
                unsigned vt = keysym - XKB_KEY_XF86Switch_VT_1 + 1;
                wlr_session_change_vt(global_server.wlr_session, vt);
            }
#endif
            return true;
        }
    }

    return false;
}

static size_t keyboard_keysyms_translated(struct wsm_keyboard *keyboard,
                                          xkb_keycode_t keycode, const xkb_keysym_t **keysyms,
                                          uint32_t *modifiers) {
    *modifiers = wlr_keyboard_get_modifiers(keyboard->wlr);
    xkb_mod_mask_t consumed = xkb_state_key_get_consumed_mods2(
        keyboard->wlr->xkb_state, keycode, XKB_CONSUMED_MODE_XKB);
    *modifiers = *modifiers & ~consumed;

    return xkb_state_key_get_syms(keyboard->wlr->xkb_state,
                                  keycode, keysyms);
}

static size_t keyboard_keysyms_raw(struct wsm_keyboard *keyboard,
                                   xkb_keycode_t keycode, const xkb_keysym_t **keysyms,
                                   uint32_t *modifiers) {
    *modifiers = wlr_keyboard_get_modifiers(keyboard->wlr);

    xkb_layout_index_t layout_index = xkb_state_key_get_layout(
        keyboard->wlr->xkb_state, keycode);
    return xkb_keymap_key_get_syms_by_level(keyboard->wlr->keymap,
                                            keycode, layout_index, 0, keysyms);
}

void wsm_keyboard_disarm_key_repeat(struct wsm_keyboard *keyboard) {
    if (!keyboard) {
        return;
    }
    keyboard->repeat_binding = NULL;
    if (wl_event_source_timer_update(keyboard->key_repeat_source, 0) < 0) {
        wsm_log(WSM_DEBUG, "failed to disarm key repeat timer");
    }
}

struct key_info {
    uint32_t keycode;
    uint32_t code_modifiers;

    const xkb_keysym_t *raw_keysyms;
    uint32_t raw_modifiers;
    size_t raw_keysyms_len;

    const xkb_keysym_t *translated_keysyms;
    uint32_t translated_modifiers;
    size_t translated_keysyms_len;
};

static void update_keyboard_state(struct wsm_keyboard *keyboard,
                                  uint32_t raw_keycode, enum wl_keyboard_key_state keystate,
                                  struct key_info *keyinfo) {
    // Identify new keycode, raw keysym(s), and translated keysym(s)
    keyinfo->keycode = raw_keycode + 8;

    keyinfo->raw_keysyms_len = keyboard_keysyms_raw(keyboard, keyinfo->keycode,
                                                    &keyinfo->raw_keysyms, &keyinfo->raw_modifiers);

    keyinfo->translated_keysyms_len = keyboard_keysyms_translated(keyboard,
                                                                  keyinfo->keycode, &keyinfo->translated_keysyms,
                                                                  &keyinfo->translated_modifiers);

    keyinfo->code_modifiers = wlr_keyboard_get_modifiers(keyboard->wlr);

    // Update shortcut model keyinfo
    update_shortcut_state(&keyboard->state_keycodes, raw_keycode, keystate,
                          keyinfo->keycode, keyinfo->code_modifiers);
    for (size_t i = 0; i < keyinfo->raw_keysyms_len; ++i) {
        update_shortcut_state(&keyboard->state_keysyms_raw,
                              raw_keycode, keystate, keyinfo->raw_keysyms[i],
                              keyinfo->code_modifiers);
    }
    for (size_t i = 0; i < keyinfo->translated_keysyms_len; ++i) {
        update_shortcut_state(&keyboard->state_keysyms_translated,
                              raw_keycode, keystate, keyinfo->translated_keysyms[i],
                              keyinfo->code_modifiers);
    }
}

static struct wlr_input_method_keyboard_grab_v2 *keyboard_get_im_grab(
    struct wsm_keyboard *keyboard) {
    struct wlr_input_method_v2 *input_method = keyboard->seat_device->
                                               wsm_seat->im_relay.input_method;
    struct wlr_virtual_keyboard_v1 *virtual_keyboard =
        wlr_input_device_get_virtual_keyboard(keyboard->seat_device->input_device->wlr_device);
    if (!input_method || !input_method->keyboard_grab || (virtual_keyboard &&
                                                          wl_resource_get_client(virtual_keyboard->resource) ==
                                                              wl_resource_get_client(input_method->keyboard_grab->resource))) {
        return NULL;
    }
    return input_method->keyboard_grab;
}

static void handle_key_event(struct wsm_keyboard *keyboard,
                             struct wlr_keyboard_key_event *event) {
    struct wsm_seat *seat = keyboard->seat_device->wsm_seat;
    struct wlr_seat *wlr_seat = seat->wlr_seat;
    struct wlr_input_device *wlr_device =
        keyboard->seat_device->input_device->wlr_device;
    char *device_identifier = input_device_get_identifier(wlr_device);
    // bool exact_identifier = keyboard->wlr->group != NULL;
    seat_idle_notify_activity(seat, WLR_INPUT_DEVICE_KEYBOARD);
    // bool locked = global_server.session_lock.lock;
    // struct wsm_keyboard_shortcuts_inhibitor *wsm_inhibitor =
        // keyboard_shortcuts_inhibitor_get_for_focused_surface(seat);
    // bool shortcuts_inhibited = wsm_inhibitor && wsm_inhibitor->inhibitor->active;

    if (event->state == WL_KEYBOARD_KEY_STATE_PRESSED) {
        // cursor_notify_key_press(seat->cursor);
    }

    // Identify new keycode, raw keysym(s), and translated keysym(s)
    struct key_info keyinfo;
    update_keyboard_state(keyboard, event->keycode, event->state, &keyinfo);

    bool handled = false;
    // // Identify active release binding
    // struct wsm_binding *binding_released = NULL;
    // get_active_binding(&keyboard->state_keycodes,
    //                    config->current_mode->keycode_bindings, &binding_released,
    //                    keyinfo.code_modifiers, true, locked,
    //                    shortcuts_inhibited, device_identifier,
    //                    exact_identifier, keyboard->effective_layout);
    // get_active_binding(&keyboard->state_keysyms_raw,
    //                    config->current_mode->keysym_bindings, &binding_released,
    //                    keyinfo.raw_modifiers, true, locked,
    //                    shortcuts_inhibited, device_identifier,
    //                    exact_identifier, keyboard->effective_layout);
    // get_active_binding(&keyboard->state_keysyms_translated,
    //                    config->current_mode->keysym_bindings, &binding_released,
    //                    keyinfo.translated_modifiers, true, locked,
    //                    shortcuts_inhibited, device_identifier,
    //                    exact_identifier, keyboard->effective_layout);

    // // Execute stored release binding once no longer active
    // if (keyboard->held_binding && binding_released != keyboard->held_binding &&
    //     event->state == WL_KEYBOARD_KEY_STATE_RELEASED) {
    //     seat_execute_command(seat, keyboard->held_binding);
    //     handled = true;
    // }
    // if (binding_released != keyboard->held_binding) {
    //     keyboard->held_binding = NULL;
    // }
    // if (binding_released && event->state == WL_KEYBOARD_KEY_STATE_PRESSED) {
    //     keyboard->held_binding = binding_released;
    // }

    // Identify and execute active pressed binding
    struct wsm_binding *binding = NULL;
    // if (event->state == WL_KEYBOARD_KEY_STATE_PRESSED) {
    //     get_active_binding(&keyboard->state_keycodes,
    //                        config->current_mode->keycode_bindings, &binding,
    //                        keyinfo.code_modifiers, false, locked,
    //                        shortcuts_inhibited, device_identifier,
    //                        exact_identifier, keyboard->effective_layout);
    //     get_active_binding(&keyboard->state_keysyms_raw,
    //                        config->current_mode->keysym_bindings, &binding,
    //                        keyinfo.raw_modifiers, false, locked,
    //                        shortcuts_inhibited, device_identifier,
    //                        exact_identifier, keyboard->effective_layout);
    //     get_active_binding(&keyboard->state_keysyms_translated,
    //                        config->current_mode->keysym_bindings, &binding,
    //                        keyinfo.translated_modifiers, false, locked,
    //                        shortcuts_inhibited, device_identifier,
    //                        exact_identifier, keyboard->effective_layout);
    // }

    // Set up (or clear) keyboard repeat for a pressed binding. Since the
    // binding may remove the keyboard, the timer needs to be updated first
    if (binding && !(binding->flags & BINDING_NOREPEAT) &&
        keyboard->wlr->repeat_info.delay > 0) {
        keyboard->repeat_binding = binding;
        if (wl_event_source_timer_update(keyboard->key_repeat_source,
                                         keyboard->wlr->repeat_info.delay) < 0) {
            wsm_log(WSM_DEBUG, "failed to set key repeat timer");
        }
    } else if (keyboard->repeat_binding) {
        wsm_keyboard_disarm_key_repeat(keyboard);
    }

    // if (binding) {
    //     seat_execute_command(seat, binding);
    //     handled = true;
    // }

    if (!handled && keyboard->wlr->group) {
        // Only handle device specific bindings for keyboards in a group
        free(device_identifier);
        return;
    }

    // Compositor bindings
    if (!handled && event->state == WL_KEYBOARD_KEY_STATE_PRESSED) {
        handled = keyboard_execute_compositor_binding(
            keyboard, keyinfo.translated_keysyms,
            keyinfo.translated_modifiers, keyinfo.translated_keysyms_len);
    }
    if (!handled && event->state == WL_KEYBOARD_KEY_STATE_PRESSED) {
        handled = keyboard_execute_compositor_binding(
            keyboard, keyinfo.raw_keysyms, keyinfo.raw_modifiers,
            keyinfo.raw_keysyms_len);
    }

    if (event->state == WL_KEYBOARD_KEY_STATE_RELEASED) {
        // If the pressed event was sent to a client and we have a focused
        // surface immediately before this event, also send the released
        // event. In particular, don't send the released event to the IM grab.
        bool pressed_sent = update_shortcut_state(
            &keyboard->state_pressed_sent, event->keycode,
            event->state, keyinfo.keycode, 0);
        if (pressed_sent && seat->wlr_seat->keyboard_state.focused_surface) {
            wlr_seat_set_keyboard(wlr_seat, keyboard->wlr);
            wlr_seat_keyboard_notify_key(wlr_seat, event->time_msec,
                                         event->keycode, event->state);
            handled = true;
        }
    }

    if (!handled) {
        struct wlr_input_method_keyboard_grab_v2 *kb_grab = keyboard_get_im_grab(keyboard);

        if (kb_grab) {
            wlr_input_method_keyboard_grab_v2_set_keyboard(kb_grab, keyboard->wlr);
            wlr_input_method_keyboard_grab_v2_send_key(kb_grab,
                                                       event->time_msec, event->keycode, event->state);
            handled = true;
        }
    }

    if (!handled && event->state != WL_KEYBOARD_KEY_STATE_RELEASED) {
        // If a released event failed pressed sent test, and not in sent to
        // keyboard grab, it is still not handled. Don't handle released here.
        update_shortcut_state(
            &keyboard->state_pressed_sent, event->keycode, event->state,
            keyinfo.keycode, 0);
        wlr_seat_set_keyboard(wlr_seat, keyboard->wlr);
        wlr_seat_keyboard_notify_key(wlr_seat, event->time_msec,
                                     event->keycode, event->state);
    }

    free(device_identifier);
}

static void handle_keyboard_key(struct wl_listener *listener, void *data) {
    struct wsm_keyboard *keyboard =
        wl_container_of(listener, keyboard, keyboard_key);
    handle_key_event(keyboard, data);
}

static void handle_keyboard_group_key(struct wl_listener *listener,
                                      void *data) {
    struct wsm_keyboard_group *wsm_group =
        wl_container_of(listener, wsm_group, keyboard_key);
    handle_key_event(wsm_group->seat_device->keyboard, data);
}

static void handle_keyboard_group_enter(struct wl_listener *listener,
                                        void *data) {
    struct wsm_keyboard_group *wsm_group =
        wl_container_of(listener, wsm_group, enter);
    struct wsm_keyboard *keyboard = wsm_group->seat_device->keyboard;
    struct wl_array *keycodes = data;

    uint32_t *keycode;
    wl_array_for_each(keycode, keycodes) {
        struct key_info keyinfo;
        update_keyboard_state(keyboard, *keycode, WL_KEYBOARD_KEY_STATE_PRESSED, &keyinfo);
    }
}

static void handle_keyboard_group_leave(struct wl_listener *listener,
                                        void *data) {
    struct wsm_keyboard_group *wsm_group =
        wl_container_of(listener, wsm_group, leave);
    struct wsm_keyboard *keyboard = wsm_group->seat_device->keyboard;
    struct wl_array *keycodes = data;

    bool pressed_sent = false;

    uint32_t *keycode;
    wl_array_for_each(keycode, keycodes) {
        struct key_info keyinfo;
        update_keyboard_state(keyboard, *keycode, WL_KEYBOARD_KEY_STATE_RELEASED, &keyinfo);

        pressed_sent |= update_shortcut_state(&keyboard->state_pressed_sent,
                                              *keycode, WL_KEYBOARD_KEY_STATE_RELEASED, keyinfo.keycode, 0);
    }

    if (!pressed_sent) {
        return;
    }

    // Refocus the focused node, layer surface, or unmanaged surface so that
    // it picks up the current keyboard state.
    struct wsm_seat *seat = keyboard->seat_device->wsm_seat;
    struct wsm_node *focus = seat_get_focus(seat);
    if (focus) {
        seat_set_focus(seat, NULL);
        seat_set_focus(seat, focus);
    } else if (seat->focused_layer) {
        struct wlr_layer_surface_v1 *layer = seat->focused_layer;
        seat_set_focus_layer(seat, NULL);
        seat_set_focus_layer(seat, layer);
    } else {
        struct wlr_surface *unmanaged = seat->wlr_seat->keyboard_state.focused_surface;
        seat_set_focus_surface(seat, NULL, false);
        seat_set_focus_surface(seat, unmanaged, false);
    }
}

static int handle_keyboard_repeat(void *data) {
    struct wsm_keyboard *keyboard = data;
    if (keyboard->repeat_binding) {
        if (keyboard->wlr->repeat_info.rate > 0) {
            // We queue the next event first, as the command might cancel it
            if (wl_event_source_timer_update(keyboard->key_repeat_source,
                                             1000 / keyboard->wlr->repeat_info.rate) < 0) {
                wsm_log(WSM_DEBUG, "failed to update key repeat timer");
            }
        }

        // seat_execute_command(keyboard->seat_device->wsm_seat,
        //                      keyboard->repeat_binding);
    }
    return 0;
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

static void handle_keyboard_group_modifiers(struct wl_listener *listener,
                                            void *data) {
    struct wsm_keyboard_group *group =
        wl_container_of(listener, group, keyboard_modifiers);
    handle_modifier_event(group->seat_device->keyboard);
}

struct wsm_keyboard *wsm_keyboard_create(struct wsm_seat *seat,
                                           struct wsm_seat_device *device) {
    struct wsm_keyboard *keyboard =
        calloc(1, sizeof(struct wsm_keyboard));
    if (!wsm_assert(keyboard, "could not allocate wsm keyboard")) {
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

static char *vformat_str(const char *fmt, va_list args) {
    char *str = NULL;
    va_list args_copy;
    va_copy(args_copy, args);

    int len = vsnprintf(NULL, 0, fmt, args);
    if (len < 0) {
        wsm_log_errno(WSM_ERROR, "vsnprintf(\"%s\") failed", fmt);
        goto out;
    }

    str = malloc(len + 1);
    if (str == NULL) {
        wsm_log_errno(WSM_ERROR, "malloc() failed");
        goto out;
    }

    vsnprintf(str, len + 1, fmt, args_copy);

out:
    va_end(args_copy);
    return str;
}

static void handle_xkb_context_log(struct xkb_context *context,
                                   enum xkb_log_level level, const char *format, va_list args) {
    char *error = vformat_str(format, args);

    size_t len = strlen(error);
    if (error[len - 1] == '\n') {
        error[len - 1] = '\0';
    }

    wsm_log_importance_t importance = WSM_DEBUG;
    if (level <= XKB_LOG_LEVEL_ERROR) { // Critical and Error
        importance = WSM_ERROR;
    } else if (level <= XKB_LOG_LEVEL_INFO) { // Warning and Info
        importance = WSM_INFO;
    }
    wsm_log(importance, "[xkbcommon] %s", error);

    char **data = xkb_context_get_user_data(context);
    if (importance == WSM_ERROR && data && !*data) {
        *data = error;
    } else {
        free(error);
    }
}

static char *format_str(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    char *str = vformat_str(fmt, args);
    va_end(args);
    return str;
}

struct xkb_keymap *wsm_keyboard_compile_keymap(struct input_config *ic,
                                                char **error) {
    struct xkb_context *context = xkb_context_new(XKB_CONTEXT_NO_SECURE_GETENV);
    if (!wsm_assert(context, "cannot create XKB context")) {
        return NULL;
    }
    xkb_context_set_user_data(context, error);
    xkb_context_set_log_fn(context, handle_xkb_context_log);

    struct xkb_keymap *keymap = NULL;

    if (ic && ic->xkb_file) {
        FILE *keymap_file = fopen(ic->xkb_file, "r");
        if (!keymap_file) {
            wsm_log_errno(WSM_ERROR, "cannot read xkb file %s", ic->xkb_file);
            if (error) {
                *error = format_str("cannot read xkb file %s: %s",
                                    ic->xkb_file, strerror(errno));
            }
            goto cleanup;
        }

        keymap = xkb_keymap_new_from_file(context, keymap_file,
                                          XKB_KEYMAP_FORMAT_TEXT_V1, XKB_KEYMAP_COMPILE_NO_FLAGS);

        if (fclose(keymap_file) != 0) {
            wsm_log_errno(WSM_ERROR, "Failed to close xkb file %s",
                           ic->xkb_file);
        }
    } else {
        struct xkb_rule_names rules = {0};
        if (ic) {
            input_config_fill_rule_names(ic, &rules);
        }
        keymap = xkb_keymap_new_from_names(context, &rules,
                                           XKB_KEYMAP_COMPILE_NO_FLAGS);
    }

cleanup:
    xkb_context_set_user_data(context, NULL);
    xkb_context_unref(context);
    return keymap;
}

static bool repeat_info_match(struct wsm_keyboard *a, struct wlr_keyboard *b) {
    return a->repeat_rate == b->repeat_info.rate &&
           a->repeat_delay == b->repeat_info.delay;
}

static void destroy_empty_wlr_keyboard_group(void *data) {
    wlr_keyboard_group_destroy(data);
}

static void wsm_keyboard_group_remove(struct wsm_keyboard *keyboard) {
    struct wsm_input_device *device = keyboard->seat_device->input_device;
    struct wlr_keyboard_group *wlr_group = keyboard->wlr->group;

    wsm_log(WSM_DEBUG, "Removing keyboard %s from group %p",
             device->identifier, wlr_group);

    wlr_keyboard_group_remove_keyboard(keyboard->wlr->group, keyboard->wlr);

    if (wl_list_empty(&wlr_group->devices)) {
        wsm_log(WSM_DEBUG, "Destroying empty keyboard group %p",
                 wlr_group);
        struct wsm_keyboard_group *wsm_group = wlr_group->data;
        wlr_group->data = NULL;
        wl_list_remove(&wsm_group->link);
        wl_list_remove(&wsm_group->keyboard_key.link);
        wl_list_remove(&wsm_group->keyboard_modifiers.link);
        wl_list_remove(&wsm_group->enter.link);
        wl_list_remove(&wsm_group->leave.link);
        wsm_keyboard_destroy(wsm_group->seat_device->keyboard);
        free(wsm_group->seat_device->input_device);
        free(wsm_group->seat_device);
        free(wsm_group);

        // To prevent use-after-free conditions when handling key events, defer
        // freeing the wlr_keyboard_group until idle
        wl_event_loop_add_idle(global_server.wl_event_loop,
                               destroy_empty_wlr_keyboard_group, wlr_group);
    }
}

static void wsm_keyboard_group_remove_invalid(struct wsm_keyboard *keyboard) {
    if (!keyboard->wlr->group) {
        return;
    }
}

static void wsm_keyboard_group_add(struct wsm_keyboard *keyboard) {
    struct wsm_input_device *device = keyboard->seat_device->input_device;
    struct wsm_seat *seat = keyboard->seat_device->wsm_seat;
    // struct seat_config *sc = seat_get_config(seat);

    if (device->is_virtual) {
        // Virtual devices should not be grouped
        return;
    }

    // if (!sc) {
    //     sc = seat_get_config_by_name("*");
    // }

    // if (sc && sc->keyboard_grouping == KEYBOARD_GROUP_NONE) {
    //     // Keyboard grouping is disabled for the seat
    //     return;
    // }

    struct wsm_keyboard_group *group;
    wl_list_for_each(group, &seat->keyboard_groups, link) {
        // switch (sc ? sc->keyboard_grouping : KEYBOARD_GROUP_DEFAULT) {
        switch (KEYBOARD_GROUP_DEFAULT) {
        case KEYBOARD_GROUP_NONE:
            // Nothing to do. This shouldn't even be reached
            return;
        case KEYBOARD_GROUP_DEFAULT: /* fallthrough */
        case KEYBOARD_GROUP_SMART:;
            struct wlr_keyboard_group *wlr_group = group->wlr_group;
            if (wlr_keyboard_keymaps_match(keyboard->keymap,
                                           wlr_group->keyboard.keymap) &&
                repeat_info_match(keyboard, &wlr_group->keyboard)) {
                wsm_log(WSM_DEBUG, "Adding keyboard %s to group %p",
                         device->identifier, wlr_group);
                wlr_keyboard_group_add_keyboard(wlr_group, keyboard->wlr);
                return;
            }
            break;
        }
    }

    struct wsm_keyboard_group *wsm_group =
        calloc(1, sizeof(struct wsm_keyboard_group));
    if (!wsm_group) {
        wsm_log(WSM_ERROR, "Failed to allocate wsm_keyboard_group");
        return;
    }

    wsm_group->wlr_group = wlr_keyboard_group_create();
    if (!wsm_group->wlr_group) {
        wsm_log(WSM_ERROR, "Failed to create keyboard group");
        goto cleanup;
    }
    wsm_group->wlr_group->data = wsm_group;
    wlr_keyboard_set_keymap(&wsm_group->wlr_group->keyboard, keyboard->keymap);
    wlr_keyboard_set_repeat_info(&wsm_group->wlr_group->keyboard,
                                 keyboard->repeat_rate, keyboard->repeat_delay);
    wsm_log(WSM_DEBUG, "Created keyboard group %p", wsm_group->wlr_group);

    wsm_group->seat_device = calloc(1, sizeof(struct wsm_seat_device));
    if (!wsm_group->seat_device) {
        wsm_log(WSM_ERROR, "Failed to allocate wsm_seat_device for group");
        goto cleanup;
    }
    wsm_group->seat_device->wsm_seat = seat;

    wsm_group->seat_device->input_device =
        calloc(1, sizeof(struct wsm_input_device));
    if (!wsm_group->seat_device->input_device) {
        wsm_log(WSM_ERROR, "Failed to allocate wsm_input_device for group");
        goto cleanup;
    }
    wsm_group->seat_device->input_device->wlr_device =
        &wsm_group->wlr_group->keyboard.base;

    if (!wsm_keyboard_create(seat, wsm_group->seat_device)) {
        wsm_log(WSM_ERROR, "Failed to allocate wsm_keyboard for group");
        goto cleanup;
    }

    wsm_log(WSM_DEBUG, "Adding keyboard %s to group %p",
             device->identifier, wsm_group->wlr_group);
    wlr_keyboard_group_add_keyboard(wsm_group->wlr_group, keyboard->wlr);

    wl_list_insert(&seat->keyboard_groups, &wsm_group->link);

    wl_signal_add(&wsm_group->wlr_group->keyboard.events.key,
                  &wsm_group->keyboard_key);
    wsm_group->keyboard_key.notify = handle_keyboard_group_key;

    wl_signal_add(&wsm_group->wlr_group->keyboard.events.modifiers,
                  &wsm_group->keyboard_modifiers);
    wsm_group->keyboard_modifiers.notify = handle_keyboard_group_modifiers;

    wl_signal_add(&wsm_group->wlr_group->events.enter, &wsm_group->enter);
    wsm_group->enter.notify = handle_keyboard_group_enter;

    wl_signal_add(&wsm_group->wlr_group->events.leave, &wsm_group->leave);
    wsm_group->leave.notify = handle_keyboard_group_leave;
    return;

cleanup:
    if (wsm_group && wsm_group->wlr_group) {
        wlr_keyboard_group_destroy(wsm_group->wlr_group);
    }
    free(wsm_group->seat_device->keyboard);
    free(wsm_group->seat_device->input_device);
    free(wsm_group->seat_device);
    free(wsm_group);
}

static void wsm_keyboard_set_layout(struct wsm_keyboard *keyboard,
                                     struct input_config *input_config) {
    struct xkb_keymap *keymap = wsm_keyboard_compile_keymap(input_config, NULL);
    if (!keymap) {
        wsm_log(WSM_ERROR, "Failed to compile keymap. Attempting defaults");
        keymap = wsm_keyboard_compile_keymap(NULL, NULL);
        if (!keymap) {
            wsm_log(WSM_ERROR,
                     "Failed to compile default keymap. Aborting configure");
            return;
        }
    }

    bool keymap_changed = keyboard->keymap ?
                              !wlr_keyboard_keymaps_match(keyboard->keymap, keymap) : true;
    // bool effective_layout_changed = keyboard->effective_layout != 0;

    if (keymap_changed || global_config.reloading) {
        xkb_keymap_unref(keyboard->keymap);
        keyboard->keymap = keymap;
        keyboard->effective_layout = 0;

        wsm_keyboard_group_remove_invalid(keyboard);
        wlr_keyboard_set_keymap(keyboard->wlr, keyboard->keymap);
        if (!keyboard->wlr->group) {
            wsm_keyboard_group_add(keyboard);
        }

        xkb_mod_mask_t locked_mods = 0;
        if (input_config && input_config->xkb_numlock > 0) {
            xkb_mod_index_t mod_index = xkb_map_mod_get_index(keymap,
                                                              XKB_MOD_NAME_NUM);
            if (mod_index != XKB_MOD_INVALID) {
                locked_mods |= (uint32_t)1 << mod_index;
            }
        }
        if (input_config && input_config->xkb_capslock > 0) {
            xkb_mod_index_t mod_index = xkb_map_mod_get_index(keymap,
                                                              XKB_MOD_NAME_CAPS);
            if (mod_index != XKB_MOD_INVALID) {
                locked_mods |= (uint32_t)1 << mod_index;
            }
        }
        if (locked_mods) {
            wlr_keyboard_notify_modifiers(keyboard->wlr, 0, 0,
                                          locked_mods, 0);
            uint32_t leds = 0;
            for (uint32_t i = 0; i < WLR_LED_COUNT; ++i) {
                if (xkb_state_led_index_is_active(keyboard->wlr->xkb_state,
                                                  keyboard->wlr->led_indexes[i])) {
                    leds |= (1 << i);
                }
            }
            if (keyboard->wlr->group) {
                wlr_keyboard_led_update(&keyboard->wlr->group->keyboard, leds);
            } else {
                wlr_keyboard_led_update(keyboard->wlr, leds);
            }
        }
    } else {
        xkb_keymap_unref(keymap);
        wsm_keyboard_group_remove_invalid(keyboard);
        if (!keyboard->wlr->group) {
            wsm_keyboard_group_add(keyboard);
        }
    }

    // If the seat has no active keyboard, set this one
    struct wlr_seat *seat = keyboard->seat_device->wsm_seat->wlr_seat;
    struct wlr_keyboard *current_keyboard = seat->keyboard_state.keyboard;
    if (current_keyboard == NULL) {
        wlr_seat_set_keyboard(seat, keyboard->wlr);
    }
}

void wsm_keyboard_configure(struct wsm_keyboard *keyboard) {
    struct input_config *input_config =
        input_device_get_config(keyboard->seat_device->input_device);

    if (!wsm_assert(!wlr_keyboard_group_from_wlr_keyboard(keyboard->wlr),
                     "wsm_keyboard_configure should not be called with a "
                     "keyboard group's keyboard")) {
        return;
    }

    int repeat_rate = 25;
    if (input_config && input_config->repeat_rate != INT_MIN) {
        repeat_rate = input_config->repeat_rate;
    }
    int repeat_delay = 600;
    if (input_config && input_config->repeat_delay != INT_MIN) {
        repeat_delay = input_config->repeat_delay;
    }

    bool repeat_info_changed = keyboard->repeat_rate != repeat_rate ||
                               keyboard->repeat_delay != repeat_delay;

    if (repeat_info_changed || global_config.reloading) {
        keyboard->repeat_rate = repeat_rate;
        keyboard->repeat_delay = repeat_delay;

        wlr_keyboard_set_repeat_info(keyboard->wlr,
                                     keyboard->repeat_rate, keyboard->repeat_delay);
    }

    if (!keyboard->seat_device->input_device->is_virtual) {
        wsm_keyboard_set_layout(keyboard, input_config);
    }

    wl_list_remove(&keyboard->keyboard_key.link);
    wl_signal_add(&keyboard->wlr->events.key, &keyboard->keyboard_key);
    keyboard->keyboard_key.notify = handle_keyboard_key;

    wl_list_remove(&keyboard->keyboard_modifiers.link);
    wl_signal_add(&keyboard->wlr->events.modifiers,
                  &keyboard->keyboard_modifiers);
    keyboard->keyboard_modifiers.notify = handle_keyboard_modifiers;

}

void wsm_keyboard_destroy(struct wsm_keyboard *keyboard) {
    if (!keyboard) {
        return;
    }
    if (keyboard->wlr->group) {
        wsm_keyboard_group_remove(keyboard);
    }
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

struct wsm_keyboard_shortcuts_inhibitor *
keyboard_shortcuts_inhibitor_get_for_surface(
    const struct wsm_seat *seat,
    const struct wlr_surface *surface) {
    struct wsm_keyboard_shortcuts_inhibitor *wsm_inhibitor = NULL;
    wl_list_for_each(wsm_inhibitor, &seat->keyboard_shortcuts_inhibitors, link) {
        if (wsm_inhibitor->inhibitor->surface == surface) {
            return wsm_inhibitor;
        }
    }

    return NULL;
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
