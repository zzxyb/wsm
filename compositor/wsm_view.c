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
#include "wsm_view.h"
#include "wsm_output.h"
#include "wsm_server.h"
#include "wsm_container.h"
#include "wsm_workspace.h"

#include <stdlib.h>
#include <assert.h>

#include <wayland-server.h>

#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/types/wlr_fractional_scale_v1.h>
#include <wlr/types/wlr_foreign_toplevel_management_v1.h>

static void view_populate_pid(struct wsm_view *view) {
    pid_t pid;
#ifdef HAVE_XWAYLAND
    if (global_server.xwayland_enabled) {
        if (view->type == WSM_VIEW_XWAYLAND) {
            struct wlr_xwayland_surface *surf =
                wlr_xwayland_surface_try_from_wlr_surface(view->surface);
            pid = surf->pid;
            view->pid = pid;
            return;
        }
    }
#endif
    struct wl_client *client =
        wl_resource_get_client(view->surface->resource);
    wl_client_get_credentials(client, &pid, NULL, NULL);
    view->pid = pid;
}

void wsm_view_init(struct wsm_view *view, enum wsm_view_type type,
               const struct wsm_view_impl *impl) {
    view->type = type;
    view->impl = impl;
    wl_signal_init(&view->events.unmap);
}

void wsm_view_destroy(struct wsm_view *view) {
    if (!view) {
        wsm_log(WSM_ERROR, "wsm_list is NULL!");
        return;
    }

    if (!wsm_assert(view->surface == NULL, "Tried to free mapped view")) {
        return;
    }

    if (!wsm_assert(view->destroying,
                     "Tried to free view which wasn't marked as destroying")) {
        return;
    }

    if (!wsm_assert(view->container == NULL,
                     "Tried to free view which still has a container "
                     "(might have a pending transaction?)")) {
        return;
    }

    wl_list_remove(&view->events.unmap.listener_list);

    free(view->title_format);

    if (view->impl->destroy) {
        view->impl->destroy(view);
    } else {
        free(view);
    }
}

void wsm_view_map(struct wsm_view *view, struct wlr_surface *wlr_surface,
              bool fullscreen, struct wlr_output *fullscreen_output,
              bool decoration) {
    if (!wsm_assert(view->surface == NULL, "cannot map mapped view")) {
        return;
    }
    view->surface = wlr_surface;
    view_populate_pid(view);

    if (WSM_VIEW_XDG_SHELL == view->type) {
        struct wlr_xdg_surface *xdg_surface = wlr_xdg_surface_try_from_wlr_surface(wlr_surface);

        view->scene_tree = wlr_scene_xdg_surface_create(
            view->workspace->tree, xdg_surface);
        if (!view->scene_tree) {
            wl_resource_post_no_memory(xdg_surface->resource);
            return;
        }
        view->scene_tree->node.data = view;
        xdg_surface->data = view->scene_tree;
    }
}

void
wsm_view_update_title(struct wsm_view *view)
{
    assert(view);
    const char *title = wsm_view_get_string_prop(view, VIEW_PROP_TITLE);
    if (!view->toplevel.handle || !title) {
        return;
    }
    wlr_foreign_toplevel_handle_v1_set_title(view->toplevel.handle, title);
}

void
wsm_view_update_app_id(struct wsm_view *view)
{
    assert(view);
    const char *app_id = wsm_view_get_string_prop(view, VIEW_PROP_APP_ID);
    if (!view->toplevel.handle || !app_id) {
        return;
    }
    wlr_foreign_toplevel_handle_v1_set_app_id(
        view->toplevel.handle, app_id);
}

void surface_update_outputs(struct wlr_surface *surface) {
    float scale = 1;
    struct wlr_surface_output *surface_output;
    wl_list_for_each(surface_output, &surface->current_outputs, link) {
        if (surface_output->output->scale > scale) {
            scale = surface_output->output->scale;
        }
    }
    wlr_fractional_scale_v1_notify_scale(surface, scale);
    wlr_surface_set_preferred_buffer_scale(surface, (int32_t)(scale));
}

void surface_enter_output(struct wlr_surface *surface,
                          struct wsm_output *output) {
    wlr_surface_send_enter(surface, output->wlr_output);
    surface_update_outputs(surface);
}

void surface_leave_output(struct wlr_surface *surface,
                          struct wsm_output *output) {
    wlr_surface_send_leave(surface, output->wlr_output);
    surface_update_outputs(surface);
}

const char *
wsm_view_get_string_prop(struct wsm_view *view, enum wsm_view_prop prop)
{
    assert(view);
    assert(prop);
    if (view->impl->get_string_prop) {
        return view->impl->get_string_prop(view, prop);
    }
    return "";
}

void
wsm_view_set_decorations(struct wsm_view *view, bool decorations)
{
    assert(view);

    if (!view->fullscreen) {
        // if (decorations) {
        //     decorate(view);
        // } else {
        //     undecorate(view);
        // }
    }
}

bool
wsm_view_is_tiled(struct wsm_view *view)
{
    assert(view);
    return (view->tiled || view->tiled_region
            || view->tiled_region_evacuate);
}

bool
wsm_view_is_floating(struct wsm_view *view)
{
    assert(view);
    return !(view->fullscreen || (view->maximized != VIEW_AXIS_NONE)
             || wsm_view_is_tiled(view));
}

void
wsm_view_set_output(struct wsm_view *view, struct wsm_output *output)
{
    assert(view);
    assert(!view->fullscreen);
    if (!wsm_output_is_usable(output)) {
        _wsm_log(WSM_ERROR, "invalid wsm_output set for wsm_view");
        return;
    }
    view->output = output;
}

