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

#include "wsm_log.h"
#include "wsm_list.h"
#include "wsm_seat.h"
#include "wsm_view.h"
#include "wsm_config.h"
#include "wsm_output.h"
#include "wsm_cursor.h"
#include "wsm_server.h"
#include "wsm_scene.h"
#include "wsm_pango.h"
#include "wsm_common.h"
#include "wsm_titlebar.h"
#include "wsm_container.h"
#include "wsm_workspace.h"
#include "wsm_xdg_shell.h"
#include "wsm_transaction.h"
#include "node/wsm_node_descriptor.h"
#include "node/wsm_text_node.h"
#include "wsm_idle_inhibit_v1.h"
#include "wsm_input_manager.h"
#include "wsm_xdg_decoration.h"
#include "wsm_arrange.h"

#include <float.h>
#include <stdlib.h>

#include <wayland-server.h>

#if HAVE_XWAYLAND
#include <xcb/xcb_icccm.h>
#include <wlr/xwayland.h>
#include <wlr/xwayland/xwayland.h>
#endif
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/types/wlr_layer_shell_v1.h>
#include <wlr/types/wlr_subcompositor.h>
#include <wlr/types/wlr_fractional_scale_v1.h>
#include <wlr/types/wlr_foreign_toplevel_management_v1.h>
#include <wlr/types/wlr_xdg_decoration_v1.h>
#include <wlr/types/wlr_pointer_constraints_v1.h>
#include <wlr/types/wlr_ext_foreign_toplevel_list_v1.h>

bool view_init(struct wsm_view *view, enum wsm_view_type type,
               const struct wsm_view_impl *impl) {
    bool failed = false;
    view->scene_tree = alloc_scene_tree(global_server.wsm_scene->staging, &failed);
    view->content_tree = alloc_scene_tree(view->scene_tree, &failed);

    if (!failed && !wsm_scene_descriptor_assign(&view->scene_tree->node,
                                            WSM_SCENE_DESC_VIEW, view)) {
        failed = true;
    }

    if (failed) {
        wlr_scene_node_destroy(&view->scene_tree->node);
        return false;
    }

    view->type = type;
    view->impl = impl;
    view->allow_request_urgent = true;
    view->enabled = true;
    wl_signal_init(&view->events.unmap);
    return true;
}

void view_destroy(struct wsm_view *view) {
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

    wlr_scene_node_destroy(&view->scene_tree->node);
    free(view->title_format);

    if (view->impl->destroy) {
        view->impl->destroy(view);
    } else {
        free(view);
    }
}

void view_begin_destroy(struct wsm_view *view) {
    if (!wsm_assert(view->surface == NULL, "Tried to destroy a mapped view")) {
        return;
    }
    view->destroying = true;

    if (!view->container) {
        view_destroy(view);
    }
}

const char *view_get_title(struct wsm_view *view) {
    if (view->impl->get_string_prop) {
        return view->impl->get_string_prop(view, VIEW_PROP_TITLE);
    }
    return NULL;
}

const char *view_get_app_id(struct wsm_view *view) {
    if (view->impl->get_string_prop) {
        return view->impl->get_string_prop(view, VIEW_PROP_APP_ID);
    }
    return NULL;
}

const char *view_get_class(struct wsm_view *view) {
    if (view->impl->get_string_prop) {
        return view->impl->get_string_prop(view, VIEW_PROP_CLASS);
    }
    return NULL;
}

const char *view_get_instance(struct wsm_view *view) {
    if (view->impl->get_string_prop) {
        return view->impl->get_string_prop(view, VIEW_PROP_INSTANCE);
    }
    return NULL;
}
#if HAVE_XWAYLAND
uint32_t view_get_x11_window_id(struct wsm_view *view) {
    if (view->impl->get_int_prop) {
        return view->impl->get_int_prop(view, VIEW_PROP_X11_WINDOW_ID);
    }
    return 0;
}

uint32_t view_get_x11_parent_id(struct wsm_view *view) {
    if (view->impl->get_int_prop) {
        return view->impl->get_int_prop(view, VIEW_PROP_X11_PARENT_ID);
    }
    return 0;
}
#endif
const char *view_get_window_role(struct wsm_view *view) {
    if (view->impl->get_string_prop) {
        return view->impl->get_string_prop(view, VIEW_PROP_WINDOW_ROLE);
    }
    return NULL;
}

