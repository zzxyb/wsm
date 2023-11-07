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
#include "wsm_xwayland.h"

#ifdef HAVE_XWAYLAND
#include "wsm_server.h"
#include "wsm_log.h"
#include "wsm_view.h"
#include "wsm_output.h"
#include "wsm_workspace.h"
#include "wsm_output_manager.h"
#include "wsm_xwayland_unmanaged.h"

#include <stdlib.h>
#include <assert.h>

#include <xcb/xcb.h>

#include <wayland-util.h>

#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_xcursor_manager.h>
#include <wlr/types/wlr_primary_selection_v1.h>

// /**
//  * @brief atom_map mark the type of X11 window
//  */
// static const char *atom_map[ATOM_LAST] = {
//     [NET_WM_WINDOW_TYPE_NORMAL] = "_NET_WM_WINDOW_TYPE_NORMAL",
//     [NET_WM_WINDOW_TYPE_DIALOG] = "_NET_WM_WINDOW_TYPE_DIALOG",
//     [NET_WM_WINDOW_TYPE_UTILITY] = "_NET_WM_WINDOW_TYPE_UTILITY",
//     [NET_WM_WINDOW_TYPE_TOOLBAR] = "_NET_WM_WINDOW_TYPE_TOOLBAR",
//     [NET_WM_WINDOW_TYPE_SPLASH] = "_NET_WM_WINDOW_TYPE_SPLASH",
//     [NET_WM_WINDOW_TYPE_MENU] = "_NET_WM_WINDOW_TYPE_MENU",
//     [NET_WM_WINDOW_TYPE_DROPDOWN_MENU] = "_NET_WM_WINDOW_TYPE_DROPDOWN_MENU",
//     [NET_WM_WINDOW_TYPE_POPUP_MENU] = "_NET_WM_WINDOW_TYPE_POPUP_MENU",
//     [NET_WM_WINDOW_TYPE_TOOLTIP] = "_NET_WM_WINDOW_TYPE_TOOLTIP",
//     [NET_WM_WINDOW_TYPE_NOTIFICATION] = "_NET_WM_WINDOW_TYPE_NOTIFICATION",
//     [NET_WM_STATE_MODAL] = "_NET_WM_STATE_MODAL",
// };

// static void unmanaged_handle_map(struct wl_listener *listener, void *data) {

// }

// static void unmanaged_handle_unmap(struct wl_listener *listener, void *data) {

// }

// // static void unmanaged_handle_request_activate(struct wl_listener *listener, void *data) {
// //     // struct wlr_xwayland_surface *xsurface = data;
// //     // if (!xsurface->mapped) {
// //     //     return;
// //     // }
// // }

// // static void unmanaged_handle_destroy(struct wl_listener *listener, void *data) {
// //     struct wsm_xwayland_unmanaged *surface =
// //         wl_container_of(listener, surface, destroy);
// //     wl_list_remove(&surface->request_configure.link);
// //     wl_list_remove(&surface->map.link);
// //     wl_list_remove(&surface->unmap.link);
// //     wl_list_remove(&surface->destroy.link);
// //     wl_list_remove(&surface->override_redirect.link);
// //     wl_list_remove(&surface->request_activate.link);
// //     free(surface);
// // }

// // static void handle_map(struct wl_listener *listener, void *data) {

// // }

// // struct wsm_xwayland_view *wsm_xwayland_view_create(struct wlr_xwayland_surface *xsurface) {
// //     wsm_log(WSM_DEBUG, "New xwayland surface title='%s' class='%s'",
// //              xsurface->title, xsurface->class);

// //     struct wsm_xwayland_view *xwayland_surface =
// //         calloc(1, sizeof(struct wsm_xwayland_view));

// //     if (!wsm_assert(xwayland_surface, "Could not create xwayland_surface: allocation failed!")) {
// //         return NULL;
// //     }

// //     return xwayland_surface;
// // }

// // static void unmanaged_handle_override_redirect(struct wl_listener *listener, void *data) {
// //     struct wsm_xwayland_unmanaged *surface =
// //         wl_container_of(listener, surface, override_redirect);
// //     // struct wlr_xwayland_surface *xsurface = surface->wlr_xwayland_surface;

// //     // bool mapped = xsurface->mapped;
// //     // if (mapped) {
// //     //     unmanaged_handle_unmap(&surface->unmap, NULL);
// //     // }

