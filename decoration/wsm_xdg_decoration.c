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

#include "wsm_view.h"
#include "wsm_log.h"
#include "wsm_server.h"
#include "wsm_container.h"
#include "wsm_transaction.h"
#include "wsm_xdg_decoration.h"
#include "wsm_xdg_decoration_manager.h"

#include <assert.h>
#include <stdlib.h>

#include <wlr/types/wlr_xdg_decoration_v1.h>

static void xdg_decoration_handle_destroy(struct wl_listener *listener,
                                          void *data) {
    struct wsm_xdg_decoration *deco =
        wl_container_of(listener, deco, destroy);
    if (deco->view) {
        deco->view->xdg_decoration = NULL;
    }
    wl_list_remove(&deco->destroy.link);
    wl_list_remove(&deco->request_mode.link);
    wl_list_remove(&deco->link);
    free(deco);
}

static void xdg_decoration_handle_request_mode(struct wl_listener *listener,
                                               void *data) {
    struct wsm_xdg_decoration *deco =
        wl_container_of(listener, deco, request_mode);
    set_xdg_decoration_mode(deco);
}

void handle_xdg_decoration(struct wl_listener *listener, void *data) {
    struct wlr_xdg_toplevel_decoration_v1 *wlr_deco = data;
    struct wsm_xdg_shell_view *xdg_shell_view = wlr_deco->toplevel->base->data;

    struct wsm_xdg_decoration *deco = calloc(1, sizeof(struct wsm_xdg_decoration));
    if (!wsm_assert(deco, "Could not create wsm_xdg_decoration: allocation failed!")) {
        return;
    }

    deco->view = &xdg_shell_view->view;
    deco->view->xdg_decoration = deco;
    deco->wlr_xdg_decoration = wlr_deco;

    wl_signal_add(&wlr_deco->events.destroy, &deco->destroy);
    deco->destroy.notify = xdg_decoration_handle_destroy;

    wl_signal_add(&wlr_deco->events.request_mode, &deco->request_mode);
    deco->request_mode.notify = xdg_decoration_handle_request_mode;

    wl_list_insert(&global_server.wsm_xdg_decoration_manager->xdg_decorations, &deco->link);

    set_xdg_decoration_mode(deco);
}

struct wsm_xdg_decoration *xdg_decoration_from_surface(
    struct wlr_surface *surface) {
    struct wsm_xdg_decoration *deco;
    wl_list_for_each(deco, &global_server.wsm_xdg_decoration_manager->xdg_decorations, link) {
        if (deco->wlr_xdg_decoration->toplevel->base->surface == surface) {
            return deco;
        }
    }
    return NULL;
}

void set_xdg_decoration_mode(struct wsm_xdg_decoration *deco) {
    struct wsm_view *view = deco->view;
    enum wlr_xdg_toplevel_decoration_v1_mode mode =
        WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE;
    enum wlr_xdg_toplevel_decoration_v1_mode client_mode =
        deco->wlr_xdg_decoration->requested_mode;

    bool floating;
    if (view->container) {
        floating = container_is_floating(view->container);
        bool csd = false;
        csd = client_mode ==
              WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE;
        view_update_csd_from_client(view, csd);
        arrange_container(view->container);
        transaction_commit_dirty();
    } else {
        floating = view->impl->wants_floating &&
                   view->impl->wants_floating(view);
    }

    if (floating && client_mode) {
        mode = client_mode;
    }

    if (view->wlr_xdg_toplevel->base->initialized) {
        wlr_xdg_toplevel_decoration_v1_set_mode(deco->wlr_xdg_decoration, mode);
    }
}
