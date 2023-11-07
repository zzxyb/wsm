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
#include "wsm_view.h"
#include "wsm_log.h"
#include "wsm_server.h"
#include "wsm_xdg_decoration.h"
#include "wsm_xdg_decoration_manager.h"

#include <assert.h>

#include <wlr/types/wlr_xdg_decoration_v1.h>

static void xdg_decoration_handle_destroy(struct wl_listener *listener,
                                          void *data) {
    struct wsm_xdg_decoration *deco =
        wl_container_of(listener, deco, destroy);
    assert(deco);
    assert(deco->view);

    wl_list_remove(&deco->destroy.link);
    wl_list_remove(&deco->request_mode.link);
    wl_list_remove(&deco->link);
    deco->view->xdg_decoration = NULL;
    free(deco);
}

static void xdg_decoration_handle_request_mode(struct wl_listener *listener,
                                               void *data) {
    struct wsm_xdg_decoration *xdg_deco =
        wl_container_of(listener, xdg_deco, request_mode);
    enum wlr_xdg_toplevel_decoration_v1_mode client_mode =
        xdg_deco->wlr_xdg_decoration->requested_mode;

    //TODO: support ssd

    client_mode = WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE;
    wlr_xdg_toplevel_decoration_v1_set_mode(xdg_deco->wlr_xdg_decoration,
                                            client_mode);
    // wsm_view_set_decorations(xdg_deco->view,
                         // client_mode == WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
}

void handle_xdg_decoration(struct wl_listener *listener, void *data) {
    struct wlr_xdg_toplevel_decoration_v1 *wlr_xdg_decoration = data;
    struct wlr_xdg_surface *xdg_surface = wlr_xdg_decoration->toplevel->base;
    if (!xdg_surface || !xdg_surface->data) {
        wsm_log(WSM_ERROR,
                "Invalid surface supplied for xdg decorations");
        return;
    }

    struct wsm_xdg_decoration *deco = calloc(1, sizeof(*deco));
    assert(deco);

    deco->view = (struct wsm_view *)xdg_surface->data;
    deco->view->xdg_decoration = deco;
    deco->wlr_xdg_decoration = wlr_xdg_decoration;

    deco->destroy.notify = xdg_decoration_handle_destroy;
    wl_signal_add(&wlr_xdg_decoration->events.destroy, &deco->destroy);

    deco->request_mode.notify = xdg_decoration_handle_request_mode;
    wl_signal_add(&wlr_xdg_decoration->events.request_mode, &deco->request_mode);

    wl_list_insert(&server.wsm_xdg_decoration_manager->xdg_decorations, &deco->link);
    xdg_decoration_handle_request_mode(&deco->request_mode, wlr_xdg_decoration);
}

struct wsm_xdg_decoration *xdg_decoration_from_surface(
    struct wlr_surface *surface) {
    struct wsm_xdg_decoration *deco;
    wl_list_for_each(deco, &server.wsm_xdg_decoration_manager->xdg_decorations, link) {
        if (deco->wlr_xdg_decoration->toplevel->base->surface == surface) {
            return deco;
        }
    }
    return NULL;
}