uint32_t view_get_window_type(struct wsm_view *view) {
    if (view->impl->get_int_prop) {
        return view->impl->get_int_prop(view, VIEW_PROP_WINDOW_TYPE);
    }
    return 0;
}

const char *view_get_shell(struct wsm_view *view) {
    switch(view->type) {
    case WSM_VIEW_XDG_SHELL:
        return "xdg_shell";
#if HAVE_XWAYLAND
    case WSM_VIEW_XWAYLAND:
        return "xwayland";
#endif
    }
    return "unknown";
}

void view_get_constraints(struct wsm_view *view, double *min_width,
                          double *max_width, double *min_height, double *max_height) {
    if (view->impl->get_constraints) {
        view->impl->get_constraints(view,
                                    min_width, max_width, min_height, max_height);
    } else {
        *min_width = DBL_MIN;
        *max_width = DBL_MAX;
        *min_height = DBL_MIN;
        *max_height = DBL_MAX;
    }
}

uint32_t view_configure(struct wsm_view *view, double lx, double ly, int width,
                        int height) {
    if (view->impl->configure) {
        return view->impl->configure(view, lx, ly, width, height);
    }
    return 0;
}

bool view_inhibit_idle(struct wsm_view *view) {
    struct wsm_idle_inhibitor_v1 *user_inhibitor =
        wsm_idle_inhibit_v1_user_inhibitor_for_view(view);

    struct wsm_idle_inhibitor_v1 *application_inhibitor =
        wsm_idle_inhibit_v1_application_inhibitor_for_view(view);

    if (!user_inhibitor && !application_inhibitor) {
        return false;
    }

    if (!user_inhibitor) {
        return wsm_idle_inhibit_v1_is_active(application_inhibitor);
    }

    if (!application_inhibitor) {
        return wsm_idle_inhibit_v1_is_active(user_inhibitor);
    }

    return wsm_idle_inhibit_v1_is_active(user_inhibitor)
           || wsm_idle_inhibit_v1_is_active(application_inhibitor);
}

void view_autoconfigure(struct wsm_view *view) {
    struct wsm_container *con = view->container;
    struct wsm_workspace *ws = con->pending.workspace;

    if (container_is_scratchpad_hidden(con) &&
        con->pending.fullscreen_mode != FULLSCREEN_GLOBAL) {
        return;
    }
    struct wsm_output *output = ws ? ws->output : NULL;

    if (con->pending.fullscreen_mode == FULLSCREEN_WORKSPACE) {
        con->pending.content_x = output->lx;
        con->pending.content_y = output->ly;
        con->pending.content_width = output->width;
        con->pending.content_height = output->height;
        return;
    } else if (con->pending.fullscreen_mode == FULLSCREEN_GLOBAL) {
        con->pending.content_x = global_server.wsm_scene->x;
        con->pending.content_y = global_server.wsm_scene->y;
        con->pending.content_width = global_server.wsm_scene->width;
        con->pending.content_height = global_server.wsm_scene->height;
        return;
    }

    con->pending.border_top = con->pending.border_bottom = true;
    con->pending.border_left = con->pending.border_right = true;

    bool show_border = true;
    con->pending.border_left &= show_border;
    con->pending.border_right &= show_border;
    con->pending.border_top &= show_border;
    con->pending.border_bottom &= show_border;

    double x, y, width, height;
    int max_thickness;
    switch (con->pending.border) {
    default:
    case B_CSD:
    case B_NONE:
        x = con->pending.x;
        y = con->pending.y;
        width = con->pending.width;
        height = con->pending.height;
        break;
    case B_NORMAL:
        max_thickness = get_max_thickness(con->pending);
        x = con->pending.x + max_thickness * con->pending.border_left;
        y = con->pending.y + container_titlebar_height()
            + max_thickness * con->pending.border_top;
        width = con->pending.width
                - max_thickness * con->pending.border_left
                - max_thickness * con->pending.border_right;
        height = con->pending.height - container_titlebar_height()
                 - max_thickness * con->pending.border_bottom
                 - max_thickness * con->pending.border_top;
        break;
    }

    con->pending.content_x = x;
    con->pending.content_y = y;
    con->pending.content_width = width;
    con->pending.content_height = height;
}

void view_set_activated(struct wsm_view *view, bool activated) {
    if (view->impl->set_activated) {
        view->impl->set_activated(view, activated);
    }
    if (view->foreign_toplevel) {
        wlr_foreign_toplevel_handle_v1_set_activated(
            view->foreign_toplevel, activated);
    }
}

