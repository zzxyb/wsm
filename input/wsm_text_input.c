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
#include "wsm_text_input.h"

#include <assert.h>

#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_text_input_v3.h>
#include <wlr/types/wlr_input_method_v2.h>

static struct wsm_text_input *relay_get_focused_text_input(
    struct wsm_input_method_relay *relay) {
    struct wsm_text_input *text_input = NULL;
    wl_list_for_each(text_input, &relay->text_inputs, link) {
        if (text_input->input->focused_surface) {
            return text_input;
        }
    }
    return NULL;
}

static void relay_handle_text_input(struct wl_listener *listener,
                                    void *data) {
    struct wsm_input_method_relay *relay = wl_container_of(listener, relay,
                                                            text_input_new);
    struct wlr_text_input_v3 *wlr_text_input = data;
    if (relay->seat->wlr_seat != wlr_text_input->seat) {
        return;
    }

    wsm_text_input_create(relay, wlr_text_input);
}

static void handle_im_commit(struct wl_listener *listener, void *data) {
    struct wsm_input_method_relay *relay = wl_container_of(listener, relay,
                                                            input_method_commit);

    struct wsm_text_input *text_input = relay_get_focused_text_input(relay);
    if (!text_input) {
        wsm_log(WSM_DEBUG, "focused_text_input is NULL!");
        return;
    }

    struct wlr_input_method_v2 *context = data;
    assert(context == relay->input_method);
    if (context->current.preedit.text) {
        wlr_text_input_v3_send_preedit_string(text_input->input,
                                              context->current.preedit.text,
                                              context->current.preedit.cursor_begin,
                                              context->current.preedit.cursor_end);
    }
    if (context->current.commit_text) {
        wlr_text_input_v3_send_commit_string(text_input->input,
                                             context->current.commit_text);
    }
    if (context->current.delete.before_length
        || context->current.delete.after_length) {
        wlr_text_input_v3_send_delete_surrounding_text(text_input->input,
                                                       context->current.delete.before_length,
                                                       context->current.delete.after_length);
    }
    wlr_text_input_v3_send_done(text_input->input);
}

static void handle_im_keyboard_grab_destroy(struct wl_listener *listener, void *data) {
    struct wsm_input_method_relay *relay = wl_container_of(listener, relay,
                                                           input_method_keyboard_grab_destroy);
    struct wlr_input_method_keyboard_grab_v2 *keyboard_grab = data;
    wl_list_remove(&relay->input_method_keyboard_grab_destroy.link);

    if (keyboard_grab->keyboard) {
        wlr_seat_keyboard_notify_modifiers(keyboard_grab->input_method->seat,
                                           &keyboard_grab->keyboard->modifiers);
    }
}

static void text_input_set_pending_focused_surface(
    struct wsm_text_input *text_input, struct wlr_surface *surface) {
    wl_list_remove(&text_input->pending_focused_surface_destroy.link);
    text_input->pending_focused_surface = surface;

    if (surface) {
        wl_signal_add(&surface->events.destroy,
                      &text_input->pending_focused_surface_destroy);
    } else {
        wl_list_init(&text_input->pending_focused_surface_destroy.link);
    }
}

static void handle_im_destroy(struct wl_listener *listener, void *data) {
    struct wsm_input_method_relay *relay = wl_container_of(listener, relay,
                                                            input_method_destroy);
    struct wlr_input_method_v2 *context = data;
    assert(context == relay->input_method);
    relay->input_method = NULL;
    struct wsm_text_input *text_input = relay_get_focused_text_input(relay);
    if (text_input) {
        text_input_set_pending_focused_surface(text_input,
                                               text_input->input->focused_surface);
        wlr_text_input_v3_send_leave(text_input->input);
    }
}

static void handle_im_grab_keyboard(struct wl_listener *listener, void *data) {
    struct wsm_input_method_relay *relay = wl_container_of(listener, relay,
                                                           input_method_grab_keyboard);
    struct wlr_input_method_keyboard_grab_v2 *keyboard_grab = data;

    struct wlr_keyboard *active_keyboard = wlr_seat_get_keyboard(relay->seat->wlr_seat);
    wlr_input_method_keyboard_grab_v2_set_keyboard(keyboard_grab,
                                                   active_keyboard);

    wl_signal_add(&keyboard_grab->events.destroy,
                  &relay->input_method_keyboard_grab_destroy);
    relay->input_method_keyboard_grab_destroy.notify =
        handle_im_keyboard_grab_destroy;
}

