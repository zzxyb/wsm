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

#include "wsm_layer_shell.h"
#include "wsm_server.h"
#include "wsm_log.h"
#include "wsm_scene.h"
#include "wsm_seat.h"
#include "wsm_output.h"
#include "wsm_cursor.h"
#include "wsm_transaction.h"
#include "wsm_input_manager.h"
#include "wsm_workspace.h"
#include "node/wsm_node_descriptor.h"

#include <stdlib.h>

#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_subcompositor.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/types/wlr_fractional_scale_v1.h>

#define WSM_LAYER_SHELL_VERSION 4

static struct wlr_scene_tree *wsm_layer_get_scene(struct wsm_output *output,
                                                   enum zwlr_layer_shell_v1_layer type) {
    switch (type) {
    case ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND:
        return output->layers.shell_background;
    case ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM:
        return output->layers.shell_bottom;
    case ZWLR_LAYER_SHELL_V1_LAYER_TOP:
        return output->layers.shell_top;
    case ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY:
        return output->layers.shell_overlay;
    }

    wsm_assert(false, "unreachable");
    return NULL;
}

static struct wsm_layer_surface *wsm_layer_surface_create(
    struct wlr_scene_layer_surface_v1 *scene) {
    struct wsm_layer_surface *surface = calloc(1, sizeof(*surface));
    if (!surface) {
        wsm_log(WSM_ERROR, "Could not allocate a scene_layer surface");
        return NULL;
    }

    struct wlr_scene_tree *popups = wlr_scene_tree_create(global_server.wsm_scene->layers.popup);
    if (!popups) {
        wsm_log(WSM_ERROR, "Could not allocate a scene_layer popup node");
        free(surface);
        return NULL;
    }

    surface->desc.relative = &scene->tree->node;

    if (!wsm_scene_descriptor_assign(&popups->node,
                                 WSM_SCENE_DESC_POPUP, &surface->desc)) {
        wsm_log(WSM_ERROR, "Failed to allocate a popup scene descriptor");
        wlr_scene_node_destroy(&popups->node);
        free(surface);
        return NULL;
    }

    surface->tree = scene->tree;
    surface->scene = scene;
    surface->layer_surface = scene->layer_surface;
    surface->popups = popups;
    surface->layer_surface->data = surface;

    return surface;
}

static void handle_surface_commit(struct wl_listener *listener, void *data) {
    struct wsm_layer_surface *surface =
        wl_container_of(listener, surface, surface_commit);

    struct wlr_layer_surface_v1 *layer_surface = surface->layer_surface;
    if (!layer_surface->initialized) {
        return;
    }

    uint32_t committed = layer_surface->current.committed;
    if (committed & WLR_LAYER_SURFACE_V1_STATE_LAYER) {
        enum zwlr_layer_shell_v1_layer layer_type = layer_surface->current.layer;
        struct wlr_scene_tree *output_layer = wsm_layer_get_scene(
            surface->output, layer_type);
        wlr_scene_node_reparent(&surface->scene->tree->node, output_layer);
    }

    if (layer_surface->initial_commit || committed || layer_surface->surface->mapped != surface->mapped) {
        surface->mapped = layer_surface->surface->mapped;
        arrange_layers(surface->output);
        transaction_commit_dirty();
    }
}

static void handle_map(struct wl_listener *listener, void *data) {
    struct wsm_layer_surface *surface = wl_container_of(listener,
                                                         surface, map);

    struct wlr_layer_surface_v1 *layer_surface =
        surface->scene->layer_surface;

    // focus on new surface
    if (layer_surface->current.keyboard_interactive &&
        (layer_surface->current.layer == ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY ||
         layer_surface->current.layer == ZWLR_LAYER_SHELL_V1_LAYER_TOP)) {
        struct wsm_seat *seat;
        wl_list_for_each(seat, &global_server.wsm_input_manager->seats, link) {
            // but only if the currently focused layer has a lower precedence
            if (!seat->focused_layer ||
                seat->focused_layer->current.layer >= layer_surface->current.layer) {
                seat_set_focus_layer(seat, layer_surface);
            }
        }
        arrange_layers(surface->output);
    }

    cursor_rebase_all();
}

static void handle_unmap(struct wl_listener *listener, void *data) {
    struct wsm_layer_surface *surface = wl_container_of(
        listener, surface, unmap);
    struct wsm_seat *seat;
    wl_list_for_each(seat, &global_server.wsm_input_manager->seats, link) {
        if (seat->focused_layer == surface->layer_surface) {
            seat_set_focus_layer(seat, NULL);
        }
    }

    cursor_rebase_all();
}

