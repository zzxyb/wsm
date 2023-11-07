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

#include <stdlib.h>
#include <assert.h>

#include <wlr/xwayland/xwayland.h>
#include <wlr/types/wlr_scene.h>

static void
handle_request_configure(struct wl_listener *listener, void *data)
{
    struct wsm_xwayland_unmanaged *unmanaged =
        wl_container_of(listener, unmanaged, request_configure);
    struct wlr_xwayland_surface *xsurface = unmanaged->xwayland_surface;
    struct wlr_xwayland_surface_configure_event *ev = data;
    wlr_xwayland_surface_configure(xsurface, ev->x, ev->y, ev->width, ev->height);
    if (unmanaged->node) {
        wlr_scene_node_set_position(unmanaged->node, ev->x, ev->y);
        // cursor_update_focus(unmanaged->server);
    }
}

// static void
// handle_set_geometry(struct wl_listener *listener, void *data)
// {
//     struct xwayland_unmanaged *unmanaged =
//         wl_container_of(listener, unmanaged, set_geometry);
//     struct wlr_xwayland_surface *xsurface = unmanaged->xwayland_surface;
//     if (unmanaged->node) {
//         wlr_scene_node_set_position(unmanaged->node, xsurface->x, xsurface->y);
//         cursor_update_focus(unmanaged->server);
//     }
// }

// static void
// handle_map(struct wl_listener *listener, void *data)
// {
//     struct xwayland_unmanaged *unmanaged =
//         wl_container_of(listener, unmanaged, mappable.map);
//     struct wlr_xwayland_surface *xsurface = unmanaged->xwayland_surface;
//     assert(!unmanaged->node);

//     /* Stack new surface on top */
//     wlr_xwayland_surface_restack(xsurface, NULL, XCB_STACK_MODE_ABOVE);
//     wl_list_append(&unmanaged->server->unmanaged_surfaces, &unmanaged->link);

//     CONNECT_SIGNAL(xsurface, unmanaged, set_geometry);

//     if (wlr_xwayland_or_surface_wants_focus(xsurface)) {
//         seat_focus_surface(&unmanaged->server->seat, xsurface->surface);
//     }

//     /* node will be destroyed automatically once surface is destroyed */
//     unmanaged->node = &wlr_scene_surface_create(
//                            unmanaged->server->unmanaged_tree,
//                            xsurface->surface)->buffer->node;
//     wlr_scene_node_set_position(unmanaged->node, xsurface->x, xsurface->y);
//     cursor_update_focus(unmanaged->server);
// }

// static void
// focus_next_surface(struct server *server, struct wlr_xwayland_surface *xsurface)
// {
//     /* Try to focus on last created unmanaged xwayland surface */
//     struct xwayland_unmanaged *u;
//     struct wl_list *list = &server->unmanaged_surfaces;
//     wl_list_for_each_reverse(u, list, link) {
//         struct wlr_xwayland_surface *prev = u->xwayland_surface;
//         if (wlr_xwayland_or_surface_wants_focus(prev)) {
//             seat_focus_surface(&server->seat, prev->surface);
//             return;
//         }
//     }

//     /*
//      * If we don't find a surface to focus fall back
//      * to the topmost mapped view. This fixes dmenu
//      * not giving focus back when closed with ESC.
//      */
//     desktop_focus_topmost_view(server);
// }

// static void
// handle_unmap(struct wl_listener *listener, void *data)
// {
//     struct xwayland_unmanaged *unmanaged =
//         wl_container_of(listener, unmanaged, mappable.unmap);
//     struct wlr_xwayland_surface *xsurface = unmanaged->xwayland_surface;
//     struct seat *seat = &unmanaged->server->seat;
//     assert(unmanaged->node);

//     wl_list_remove(&unmanaged->link);
//     wl_list_remove(&unmanaged->set_geometry.link);
//     wlr_scene_node_set_enabled(unmanaged->node, false);

//     /*
//      * Mark the node as gone so a racing configure event
//      * won't try to reposition the node while unmapped.
//      */
//     unmanaged->node = NULL;
//     cursor_update_focus(unmanaged->server);

//     if (seat->seat->keyboard_state.focused_surface == xsurface->surface) {
//         focus_next_surface(unmanaged->server, xsurface);
//     }
// }

// static void
// handle_associate(struct wl_listener *listener, void *data)
// {
//     struct wsm_xwayland_unmanaged *unmanaged =
//         wl_container_of(listener, unmanaged, associate);
//     assert(unmanaged->xwayland_surface &&
//            unmanaged->xwayland_surface->surface);

