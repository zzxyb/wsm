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
#include "wsm_input_manager.h"
#include "wsm_xwayland_unmanaged.h"

#include <stdlib.h>
#include <assert.h>

#include <xcb/xcb.h>

#include <wayland-util.h>

#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_xcursor_manager.h>
#include <wlr/types/wlr_primary_selection_v1.h>

/**
 * @brief atom_map mark the type of X11 window
 */
static const char *atom_map[ATOM_LAST] = {
    [NET_WM_WINDOW_TYPE_NORMAL] = "_NET_WM_WINDOW_TYPE_NORMAL",
    [NET_WM_WINDOW_TYPE_DIALOG] = "_NET_WM_WINDOW_TYPE_DIALOG",
    [NET_WM_WINDOW_TYPE_UTILITY] = "_NET_WM_WINDOW_TYPE_UTILITY",
    [NET_WM_WINDOW_TYPE_TOOLBAR] = "_NET_WM_WINDOW_TYPE_TOOLBAR",
    [NET_WM_WINDOW_TYPE_SPLASH] = "_NET_WM_WINDOW_TYPE_SPLASH",
    [NET_WM_WINDOW_TYPE_MENU] = "_NET_WM_WINDOW_TYPE_MENU",
    [NET_WM_WINDOW_TYPE_DROPDOWN_MENU] = "_NET_WM_WINDOW_TYPE_DROPDOWN_MENU",
    [NET_WM_WINDOW_TYPE_POPUP_MENU] = "_NET_WM_WINDOW_TYPE_POPUP_MENU",
    [NET_WM_WINDOW_TYPE_TOOLTIP] = "_NET_WM_WINDOW_TYPE_TOOLTIP",
    [NET_WM_WINDOW_TYPE_NOTIFICATION] = "_NET_WM_WINDOW_TYPE_NOTIFICATION",
    [NET_WM_STATE_MODAL] = "_NET_WM_STATE_MODAL",
};

void handle_xwayland_ready(struct wl_listener *listener, void *data) {
    struct wsm_server *server =
        wl_container_of(listener, server, xwayland_ready);
    struct wsm_xwayland *xwayland = &server->xwayland;

    xcb_connection_t *xcb_conn = xcb_connect(NULL, NULL);
    int err = xcb_connection_has_error(xcb_conn);
    if (err) {
        wsm_log(WSM_ERROR, "XCB connect failed: %d", err);
        return;
    }

    xcb_intern_atom_cookie_t cookies[ATOM_LAST];
    for (size_t i = 0; i < ATOM_LAST; i++) {
        cookies[i] =
            xcb_intern_atom(xcb_conn, 0, strlen(atom_map[i]), atom_map[i]);
    }
    for (size_t i = 0; i < ATOM_LAST; i++) {
        xcb_generic_error_t *error = NULL;
        xcb_intern_atom_reply_t *reply =
            xcb_intern_atom_reply(xcb_conn, cookies[i], &error);
        if (reply != NULL && error == NULL) {
            xwayland->atoms[i] = reply->atom;
        }
        free(reply);

        if (error != NULL) {
            wsm_log(WSM_ERROR, "could not resolve atom %s, X11 error code %d",
                    atom_map[i], error->error_code);
            free(error);
            break;
        }
    }

    xcb_disconnect(xcb_conn);
}

void handle_new_xwayland_surface(struct wl_listener *listener, void *data) {
    struct wlr_xwayland_surface *xsurface = data;
    wlr_xwayland_surface_ping(xsurface);
    if (xsurface->override_redirect) {
        wsm_log(WSM_DEBUG, "New xwayland unmanaged surface");
        wsm_xwayland_unmanaged_create(xsurface);
    } else {
        create_xwayland_view(xsurface);
    }
}

/**
 * @brief xwayland_start start a xwayland server
 * @param server
 * @return true represents successed; false maybe failed to start Xwayland or wsm disabled Xwayland
 */
