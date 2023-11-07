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
#include "wsm_xdg_shell.h"
#include "wsm_server.h"
#include "wsm_log.h"
#include "wsm_view.h"
#include "wsm_output.h"
#include "wsm_xdg_popup.h"
#include "wsm_workspace.h"
#include "wsm_xdg_decoration.h"
#include "wsm_server_decoration.h"

#include <stdlib.h>
#include <assert.h>

#include <wlr/util/edges.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/backend/wayland.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/types/wlr_xdg_activation_v1.h>
#include <wlr/types/wlr_layer_shell_v1.h>
#include <wlr/types/wlr_xdg_decoration_v1.h>
#include <wlr/types/wlr_server_decoration.h>
#include <wlr/types/wlr_foreign_toplevel_management_v1.h>
#include <wlr/types/wlr_fractional_scale_v1.h>

#define WSM_XDG_SHELL_VERSION 2
#define CONFIGURE_TIMEOUT_MS 100

static const char *xdg_toplevel_wsm_view_get_string_prop(struct wsm_view *view,
                                   enum wsm_view_prop prop) {
    if (xdg_shell_view_from_view(view) == NULL) {
        return NULL;
    }
    switch (prop) {
    case VIEW_PROP_TITLE:
        return xdg_toplevel_from_view(view)->title;
    case VIEW_PROP_APP_ID:
        return xdg_toplevel_from_view(view)->app_id;
    default:
        return "";
    }
}

static void xdg_toplevel_view_configure(struct wsm_view *view, struct wlr_box geo) {
    struct wsm_xdg_shell_view *xdg_shell_view =
        xdg_shell_view_from_view(view);
    if (xdg_shell_view == NULL) {
        return;
    }
    wlr_xdg_toplevel_set_size(xdg_toplevel_from_view(view),
                                     geo.width, geo.height);
}

static void xdg_toplevel_wsm_view_set_activated(struct wsm_view *view, bool activated) {
    if (xdg_shell_view_from_view(view) == NULL) {
        return;
    }
    wlr_xdg_toplevel_set_activated(xdg_toplevel_from_view(view), activated);
}

static void xdg_toplevel_view_set_tiled(struct wsm_view *view, bool tiled) {
    if (xdg_shell_view_from_view(view) == NULL) {
        return;
    }
    if (wl_resource_get_version(xdg_toplevel_from_view(view)->resource) >=
        XDG_TOPLEVEL_STATE_TILED_LEFT_SINCE_VERSION) {
        enum wlr_edges edges = WLR_EDGE_NONE;
        if (tiled) {
            edges = WLR_EDGE_LEFT | WLR_EDGE_RIGHT | WLR_EDGE_TOP |
                    WLR_EDGE_BOTTOM;
        }
        wlr_xdg_toplevel_set_tiled(xdg_toplevel_from_view(view), edges);
    } else {
        wlr_xdg_toplevel_set_maximized(xdg_toplevel_from_view(view), tiled);
    }
}

static void xdg_toplevel_wsm_view_set_fullscreen(struct wsm_view *view, bool fullscreen) {
    if (xdg_shell_view_from_view(view) == NULL) {
        return;
    }
    wlr_xdg_toplevel_set_fullscreen(xdg_toplevel_from_view(view), fullscreen);
}

static void xdg_toplevel_view_set_resizing(struct wsm_view *view, bool resizing) {
    if (xdg_shell_view_from_view(view) == NULL) {
        return;
    }
    wlr_xdg_toplevel_set_resizing(xdg_toplevel_from_view(view), resizing);
}

static void xdg_toplevel_wsm_view_close(struct wsm_view *view) {
    if (xdg_shell_view_from_view(view) == NULL) {
        return;
    }
    wlr_xdg_toplevel_send_close(xdg_toplevel_from_view(view));
}

static void xdg_toplevel_wsm_view_close_popups(struct wsm_view *view) {
    struct wlr_xdg_popup *popup, *tmp;
    wl_list_for_each_safe(popup, tmp, &xdg_toplevel_from_view(view)->base->popups, link) {
        wlr_xdg_popup_destroy(popup);
    }
}

