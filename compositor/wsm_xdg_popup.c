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
#include "wsm_scene.h"
#include "wsm_output.h"
#include "wsm_server.h"
#include "wsm_xdg_popup.h"

#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_xdg_shell.h>

static void
popup_unconstrain(struct wsm_xdg_popup *popup)
{
    struct wsm_view *view = popup->parent_view;
    struct wlr_box *popup_box = &popup->wlr_popup->current.geometry;
    struct wsm_output *output = wsm_output_nearest_to(view->current.x + popup_box->x,
                                                  view->current.y + popup_box->y);
    struct wlr_box usable = wsm_output_usable_area_in_layout_coords(output);

    struct wlr_box output_toplevel_box = {
        .x = usable.x - view->current.x,
        .y = usable.y - view->current.y,
        .width = usable.width,
        .height = usable.height,
    };
    wlr_xdg_popup_unconstrain_from_box(popup->wlr_popup, &output_toplevel_box);
}

static void
handle_xdg_popup_commit(struct wl_listener *listener, void *data)
{
    struct wsm_xdg_popup *popup = wl_container_of(listener, popup, commit);

    if (popup->wlr_popup->base->initial_commit) {
        popup_unconstrain(popup);

        wl_list_remove(&popup->commit.link);
        popup->commit.notify = NULL;
    }
}

static void
handle_xdg_popup_destroy(struct wl_listener *listener, void *data)
{
    struct wsm_xdg_popup *popup = wl_container_of(listener, popup, destroy);
    if (!popup) {
        wsm_log(WSM_ERROR, "wsm_xdg_popup is NULL!");
        return;
    }

    wl_list_remove(&popup->destroy.link);
    wl_list_remove(&popup->new_popup.link);

    /* Usually already removed unless there was no commit at all */
    if (popup->commit.notify) {
        wl_list_remove(&popup->commit.link);
    }
    free(popup);
}

static void
popup_handle_new_xdg_popup(struct wl_listener *listener, void *data)
{
    struct wsm_xdg_popup *popup = wl_container_of(listener, popup, new_popup);
    struct wlr_xdg_popup *wlr_popup = data;
    xdg_popup_create(popup->parent_view, wlr_popup);
}

struct wsm_xdg_popup *
xdg_popup_create(struct wsm_view *view, struct wlr_xdg_popup *wlr_popup) {
    struct wlr_xdg_surface *parent =
        wlr_xdg_surface_try_from_wlr_surface(wlr_popup->parent);
    if (!parent) {
        wsm_log(WSM_ERROR, "parent is not a valid XDG surface");
        return NULL;
    }

    struct wsm_xdg_popup *xdg_popup = calloc(1, sizeof(struct wsm_xdg_popup));
    if (!wsm_assert(xdg_popup, "Could not create wsm_xdg_popup: allocation failed!")) {
        return NULL;
    }

    xdg_popup->parent_view = view;
    xdg_popup->wlr_popup = wlr_popup;

    xdg_popup->commit.notify = handle_xdg_popup_commit;
    wl_signal_add(&wlr_popup->base->surface->events.commit, &xdg_popup->commit);

    xdg_popup->destroy.notify = handle_xdg_popup_destroy;
    wl_signal_add(&wlr_popup->base->events.destroy, &xdg_popup->destroy);

    xdg_popup->new_popup.notify = popup_handle_new_xdg_popup;
    wl_signal_add(&wlr_popup->base->events.new_popup, &xdg_popup->new_popup);

    struct wlr_scene_tree *parent_tree = NULL;
    if (parent->role == WLR_XDG_SURFACE_ROLE_POPUP) {
        parent_tree = parent->surface->data;
    } else {
        parent_tree = server.wsm_scene->xdg_popup_tree;
        wlr_scene_node_set_position(&server.wsm_scene->xdg_popup_tree->node,
                                    view->current.x, view->current.y);
    }
    wlr_popup->base->surface->data =
        wlr_scene_xdg_surface_create(parent_tree, wlr_popup->base);

    return xdg_popup;
}