void view_request_activate(struct wsm_view *view, struct wsm_seat *seat) {
    struct wsm_workspace *ws = view->container->pending.workspace;
    if (!seat) {
        seat = input_manager_current_seat();
    }

    if (ws && workspace_is_visible(ws)) {
        seat_set_focus_container(seat, view->container);
        container_raise_floating(view->container);
    } else {
        view_set_urgent(view, true);
    }
    transaction_commit_dirty();
}

void view_set_csd_from_server(struct wsm_view *view, bool enabled) {
    wsm_log(WSM_DEBUG, "Telling view %p to set CSD to %i", view, enabled);
    if (view->xdg_decoration) {
        uint32_t mode = enabled ?
                            WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE :
                            WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE;
        wlr_xdg_toplevel_decoration_v1_set_mode(
            view->xdg_decoration->wlr_xdg_decoration, mode);
    }
    view->using_csd = enabled;
}

void view_update_csd_from_client(struct wsm_view *view, bool enabled) {
    wsm_log(WSM_DEBUG, "View %p updated CSD to %i", view, enabled);
    struct wsm_container *con = view->container;
    if (enabled && con && con->pending.border != B_CSD) {
        con->saved_border = con->pending.border;
        if (container_is_floating(con)) {
            con->pending.border = B_CSD;
        }
    } else if (!enabled && con && con->pending.border == B_CSD) {
        con->pending.border = con->saved_border;
    }
    view->using_csd = enabled;
}

void view_set_tiled(struct wsm_view *view, bool tiled) {
    if (view->impl->set_tiled) {
        view->impl->set_tiled(view, tiled);
    }
}

void view_maximize(struct wsm_view *view, bool maximize) {
    if (view->impl->maximize) {
        view->impl->maximize(view, maximize);
    }
}

void view_minimize(struct wsm_view *view, bool minimize) {
    if (view->impl->minimize) {
        view->impl->minimize(view, minimize);
    }
}

void view_close(struct wsm_view *view) {
    if (view->impl->close) {
        view->impl->close(view);
    }
}

void view_close_popups(struct wsm_view *view) {
    if (view->impl->close_popups) {
        view->impl->close_popups(view);
    }
}

void view_set_enable(struct wsm_view *view, bool enable) {
    if (view->scene_tree) {
        wlr_scene_node_set_enabled(&view->scene_tree->node, enable);

        if (view->container) {
            if (view->container->scene_tree) {
                wlr_scene_node_set_enabled(&view->container->scene_tree->node, enable);
            }

            if (view->container->title_bar) {
                wlr_scene_node_set_enabled(&view->container->title_bar->tree->node, enable);
            }

            if (view->container->sensing.tree) {
                wlr_scene_node_set_enabled(&view->container->sensing.tree->node, enable);
            }
        } else {
            wsm_log(WSM_ERROR, "wsm_view's container is NULL");
        }
    } else {
        wsm_log(WSM_ERROR, "wsm_view's scene_tree is NULL");
    }
    view->enabled = enable;
}

static void view_populate_pid(struct wsm_view *view) {
    pid_t pid;
    switch (view->type) {
#if HAVE_XWAYLAND
    case WSM_VIEW_XWAYLAND:;
        struct wlr_xwayland_surface *surf =
            wlr_xwayland_surface_try_from_wlr_surface(view->surface);
        pid = surf->pid;
        break;
#endif
    case WSM_VIEW_XDG_SHELL:;
        struct wl_client *client =
            wl_resource_get_client(view->surface->resource);
        wl_client_get_credentials(client, &pid, NULL, NULL);
        break;
    }
    view->pid = pid;
}

static struct wsm_workspace *select_workspace(struct wsm_view *view) {
    struct wsm_seat *seat = input_manager_current_seat();

    // Use the focused workspace
    struct wsm_node *node = seat_get_focus_inactive(seat, &global_server.wsm_scene->node);
    if (node && node->type == N_WORKSPACE) {
        return node->wsm_workspace;
    } else if (node && node->type == N_CONTAINER) {
        return node->wsm_container->pending.workspace;
    }

    // When there's no outputs connected, the above should match a workspace on
    // the noop output.
    wsm_assert(false, "Expected to find a workspace");
    return NULL;
}

