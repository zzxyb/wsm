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
#include "wsm_xwayland_unmanaged.h"

#ifdef HAVE_XWAYLAND
#include "wsm_log.h"
#include "wsm_server.h"
#include "wsm_scene.h"
#include "wsm_seat.h"
#include "wsm_view.h"
#include "wsm_input_manager.h"

#include <stdlib.h>
#include <assert.h>

#include <wlr/types/wlr_compositor.h>
#include <wlr/xwayland/xwayland.h>
#include <wlr/types/wlr_scene.h>

static void
unmanaged_handle_request_configure(struct wl_listener *listener, void *data)
{
    struct wsm_xwayland_unmanaged *unmanaged =
        wl_container_of(listener, unmanaged, request_configure);
    struct wlr_xwayland_surface *xsurface = unmanaged->wlr_xwayland_surface;
    struct wlr_xwayland_surface_configure_event *ev = data;
    wlr_xwayland_surface_configure(xsurface, ev->x, ev->y, ev->width, ev->height);
    if (unmanaged->surface_scene) {
        wlr_scene_node_set_position(unmanaged->surface_scene, ev->x, ev->y);
        // cursor_update_focus(unmanaged->server);
    }
}

static void
unmanaged_handle_set_geometry(struct wl_listener *listener, void *data)
{
    struct wsm_xwayland_unmanaged *unmanaged =
        wl_container_of(listener, unmanaged, set_geometry);
    struct wlr_xwayland_surface *xsurface = unmanaged->wlr_xwayland_surface;
    if (unmanaged->surface_scene) {
        wlr_scene_node_set_position(unmanaged->surface_scene, xsurface->x, xsurface->y);
        // cursor_update_focus(unmanaged->server);
    }
}

static void
unmanaged_handle_map(struct wl_listener *listener, void *data)
{
    struct wsm_xwayland_unmanaged *unmanaged =
        wl_container_of(listener, unmanaged, map);
    struct wlr_xwayland_surface *xsurface = unmanaged->wlr_xwayland_surface;
    assert(!unmanaged->surface_scene);

    unmanaged->surface_scene = &wlr_scene_surface_create(
                           global_server.wsm_scene->unmanaged_tree,
                           xsurface->surface)->buffer->node;
    if (unmanaged->surface_scene) {
        wlr_scene_node_set_position(unmanaged->surface_scene, xsurface->x, xsurface->y);

        wl_signal_add(&xsurface->events.set_geometry, &unmanaged->set_geometry);
        unmanaged->set_geometry.notify = unmanaged_handle_set_geometry;
    }

    if (wlr_xwayland_or_surface_wants_focus(xsurface)) {
        struct wsm_seat *seat = input_manager_current_seat();
        struct wlr_xwayland *xwayland = global_server.xwayland.wlr_xwayland;
        wlr_xwayland_set_seat(xwayland, seat->wlr_seat);
        seat_set_focus_surface(seat, xsurface->surface, false);
    }
}

static void
focus_next_surface(struct wlr_xwayland_surface *xsurface)
{

}

static void
unmanaged_handle_unmap(struct wl_listener *listener, void *data)
{
    struct wsm_xwayland_unmanaged *unmanaged =
        wl_container_of(listener, unmanaged, unmap);
    struct wlr_xwayland_surface *xsurface = unmanaged->wlr_xwayland_surface;
    struct wsm_seat *seat = input_manager_current_seat();
    assert(unmanaged->surface_scene);

    wl_list_remove(&unmanaged->link);
    wl_list_remove(&unmanaged->set_geometry.link);
    wlr_scene_node_set_enabled(unmanaged->surface_scene, false);

    unmanaged->surface_scene = NULL;
    // cursor_update_focus(unmanaged->server);

    if (seat->wlr_seat->keyboard_state.focused_surface == xsurface->surface) {
        focus_next_surface(xsurface);
    }
}

static void
unmanaged_handle_associate(struct wl_listener *listener, void *data)
{
    struct wsm_xwayland_unmanaged *unmanaged =
        wl_container_of(listener, unmanaged, associate);
    assert(unmanaged->wlr_xwayland_surface &&
           unmanaged->wlr_xwayland_surface->surface);

    struct wlr_xwayland_surface *xsurface = unmanaged->wlr_xwayland_surface;
    unmanaged->map.notify = unmanaged_handle_map;
    wl_signal_add(&xsurface->surface->events.map, &unmanaged->map);
        unmanaged->unmap.notify = unmanaged_handle_unmap;
    wl_signal_add(&xsurface->surface->events.unmap, &unmanaged->unmap);
}