static void popup_unconstrain(struct wsm_layer_popup *popup) {
    struct wlr_xdg_popup *wlr_popup = popup->wlr_popup;
    struct wsm_output *output = popup->toplevel->output;

    // if a client tries to create a popup while we are in the process of destroying
    // its output, don't crash.
    if (!output) {
        return;
    }

    int lx, ly;
    wlr_scene_node_coords(&popup->toplevel->scene->tree->node, &lx, &ly);

    // the output box expressed in the coordinate system of the toplevel parent
    // of the popup
    struct wlr_box output_toplevel_sx_box = {
        .x = output->lx - lx,
        .y = output->ly - ly,
        .width = output->width,
        .height = output->height,
    };

    wlr_xdg_popup_unconstrain_from_box(wlr_popup, &output_toplevel_sx_box);
}

static void popup_handle_destroy(struct wl_listener *listener, void *data) {
    struct wsm_layer_popup *popup =
        wl_container_of(listener, popup, destroy);

    wl_list_remove(&popup->destroy.link);
    wl_list_remove(&popup->new_popup.link);
    wl_list_remove(&popup->commit.link);
    free(popup);
}

static void popup_handle_commit(struct wl_listener *listener, void *data) {
    struct wsm_layer_popup *popup = wl_container_of(listener, popup, commit);
    if (popup->wlr_popup->base->initial_commit) {
        popup_unconstrain(popup);
    }
}

static void popup_handle_new_popup(struct wl_listener *listener, void *data);

static struct wsm_layer_popup *create_popup(struct wlr_xdg_popup *wlr_popup,
                                             struct wsm_layer_surface *toplevel, struct wlr_scene_tree *parent) {
    struct wsm_layer_popup *popup = calloc(1, sizeof(struct wsm_layer_popup));
    if (!wsm_assert(popup, "Could not create wsm_layer_popup: allocation failed!")) {
        return NULL;
    }

    popup->toplevel = toplevel;
    popup->wlr_popup = wlr_popup;
    popup->scene = wlr_scene_xdg_surface_create(parent,
                                                wlr_popup->base);

    if (!popup->scene) {
        free(popup);
        return NULL;
    }

    popup->destroy.notify = popup_handle_destroy;
    wl_signal_add(&wlr_popup->base->events.destroy, &popup->destroy);
    popup->new_popup.notify = popup_handle_new_popup;
    wl_signal_add(&wlr_popup->base->events.new_popup, &popup->new_popup);
    popup->commit.notify = popup_handle_commit;
    wl_signal_add(&wlr_popup->base->surface->events.commit, &popup->commit);

    return popup;
}

static void popup_handle_new_popup(struct wl_listener *listener, void *data) {
    struct wsm_layer_popup *wsm_layer_popup =
        wl_container_of(listener, wsm_layer_popup, new_popup);
    struct wlr_xdg_popup *wlr_popup = data;
    create_popup(wlr_popup, wsm_layer_popup->toplevel, wsm_layer_popup->scene);
}

static void handle_new_popup(struct wl_listener *listener, void *data) {
    struct wsm_layer_surface *wsm_layer_surface =
        wl_container_of(listener, wsm_layer_surface, new_popup);
    struct wlr_xdg_popup *wlr_popup = data;
    create_popup(wlr_popup, wsm_layer_surface, wsm_layer_surface->popups);
}

static void handle_output_destroy(struct wl_listener *listener, void *data) {
    struct wsm_layer_surface *layer =
        wl_container_of(listener, layer, output_destroy);

    layer->output = NULL;
    wlr_scene_node_destroy(&layer->scene->tree->node);
}

static struct wsm_layer_surface *find_mapped_layer_by_client(
    struct wl_client *client, struct wsm_output *ignore_output) {
    for (int i = 0; i < global_server.wsm_scene->outputs->length; ++i) {
        struct wsm_output *output = global_server.wsm_scene->outputs->items[i];
        if (output == ignore_output) {
            continue;
        }
        // For now we'll only check the overlay layer
        struct wlr_scene_node *node;
        wl_list_for_each (node, &output->layers.shell_overlay->children, link) {
            struct wsm_layer_surface *surface = wsm_scene_descriptor_try_get(node,
                                                                          WSM_SCENE_DESC_LAYER_SHELL);
            if (!surface) {
                continue;
            }

            struct wlr_layer_surface_v1 *layer_surface = surface->layer_surface;
            struct wl_resource *resource = layer_surface->resource;
            if (wl_resource_get_client(resource) == client
                && layer_surface->surface->mapped) {
                return surface;
            }
        }
    }
    return NULL;
}