static bool should_focus(struct wsm_view *view) {
    struct wsm_seat *seat = input_manager_current_seat();
    struct wsm_container *prev_con = seat_get_focused_container(seat);
    struct wsm_workspace *prev_ws = seat_get_focused_workspace(seat);
    struct wsm_workspace *map_ws = view->container->pending.workspace;

    if (view->container->pending.fullscreen_mode == FULLSCREEN_GLOBAL) {
        return true;
    }

    // View opened "under" fullscreen view should not be given focus.
    if (global_server.wsm_scene->fullscreen_global || !map_ws || map_ws->fullscreen) {
        return false;
    }

    // Views can only take focus if they are mapped into the active workspace
    if (prev_ws != map_ws) {
        return false;
    }

    // If the view is the only one in the focused workspace, it'll get focus
    // regardless of any no_focus criteria.
    if (!view->container->pending.parent && !prev_con) {
        size_t num_children = view->container->pending.workspace->tiling->length +
                              view->container->pending.workspace->floating->length;
        if (num_children == 1) {
            return true;
        }
    }

    return true;
}

static void handle_foreign_activate_request(
    struct wl_listener *listener, void *data) {
    struct wsm_view *view = wl_container_of(
        listener, view, foreign_activate_request);
    struct wlr_foreign_toplevel_handle_v1_activated_event *event = data;
    struct wsm_seat *seat;
    wl_list_for_each(seat, &global_server.wsm_input_manager->seats, link) {
        if (seat->wlr_seat == event->seat) {
            if (container_is_scratchpad_hidden_or_child(view->container)) {
                root_scratchpad_show(view->container);
            }
            seat_set_focus_container(seat, view->container);
            seat_consider_warp_to_focus(seat);
            container_raise_floating(view->container);
            break;
        }
    }
    transaction_commit_dirty();
}

static void handle_foreign_fullscreen_request(
    struct wl_listener *listener, void *data) {
    struct wsm_view *view = wl_container_of(
        listener, view, foreign_fullscreen_request);
    struct wlr_foreign_toplevel_handle_v1_fullscreen_event *event = data;

    // Match fullscreen command behavior for scratchpad hidden views
    struct wsm_container *container = view->container;
    if (!container->pending.workspace) {
        while (container->pending.parent) {
            container = container->pending.parent;
        }
    }

    if (event->fullscreen && event->output && event->output->data) {
        struct wsm_output *output = event->output->data;
        struct wsm_workspace *ws = output_get_active_workspace(output);
        if (ws && !container_is_scratchpad_hidden(view->container)) {
            if (container_is_floating(view->container)) {
                workspace_add_floating(ws, view->container);
            } else {
                workspace_add_tiling(ws, view->container);
            }
        }
    }

    container_set_fullscreen(container,
                             event->fullscreen ? FULLSCREEN_WORKSPACE : FULLSCREEN_NONE);
    if (event->fullscreen) {
        arrange_root_auto();
    } else {
        if (container->pending.parent) {
            wsm_arrange_container_auto(container->pending.parent);
        } else if (container->pending.workspace) {
            wsm_arrange_workspace_auto(container->pending.workspace);
        }
    }
    transaction_commit_dirty();
}

static void handle_foreign_close_request(
    struct wl_listener *listener, void *data) {
    struct wsm_view *view = wl_container_of(
        listener, view, foreign_close_request);
    view_close(view);
}

static void handle_foreign_destroy(
    struct wl_listener *listener, void *data) {
    struct wsm_view *view = wl_container_of(
        listener, view, foreign_destroy);

    wl_list_remove(&view->foreign_activate_request.link);
    wl_list_remove(&view->foreign_fullscreen_request.link);
    wl_list_remove(&view->foreign_close_request.link);
    wl_list_remove(&view->foreign_destroy.link);
}

