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
#include "wsm_server.h"
#include "wsm_output.h"
#include "wsm_output_manager.h"
#include "wsm_foreign_toplevel.h"

#include <assert.h>

#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_foreign_toplevel_management_v1.h>

static void
handle_request_minimize(struct wl_listener *listener, void *data)
{
    struct wsm_view *view = wl_container_of(listener, view, toplevel.minimize);
    struct wlr_foreign_toplevel_handle_v1_minimized_event *event = data;
    wsm_view_minimize(view, event->minimized);
}

static void
handle_request_maximize(struct wl_listener *listener, void *data)
{
    struct wsm_view *view = wl_container_of(listener, view, toplevel.maximize);
    struct wlr_foreign_toplevel_handle_v1_maximized_event *event = data;
    wsm_view_maximize(view, event->maximized ? VIEW_AXIS_BOTH : VIEW_AXIS_NONE, true);
}

static void
handle_request_fullscreen(struct wl_listener *listener, void *data)
{
    struct wsm_view *view = wl_container_of(listener, view, toplevel.fullscreen);
    struct wlr_foreign_toplevel_handle_v1_fullscreen_event *event = data;
    wsm_view_set_fullscreen(view, event->fullscreen);
}

static void
handle_request_activate(struct wl_listener *listener, void *data)
{
    struct wsm_view *view = wl_container_of(listener, view, toplevel.activate);
    wsm_view_set_activated(view, true);
}

static void
handle_request_close(struct wl_listener *listener, void *data)
{
    struct wsm_view *view = wl_container_of(listener, view, toplevel.close);
    wsm_view_close(view);
}

static void
handle_destroy(struct wl_listener *listener, void *data)
{
    struct wsm_view *view = wl_container_of(listener, view, toplevel.destroy);
    struct foreign_toplevel *toplevel = &view->toplevel;
    wl_list_remove(&toplevel->maximize.link);
    wl_list_remove(&toplevel->minimize.link);
    wl_list_remove(&toplevel->fullscreen.link);
    wl_list_remove(&toplevel->activate.link);
    wl_list_remove(&toplevel->close.link);
    wl_list_remove(&toplevel->destroy.link);
    toplevel->handle = NULL;
}

void foreign_toplevel_handle_create(struct wsm_view *view) {
    struct foreign_toplevel *toplevel = &view->toplevel;

    toplevel->handle = wlr_foreign_toplevel_handle_v1_create(server.foreign_toplevel_manager);
    if (!wsm_assert(toplevel->handle, "Could not create foreign toplevel handle for (%s)",
                    wsm_view_get_string_prop(view, VIEW_PROP_TITLE))) {
        return;
    }

    toplevel->maximize.notify = handle_request_maximize;
    wl_signal_add(&toplevel->handle->events.request_maximize,
                  &toplevel->maximize);

    toplevel->minimize.notify = handle_request_minimize;
    wl_signal_add(&toplevel->handle->events.request_minimize,
                  &toplevel->minimize);

    toplevel->fullscreen.notify = handle_request_fullscreen;
    wl_signal_add(&toplevel->handle->events.request_fullscreen,
                  &toplevel->fullscreen);

    toplevel->activate.notify = handle_request_activate;
    wl_signal_add(&toplevel->handle->events.request_activate,
                  &toplevel->activate);

    toplevel->close.notify = handle_request_close;
    wl_signal_add(&toplevel->handle->events.request_close,
                  &toplevel->close);

    toplevel->destroy.notify = handle_destroy;
    wl_signal_add(&toplevel->handle->events.destroy, &toplevel->destroy);
}

void foreign_toplevel_update_outputs(struct wsm_view *view) {
    assert(view->toplevel.handle);

    struct wsm_output *output;
    wl_list_for_each(output, &server.wsm_output_manager->outputs, link) {
        if (wsm_view_on_output(view, output)) {
            wlr_foreign_toplevel_handle_v1_output_enter(
                view->toplevel.handle, output->wlr_output);
        } else {
            wlr_foreign_toplevel_handle_v1_output_leave(
                view->toplevel.handle, output->wlr_output);
        }
    }
}