static void xdg_toplevel_wsm_view_destroy(struct wsm_view *view) {
    if (!view) {
        wsm_log(WSM_ERROR, "wsm_view is NULL!");
        return;
    }

    struct wsm_xdg_shell_view *xdg_shell_view =
        xdg_shell_view_from_view(view);
    if (xdg_shell_view == NULL) {
        return;
    }
    free(xdg_shell_view);
}

static void
xdg_toplevel_wsm_view_map(struct wsm_view *view) {

}

static void
xdg_toplevel_view_unmap(struct wsm_view *view, bool client_request) {
    if (view->mapped) {
        view->mapped = false;
        wlr_scene_node_set_enabled(&view->scene_tree->node, false);
    }
}

static void
xdg_toplevel_wsm_view_maximize(struct wsm_view *view, bool maximized) {
    wlr_xdg_toplevel_set_maximized(xdg_toplevel_from_view(view), maximized);
}

static void
xdg_toplevel_wsm_view_minimize(struct wsm_view *view, bool minimized) {

}

static void
view_impl_move_to_front(struct wsm_view *view) {
    wlr_scene_node_raise_to_top(&view->scene_tree->node);
}

void
view_impl_move_to_back(struct wsm_view *view) {
    wlr_scene_node_lower_to_bottom(&view->scene_tree->node);
}

static struct wsm_view *
xdg_toplevel_wsm_view_get_root(struct wsm_view *view) {
    struct wlr_xdg_toplevel *root = top_parent_of(view);
    struct wlr_xdg_surface *surface = (struct wlr_xdg_surface *)root->base;
    return (struct wsm_view *)surface->data;
}

static void
xdg_toplevel_wsm_view_append_children(struct wsm_view *self, struct wl_array *children) {

}

static bool
xdg_toplevel_view_contains_window_type(struct wsm_view *view, int32_t window_type) {
    assert(view);

    return false;
}

static pid_t
xdg_view_get_pid(struct wsm_view *view) {
    assert(view);
    pid_t pid = -1;

    if (view->surface && view->surface->resource
        && view->surface->resource->client) {
        struct wl_client *client = view->surface->resource->client;
        wl_client_get_credentials(client, &pid, NULL, NULL);
    }
    return pid;
}

static const struct wsm_view_impl xdg_toplevel_view_impl = {
    .configure = xdg_toplevel_view_configure,
    .close = xdg_toplevel_wsm_view_close,
    .close_popups = xdg_toplevel_wsm_view_close_popups,
    .destroy = xdg_toplevel_wsm_view_destroy,
    .get_string_prop = xdg_toplevel_wsm_view_get_string_prop,
    .map = xdg_toplevel_wsm_view_map,
    .set_activated = xdg_toplevel_wsm_view_set_activated,
    .set_fullscreen = xdg_toplevel_wsm_view_set_fullscreen,
    .set_tiled = xdg_toplevel_view_set_tiled,
    .set_resizing = xdg_toplevel_view_set_resizing,
    .unmap = xdg_toplevel_view_unmap,
    .maximize = xdg_toplevel_wsm_view_maximize,
    .minimize = xdg_toplevel_wsm_view_minimize,
    .move_to_front = view_impl_move_to_front,
    .move_to_back = view_impl_move_to_back,
    .get_root = xdg_toplevel_wsm_view_get_root,
    .append_children = xdg_toplevel_wsm_view_append_children,
    .contains_window_type = xdg_toplevel_view_contains_window_type,
    .get_pid = xdg_view_get_pid,
};

static void handle_commit(struct wl_listener *listener, void *data) {

}

static void handle_new_popup(struct wl_listener *listener, void *data) {
    struct wsm_xdg_toplevel_view *xdg_toplevel_view =
        wl_container_of(listener, xdg_toplevel_view, new_popup);
    struct wsm_view *view = &xdg_toplevel_view->base;
    struct wlr_xdg_popup *wlr_popup = data;
    xdg_popup_create(view, wlr_popup);
}