void view_map(struct wsm_view *view, struct wlr_surface *wlr_surface,
              bool fullscreen, struct wlr_output *fullscreen_output,
              bool decoration) {
    if (!wsm_assert(view->surface == NULL, "cannot map mapped view")) {
        return;
    }
    view->surface = wlr_surface;
    view_populate_pid(view);
    view->container = container_create(view);

    // If there is a request to be opened fullscreen on a specific output, try
    // to honor that request. Otherwise, fallback to assigns, pid mappings,
    // focused workspace, etc
    struct wsm_workspace *ws = NULL;
    if (fullscreen_output && fullscreen_output->data) {
        struct wsm_output *output = fullscreen_output->data;
        ws = output_get_active_workspace(output);
    }
    if (!ws) {
        ws = select_workspace(view);
    }

    struct wsm_seat *seat = input_manager_current_seat();
    struct wsm_node *node =
        seat_get_focus_inactive(seat, ws ? &ws->node : &global_server.wsm_scene->node);
    struct wsm_container *target_sibling = NULL;
    if (node && node->type == N_CONTAINER) {
        if (container_is_floating(node->wsm_container)) {
            // If we're about to launch the view into the floating container, then
            // launch it as a tiled view instead.
            if (ws) {
                target_sibling = seat_get_focus_inactive_tiling(seat, ws);
                if (target_sibling) {
                    struct wsm_container *con =
                        seat_get_focus_inactive_view(seat, &target_sibling->node);
                    if (con)  {
                        target_sibling = con;
                    }
                }
            } else {
                ws = seat_get_last_known_workspace(seat);
            }
        } else {
            target_sibling = node->wsm_container;
        }
    }

    struct wlr_ext_foreign_toplevel_handle_v1_state foreign_toplevel_state = {
        .app_id = view_get_app_id(view),
        .title = view_get_title(view),
    };
    view->ext_foreign_toplevel =
        wlr_ext_foreign_toplevel_handle_v1_create(global_server.foreign_toplevel_list, &foreign_toplevel_state);

    view->foreign_toplevel =
        wlr_foreign_toplevel_handle_v1_create(global_server.foreign_toplevel_manager);
    view->foreign_activate_request.notify = handle_foreign_activate_request;
    wl_signal_add(&view->foreign_toplevel->events.request_activate,
                  &view->foreign_activate_request);
    view->foreign_fullscreen_request.notify = handle_foreign_fullscreen_request;
    wl_signal_add(&view->foreign_toplevel->events.request_fullscreen,
                  &view->foreign_fullscreen_request);
    view->foreign_close_request.notify = handle_foreign_close_request;
    wl_signal_add(&view->foreign_toplevel->events.request_close,
                  &view->foreign_close_request);
    view->foreign_destroy.notify = handle_foreign_destroy;
    wl_signal_add(&view->foreign_toplevel->events.destroy,
                  &view->foreign_destroy);

    struct wsm_container *container = view->container;
    if (target_sibling) {
        container_add_sibling(target_sibling, container, 1);
    } else if (ws) {
        container = workspace_add_tiling(ws, container);
    }

    if (decoration) {
        view_update_csd_from_client(view, decoration);
    }

    view->container->pending.border = global_config.floating_border;
    view->container->pending.border_thickness = global_config.floating_border_thickness;
    view->container->pending.sensing_thickness = global_config.sensing_border_thickness;
    container_set_floating(view->container, true);

    if (global_config.popup_during_fullscreen == POPUP_LEAVE &&
        container->pending.workspace &&
        container->pending.workspace->fullscreen &&
        container->pending.workspace->fullscreen->view) {
        struct wsm_container *fs = container->pending.workspace->fullscreen;
        if (view_is_transient_for(view, fs->view)) {
            container_set_fullscreen(fs, false);
        }
    }

    view_update_title(view, false);
    container_update_representation(container);

    if (fullscreen) {
        container_set_fullscreen(view->container, true);
        wsm_arrange_workspace_auto(view->container->pending.workspace);
    } else {
        if (container->pending.parent) {
            wsm_arrange_container_auto(container->pending.parent);
        } else if (container->pending.workspace) {
            wsm_arrange_workspace_auto(container->pending.workspace);
        }
    }

    // TODO: container_set_floating
    bool floating = true;
    if (!container && ws->tiling->length == 0) {
        floating = false;
    }

    if (floating && container_is_scratchpad_hidden(container)) {
        floating = false;
    }

    if (container_is_floating_or_child(container)) {
        while (container->pending.parent) {
            container = container->pending.parent;
        }
    }

    if (floating) {
        container_set_floating(container, true);
        if (container->pending.workspace) {
            wsm_arrange_workspace_auto(container->pending.workspace);
        }
    }

    bool set_focus = should_focus(view);

#if HAVE_XWAYLAND
    struct wlr_xwayland_surface *xsurface;
    if ((xsurface = wlr_xwayland_surface_try_from_wlr_surface(wlr_surface))) {
        set_focus &= wlr_xwayland_icccm_input_model(xsurface) !=
                     WLR_ICCCM_INPUT_MODEL_NONE;
    }
#endif

    if (set_focus) {
        input_manager_set_focus(&view->container->node);
    }

    const char *app_id;
    const char *class;
    if ((app_id = view_get_app_id(view)) != NULL) {
        wlr_foreign_toplevel_handle_v1_set_app_id(view->foreign_toplevel, app_id);
    } else if ((class = view_get_class(view)) != NULL) {
        wlr_foreign_toplevel_handle_v1_set_app_id(view->foreign_toplevel, class);
    }
}

