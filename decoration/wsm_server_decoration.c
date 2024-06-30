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

#include "wsm_server.h"
#include "wsm_server_decoration_manager.h"
#include "wsm_server_decoration.h"
#include "wsm_log.h"
#include "wsm_view.h"
#include "wsm_container.h"
#include "wsm_transaction.h"

#include <stdlib.h>

#include <wlr/types/wlr_server_decoration.h>
#include <wlr/types/wlr_compositor.h>

static void server_decoration_handle_destroy(struct wl_listener *listener,
                                             void *data) {
    struct wsm_server_decoration *deco =
        wl_container_of(listener, deco, destroy);
    wl_list_remove(&deco->destroy.link);
    wl_list_remove(&deco->mode.link);
    wl_list_remove(&deco->link);
    free(deco);
}

static void server_decoration_handle_mode(struct wl_listener *listener,
                                          void *data) {
    struct wsm_server_decoration *deco =
        wl_container_of(listener, deco, mode);
    struct wsm_view *view =
        view_from_wlr_surface(deco->wlr_server_decoration->surface);
    if (view == NULL || view->surface != deco->wlr_server_decoration->surface) {
        return;
    }

    bool csd = deco->wlr_server_decoration->mode ==
               WLR_SERVER_DECORATION_MANAGER_MODE_CLIENT;
    view_update_csd_from_client(view, csd);

    arrange_container(view->container);
    transaction_commit_dirty();
}

struct wsm_server_decoration *decoration_from_surface(
    struct wlr_surface *surface) {
    struct wsm_server_decoration *deco;
    wl_list_for_each(deco, &global_server.wsm_server_decoration_manager->decorations, link) {
        if (deco->wlr_server_decoration->surface == surface) {
            return deco;
        }
    }
    return NULL;
}

void handle_server_decoration(struct wl_listener *listener, void *data) {
    struct wlr_server_decoration *wlr_deco = data;

    struct wsm_server_decoration *deco = calloc(1, sizeof(struct wsm_server_decoration));
    if (!wsm_assert(deco, "Could not create wsm_server_decoration: allocation failed!")) {
        return;
    }

    deco->wlr_server_decoration = wlr_deco;

    wl_signal_add(&wlr_deco->events.destroy, &deco->destroy);
    deco->destroy.notify = server_decoration_handle_destroy;

    wl_signal_add(&wlr_deco->events.mode, &deco->mode);
    deco->mode.notify = server_decoration_handle_mode;

    wl_list_insert(&global_server.wsm_server_decoration_manager->decorations, &deco->link);
}