static void handle_request_maximize(struct wl_listener *listener, void *data) {
    // struct wsm_xdg_shell_view *view = wl_container_of(listener, view, request_minimize);
    // wsm_view_minimize(view, xdg_toplevel_from_view(view)->requested.minimized);
}

static void handle_request_fullscreen(struct wl_listener *listener, void *data) {
    // struct wsm_xdg_shell_view *xdg_toplevel_view = wl_container_of(listener, xdg_toplevel_view, request_fullscreen);
    // struct wsm_view *view = &xdg_toplevel_view->view;
    // if (!view->mapped && !view->output) {
    //     wsm_view_set_output(view, wsm_wsm_output_nearest_to_cursor(view->server));
    // }
    // set_fullscreen_from_request(view,
    //                             &xdg_toplevel_from_view(view)->requested);
}

static void handle_request_move(struct wl_listener *listener, void *data) {

}

static void handle_request_resize(struct wl_listener *listener, void *data) {

}

static void handle_set_title(struct wl_listener *listener, void *data) {
    struct wsm_xdg_shell_view *xdg_toplevel_view =
        wl_container_of(listener, xdg_toplevel_view, set_title);
    struct wsm_view *view = &xdg_toplevel_view->view;
    wsm_view_update_title(view);
}

static void handle_set_app_id(struct wl_listener *listener, void *data) {
    struct wsm_xdg_shell_view *xdg_toplevel_view =
        wl_container_of(listener, xdg_toplevel_view, set_app_id);
    struct wsm_view *view = &xdg_toplevel_view->view;
    wsm_view_update_app_id(view);
}

static void handle_map(struct wl_listener *listener, void *data) {
    struct wsm_xdg_shell_view *xdg_shell_view =
        wl_container_of(listener, xdg_shell_view, map);
    struct wsm_view *view = &xdg_shell_view->view;
    if (view->mapped)
        return;

    view->mapped = true;
    struct wlr_xdg_toplevel *toplevel = xdg_toplevel_from_view(view);

    // view->natural_width = toplevel->base->current.geometry.width;
    // view->natural_height = toplevel->base->current.geometry.height;
    // if (!view->natural_width && !view->natural_height) {
    //     view->natural_width = toplevel->base->surface->current.width;
    //     view->natural_height = toplevel->base->surface->current.height;
    // }

    xdg_shell_view->commit.notify = handle_commit;
    wl_signal_add(&toplevel->base->surface->events.commit,
                  &xdg_shell_view->commit);

    xdg_shell_view->new_popup.notify = handle_new_popup;
    wl_signal_add(&toplevel->base->events.new_popup,
                  &xdg_shell_view->new_popup);

    xdg_shell_view->request_maximize.notify = handle_request_maximize;
    wl_signal_add(&toplevel->events.request_maximize,
                  &xdg_shell_view->request_maximize);

    xdg_shell_view->request_fullscreen.notify = handle_request_fullscreen;
    wl_signal_add(&toplevel->events.request_fullscreen,
                  &xdg_shell_view->request_fullscreen);

    xdg_shell_view->request_move.notify = handle_request_move;
    wl_signal_add(&toplevel->events.request_move,
                  &xdg_shell_view->request_move);

    xdg_shell_view->request_resize.notify = handle_request_resize;
    wl_signal_add(&toplevel->events.request_resize,
                  &xdg_shell_view->request_resize);

    xdg_shell_view->set_title.notify = handle_set_title;
    wl_signal_add(&toplevel->events.set_title,
                  &xdg_shell_view->set_title);

    xdg_shell_view->set_app_id.notify = handle_set_app_id;
    wl_signal_add(&toplevel->events.set_app_id,
                  &xdg_shell_view->set_app_id);
}