void view_unmap(struct wsm_view *view) {
    wl_signal_emit_mutable(&view->events.unmap, view);

    if (view->urgent_timer) {
        wl_event_source_remove(view->urgent_timer);
        view->urgent_timer = NULL;
    }

    if (view->foreign_toplevel) {
        wlr_foreign_toplevel_handle_v1_destroy(view->foreign_toplevel);
        view->foreign_toplevel = NULL;
    }

    struct wsm_container *parent = view->container->pending.parent;
    struct wsm_workspace *ws = view->container->pending.workspace;
    container_begin_destroy(view->container);
    if (parent) {
        container_reap_empty(parent);
    } else if (ws) {
        workspace_consider_destroy(ws);
    }

    if (global_server.wsm_scene->fullscreen_global) {
        arrange_root_auto();
    } else if (ws && !ws->node.destroying) {
        wsm_arrange_workspace_auto(ws);
        workspace_detect_urgent(ws);
    }

    struct wsm_seat *seat;
    wl_list_for_each(seat, &global_server.wsm_input_manager->seats, link) {
        seat->wsm_cursor->image_surface = NULL;
        if (seat->wsm_cursor->active_constraint) {
            struct wlr_surface *constrain_surface =
                seat->wsm_cursor->active_constraint->surface;
            if (view_from_wlr_surface(constrain_surface) == view) {
                wsm_cursor_constrain(seat->wsm_cursor, NULL);
            }
        }
        seat_consider_warp_to_focus(seat);
    }

    transaction_commit_dirty();
    view->surface = NULL;
}

void view_update_size(struct wsm_view *view) {
    struct wsm_container *con = view->container;
    con->pending.content_width = view->geometry.width;
    con->pending.content_height = view->geometry.height;
    container_set_geometry_from_content(con);
}

void view_center_and_clip_surface(struct wsm_view *view) {
    struct wsm_container *con = view->container;

    bool clip_to_geometry = true;

    if (container_is_floating(con)) {
        clip_to_geometry = !view->using_csd;
    } else {
        wlr_scene_node_set_position(&view->content_tree->node, 0, 0);
    }

    // only make sure to clip the content if there is content to clip
    if (!wl_list_empty(&con->view->content_tree->children)) {
        struct wlr_box clip = {0};
        if (clip_to_geometry) {
            clip = (struct wlr_box){
                .x = con->view->geometry.x,
                .y = con->view->geometry.y,
                .width = con->current.content_width,
                .height = con->current.content_height,
            };
        }
        wlr_scene_subsurface_tree_set_clip(&con->view->content_tree->node, &clip);
    }
}

struct wsm_view *view_from_wlr_surface(struct wlr_surface *wlr_surface) {
    struct wlr_xdg_surface *xdg_surface;
    if ((xdg_surface = wlr_xdg_surface_try_from_wlr_surface(wlr_surface))) {
        return view_from_wlr_xdg_surface(xdg_surface);
    }
#if HAVE_XWAYLAND
    struct wlr_xwayland_surface *xsurface;
    if ((xsurface = wlr_xwayland_surface_try_from_wlr_surface(wlr_surface))) {
        return view_from_wlr_xwayland_surface(xsurface);
    }
#endif
    struct wlr_subsurface *subsurface;
    if ((subsurface = wlr_subsurface_try_from_wlr_surface(wlr_surface))) {
        return view_from_wlr_surface(subsurface->parent);
    }
    if (wlr_layer_surface_v1_try_from_wlr_surface(wlr_surface) != NULL) {
        return NULL;
    }

    const char *role = wlr_surface->role ? wlr_surface->role->name : NULL;
    wsm_log(WSM_DEBUG, "Surface of unknown type (role %s): %p",
             role, wlr_surface);
    return NULL;
}

static char *escape_pango_markup(const char *buffer) {
    size_t length = escape_markup_text(buffer, NULL);
    char *escaped_title = calloc(length + 1, sizeof(char));
    escape_markup_text(buffer, escaped_title);
    return escaped_title;
}