bool xwayland_start(struct wsm_server *server) {
#ifdef HAVE_XWAYLAND
    if (global_server.xwayland_enabled) {
        server->xwayland.wlr_xwayland = wlr_xwayland_create(server->wl_display, server->wlr_compositor, true);
        if (!server->xwayland.wlr_xwayland) {
            wsm_log(WSM_ERROR, "Failed to start Xwayland");
            unsetenv("DISPLAY");
            return false;
        }

        server->xwayland_surface.notify = handle_new_xwayland_surface;
        wl_signal_add(&server->xwayland.wlr_xwayland->events.new_surface,
                      &server->xwayland_surface);
        server->xwayland_ready.notify = handle_xwayland_ready;
        wl_signal_add(&server->xwayland.wlr_xwayland->events.ready,
                      &server->xwayland_ready);
        setenv("DISPLAY", server->xwayland.wlr_xwayland->display_name, true);
    }
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
                        !wlr_output_layout_intersects(global_server.wsm_output_manager->wlr_output_layout, NULL,
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

static void handle_destroy(struct wl_listener *listener, void *data) {
    struct wsm_xwayland_view *xwayland_view =
        wl_container_of(listener, xwayland_view, destroy);
    struct wsm_view *view = &xwayland_view->view;

    if (view->surface) {
        // view_unmap(view);
        wl_list_remove(&xwayland_view->commit.link);
    }

    xwayland_view->view.wlr_xwayland_surface = NULL;

    wl_list_remove(&xwayland_view->destroy.link);
    wl_list_remove(&xwayland_view->request_configure.link);
    wl_list_remove(&xwayland_view->request_fullscreen.link);
    wl_list_remove(&xwayland_view->request_minimize.link);
    wl_list_remove(&xwayland_view->request_move.link);
    wl_list_remove(&xwayland_view->request_resize.link);
    wl_list_remove(&xwayland_view->request_activate.link);
    wl_list_remove(&xwayland_view->set_title.link);
    wl_list_remove(&xwayland_view->set_class.link);
    wl_list_remove(&xwayland_view->set_role.link);
    wl_list_remove(&xwayland_view->set_startup_id.link);
    wl_list_remove(&xwayland_view->set_window_type.link);
    wl_list_remove(&xwayland_view->set_hints.link);
    wl_list_remove(&xwayland_view->set_decorations.link);
    wl_list_remove(&xwayland_view->associate.link);
    wl_list_remove(&xwayland_view->dissociate.link);
    wl_list_remove(&xwayland_view->override_redirect.link);
    // view_begin_destroy(&xwayland_view->view);
}

static void handle_request_configure(struct wl_listener *listener, void *data) {
    struct wsm_xwayland_view *xwayland_view =
        wl_container_of(listener, xwayland_view, request_configure);
    struct wlr_xwayland_surface_configure_event *ev = data;
    struct wsm_view *view = &xwayland_view->view;
    struct wlr_xwayland_surface *xsurface = view->wlr_xwayland_surface;
    if (xsurface->surface == NULL || !xsurface->surface->mapped) {
        wlr_xwayland_surface_configure(xsurface, ev->x, ev->y,
                                       ev->width, ev->height);
        return;
    }
    // if (container_is_floating(view->container)) {
    //     // Respect minimum and maximum sizes
    //     view->natural_width = ev->width;
    //     view->natural_height = ev->height;
    //     container_floating_resize_and_center(view->container);

    //     configure(view, view->container->pending.content_x,
    //               view->container->pending.content_y,
    //               view->container->pending.content_width,
    //               view->container->pending.content_height);
    //     node_set_dirty(&view->container->node);
    // } else {
    //     configure(view, view->container->current.content_x,
    //               view->container->current.content_y,
    //               view->container->current.content_width,
    //               view->container->current.content_height);
    // }
}

static void handle_request_minimize(struct wl_listener *listener, void *data) {
    struct wsm_xwayland_view *xwayland_view =
        wl_container_of(listener, xwayland_view, request_minimize);
    struct wsm_view *view = &xwayland_view->view;
    struct wlr_xwayland_surface *xsurface = view->wlr_xwayland_surface;
    if (xsurface->surface == NULL || !xsurface->surface->mapped) {
        return;
    }

    // struct wlr_xwayland_minimize_event *e = data;
    // struct wsm_seat *seat = input_manager_current_seat();
    // bool focused = seat_get_focus(seat) == &view->container->node;
    // wlr_xwayland_surface_set_minimized(xsurface, !focused && e->minimize);
}

static void handle_request_fullscreen(struct wl_listener *listener, void *data) {
    struct wsm_xwayland_view *xwayland_view =
        wl_container_of(listener, xwayland_view, request_fullscreen);
    struct wsm_view *view = &xwayland_view->view;
    struct wlr_xwayland_surface *xsurface = view->wlr_xwayland_surface;
    if (xsurface->surface == NULL || !xsurface->surface->mapped) {
        return;
    }
    // container_set_fullscreen(view->container, xsurface->fullscreen);

    // arrange_root();
    // transaction_commit_dirty();
}

static void handle_request_activate(struct wl_listener *listener, void *data) {
    struct wsm_xwayland_view *xwayland_view =
        wl_container_of(listener, xwayland_view, request_activate);
    struct wsm_view *view = &xwayland_view->view;
    struct wlr_xwayland_surface *xsurface = view->wlr_xwayland_surface;
    if (xsurface->surface == NULL || !xsurface->surface->mapped) {
        return;
    }
    // view_request_activate(view, NULL);

    // transaction_commit_dirty();
}

static void handle_request_move(struct wl_listener *listener, void *data) {
    struct wsm_xwayland_view *xwayland_view =
        wl_container_of(listener, xwayland_view, request_move);
    struct wsm_view *view = &xwayland_view->view;
    struct wlr_xwayland_surface *xsurface = view->wlr_xwayland_surface;
    if (xsurface->surface == NULL || !xsurface->surface->mapped) {
        return;
    }
    // if (!container_is_floating(view->container) ||
    //     view->container->pending.fullscreen_mode) {
    //     return;
    // }
    // struct wsm_seat *seat = input_manager_current_seat();
    // seatop_begin_move_floating(seat, view->container);
}

static void handle_request_resize(struct wl_listener *listener, void *data) {
    struct wsm_xwayland_view *xwayland_view =
        wl_container_of(listener, xwayland_view, request_resize);
    struct wsm_view *view = &xwayland_view->view;
    struct wlr_xwayland_surface *xsurface = view->wlr_xwayland_surface;
    if (xsurface->surface == NULL || !xsurface->surface->mapped) {
        return;
    }
    // if (!container_is_floating(view->container)) {
    //     return;
    // }
    // struct wlr_xwayland_resize_event *e = data;
    // struct wsm_seat *seat = input_manager_current_seat();
    // seatop_begin_resize_floating(seat, view->container, e->edges);
}

static void handle_set_title(struct wl_listener *listener, void *data) {
    struct wsm_xwayland_view *xwayland_view =
        wl_container_of(listener, xwayland_view, set_title);
    struct wsm_view *view = &xwayland_view->view;
    struct wlr_xwayland_surface *xsurface = view->wlr_xwayland_surface;
    if (xsurface->surface == NULL || !xsurface->surface->mapped) {
        return;
    }
    // view_update_title(view, false);
    // view_execute_criteria(view);
}

static void handle_set_class(struct wl_listener *listener, void *data) {
    struct wsm_xwayland_view *xwayland_view =
        wl_container_of(listener, xwayland_view, set_class);
    struct wsm_view *view = &xwayland_view->view;
    struct wlr_xwayland_surface *xsurface = view->wlr_xwayland_surface;
    if (xsurface->surface == NULL || !xsurface->surface->mapped) {
        return;
    }
    // view_execute_criteria(view);
}

static void handle_set_role(struct wl_listener *listener, void *data) {
    struct wsm_xwayland_view *xwayland_view =
        wl_container_of(listener, xwayland_view, set_role);
    struct wsm_view *view = &xwayland_view->view;
    struct wlr_xwayland_surface *xsurface = view->wlr_xwayland_surface;
    if (xsurface->surface == NULL || !xsurface->surface->mapped) {
        return;
    }
    // view_execute_criteria(view);
}

static void handle_set_startup_id(struct wl_listener *listener, void *data) {
    struct wsm_xwayland_view *xwayland_view =
        wl_container_of(listener, xwayland_view, set_startup_id);
    struct wsm_view *view = &xwayland_view->view;
    struct wlr_xwayland_surface *xsurface = view->wlr_xwayland_surface;
    if (xsurface->startup_id == NULL) {
        return;
    }
}

static void handle_set_window_type(struct wl_listener *listener, void *data) {
    struct wsm_xwayland_view *xwayland_view =
        wl_container_of(listener, xwayland_view, set_window_type);
    struct wsm_view *view = &xwayland_view->view;
    struct wlr_xwayland_surface *xsurface = view->wlr_xwayland_surface;
    if (xsurface->surface == NULL || !xsurface->surface->mapped) {
        return;
    }
}

static void handle_set_hints(struct wl_listener *listener, void *data) {
    struct wsm_xwayland_view *xwayland_view =
        wl_container_of(listener, xwayland_view, set_hints);
    struct wsm_view *view = &xwayland_view->view;
    struct wlr_xwayland_surface *xsurface = view->wlr_xwayland_surface;
    if (xsurface->surface == NULL || !xsurface->surface->mapped) {
        return;
    }
    // const bool hints_urgency = xcb_icccm_wm_hints_get_urgency(xsurface->hints);
    // if (!hints_urgency && view->urgent_timer) {
    //     // The view is in the timeout period. We'll ignore the request to
    //     // unset urgency so that the view remains urgent until the timer clears
    //     // it.
    //     return;
    // }
    // if (view->allow_request_urgent) {
    //     view_set_urgent(view, hints_urgency);
    // }
}

static void handle_set_decorations(struct wl_listener *listener, void *data) {
    struct wsm_xwayland_view *xwayland_view =
        wl_container_of(listener, xwayland_view, set_decorations);
    // struct wsm_view *view = &xwayland_view->view;
    // struct wlr_xwayland_surface *xsurface = view->wlr_xwayland_surface;

    // bool csd = xsurface->decorations != WLR_XWAYLAND_SURFACE_DECORATIONS_ALL;
    // view_update_csd_from_client(view, csd);
}

static void handle_commit(struct wl_listener *listener, void *data) {
    struct wsm_xwayland_view *xwayland_view =
        wl_container_of(listener, xwayland_view, commit);
    // struct wsm_view *view = &xwayland_view->view;
    // struct wlr_xwayland_surface *xsurface = view->wlr_xwayland_surface;
    // struct wlr_surface_state *state = &xsurface->surface->current;

    // struct wlr_box new_geo = {0};
    // new_geo.width = state->width;
    // new_geo.height = state->height;

    // bool new_size = new_geo.width != view->geometry.width ||
    //                 new_geo.height != view->geometry.height;

    // if (new_size) {
    //     // The client changed its surface size in this commit. For floating
    //     // containers, we resize the container to match. For tiling containers,
    //     // we only recenter the surface.
    //     memcpy(&view->geometry, &new_geo, sizeof(struct wlr_box));
    //     if (container_is_floating(view->container)) {
    //         view_update_size(view);
    //         transaction_commit_dirty_client();
    //     }

    //     view_center_and_clip_surface(view);
    // }

    // if (view->container->node.instruction) {
    //     bool successful = transaction_notify_view_ready_by_geometry(view,
    //                                                                 xsurface->x, xsurface->y, state->width, state->height);

    //     // If we saved the view and this commit isn't what we're looking for
    //     // that means the user will never actually see the buffers submitted to
    //     // us here. Just send frame done events to these surfaces so they can
    //     // commit another time for us.
    //     if (view->saved_surface_tree && !successful) {
    //         view_send_frame_done(view);
    //     }
    // }
}

static void handle_map(struct wl_listener *listener, void *data) {
    struct wsm_xwayland_view *xwayland_view =
        wl_container_of(listener, xwayland_view, map);
    struct wsm_view *view = &xwayland_view->view;
    struct wlr_xwayland_surface *xsurface = view->wlr_xwayland_surface;

    view->natural_width = xsurface->width;
    view->natural_height = xsurface->height;

    xwayland_view->commit.notify = handle_commit;
    wl_signal_add(&xsurface->surface->events.commit, &xwayland_view->commit);

    // // Put it back into the tree
    // view_map(view, xsurface->surface, xsurface->fullscreen, NULL, false);

    // xwayland_view->surface_tree = wlr_scene_subsurface_tree_create(
    //     xwayland_view->view.content_tree, xsurface->surface);

    // if (xwayland_view->surface_tree) {
    //     xwayland_view->surface_tree_destroy.notify = handle_surface_tree_destroy;
    //     wl_signal_add(&xwayland_view->surface_tree->node.events.destroy,
    //                   &xwayland_view->surface_tree_destroy);
    // }

    // transaction_commit_dirty();
}

static void handle_unmap(struct wl_listener *listener, void *data) {
    struct wsm_xwayland_view *xwayland_view =
        wl_container_of(listener, xwayland_view, unmap);
    struct wsm_view *view = &xwayland_view->view;

    if (!wsm_assert(view->surface, "Cannot unmap unmapped view")) {
        return;
    }

    wl_list_remove(&xwayland_view->commit.link);
    // wl_list_remove(&xwayland_view->surface_tree_destroy.link);

    // if (xwayland_view->surface_tree) {
    //     wlr_scene_node_destroy(&xwayland_view->surface_tree->node);
    //     xwayland_view->surface_tree = NULL;
    // }

    // view_unmap(view);
}

static void handle_associate(struct wl_listener *listener, void *data) {
    struct wsm_xwayland_view *xwayland_view =
        wl_container_of(listener, xwayland_view, associate);
    struct wlr_xwayland_surface *xsurface =
        xwayland_view->view.wlr_xwayland_surface;

    xwayland_view->map.notify = handle_map;
    wl_signal_add(&xsurface->surface->events.map, &xwayland_view->map);
    wl_signal_add(&xsurface->surface->events.unmap, &xwayland_view->unmap);
    xwayland_view->unmap.notify = handle_unmap;
}

static void handle_dissociate(struct wl_listener *listener, void *data) {
    struct wsm_xwayland_view *xwayland_view =
        wl_container_of(listener, xwayland_view, dissociate);
    wl_list_remove(&xwayland_view->map.link);
    wl_list_remove(&xwayland_view->unmap.link);
}

static void handle_override_redirect(struct wl_listener *listener, void *data) {
    struct wsm_xwayland_view *xwayland_view =
        wl_container_of(listener, xwayland_view, override_redirect);
    struct wsm_view *view = &xwayland_view->view;
    struct wlr_xwayland_surface *xsurface = view->wlr_xwayland_surface;

    bool associated = xsurface->surface != NULL;
    bool mapped = associated && xsurface->surface->mapped;
    if (mapped) {
        handle_unmap(&xwayland_view->unmap, NULL);
    }
    if (associated) {
        handle_dissociate(&xwayland_view->dissociate, NULL);
    }

    handle_destroy(&xwayland_view->destroy, view);
    xsurface->data = NULL;

    struct wsm_xwayland_unmanaged *unmanaged = wsm_xwayland_unmanaged_create(xsurface);
    if (associated) {
        unmanaged_xsurface_associate(&unmanaged->associate, NULL);
    }
    if (mapped) {
        unmanaged_xsurface_map(&unmanaged->map, xsurface);
    }
}

struct wsm_xwayland_view *xwayland_view_create(struct wlr_xwayland_surface *xsurface) {
    wsm_log(WSM_DEBUG, "New xwayland surface title='%s' class='%s'",
             xsurface->title, xsurface->class);

    struct wsm_xwayland_view *xwayland_view =
        calloc(1, sizeof(struct wsm_xwayland_view));
    if (xwayland_view == NULL) {
        wsm_log(WSM_ERROR, "Could not create wsm_xwayland_view failed: allocation failed!");
        return NULL;
    }

    wsm_view_init(&xwayland_view->view, WSM_VIEW_XWAYLAND, &xwayland_view_impl);
    xwayland_view->xwayland_surface = xsurface;

    xwayland_view->destroy.notify = handle_destroy;
    wl_signal_add(&xsurface->events.destroy, &xwayland_view->destroy);

    xwayland_view->request_configure.notify = handle_request_configure;
    wl_signal_add(&xsurface->events.request_configure,
                  &xwayland_view->request_configure);

    xwayland_view->request_fullscreen.notify = handle_request_fullscreen;
    wl_signal_add(&xsurface->events.request_fullscreen,
                  &xwayland_view->request_fullscreen);

    xwayland_view->request_minimize.notify = handle_request_minimize;
    wl_signal_add(&xsurface->events.request_minimize,
                  &xwayland_view->request_minimize);

    xwayland_view->request_activate.notify = handle_request_activate;
    wl_signal_add(&xsurface->events.request_activate,
                  &xwayland_view->request_activate);

    xwayland_view->request_move.notify = handle_request_move;
    wl_signal_add(&xsurface->events.request_move,
                  &xwayland_view->request_move);

    xwayland_view->request_resize.notify = handle_request_resize;
    wl_signal_add(&xsurface->events.request_resize,
                  &xwayland_view->request_resize);

    xwayland_view->set_title.notify = handle_set_title;
    wl_signal_add(&xsurface->events.set_title, &xwayland_view->set_title);

    xwayland_view->set_class.notify = handle_set_class;
    wl_signal_add(&xsurface->events.set_class, &xwayland_view->set_class);

    xwayland_view->set_role.notify = handle_set_role;
    wl_signal_add(&xsurface->events.set_role, &xwayland_view->set_role);

    xwayland_view->set_startup_id.notify = handle_set_startup_id;
    wl_signal_add(&xsurface->events.set_startup_id,
                  &xwayland_view->set_startup_id);

    xwayland_view->set_window_type.notify = handle_set_window_type;
    wl_signal_add(&xsurface->events.set_window_type,
                  &xwayland_view->set_window_type);

    xwayland_view->set_hints.notify = handle_set_hints;
    wl_signal_add(&xsurface->events.set_hints, &xwayland_view->set_hints);

    xwayland_view->set_decorations.notify = handle_set_decorations;
    wl_signal_add(&xsurface->events.set_decorations,
                  &xwayland_view->set_decorations);

    xwayland_view->associate.notify = handle_associate;
    wl_signal_add(&xsurface->events.associate, &xwayland_view->associate);

    xwayland_view->dissociate.notify = handle_dissociate;
    wl_signal_add(&xsurface->events.dissociate, &xwayland_view->dissociate);

    xwayland_view->override_redirect.notify = handle_override_redirect;
    wl_signal_add(&xsurface->events.set_override_redirect,
                  &xwayland_view->override_redirect);

    // struct wsm_workspace *ws = NULL;
    // struct wsm_output *output = wsm_output_nearest_to(xsurface->x, xsurface->y);
    // ws = wsm_output_get_active_workspace(output);
    // view->workspace = ws;
    // view->scene_tree = wlr_scene_tree_create(view->workspace->tree);

    xsurface->data = xwayland_view;

    return xwayland_view;
}

struct wsm_xwayland_view *xwayland_view_from_view(struct wsm_view *view) {
    return (struct wsm_xwayland_view *)view;
}

struct wlr_xwayland_surface *xwayland_surface_from_view(struct wsm_view *view) {
    struct wsm_xwayland_view *xwayland_view = xwayland_view_from_view(view);
    assert(xwayland_view->xwayland_surface);
    return xwayland_view->xwayland_surface;
}

struct wsm_xwayland_view *create_xwayland_view(struct wlr_xwayland_surface *xsurface) {
    wsm_log(WSM_DEBUG, "New xwayland surface title='%s' class='%s'",
             xsurface->title, xsurface->class);

    struct wsm_xwayland_view *xwayland_view =
        calloc(1, sizeof(struct wsm_xwayland_view));
    if (!wsm_assert(xwayland_view, "Could not create wsm_xwayland_view: allocation failed!")) {
        return NULL;
    }
    wsm_view_init(&xwayland_view->view, WSM_VIEW_XWAYLAND, &xwayland_view_impl);

    xwayland_view->view.wlr_xwayland_surface = xsurface;
    return xwayland_view;
}

void xsurface_associate(struct wl_listener *listener, void *data) {
    handle_associate(listener, data);
}
void xsurface_map(struct wl_listener *listener, void *data) {
    handle_map(listener, data);
}

#endif