static void handle_node_destroy(struct wl_listener *listener, void *data) {
    struct wsm_layer_surface *layer =
        wl_container_of(listener, layer, node_destroy);

    // destroy the scene descriptor straight away if it exists, otherwise
    // we will try to reflow still considering the destroyed node.
    wsm_scene_descriptor_destroy(&layer->tree->node, WSM_SCENE_DESC_LAYER_SHELL);

    // Determine if this layer is being used by an exclusive client. If it is,
    // try and find another layer owned by this client to pass focus to.
    struct wsm_seat *seat = input_manager_get_default_seat();
    struct wl_client *client =
        wl_resource_get_client(layer->layer_surface->resource);
    // if (!server.session_lock.lock) {
        struct wsm_layer_surface *consider_layer =
            find_mapped_layer_by_client(client, layer->output);
        if (consider_layer) {
            seat_set_focus_layer(seat, consider_layer->layer_surface);
        }
    // }

    if (layer->output) {
        arrange_layers(layer->output);
        transaction_commit_dirty();
    }

    wlr_scene_node_destroy(&layer->popups->node);

    wl_list_remove(&layer->map.link);
    wl_list_remove(&layer->unmap.link);
    wl_list_remove(&layer->surface_commit.link);
    wl_list_remove(&layer->node_destroy.link);
    wl_list_remove(&layer->output_destroy.link);

    layer->layer_surface->data = NULL;

    free(layer);
}

void handle_layer_shell_surface(struct wl_listener *listener, void *data) {
    struct wlr_layer_surface_v1 *layer_surface = data;
    wsm_log(WSM_DEBUG, "new layer surface: namespace %s layer %d anchor %" PRIu32
                         " size %" PRIu32 "x%" PRIu32 " margin %" PRIu32 ",%" PRIu32 ",%" PRIu32 ",%" PRIu32 ",",
             layer_surface->namespace,
             layer_surface->pending.layer,
             layer_surface->pending.anchor,
             layer_surface->pending.desired_width,
             layer_surface->pending.desired_height,
             layer_surface->pending.margin.top,
             layer_surface->pending.margin.right,
             layer_surface->pending.margin.bottom,
             layer_surface->pending.margin.left);

    if (!layer_surface->output) {
        // Assign last active output
        struct wsm_output *output = NULL;
        struct wsm_seat *seat = input_manager_get_default_seat();
        if (seat) {
            struct wsm_workspace *ws = seat_get_focused_workspace(seat);
            if (ws != NULL) {
                wsm_log(WSM_ERROR, "seat_get_focused_workspace failed!");
                output = ws->output;
            }
        }
        if (!output || output == global_server.wsm_scene->fallback_output) {
            if (!global_server.wsm_scene->outputs->length) {
                wsm_log(WSM_ERROR,
                         "no output to auto-assign layer surface '%s' to",
                         layer_surface->namespace);
                wlr_layer_surface_v1_destroy(layer_surface);
                return;
            }
            output = global_server.wsm_scene->outputs->items[0];
        }
        layer_surface->output = output->wlr_output;
    }

    struct wsm_output *output = layer_surface->output->data;

    enum zwlr_layer_shell_v1_layer layer_type = layer_surface->pending.layer;
    struct wlr_scene_tree *output_layer = wsm_layer_get_scene(
        output, layer_type);
    struct wlr_scene_layer_surface_v1 *scene_surface =
        wlr_scene_layer_surface_v1_create(output_layer, layer_surface);
    if (!scene_surface) {
        wsm_log(WSM_ERROR, "Could not allocate a layer_surface_v1");
        return;
    }

    struct wsm_layer_surface *surface =
        wsm_layer_surface_create(scene_surface);
    if (!surface) {
        wlr_layer_surface_v1_destroy(layer_surface);

        wsm_log(WSM_ERROR, "Could not allocate a wsm_layer_surface");
        return;
    }

    if (!wsm_scene_descriptor_assign(&scene_surface->tree->node,
                                 WSM_SCENE_DESC_LAYER_SHELL, surface)) {
        wsm_log(WSM_ERROR, "Failed to allocate a layer surface descriptor");
        // destroying the layer_surface will also destroy its corresponding
        // scene node
        wlr_layer_surface_v1_destroy(layer_surface);
        return;
    }

    surface->output = output;

    // now that the surface's output is known, we can advertise its scale
    wlr_fractional_scale_v1_notify_scale(surface->layer_surface->surface,
                                         layer_surface->output->scale);
    wlr_surface_set_preferred_buffer_scale(surface->layer_surface->surface,
                                           ceil(layer_surface->output->scale));

    surface->surface_commit.notify = handle_surface_commit;
    wl_signal_add(&layer_surface->surface->events.commit,
                  &surface->surface_commit);
    surface->map.notify = handle_map;
    wl_signal_add(&layer_surface->surface->events.map, &surface->map);
    surface->unmap.notify = handle_unmap;
    wl_signal_add(&layer_surface->surface->events.unmap, &surface->unmap);
    surface->new_popup.notify = handle_new_popup;
    wl_signal_add(&layer_surface->events.new_popup, &surface->new_popup);

    surface->output_destroy.notify = handle_output_destroy;
    wl_signal_add(&output->events.disable, &surface->output_destroy);

    surface->node_destroy.notify = handle_node_destroy;
    wl_signal_add(&scene_surface->tree->node.events.destroy, &surface->node_destroy);
}