static size_t append_prop(char *buffer, const char *value) {
    if (!value) {
        return 0;
    }
    // If using pango_markup in font, we need to escape all markup chars
    // from values to make sure tags are not inserted by clients
    char *escaped_value = escape_pango_markup(value);
    lenient_strcat(buffer, escaped_value);
    size_t len = strlen(escaped_value);
    free(escaped_value);
    return len;
}

/**
 * Calculate and return the length of the formatted title.
 * If buffer is not NULL, also populate the buffer with the formatted title.
 */
static size_t parse_title_format(struct wsm_view *view, char *buffer) {
    if (!view->title_format || strcmp(view->title_format, "%title") == 0) {
        return append_prop(buffer, view_get_title(view));
    }

    size_t len = 0;
    char *format = view->title_format;
    char *next = strchr(format, '%');
    while (next) {
        // Copy everything up to the %
        lenient_strncat(buffer, format, next - format);
        len += next - format;
        format = next;

        if (strncmp(next, "%title", 6) == 0) {
            len += append_prop(buffer, view_get_title(view));
            format += 6;
        } else if (strncmp(next, "%app_id", 7) == 0) {
            len += append_prop(buffer, view_get_app_id(view));
            format += 7;
        } else if (strncmp(next, "%class", 6) == 0) {
            len += append_prop(buffer, view_get_class(view));
            format += 6;
        } else if (strncmp(next, "%instance", 9) == 0) {
            len += append_prop(buffer, view_get_instance(view));
            format += 9;
        } else if (strncmp(next, "%shell", 6) == 0) {
            len += append_prop(buffer, view_get_shell(view));
            format += 6;
        } else {
            lenient_strcat(buffer, "%");
            ++format;
            ++len;
        }
        next = strchr(format, '%');
    }
    lenient_strcat(buffer, format);
    len += strlen(format);

    return len;
}

void view_update_app_id(struct wsm_view *view) {
    const char *app_id = view_get_app_id(view);

    if (view->foreign_toplevel && app_id) {
        wlr_foreign_toplevel_handle_v1_set_app_id(view->foreign_toplevel, app_id);
    }
}

void view_update_title(struct wsm_view *view, bool force) {
    const char *title = view_get_title(view);

    if (!force) {
        if (title && view->container->title &&
            strcmp(title, view->container->title) == 0) {
            return;
        }
        if (!title && !view->container->title) {
            return;
        }
    }

    free(view->container->title);
    free(view->container->formatted_title);

    size_t len = parse_title_format(view, NULL);

    if (len) {
        char *buffer = calloc(len + 1, sizeof(char));
        if (!wsm_assert(buffer, "Unable to allocate title string")) {
            return;
        }

        parse_title_format(view, buffer);
        view->container->formatted_title = buffer;
    } else {
        view->container->formatted_title = NULL;
    }

    view->container->title = title ? strdup(title) : NULL;

    // Update title after the global font height is updated
    if (view->container->title_bar->title_text && len) {
        wsm_text_node_set_text(view->container->title_bar->title_text,
                                view->container->formatted_title);
        container_arrange_title_bar_node(view->container);
    } else {
        container_update_title_bar(view->container);
    }

    if (view->foreign_toplevel && title) {
        wlr_foreign_toplevel_handle_v1_set_title(view->foreign_toplevel, title);
    }
}

bool view_is_visible(struct wsm_view *view) {
    if (view->container->node.destroying) {
        return false;
    }
    struct wsm_workspace *workspace = view->container->pending.workspace;
    if (!workspace && view->container->pending.fullscreen_mode != FULLSCREEN_GLOBAL) {
        bool fs_global_descendant = false;
        struct wsm_container *parent = view->container->pending.parent;
        while (parent) {
            if (parent->pending.fullscreen_mode == FULLSCREEN_GLOBAL) {
                fs_global_descendant = true;
            }
            parent = parent->pending.parent;
        }
        if (!fs_global_descendant) {
            return false;
        }
    }

    if (!container_is_sticky_or_child(view->container) && workspace &&
        !workspace_is_visible(workspace)) {
        return false;
    }
    // Check view isn't in a tabbed or stacked container on an inactive tab
    struct wsm_seat *seat = input_manager_current_seat();
    struct wsm_container *con = view->container;
    while (con) {
        // enum wsm_container_layout layout = container_parent_layout(con);
        if (!container_is_floating(con)) {
            struct wsm_node *parent = con->pending.parent ?
                                           &con->pending.parent->node : &con->pending.workspace->node;
            if (seat_get_active_tiling_child(seat, parent) != &con->node) {
                return false;
            }
        }
        con = con->pending.parent;
    }
    // Check view isn't hidden by another fullscreen view
    struct wsm_container *fs = global_server.wsm_scene->fullscreen_global ?
                                    global_server.wsm_scene->fullscreen_global : workspace->fullscreen;
    if (fs && !container_is_fullscreen_or_child(view->container) &&
        !container_is_transient_for(view->container, fs)) {
        return false;
    }
    return true;
}

