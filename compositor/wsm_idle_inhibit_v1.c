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
#include "wsm_idle_inhibit_v1.h"

#include <stdlib.h>

#include <wlr/types/wlr_idle_notify_v1.h>
#include <wlr/types/wlr_idle_inhibit_v1.h>

static void handle_idle_inhibit_destroy(struct wl_listener *listener, void *data) {
    struct wsm_idle_inhibit_v1 *inhibitor =
        wl_container_of(listener, inhibitor, destroy);
    wl_list_remove(&inhibitor->link);
    wl_list_remove(&inhibitor->destroy.link);
    wsm_idle_inhibit_v1_check_active();
    free(inhibitor);
}

static void handle_new_idle_inhibit_v1(struct wl_listener *listener, void *data) {
    struct wlr_idle_inhibitor_v1 *wlr_idle_inhibit = data;
    struct wsm_idle_inhibit_manager_v1 *manager = wl_container_of(listener, manager, new_idle_inhibit_v1);

    struct wsm_idle_inhibit_v1 *idle_inhibit =
        calloc(1, sizeof(struct wsm_idle_inhibit_v1));
    if (!wsm_assert(idle_inhibit, "Could not create wsm_idle_inhibit_v1: allocation failed!")) {
        return;
    }

    idle_inhibit->wlr_inhibit_v1 = wlr_idle_inhibit;
    wl_list_insert(&manager->inhibits, &idle_inhibit->link);

    idle_inhibit->destroy.notify = handle_idle_inhibit_destroy;
    wl_signal_add(&wlr_idle_inhibit->events.destroy, &idle_inhibit->destroy);
}

struct wsm_idle_inhibit_manager_v1 *wsm_idle_inhibit_manager_v1_create() {
    struct wsm_idle_inhibit_manager_v1 *idle_inhibit_manager_v1 = calloc(1, sizeof(struct wsm_idle_inhibit_manager_v1));
    if (!wsm_assert(idle_inhibit_manager_v1, "Could not create wsm_idle_inhibit_manager_v1: allocation failed!")) {
        return NULL;
    }

    idle_inhibit_manager_v1->wlr_idle_notifier_v1 = wlr_idle_notifier_v1_create(server.wl_display);
    if (!wsm_assert(idle_inhibit_manager_v1->wlr_idle_notifier_v1, "could not allocate wlr_idle_notifier_v1")) {
        return NULL;
    }

    idle_inhibit_manager_v1->wlr_inhibit_manager_v1 = wlr_idle_inhibit_v1_create(server.wl_display);
    if (!wsm_assert(idle_inhibit_manager_v1->wlr_inhibit_manager_v1, "could not allocate wlr_inhibit_manager_v1")) {
        return NULL;
    }

    idle_inhibit_manager_v1->new_idle_inhibit_v1.notify = handle_new_idle_inhibit_v1;
    wl_signal_add(&idle_inhibit_manager_v1->wlr_inhibit_manager_v1->events.new_inhibitor,
                  &idle_inhibit_manager_v1->new_idle_inhibit_v1);
    wl_list_init(&idle_inhibit_manager_v1->inhibits);

    return idle_inhibit_manager_v1;
}

bool wsm_idle_inhibit_v1_is_active(struct wsm_idle_inhibit_v1 *inhibitor) {
    return false;
}

void wsm_idle_inhibit_v1_check_active() {
    struct wsm_idle_inhibit_manager_v1 *manager = server.wsm_idle_inhibit_manager_v1;
    struct wsm_idle_inhibit_v1 *inhibitor;
    bool inhibited = false;
    wl_list_for_each(inhibitor, &manager->inhibits, link) {
        if ((inhibited = wsm_idle_inhibit_v1_is_active(inhibitor))) {
            break;
        }
    }
    wlr_idle_notifier_v1_set_inhibited(manager->wlr_idle_notifier_v1, inhibited);
}