// //     // unmanaged_handle_destroy(&surface->destroy, NULL);
// //     // xsurface->data = NULL;
// //     // struct wsm_xwayland_view *xwayland_surface = wsm_xwayland_view_create(xsurface);
// //     // if (mapped) {
// //     //     handle_map(&xwayland_surface->map, xsurface);
// //     // }
// // }

// // static void unmanaged_handle_associate(struct wl_listener *listener, void *data) {
// //     struct wsm_xwayland_unmanaged *surface =
// //         wl_container_of(listener, surface, associate);
// //     struct wlr_xwayland_surface *xsurface = surface->wlr_xwayland_surface;
// //     wl_signal_add(&xsurface->surface->events.map, &surface->map);
// //     surface->map.notify = unmanaged_handle_map;
// //     wl_signal_add(&xsurface->surface->events.unmap, &surface->unmap);
// //     surface->unmap.notify = unmanaged_handle_unmap;
// // }

// // static void unmanaged_handle_dissociate(struct wl_listener *listener, void *data) {
// //     struct wsm_xwayland_unmanaged *surface =
// //         wl_container_of(listener, surface, dissociate);
// //     wl_list_remove(&surface->map.link);
// //     wl_list_remove(&surface->unmap.link);
// // }

// // static void unmanaged_handle_destroy(struct wl_listener *listener, void *data) {
// //     struct wsm_xwayland_unmanaged *surface =
// //         wl_container_of(listener, surface, destroy);
// //     wl_list_remove(&surface->request_configure.link);
// //     wl_list_remove(&surface->destroy.link);
// //     wl_list_remove(&surface->override_redirect.link);
// //     wl_list_remove(&surface->request_activate.link);
// //     free(surface);
// // }

// void handle_xwayland_ready(struct wl_listener *listener, void *data) {
//     struct wsm_server *server =
//         wl_container_of(listener, server, xwayland_ready);
//     struct wsm_xwayland *xwayland = &server->xwayland;

//     xcb_connection_t *xcb_conn = xcb_connect(NULL, NULL);
//     int err = xcb_connection_has_error(xcb_conn);
//     if (err) {
//         wsm_log(WSM_ERROR, "XCB connect failed: %d", err);
//         return;
//     }

//     xcb_intern_atom_cookie_t cookies[ATOM_LAST];
//     for (size_t i = 0; i < ATOM_LAST; i++) {
//         cookies[i] =
//             xcb_intern_atom(xcb_conn, 0, strlen(atom_map[i]), atom_map[i]);
//     }
//     for (size_t i = 0; i < ATOM_LAST; i++) {
//         xcb_generic_error_t *error = NULL;
//         xcb_intern_atom_reply_t *reply =
//             xcb_intern_atom_reply(xcb_conn, cookies[i], &error);
//         if (reply != NULL && error == NULL) {
//             xwayland->atoms[i] = reply->atom;
//         }
//         free(reply);

//         if (error != NULL) {
//             wsm_log(WSM_ERROR, "could not resolve atom %s, X11 error code %d",
//                     atom_map[i], error->error_code);
//             free(error);
//             break;
//         }
//     }

//     xcb_disconnect(xcb_conn);
// }

void handle_new_xwayland_surface(struct wl_listener *listener, void *data) {
    struct wlr_xwayland_surface *xsurface = data;
    wlr_xwayland_surface_ping(xsurface);
    if (xsurface->override_redirect) {
        wsm_log(WSM_ERROR, "New xwayland unmanaged surface");
        wsm_xwayland_unmanaged_create(xsurface, false);
        return;
    }

    xwayland_view_create(xsurface, false);
}

/**
 * @brief xwayland_start start a xwayland server
 * @param server
 * @return true represents successed; false maybe failed to start Xwayland or wsm disabled Xwayland
 */
bool xwayland_start(struct wsm_server *server) {
#ifdef HAVE_XWAYLAND
    server->xwayland.wlr_xwayland = wlr_xwayland_create(server->wl_display, server->wlr_compositor, true);
    if (!server->xwayland.wlr_xwayland) {
        wsm_log(WSM_ERROR, "Failed to start Xwayland");
        unsetenv("DISPLAY");
        return false;
    }

    server->xwayland_surface.notify = handle_new_xwayland_surface;
    wl_signal_add(&server->xwayland.wlr_xwayland->events.new_surface,
                  &server->xwayland_surface);
    // server->xwayland_ready.notify = handle_xwayland_ready;
    // wl_signal_add(&server->xwayland.wlr_xwayland->events.ready,
    //               &server->xwayland_ready);
    setenv("DISPLAY", server->xwayland.wlr_xwayland->display_name, true);
#endif

    wlr_primary_selection_v1_device_manager_create(server->wl_display);
    return true;
}