void view_set_urgent(struct wsm_view *view, bool enable) {
    if (view_is_urgent(view) == enable) {
        return;
    }
    if (enable) {
        struct wsm_seat *seat = input_manager_current_seat();
        if (seat_get_focused_container(seat) == view->container) {
            return;
        }
        clock_gettime(CLOCK_MONOTONIC, &view->urgent);
        container_update_itself_and_parents(view->container);
    } else {
        view->urgent = (struct timespec){ 0 };
        if (view->urgent_timer) {
            wl_event_source_remove(view->urgent_timer);
            view->urgent_timer = NULL;
        }
    }

    if (!container_is_scratchpad_hidden(view->container)) {
        workspace_detect_urgent(view->container->pending.workspace);
    }
}

bool view_is_urgent(struct wsm_view *view) {
    return view->urgent.tv_sec || view->urgent.tv_nsec;
}

void view_remove_saved_buffer(struct wsm_view *view) {
    if (!wsm_assert(view->saved_surface_tree, "Expected a saved buffer")) {
        return;
    }

    wlr_scene_node_destroy(&view->saved_surface_tree->node);
    view->saved_surface_tree = NULL;
    wlr_scene_node_set_enabled(&view->content_tree->node, true);
}

static void view_save_buffer_iterator(struct wlr_scene_buffer *buffer,
                                      int sx, int sy, void *data) {
    struct wlr_scene_tree *tree = data;

    struct wlr_scene_buffer *sbuf = wlr_scene_buffer_create(tree, NULL);
    if (!sbuf) {
        wsm_log(WSM_ERROR, "Could not allocate a scene buffer when saving a surface");
        return;
    }

    wlr_scene_buffer_set_dest_size(sbuf,
                                   buffer->dst_width, buffer->dst_height);
    wlr_scene_buffer_set_opaque_region(sbuf, &buffer->opaque_region);
    wlr_scene_buffer_set_source_box(sbuf, &buffer->src_box);
    wlr_scene_node_set_position(&sbuf->node, sx, sy);
    wlr_scene_buffer_set_transform(sbuf, buffer->transform);
    wlr_scene_buffer_set_buffer(sbuf, buffer->buffer);
}

void view_save_buffer(struct wsm_view *view) {
    if (!wsm_assert(!view->saved_surface_tree, "Didn't expect saved buffer")) {
        view_remove_saved_buffer(view);
    }

    view->saved_surface_tree = wlr_scene_tree_create(view->scene_tree);
    if (!view->saved_surface_tree) {
        wsm_log(WSM_ERROR, "Could not allocate a scene tree node when saving a surface");
        return;
    }

    // Enable and disable the saved surface tree like so to atomitaclly update
    // the tree. This will prevent over damaging or other weirdness.
    wlr_scene_node_set_enabled(&view->saved_surface_tree->node, false);

    wlr_scene_node_for_each_buffer(&view->content_tree->node,
                                   view_save_buffer_iterator, view->saved_surface_tree);

    wlr_scene_node_set_enabled(&view->content_tree->node, false);
    wlr_scene_node_set_enabled(&view->saved_surface_tree->node, true);
}

bool view_is_transient_for(struct wsm_view *child,
                           struct wsm_view *ancestor) {
    return child->impl->is_transient_for &&
           child->impl->is_transient_for(child, ancestor);
}

static void send_frame_done_iterator(struct wlr_scene_buffer *scene_buffer,
                                     int x, int y, void *data) {
    struct timespec *when = data;
    wl_signal_emit_mutable(&scene_buffer->events.frame_done, when);
}

void view_send_frame_done(struct wsm_view *view) {
    struct timespec when;
    clock_gettime(CLOCK_MONOTONIC, &when);

    struct wlr_scene_node *node;
    wl_list_for_each(node, &view->content_tree->children, link) {
        wlr_scene_node_for_each_buffer(node, send_frame_done_iterator, &when);
    }
}