static struct wsm_text_input *relay_get_focusable_text_input(
    struct wsm_input_method_relay *relay) {
    struct wsm_text_input *text_input = NULL;
    wl_list_for_each(text_input, &relay->text_inputs, link) {
        if (text_input->pending_focused_surface) {
            return text_input;
        }
    }
    return NULL;
}

static void relay_handle_input_method(struct wl_listener *listener,
                                      void *data) {
    struct wsm_input_method_relay *relay = wl_container_of(listener, relay, input_method_new);
    struct wlr_input_method_v2 *input_method = data;
    if (relay->seat->wlr_seat != input_method->seat) {
        wsm_log(WSM_DEBUG, "not same wlr_seat!");
        return;
    }

    if (relay->input_method != NULL) {
        wsm_log(WSM_INFO, "Attempted to connect second input method to a seat");
        wlr_input_method_v2_send_unavailable(input_method);
        return;
    }

    relay->input_method = input_method;

    relay->input_method_commit.notify = handle_im_commit;
    wl_signal_add(&relay->input_method->events.commit,
                  &relay->input_method_commit);
    relay->input_method_grab_keyboard.notify = handle_im_grab_keyboard;
    wl_signal_add(&relay->input_method->events.grab_keyboard,
                  &relay->input_method_grab_keyboard);
    relay->input_method_destroy.notify = handle_im_destroy;
    wl_signal_add(&relay->input_method->events.destroy,
                  &relay->input_method_destroy);

    struct wsm_text_input *text_input = relay_get_focusable_text_input(relay);
    if (text_input) {
        wlr_text_input_v3_send_enter(text_input->input,
                                     text_input->pending_focused_surface);
        text_input_set_pending_focused_surface(text_input, NULL);
    }
}

struct wsm_input_method_relay *wsm_input_method_relay_create(struct wsm_seat *seat) {
    struct wsm_input_method_relay *relay = calloc(1, sizeof(struct wsm_input_method_relay));
    if (!wsm_assert(relay, "Could not create wsm_input_method_relay: allocation failed!")) {
        return NULL;
    }

    relay->seat = seat;
    wl_list_init(&relay->text_inputs);

    relay->wlr_input_method_manager = wlr_input_method_manager_v2_create(global_server.wl_display);
    relay->wlr_text_input_manager = wlr_text_input_manager_v3_create(global_server.wl_display);

    relay->text_input_new.notify = relay_handle_text_input;
    wl_signal_add(&relay->wlr_text_input_manager->events.text_input,
                  &relay->text_input_new);

    relay->input_method_new.notify = relay_handle_input_method;
    wl_signal_add(
        &relay->wlr_input_method_manager->events.input_method,
        &relay->input_method_new);
    return relay;
}

void wsm_input_method_relay_destroy(struct wsm_input_method_relay *relay) {
    struct wsm_text_input *text_input = NULL;
    wl_list_for_each(text_input, &relay->text_inputs, link) {
        wsm_text_input_destroy(text_input);
    }

    wl_list_remove(&relay->text_inputs);

    wl_list_remove(&relay->text_input_new.link);
    wl_list_remove(&relay->input_method_new.link);
    wl_list_remove(&relay->input_method_commit.link);
    wl_list_remove(&relay->input_method_grab_keyboard.link);
    wl_list_remove(&relay->input_method_destroy.link);
    wl_list_remove(&relay->input_method_keyboard_grab_destroy.link);

    free(relay);
}

static void relay_send_im_state(struct wsm_input_method_relay *relay,
                                struct wlr_text_input_v3 *input) {
    struct wlr_input_method_v2 *input_method = relay->input_method;
    if (!input_method) {
        wsm_log(WSM_INFO, "Sending IM_DONE but im is gone");
        return;
    }
    if (input->active_features & WLR_TEXT_INPUT_V3_FEATURE_SURROUNDING_TEXT) {
        wlr_input_method_v2_send_surrounding_text(input_method,
                                                  input->current.surrounding.text, input->current.surrounding.cursor,
                                                  input->current.surrounding.anchor);
    }
    wlr_input_method_v2_send_text_change_cause(input_method,
                                               input->current.text_change_cause);
    if (input->active_features & WLR_TEXT_INPUT_V3_FEATURE_CONTENT_TYPE) {
        wlr_input_method_v2_send_content_type(input_method,
                                              input->current.content_type.hint,
                                              input->current.content_type.purpose);
    }
    wlr_input_method_v2_send_done(input_method);
}