static void
xwayland_view_configure(struct wsm_view *view, struct wlr_box geo)
{
    view->pending = geo;
    wlr_xwayland_surface_configure(xwayland_surface_from_view(view),
                                   geo.x, geo.y, geo.width, geo.height);

    bool is_offscreen = !wlr_box_empty(&view->current) &&
                        !wlr_output_layout_intersects(server.wsm_output_manager->wlr_output_layout, NULL,
                                                      &view->current);

    if (is_offscreen || (view->current.width == geo.width
                         && view->current.height == geo.height)) {
        view->current.x = geo.x;
        view->current.y = geo.y;
        // wsm_view_moved(view);
    }
}

static void
xwayland_wsm_view_close(struct wsm_view *view)
{
    wlr_xwayland_surface_close(xwayland_surface_from_view(view));
}

static const char *xwayland_wsm_view_get_string_prop(struct wsm_view *view,
                                                 enum wsm_view_prop prop) {
    struct wsm_xwayland_view *xwayland_view = xwayland_view_from_view(view);
    struct wlr_xwayland_surface *xwayland_surface = xwayland_view->xwayland_surface;
    if (!xwayland_surface) {
        return "";
    }

    switch (prop) {
    case VIEW_PROP_TITLE:
        return xwayland_surface->title;
    case VIEW_PROP_CLASS:
        return xwayland_surface->class;
    default:
        return "";
    }
}

static void
xwayland_wsm_view_map(struct wsm_view *view) {
    struct wlr_xwayland_surface *xwayland_surface = xwayland_surface_from_view(view);
    if (view->mapped) {
        return;
    }
    if (!xwayland_surface->surface) {
        wsm_log(WSM_DEBUG, "Cannot map view without wlr_surface");
        return;
    }
    view->mapped = true;
}

static const struct wsm_view_impl xwayland_view_impl = {
    .configure = xwayland_view_configure,
    .close = xwayland_wsm_view_close,
    .get_string_prop = xwayland_wsm_view_get_string_prop,
    .map = xwayland_wsm_view_map,
    // .set_activated = xwayland_wsm_view_set_activated,
    // .set_fullscreen = xwayland_wsm_view_set_fullscreen,
    // .unmap = xwayland_view_unmap,
    // .maximize = xwayland_wsm_view_maximize,
    // .minimize = xwayland_wsm_view_minimize,
    // .move_to_front = xwayland_wsm_view_move_to_front,
    // .move_to_back = xwayland_wsm_view_move_to_back,
    // .get_root = xwayland_wsm_view_get_root,
    // .append_children = xwayland_wsm_view_append_children,
    // .get_size_hints = xwayland_wsm_view_get_size_hints,
    // .wants_focus = xwayland_view_wants_focus,
    // .has_strut_partial = xwayland_wsm_view_has_strut_partial,
    // .contains_window_type = xwayland_view_contains_window_type,
    // .get_pid = xwayland_view_get_pid,
};

void xwayland_view_create(struct wlr_xwayland_surface *xsurface, bool mapped) {
    struct wsm_xwayland_view *xwayland_view =
        calloc(1, sizeof(struct wsm_xwayland_view));
    if (xwayland_view == NULL) {
        wsm_log(WSM_ERROR, "Could not create wsm_xwayland_view failed: allocation failed!");
        return;
    }
    struct wsm_view *view = &xwayland_view->base;

    view->type = WSM_VIEW_XWAYLAND;
    view->impl = &xwayland_view_impl;

    xwayland_view->xwayland_surface = xsurface;
    xsurface->data = view;

    struct wsm_workspace *ws = NULL;
    struct wsm_output *output = wsm_output_nearest_to(xsurface->x, xsurface->y);
    ws = wsm_output_get_active_workspace(output);
    view->workspace = ws;
    view->scene_tree = wlr_scene_tree_create(view->workspace->tree);
}

struct wsm_xwayland_view *xwayland_view_from_view(struct wsm_view *view) {
    return (struct wsm_xwayland_view *)view;
}

struct wlr_xwayland_surface *xwayland_surface_from_view(struct wsm_view *view) {
    struct wsm_xwayland_view *xwayland_view = xwayland_view_from_view(view);
    assert(xwayland_view->xwayland_surface);
    return xwayland_view->xwayland_surface;
}

#endif