static void
unmanaged_handle_dissociate(struct wl_listener *listener, void *data)
{
    struct wsm_xwayland_unmanaged *surface =
        wl_container_of(listener, surface, dissociate);
    wl_list_remove(&surface->map.link);
    wl_list_remove(&surface->unmap.link);
}

static void
unmanaged_handle_destroy(struct wl_listener *listener, void *data)
{
    struct wsm_xwayland_unmanaged *surface =
        wl_container_of(listener, surface, destroy);
    wl_list_remove(&surface->request_configure.link);
    wl_list_remove(&surface->associate.link);
    wl_list_remove(&surface->dissociate.link);
    wl_list_remove(&surface->destroy.link);
    wl_list_remove(&surface->override_redirect.link);
    wl_list_remove(&surface->request_activate.link);
    free(surface);
}

static void
unmanaged_handle_override_redirect(struct wl_listener *listener, void *data)
{
    struct wsm_xwayland_unmanaged *surface =
        wl_container_of(listener, surface, override_redirect);
    struct wlr_xwayland_surface *xsurface = surface->wlr_xwayland_surface;

    bool associated = xsurface->surface != NULL;
    bool mapped = associated && xsurface->surface->mapped;
    if (mapped) {
        unmanaged_handle_unmap(&surface->unmap, NULL);
    }
    if (associated) {
        unmanaged_handle_dissociate(&surface->dissociate, NULL);
    }

    unmanaged_handle_destroy(&surface->destroy, NULL);
    xsurface->data = NULL;

    struct wsm_xwayland_view *xwayland_view = xwayland_view_create(xsurface);
    if (associated) {
        xsurface_associate(&xwayland_view->associate, NULL);
    }
    if (mapped) {
        xsurface_map(&xwayland_view->map, xsurface);
    }
}

static void
handle_request_activate(struct wl_listener *listener, void *data) {
    struct wsm_xwayland_unmanaged *surface =
        wl_container_of(listener, surface, request_activate);
    struct wlr_xwayland_surface *xsurface = surface->wlr_xwayland_surface;
    if (xsurface->surface == NULL || !xsurface->surface->mapped) {
        return;
    }
    struct wsm_seat *seat = input_manager_current_seat();
    // struct wsm_container *focus = seat_get_focused_container(seat);
    // if (focus && focus->view && focus->view->pid != xsurface->pid) {
    //     return;
    // }

    seat_set_focus_surface(seat, xsurface->surface, false);
}

struct wsm_xwayland_unmanaged *wsm_xwayland_unmanaged_create(struct wlr_xwayland_surface *xsurface) {
    struct wsm_xwayland_unmanaged *unmanaged =
        calloc(1, sizeof(struct wsm_xwayland_unmanaged));
    if (unmanaged == NULL) {
        wsm_log(WSM_ERROR, "Could not create wsm_xwayland_unmanaged failed: allocation failed!");
        return NULL;
    }

    unmanaged->wlr_xwayland_surface = xsurface;

    unmanaged->associate.notify = unmanaged_handle_associate;
    wl_signal_add(&xsurface->events.associate, &unmanaged->associate);
    unmanaged->dissociate.notify = unmanaged_handle_dissociate;
    wl_signal_add(&xsurface->events.dissociate, &unmanaged->dissociate);
    unmanaged->destroy.notify = unmanaged_handle_destroy;
    wl_signal_add(&xsurface->events.destroy, &unmanaged->destroy);
    unmanaged->override_redirect.notify = unmanaged_handle_override_redirect;
    wl_signal_add(&xsurface->events.set_override_redirect, &unmanaged->override_redirect);
    unmanaged->request_activate.notify = handle_request_activate;
    wl_signal_add(&xsurface->events.request_activate, &unmanaged->request_activate);
    unmanaged->request_configure.notify = unmanaged_handle_request_configure;
    wl_signal_add(&xsurface->events.request_configure, &unmanaged->request_configure);

    return unmanaged;
}

void destory_wsm_xwayland_unmanaged(struct wsm_xwayland_unmanaged *xwayland_unmanaged) {
    wl_list_remove(&xwayland_unmanaged->request_configure.link);
    wl_list_remove(&xwayland_unmanaged->destroy.link);
    wl_list_remove(&xwayland_unmanaged->override_redirect.link);
    wl_list_remove(&xwayland_unmanaged->request_activate.link);
    free(xwayland_unmanaged);
}

void unmanaged_xsurface_associate(struct wl_listener *listener, void *data) {
    unmanaged_handle_associate(listener, data);
}
void unmanaged_xsurface_map(struct wl_listener *listener, void *data) {
    unmanaged_handle_map(listener, data);
}

#endif