static void handle_text_input_enable(struct wl_listener *listener, void *data) {
    struct wsm_text_input *text_input = wl_container_of(listener, text_input,
                                                         text_input_enable);
    if (text_input->relay->input_method == NULL) {
        wsm_log(WSM_INFO, "Enabling text input when input method is gone");
        return;
    }
    wlr_input_method_v2_send_activate(text_input->relay->input_method);
    relay_send_im_state(text_input->relay, text_input->input);
}

static void handle_text_input_commit(struct wl_listener *listener,
                                     void *data) {
    struct wsm_text_input *text_input = wl_container_of(listener, text_input,
                                                         text_input_commit);
    if (!text_input->input->current_enabled) {
        wsm_log(WSM_INFO, "Inactive text input tried to commit an update");
        return;
    }
    wsm_log(WSM_INFO, "Text input committed update");
    if (text_input->relay->input_method == NULL) {
        wsm_log(WSM_INFO, "Text input committed, but input method is gone");
        return;
    }
    relay_send_im_state(text_input->relay, text_input->input);
}

static void relay_disable_text_input(struct wsm_input_method_relay *relay,
                                     struct wsm_text_input *text_input) {
    if (relay->input_method == NULL) {
        wsm_log(WSM_DEBUG, "Disabling text input, but input method is gone");
        return;
    }
    wlr_input_method_v2_send_deactivate(relay->input_method);
    relay_send_im_state(relay, text_input->input);
}


static void handle_text_input_disable(struct wl_listener *listener,
                                      void *data) {
    struct wsm_text_input *text_input = wl_container_of(listener, text_input,
                                                         text_input_disable);
    if (text_input->input->focused_surface == NULL) {
        wsm_log(WSM_DEBUG, "Disabling text input, but no longer focused");
        return;
    }
    relay_disable_text_input(text_input->relay, text_input);
}

static void handle_text_input_destroy(struct wl_listener *listener,
                                      void *data) {
    struct wsm_text_input *text_input = wl_container_of(listener, text_input,
                                                         text_input_destroy);

    if (text_input->input->current_enabled) {
        relay_disable_text_input(text_input->relay, text_input);
    }
    text_input_set_pending_focused_surface(text_input, NULL);
    wsm_text_input_destroy(text_input);
}

static void handle_pending_focused_surface_destroy(struct wl_listener *listener,
                                                   void *data) {
    struct wsm_text_input *text_input = wl_container_of(listener, text_input,
                                                         pending_focused_surface_destroy);
    struct wlr_surface *surface = data;
    assert(text_input->pending_focused_surface == surface);
    text_input->pending_focused_surface = NULL;
    wl_list_remove(&text_input->pending_focused_surface_destroy.link);
    wl_list_init(&text_input->pending_focused_surface_destroy.link);
}

struct wsm_text_input *wsm_text_input_create(struct wsm_input_method_relay *relay
                                      , struct wlr_text_input_v3 *text_input) {
    struct wsm_text_input *input = calloc(1, sizeof(struct wsm_text_input));
    if (!wsm_assert(!input, "Could not create wsm_text_input: allocation failed!")) {
        return NULL;
    }
    input->input = text_input;
    input->relay = relay;

    wl_list_insert(&relay->text_inputs, &input->link);

    input->text_input_enable.notify = handle_text_input_enable;
    wl_signal_add(&text_input->events.enable, &input->text_input_enable);

    input->text_input_commit.notify = handle_text_input_commit;
    wl_signal_add(&text_input->events.commit, &input->text_input_commit);

    input->text_input_disable.notify = handle_text_input_disable;
    wl_signal_add(&text_input->events.disable, &input->text_input_disable);

    input->text_input_destroy.notify = handle_text_input_destroy;
    wl_signal_add(&text_input->events.destroy, &input->text_input_destroy);

    input->pending_focused_surface_destroy.notify =
        handle_pending_focused_surface_destroy;
    wl_list_init(&input->pending_focused_surface_destroy.link);
    return input;
}

void wsm_text_input_destroy(struct wsm_text_input *text_input) {
    if (!text_input) {
        wsm_log(WSM_ERROR, "wsm_text_input is NULL!");
        return;
    }

    wl_list_remove(&text_input->text_input_commit.link);
    wl_list_remove(&text_input->text_input_destroy.link);
    wl_list_remove(&text_input->text_input_disable.link);
    wl_list_remove(&text_input->text_input_enable.link);
    wl_list_remove(&text_input->link);
    free(text_input);
}