struct wsm_layer_shell *wsm_layer_shell_create(const struct wsm_server *server) {
    struct wsm_layer_shell *layer_shell = calloc(1, sizeof(struct wsm_layer_shell));
    if (!wsm_assert(layer_shell, "Could not create wsm_layer_shell: allocation failed!")) {
        return NULL;
    }

    layer_shell->wlr_layer_shell = wlr_layer_shell_v1_create(server->wl_display,
                                                    WSM_LAYER_SHELL_VERSION);
    layer_shell->layer_shell_surface.notify = handle_layer_shell_surface;
    wl_signal_add(&layer_shell->wlr_layer_shell->events.new_surface,
                  &layer_shell->layer_shell_surface);

    return layer_shell;
}

static void arrange_surface(struct wsm_output *output, const struct wlr_box *full_area,
                            struct wlr_box *usable_area, struct wlr_scene_tree *tree) {
    struct wlr_scene_node *node;
    wl_list_for_each(node, &tree->children, link) {
        struct wsm_layer_surface *surface = wsm_scene_descriptor_try_get(node,
                                                                      WSM_SCENE_DESC_LAYER_SHELL);
        // surface could be null during destruction
        if (!surface) {
            continue;
        }

        if (!surface->scene->layer_surface->initialized) {
            continue;
        }

        wlr_scene_layer_surface_v1_configure(surface->scene, full_area, usable_area);
    }
}

void arrange_layers(struct wsm_output *output) {
    struct wlr_box usable_area = { 0 };
    wlr_output_effective_resolution(output->wlr_output,
                                    &usable_area.width, &usable_area.height);
    const struct wlr_box full_area = usable_area;

    arrange_surface(output, &full_area, &usable_area, output->layers.shell_background);
    arrange_surface(output, &full_area, &usable_area, output->layers.shell_bottom);
    arrange_surface(output, &full_area, &usable_area, output->layers.shell_top);
    arrange_surface(output, &full_area, &usable_area, output->layers.shell_overlay);

    if (!wlr_box_equal(&usable_area, &output->usable_area)) {
        wsm_log(WSM_DEBUG, "Usable area changed, rearranging output");
        output->usable_area = usable_area;
        arrange_output(output);
    } else {
        arrange_popups(global_server.wsm_scene->layers.popup);
    }
}

struct wlr_layer_surface_v1 *toplevel_layer_surface_from_surface(
    struct wlr_surface *surface) {
    struct wlr_layer_surface_v1 *layer;
    do {
        if (!surface) {
            return NULL;
        }
        // Topmost layer surface
        if ((layer = wlr_layer_surface_v1_try_from_wlr_surface(surface))) {
            return layer;
        }
        // Layer subsurface
        if (wlr_subsurface_try_from_wlr_surface(surface)) {
            surface = wlr_surface_get_root_surface(surface);
            continue;
        }

        // Layer surface popup
        struct wlr_xdg_surface *xdg_surface = NULL;
        if ((xdg_surface = wlr_xdg_surface_try_from_wlr_surface(surface)) &&
            xdg_surface->role == WLR_XDG_SURFACE_ROLE_POPUP && xdg_surface->popup != NULL) {
            if (!xdg_surface->popup->parent) {
                return NULL;
            }
            surface = wlr_surface_get_root_surface(xdg_surface->popup->parent);
            continue;
        }

        // Return early if the surface is not a layer/xdg_popup/sub surface
        return NULL;
    } while (true);
}