static void handle_unmap(struct wl_listener *listener, void *data) {
    struct wsm_xdg_shell_view *xdg_shell_view =
        wl_container_of(listener, xdg_shell_view, unmap);
    // struct wsm_view *view = &xdg_shell_view->view;

    // if (!wsm_assert(view->surface, "Cannot unmap unmapped view")) {
    //     return;
    // }

    // view_unmap(view);

    wl_list_remove(&xdg_shell_view->commit.link);
    wl_list_remove(&xdg_shell_view->new_popup.link);
    wl_list_remove(&xdg_shell_view->request_maximize.link);
    wl_list_remove(&xdg_shell_view->request_fullscreen.link);
    wl_list_remove(&xdg_shell_view->request_move.link);
    wl_list_remove(&xdg_shell_view->request_resize.link);
    wl_list_remove(&xdg_shell_view->set_title.link);
    wl_list_remove(&xdg_shell_view->set_app_id.link);
}

static void handle_destroy(struct wl_listener *listener, void *data) {
    struct wsm_xdg_shell_view *xdg_shell_view =
        wl_container_of(listener, xdg_shell_view, destroy);
    struct wsm_view *view = &xdg_shell_view->view;
    // if (!wsm_assert(view->surface == NULL, "Tried to destroy a mapped view")) {
    //     return;
    // }
    wl_list_remove(&xdg_shell_view->destroy.link);
    wl_list_remove(&xdg_shell_view->map.link);
    wl_list_remove(&xdg_shell_view->unmap.link);
    if (view->xdg_decoration) {
        view->xdg_decoration->view = NULL;
    }
}

static void handle_new_xdg_shell_surface(struct wl_listener *listener, void *data) {
    struct wsm_xdg_shell *xdg_shell =
        wl_container_of(listener, xdg_shell, xdg_shell_surface);
    struct wlr_xdg_surface *xdg_surface = data;

    if (xdg_surface->role != WLR_XDG_SURFACE_ROLE_TOPLEVEL) {
        return;
    }

    wsm_log(WSM_DEBUG, "New xdg_shell toplevel title='%s' app_id='%s'",
            xdg_surface->toplevel->title, xdg_surface->toplevel->app_id);

    wlr_xdg_surface_ping(xdg_surface);

    struct wsm_xdg_shell_view *xdg_shell_view =
        calloc(1, sizeof(struct wsm_xdg_shell_view));
    if (!wsm_assert(xdg_shell_view, "Failed to allocate view")) {
        return;
    }

    struct wsm_view *view = &xdg_shell_view->view;
    xdg_shell_view->xdg_surface = xdg_surface;
    view->type = WSM_VIEW_XDG_SHELL;
    view->impl = &xdg_toplevel_view_impl;
    wsm_view_init(&xdg_shell_view->view, WSM_VIEW_XDG_SHELL, &xdg_toplevel_view_impl);

    struct wlr_box geometry;
    wlr_xdg_surface_get_geometry(xdg_surface, &geometry);
    struct wsm_workspace *ws = NULL;
    struct wsm_output *output = wsm_output_nearest_to(geometry.x, geometry.y);
    ws = wsm_output_get_active_workspace(output);
    view->workspace = ws;
    wsm_view_set_output(view, output);
        wlr_fractional_scale_v1_notify_scale(xdg_surface->surface,
                                             view->output->wlr_output->scale);

    view->scene_tree = wlr_scene_xdg_surface_create(ws->tree, xdg_surface);
    if (!view->scene_tree) {
        wl_resource_post_no_memory(xdg_surface->resource);
        return;
    }
    xdg_surface->data = xdg_shell_view;
    xdg_surface->surface->data = view->scene_tree;

    xdg_shell_view->map.notify = handle_map;
    wl_signal_add(&xdg_surface->surface->events.map, &xdg_shell_view->map);

    xdg_shell_view->unmap.notify = handle_unmap;
    wl_signal_add(&xdg_surface->surface->events.unmap, &xdg_shell_view->unmap);

    xdg_shell_view->destroy.notify = handle_destroy;
    wl_signal_add(&xdg_surface->events.destroy, &xdg_shell_view->destroy);

    struct wlr_xdg_toplevel *toplevel = xdg_surface->toplevel;

    xdg_shell_view->commit.notify = handle_commit;
    wl_signal_add(&toplevel->base->surface->events.commit,
                  &xdg_shell_view->commit);

    xdg_shell_view->new_popup.notify = handle_new_popup;
    wl_signal_add(&toplevel->base->events.new_popup,
                  &xdg_shell_view->new_popup);

    xdg_shell_view->request_maximize.notify = handle_request_maximize;
    wl_signal_add(&toplevel->events.request_maximize,
                  &xdg_shell_view->request_maximize);

    xdg_shell_view->request_fullscreen.notify = handle_request_fullscreen;
    wl_signal_add(&toplevel->events.request_fullscreen,
                  &xdg_shell_view->request_fullscreen);

    xdg_shell_view->request_move.notify = handle_request_move;
    wl_signal_add(&toplevel->events.request_move,
                  &xdg_shell_view->request_move);

    xdg_shell_view->request_resize.notify = handle_request_resize;
    wl_signal_add(&toplevel->events.request_resize,
                  &xdg_shell_view->request_resize);

    xdg_shell_view->set_title.notify = handle_set_title;
    wl_signal_add(&toplevel->events.set_title,
                  &xdg_shell_view->set_title);

    xdg_shell_view->set_app_id.notify = handle_set_app_id;
    wl_signal_add(&toplevel->events.set_app_id,
                  &xdg_shell_view->set_app_id);
}