//     // mappable_connect(&unmanaged->mappable,
//     //                  unmanaged->xwayland_surface->surface,
//     //                  handle_map, handle_unmap);
// }

// static void
// handle_dissociate(struct wl_listener *listener, void *data)
// {
//     struct xwayland_unmanaged *unmanaged =
//         wl_container_of(listener, unmanaged, dissociate);

//     if (!unmanaged->mappable.connected) {
//         wlr_log(WLR_ERROR, "dissociate received before associate");
//         return;
//     }
//     mappable_disconnect(&unmanaged->mappable);
// }

// static void
// handle_destroy(struct wl_listener *listener, void *data)
// {
//     struct xwayland_unmanaged *unmanaged =
//         wl_container_of(listener, unmanaged, destroy);

//     if (unmanaged->mappable.connected) {
//         mappable_disconnect(&unmanaged->mappable);
//     }

//     wl_list_remove(&unmanaged->associate.link);
//     wl_list_remove(&unmanaged->dissociate.link);
//     wl_list_remove(&unmanaged->request_configure.link);
//     wl_list_remove(&unmanaged->set_override_redirect.link);
//     wl_list_remove(&unmanaged->request_activate.link);
//     wl_list_remove(&unmanaged->destroy.link);
//     free(unmanaged);
// }

// static void
// handle_set_override_redirect(struct wl_listener *listener, void *data)
// {
//     wlr_log(WLR_DEBUG, "handle unmanaged override_redirect");
//     struct xwayland_unmanaged *unmanaged =
//         wl_container_of(listener, unmanaged, set_override_redirect);
//     struct wlr_xwayland_surface *xsurface = unmanaged->xwayland_surface;
//     struct server *server = unmanaged->server;

//     bool mapped = xsurface->surface && xsurface->surface->mapped;
//     if (mapped) {
//         handle_unmap(&unmanaged->mappable.unmap, NULL);
//     }
//     handle_destroy(&unmanaged->destroy, NULL);

//     xwayland_view_create(server, xsurface, mapped);
// }

// static void
// handle_request_activate(struct wl_listener *listener, void *data)
// {
//     wlr_log(WLR_DEBUG, "handle unmanaged request_activate");
//     struct xwayland_unmanaged *unmanaged =
//         wl_container_of(listener, unmanaged, request_activate);
//     struct wlr_xwayland_surface *xsurface = unmanaged->xwayland_surface;
//     if (!xsurface->surface || !xsurface->surface->mapped) {
//         return;
//     }
//     struct server *server = unmanaged->server;
//     struct seat *seat = &server->seat;

//     seat_focus_surface(seat, xsurface->surface);
// }

struct wsm_xwayland_unmanaged *wsm_xwayland_unmanaged_create(struct wlr_xwayland_surface *xsurface, bool mapped) {
    struct wsm_xwayland_unmanaged *unmanaged =
        calloc(1, sizeof(struct wsm_xwayland_unmanaged));
    if (unmanaged == NULL) {
        wsm_log(WSM_ERROR, "Could not create wsm_xwayland_unmanaged failed: allocation failed!");
        return NULL;
    }

    unmanaged->xwayland_surface = xsurface;

    unmanaged->request_configure.notify = handle_request_configure;
    wl_signal_add(&xsurface->events.request_configure,
                  &unmanaged->request_configure);
    // unmanaged->associate.notify = unmanaged_handle_associate;
    // wl_signal_add(&xsurface->events.associate, &unmanaged->associate);
    // unmanaged->dissociate.notify = unmanaged_handle_dissociate;
    // wl_signal_add(&xsurface->events.dissociate, &unmanaged->dissociate);
    // unmanaged->destroy.notify = unmanaged_handle_destroy;
    // wl_signal_add(&xsurface->events.destroy, &unmanaged->destroy);
    // unmanaged->override_redirect.notify = unmanaged_handle_override_redirect;
    // wl_signal_add(&xsurface->events.set_override_redirect, &unmanaged->override_redirect);
    // unmanaged->request_activate.notify = unmanaged_handle_request_activate;
    // wl_signal_add(&xsurface->events.request_activate, &unmanaged->request_activate);

    return unmanaged;
}

void destory_wsm_xwayland_unmanaged(struct wsm_xwayland_unmanaged *xwayland_unmanaged) {
    wl_list_remove(&xwayland_unmanaged->request_configure.link);
    wl_list_remove(&xwayland_unmanaged->destroy.link);
    wl_list_remove(&xwayland_unmanaged->override_redirect.link);
    wl_list_remove(&xwayland_unmanaged->request_activate.link);
    free(xwayland_unmanaged);
}

#endif