static void
handle_xdg_activation_request(struct wl_listener *listener, void *data)
{

}

struct wsm_xdg_shell *wsm_xdg_shell_create() {
    struct wsm_xdg_shell *shell = calloc(1, sizeof(struct wsm_xdg_shell));
    if (!wsm_assert(shell, "Could not create wsm_xdg_shell: allocation failed!")) {
        return NULL;
    }

    shell->wlr_xdg_shell = wlr_xdg_shell_create(server.wl_display, WSM_XDG_SHELL_VERSION);
    shell->xdg_shell_surface.notify = handle_new_xdg_shell_surface;
    wl_signal_add(&shell->wlr_xdg_shell->events.new_surface,
                  &shell->xdg_shell_surface);

    shell->xdg_activation = wlr_xdg_activation_v1_create(server.wl_display);
    if (!wsm_assert(shell->xdg_activation, "unable to create wlr_xdg_activation_v1 interface")) {
        return NULL;
    }
    shell->xdg_activation_request.notify = handle_xdg_activation_request;
    wl_signal_add(&shell->xdg_activation->events.request_activate,
                  &shell->xdg_activation_request);
    return shell;
}

void wsm_xdg_shell_destroy(struct wsm_xdg_shell *shell) {
    if (!shell) {
        wsm_log(WSM_ERROR, "wsm_xdg_shell is NULL!");
        return;
    }

    wl_list_remove(&shell->xdg_shell_surface.link);

    free(shell);
}

struct wsm_xdg_shell_view *xdg_shell_view_from_view(
    struct wsm_view *view) {
    assert(view->type == WSM_VIEW_XDG_SHELL);
    return (struct wsm_xdg_shell_view *)view;
}

struct wlr_xdg_toplevel *top_parent_of(struct wsm_view *view) {
    struct wlr_xdg_toplevel *toplevel = xdg_toplevel_from_view(view);
    while (toplevel->parent) {
        toplevel = toplevel->parent;
    }
    return toplevel;
}

struct wlr_xdg_surface *xdg_surface_from_view(struct wsm_view *view) {
    assert(view->type == WSM_VIEW_XDG_SHELL);
    struct wsm_xdg_shell_view *xdg_toplevel_view =
        xdg_shell_view_from_view(view);
    assert(xdg_toplevel_view->xdg_surface);
    return xdg_toplevel_view->xdg_surface;
}

struct wlr_xdg_toplevel *xdg_toplevel_from_view(struct wsm_view *view) {
    struct wlr_xdg_surface *xdg_surface = xdg_surface_from_view(view);
    assert(xdg_surface->role == WLR_XDG_SURFACE_ROLE_TOPLEVEL);
    assert(xdg_surface->toplevel);
    return xdg_surface->toplevel;
}
