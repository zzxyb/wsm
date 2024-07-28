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
#include "wsm_seat.h"
#include "wsm_cursor.h"
#include "wsm_server.h"
#include "wsm_scene.h"
#include "wsm_view.h"
#include "wsm_list.h"
#include "wsm_common.h"
#include "wsm_output.h"
#include "wsm_arrange.h"
#include "wsm_workspace.h"
#include "wsm_transaction.h"
#include "wsm_input_manager.h"
#include "wsm_output_manager.h"
#include "wsm_workspace_manager.h"
#include "wsm_layer_shell.h"
#include "wsm_output_config.h"
#include "node/wsm_node_descriptor.h"

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include <pixman.h>

#include <wlr/backend/x11.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_output.h>
#include <wlr/backend/headless.h>
#include <wlr/backend/wayland.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/backend/drm.h>
#include <wlr/types/wlr_drm_lease_v1.h>
#include <wlr/types/wlr_gamma_control_v1.h>
#include <wlr/types/wlr_output_power_management_v1.h>

struct send_frame_done_data {
    struct timespec when;
    int msec_until_refresh;
    struct wsm_output *output;
};

struct buffer_timer {
    struct wl_listener destroy;
    struct wl_event_source *frame_done_timer;
};

static void begin_destroy(struct wsm_output *output) {
    if (output->enabled) {
        output_disable(output);
    }

    output_begin_destroy(output);

    wl_list_remove(&output->link);

    wl_list_remove(&output->layout_destroy.link);
    wl_list_remove(&output->destroy.link);
    wl_list_remove(&output->commit.link);
    wl_list_remove(&output->present.link);
    wl_list_remove(&output->frame.link);
    wl_list_remove(&output->request_state.link);

    wlr_scene_output_destroy(output->scene_output);
    output->scene_output = NULL;
    output->wlr_output->data = NULL;
    output->wlr_output = NULL;

    request_modeset();
}

static void handle_layout_destroy(struct wl_listener *listener, void *data) {
    struct wsm_output *output = wl_container_of(listener, output, layout_destroy);
    begin_destroy(output);
}

static void handle_destroy(struct wl_listener *listener, void *data) {
    struct wsm_output *output = wl_container_of(listener, output, destroy);
    begin_destroy(output);
}

static void handle_commit(struct wl_listener *listener, void *data) {
    struct wsm_output *output = wl_container_of(listener, output, commit);
    struct wlr_output_event_commit *event = data;

    if (!output->enabled) {
        return;
    }

    if (event->state->committed & (
            WLR_OUTPUT_STATE_MODE |
            WLR_OUTPUT_STATE_TRANSFORM |
            WLR_OUTPUT_STATE_SCALE)) {
        wsm_arrange_layers(output);
        wsm_arrange_output_auto(output);

        update_output_manager_config(&global_server);
    }

    // Next time the output is enabled, try to re-apply the gamma LUT
    if ((event->state->committed & WLR_OUTPUT_STATE_ENABLED) && !output->wlr_output->enabled) {
        output->gamma_lut_changed = true;
    }
}


static void handle_present(struct wl_listener *listener, void *data) {
    struct wsm_output *output = wl_container_of(listener, output, present);
    struct wlr_output_event_present *output_event = data;

    if (!output->enabled || !output_event->presented) {
        return;
    }

    output->last_presentation = *output_event->when;
    output->refresh_nsec = output_event->refresh;
}

static int handle_buffer_timer(void *data) {
    struct wlr_scene_buffer *buffer = data;

    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    wlr_scene_buffer_send_frame_done(buffer, &now);
    return 0;
}

static void handle_buffer_timer_destroy(struct wl_listener *listener,
                                        void *data) {
    struct buffer_timer *timer = wl_container_of(listener, timer, destroy);

    wl_list_remove(&timer->destroy.link);
    wl_event_source_remove(timer->frame_done_timer);
    free(timer);
}

static struct buffer_timer *buffer_timer_get_or_create(struct wlr_scene_buffer *buffer) {
    struct buffer_timer *timer =
        wsm_scene_descriptor_try_get(&buffer->node, WSM_SCENE_DESC_BUFFER_TIMER);
    if (timer) {
        return timer;
    }

    timer = calloc(1, sizeof(struct buffer_timer));
    if (!timer) {
        return NULL;
    }

    timer->frame_done_timer = wl_event_loop_add_timer(global_server.wl_event_loop,
                                                      handle_buffer_timer, buffer);
    if (!timer->frame_done_timer) {
        free(timer);
        return NULL;
    }

    wsm_scene_descriptor_assign(&buffer->node, WSM_SCENE_DESC_BUFFER_TIMER, timer);

    timer->destroy.notify = handle_buffer_timer_destroy;
    wl_signal_add(&buffer->node.events.destroy, &timer->destroy);

    return timer;
}

static void send_frame_done_iterator(struct wlr_scene_buffer *buffer,
                                     int x, int y, void *user_data) {
    struct send_frame_done_data *data = user_data;
    struct wsm_output *output = data->output;
    int view_max_render_time = 0;

    if (buffer->primary_output != data->output->scene_output) {
        return;
    }

    struct wlr_scene_node *current = &buffer->node;
    while (true) {
        struct wsm_view *view = wsm_scene_descriptor_try_get(current,
                                                          WSM_SCENE_DESC_VIEW);
        if (view) {
            view_max_render_time = view->max_render_time;
            break;
        }

        if (!current->parent) {
            break;
        }

        current = &current->parent->node;
    }

    int delay = data->msec_until_refresh - output->max_render_time
                - view_max_render_time;

    struct buffer_timer *timer = NULL;

    if (output->max_render_time != 0 && view_max_render_time != 0 && delay > 0) {
        timer = buffer_timer_get_or_create(buffer);
    }

    if (timer) {
        wl_event_source_timer_update(timer->frame_done_timer, delay);
    } else {
        wlr_scene_buffer_send_frame_done(buffer, &data->when);
    }
}

static enum wlr_scale_filter_mode get_scale_filter(struct wsm_output *output,
                                                   struct wlr_scene_buffer *buffer) {
    // if we are scaling down, we should always choose linear
    if (buffer->dst_width > 0 && buffer->dst_height > 0) {
        return WLR_SCALE_FILTER_BILINEAR;
    }

    switch (output->scale_filter) {
    case SCALE_FILTER_LINEAR:
        return WLR_SCALE_FILTER_BILINEAR;
    case SCALE_FILTER_NEAREST:
        return WLR_SCALE_FILTER_NEAREST;
    default:
        abort(); // unreachable
    }
}

static void output_configure_scene(struct wsm_output *output,
                                   struct wlr_scene_node *node, float opacity) {
    if (!node->enabled) {
        return;
    }

    struct wsm_container *con =
        wsm_scene_descriptor_try_get(node, WSM_SCENE_DESC_CONTAINER);
    if (con) {
        opacity = con->alpha;
    }

    if (node->type == WLR_SCENE_NODE_BUFFER) {
        struct wlr_scene_buffer *buffer = wlr_scene_buffer_from_node(node);

        // hack: don't call the scene setter because that will damage all outputs
        // We don't want to damage outputs that aren't our current output that
        // we're configuring
        buffer->filter_mode = get_scale_filter(output, buffer);

        wlr_scene_buffer_set_opacity(buffer, opacity);
    } else if (node->type == WLR_SCENE_NODE_TREE) {
        struct wlr_scene_tree *tree = wlr_scene_tree_from_node(node);
        struct wlr_scene_node *node;
        wl_list_for_each(node, &tree->children, link) {
            output_configure_scene(output, node, opacity);
        }
    }
}

static int output_repaint_timer_handler(void *data) {
    struct wsm_output *output = data;

    if (!output->enabled) {
        return 0;
    }

    output->wlr_output->frame_pending = false;

    output_configure_scene(output, &global_server.wsm_scene->root_scene->tree.node, 1.0f);

    if (output->gamma_lut_changed) {
        struct wlr_output_state pending;
        wlr_output_state_init(&pending);
        if (!wlr_scene_output_build_state(output->scene_output, &pending, NULL)) {
            return 0;
        }

        output->gamma_lut_changed = false;
        struct wlr_gamma_control_v1 *gamma_control =
            wlr_gamma_control_manager_v1_get_control(
            global_server.wsm_output_manager->wlr_gamma_control_manager_v1, output->wlr_output);
        if (!wlr_gamma_control_v1_apply(gamma_control, &pending)) {
            wlr_output_state_finish(&pending);
            return 0;
        }

        if (!wlr_output_commit_state(output->wlr_output, &pending)) {
            wlr_gamma_control_v1_send_failed_and_destroy(gamma_control);
            wlr_output_state_finish(&pending);
            return 0;
        }

        wlr_output_state_finish(&pending);
        return 0;
    }

    // TODO: Need to refactor for post effect
    wlr_scene_output_commit(output->scene_output, NULL);
    return 0;
}

static void handle_frame(struct wl_listener *listener, void *user_data) {
    struct wsm_output *output =
        wl_container_of(listener, output, frame);
    if (!output->enabled || !output->wlr_output->enabled) {
        return;
    }

    // Compute predicted milliseconds until the next refresh. It's used for
    // delaying both output rendering and surface frame callbacks.
    int msec_until_refresh = 0;

    if (output->max_render_time != 0) {
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);

        const long NSEC_IN_SECONDS = 1000000000;
        struct timespec predicted_refresh = output->last_presentation;
        predicted_refresh.tv_nsec += output->refresh_nsec % NSEC_IN_SECONDS;
        predicted_refresh.tv_sec += output->refresh_nsec / NSEC_IN_SECONDS;
        if (predicted_refresh.tv_nsec >= NSEC_IN_SECONDS) {
            predicted_refresh.tv_sec += 1;
            predicted_refresh.tv_nsec -= NSEC_IN_SECONDS;
        }

        // If the predicted refresh time is before the current time then
        // there's no point in delaying.
        //
        // We only check tv_sec because if the predicted refresh time is less
        // than a second before the current time, then msec_until_refresh will
        // end up slightly below zero, which will effectively disable the delay
        // without potential disastrous negative overflows that could occur if
        // tv_sec was not checked.
        if (predicted_refresh.tv_sec >= now.tv_sec) {
            long nsec_until_refresh
                = (predicted_refresh.tv_sec - now.tv_sec) * NSEC_IN_SECONDS
                  + (predicted_refresh.tv_nsec - now.tv_nsec);

            // We want msec_until_refresh to be conservative, that is, floored.
            // If we have 7.9 msec until refresh, we better compute the delay
            // as if we had only 7 msec, so that we don't accidentally delay
            // more than necessary and miss a frame.
            msec_until_refresh = nsec_until_refresh / 1000000;
        }
    }

    int delay = msec_until_refresh - output->max_render_time;

    // If the delay is less than 1 millisecond (which is the least we can wait)
    // then just render right away.
    if (delay < 1) {
        output_repaint_timer_handler(output);
    } else {
        output->wlr_output->frame_pending = true;
        wl_event_source_timer_update(output->repaint_timer, delay);
    }

    // Send frame done to all visible surfaces
    struct send_frame_done_data data = {0};
    clock_gettime(CLOCK_MONOTONIC, &data.when);
    data.msec_until_refresh = msec_until_refresh;
    data.output = output;
    wlr_scene_output_for_each_buffer(output->scene_output, send_frame_done_iterator, &data);
}

static void handle_request_state(struct wl_listener *listener, void *data) {
    struct wsm_output *output = wl_container_of(listener, output, request_state);
    const struct wlr_output_event_request_state *event = data;
    wlr_output_commit_state(output->wlr_output, event->state);
}

static void destroy_scene_layers(struct wsm_output *output) {
    wlr_scene_node_destroy(&output->fullscreen_background->node);

    scene_node_disown_children(output->layers.tiling);
    scene_node_disown_children(output->layers.fullscreen);

    wlr_scene_node_destroy(&output->layers.shell_background->node);
    wlr_scene_node_destroy(&output->layers.shell_bottom->node);
    wlr_scene_node_destroy(&output->layers.tiling->node);
    wlr_scene_node_destroy(&output->layers.fullscreen->node);
    wlr_scene_node_destroy(&output->layers.shell_top->node);
    wlr_scene_node_destroy(&output->layers.shell_overlay->node);
    wlr_scene_node_destroy(&output->layers.session_lock->node);
    wlr_scene_node_destroy(&output->layers.osd->node);
    wlr_scene_node_destroy(&output->layers.water_mark->node);
    wlr_scene_node_destroy(&output->layers.black_screen->node);
}

static unsigned int last_headless_num = 0;

struct wsm_output *wsm_ouput_create(struct wlr_output *wlr_output) {
    struct wsm_output *output = calloc(1, sizeof(struct wsm_output));
    if (!wsm_assert(output, "Could not create wsm_output: allocation failed!")) {
        return NULL;
    }

    node_init(&output->node, N_OUTPUT, output);

    if (wlr_output_is_headless(wlr_output)) {
        char name[64];
        snprintf(name, sizeof(name), "HEADLESS-%u", ++last_headless_num);
        wlr_output_set_name(wlr_output, name);
    }

    if (wlr_output_is_wl(wlr_output) ||
        wlr_output_is_x11(wlr_output)) {
        char *title = NULL;
        char wl_title[32];
        if (title == NULL) {
            if (snprintf(wl_title, sizeof(wl_title), "wsm - %s", wlr_output->name) <= 0) {
                wsm_log(WSM_INFO, "snprintf failed!");
            }
            title = wl_title;
        }
        wlr_wl_output_set_title(wlr_output, title);
    }

    bool failed = false;
    output->layers.shell_background = alloc_scene_tree(global_server.wsm_scene->staging, &failed);
    output->layers.shell_bottom = alloc_scene_tree(global_server.wsm_scene->staging, &failed);
    output->layers.tiling = alloc_scene_tree(global_server.wsm_scene->staging, &failed);
    output->layers.fullscreen = alloc_scene_tree(global_server.wsm_scene->staging, &failed);
    output->layers.shell_top = alloc_scene_tree(global_server.wsm_scene->staging, &failed);
    output->layers.shell_overlay = alloc_scene_tree(global_server.wsm_scene->staging, &failed);
    output->layers.session_lock = alloc_scene_tree(global_server.wsm_scene->staging, &failed);
    output->layers.osd = alloc_scene_tree(global_server.wsm_scene->staging, &failed);
    output->layers.water_mark = alloc_scene_tree(global_server.wsm_scene->staging, &failed);
    output->layers.black_screen = alloc_scene_tree(global_server.wsm_scene->staging, &failed);

    if (!failed) {
        output->fullscreen_background = wlr_scene_rect_create(
            output->layers.fullscreen, 0, 0, (float[4]){0.f, 0.f, 0.f, 1.f});

        if (!output->fullscreen_background) {
            wsm_log(WSM_ERROR, "Unable to allocate a black background rect");
            failed = true;
        }
    }

    if (failed) {
        goto out;
    }

    output->wlr_output = wlr_output;
    wlr_output->data = output;
    output->detected_subpixel = wlr_output->subpixel;
    output->scale_filter = SCALE_FILTER_NEAREST;

    output->workspace_manager = calloc(1, sizeof(struct wsm_workspace_manager));
    if (!wsm_assert(output, "Could not create wsm_workspace_manager: allocation failed!")) {
        goto out;
    }

    output->workspace_manager->current.workspaces = create_list();

    wl_signal_init(&output->events.disable);

    wl_list_insert(&global_server.wsm_scene->all_outputs, &output->link);

    output->layout_destroy.notify = handle_layout_destroy;
    wl_signal_add(&global_server.wsm_scene->output_layout->events.destroy, &output->layout_destroy);

    output->destroy.notify = handle_destroy;
    wl_signal_add(&wlr_output->events.destroy, &output->destroy);

    output->commit.notify = handle_commit;
    wl_signal_add(&wlr_output->events.commit, &output->commit);

    output->present.notify = handle_present;
    wl_signal_add(&wlr_output->events.present, &output->present);

    output->frame.notify = handle_frame;
    wl_signal_add(&wlr_output->events.frame, &output->frame);

    output->request_state.notify = handle_request_state;
    wl_signal_add(&wlr_output->events.request_state, &output->request_state);

    output->repaint_timer = wl_event_loop_add_timer(global_server.wl_event_loop,
                                                    output_repaint_timer_handler, output);

    return output;

out:
    destroy_scene_layers(output);
    wlr_scene_output_destroy(output->scene_output);
    free(output);
    return NULL;
}

void wsm_output_destroy(struct wsm_output *output) {
    if (!wsm_assert(output->node.destroying,
                     "Tried to free output which wasn't marked as destroying")) {
        return;
    }
    if (!wsm_assert(output->wlr_output == NULL,
                     "Tried to free output which still had a wlr_output")) {
        return;
    }
    if (!wsm_assert(output->node.ntxnrefs == 0, "Tried to free output "
                                                 "which is still referenced by transactions")) {
        return;
    }

    destroy_scene_layers(output);
    wsm_workspace_manager_destroy(output->workspace_manager);
    wl_event_source_remove(output->repaint_timer);
    free(output);
}

void output_begin_destroy(struct wsm_output *output) {
    if (!wsm_assert(!output->enabled, "Expected a disabled output")) {
        return;
    }
    wsm_log(WSM_DEBUG, "Destroying output '%s'", output->wlr_output->name);
    wl_signal_emit_mutable(&output->node.events.destroy, &output->node);

    output->node.destroying = true;
    node_set_dirty(&output->node);
}

void output_enable(struct wsm_output *output) {
    if (!wsm_assert(!output->enabled, "output is already enabled")) {
        return;
    }

    output->enabled = true;
    list_add(global_server.wsm_scene->outputs, output);

    // Saved workspaces

    struct wsm_workspace *ws = NULL;
    if (!output->workspace_manager->current.workspaces->length) {
        char *ws_name = int_to_string(output->workspace_manager->current.workspaces->length);
        ws = workspace_create(output, ws_name);
        struct wsm_seat *seat = NULL;
        wl_list_for_each(seat, &global_server.wsm_input_manager->seats, link) {
            if (!seat->has_focus) {
                seat_set_focus_workspace(seat, ws);
            }
        }
        free(ws_name);
    }

    ws->layout = L_NONE;

    input_manager_configure_xcursor();

    wl_signal_emit_mutable(&global_server.wsm_scene->events.new_node, &output->node);

    wsm_arrange_layers(output);
    arrange_root_auto();
}

static void evacuate_sticky(struct wsm_workspace *old_ws,
                            struct wsm_output *new_output) {
    struct wsm_workspace *new_ws = output_get_active_workspace(new_output);
    if (!wsm_assert(new_ws, "New output does not have a workspace")) {
        return;
    }
    while(old_ws->floating->length) {
        struct wsm_container *sticky = old_ws->floating->items[0];
        container_detach(sticky);
        workspace_add_floating(new_ws, sticky);
        container_handle_fullscreen_reparent(sticky);
        container_floating_move_to_center(sticky);
    }
    workspace_detect_urgent(new_ws);
}

static void output_evacuate(struct wsm_output *output) {
    struct wsm_list *workspaces = output->workspace_manager->current.workspaces;
    if (!workspaces->length) {
        return;
    }
    struct wsm_output *fallback_output = NULL;
    struct wsm_scene *root = global_server.wsm_scene;
    if (root->outputs->length > 1) {
        fallback_output = root->outputs->items[0];
        if (fallback_output == output) {
            fallback_output = root->outputs->items[1];
        }
    }

    while (workspaces->length) {
        struct wsm_workspace *workspace = workspaces->items[0];

        workspace_detach(workspace);

        struct wsm_output *new_output =
            workspace_output_get_highest_available(workspace, output);
        if (!new_output) {
            new_output = fallback_output;
        }
        if (!new_output) {
            new_output = root->fallback_output;
        }

        struct wsm_workspace *new_output_ws =
            output_get_active_workspace(new_output);

        if (workspace_is_empty(workspace)) {
            // If the new output has an active workspace (the noop output may
            // not have one), move all sticky containers to it
            if (new_output_ws &&
                workspace_num_sticky_containers(workspace) > 0) {
                evacuate_sticky(workspace, new_output);
            }

            if (workspace_num_sticky_containers(workspace) == 0) {
                workspace_begin_destroy(workspace);
                continue;
            }
        }

        workspace_output_add_priority(workspace, new_output);
        output_add_workspace(new_output, workspace);
        output_sort_workspaces(new_output);

        // If there is an old workspace (the noop output may not have one),
        // check to see if it is empty and should be destroyed.
        if (new_output_ws) {
            workspace_consider_destroy(new_output_ws);
        }
    }
}

void output_disable(struct wsm_output *output) {
    if (!wsm_assert(output->enabled, "Expected an enabled output")) {
        return;
    }
    int index = list_find(global_server.wsm_scene->outputs, output);
    if (!wsm_assert(index >= 0, "Output not found in root node")) {
        return;
    }

    wsm_log(WSM_DEBUG, "Disabling output '%s'", output->wlr_output->name);
    wl_signal_emit_mutable(&output->events.disable, output);

    output_evacuate(output);

    list_del(global_server.wsm_scene->outputs, index);

    output->enabled = false;

    arrange_root_auto();

    input_manager_configure_all_input_mappings();
}

void wsm_output_add_workspace(struct wsm_output *output,
                              struct wsm_workspace *workspace) {
    if (workspace->output) {
        workspace_detach(workspace);
    }
    list_add(output->workspace_manager->current.workspaces, workspace);
    workspace->output = output;
    node_set_dirty(&output->node);
    node_set_dirty(&workspace->node);
}

struct wsm_workspace *wsm_output_get_active_workspace(struct wsm_output *output) {
    return output->workspace_manager->current.active_workspace;
}

struct wsm_output *wsm_wsm_output_nearest_to_cursor() {
    struct wsm_seat *seat = input_manager_get_default_seat();
    return wsm_output_nearest_to(seat->wsm_cursor->wlr_cursor->x,
                             seat->wsm_cursor->wlr_cursor->y);
}

struct wsm_output *wsm_output_nearest_to(int lx, int ly) {
    double closest_x, closest_y;
    wlr_output_layout_closest_point(global_server.wsm_scene->output_layout, NULL, lx, ly,
                                    &closest_x, &closest_y);

    return wsm_output_from_wlr_output(wlr_output_layout_output_at(global_server.wsm_scene->output_layout,
                                                              closest_x, closest_y));
}

struct wsm_output *wsm_output_from_wlr_output(struct wlr_output *wlr_output) {
    struct wsm_output *wsm_output;
    wl_list_for_each(wsm_output, &global_server.wsm_output_manager->outputs, link) {
        if (wsm_output->wlr_output == wlr_output) {
            return wsm_output;
        }
    }
    return NULL;
}

struct wlr_box
wsm_output_usable_area_in_layout_coords(struct wsm_output *output)
{
    if (!output) {
        return (struct wlr_box){0};
    }
    struct wlr_box box = output->usable_area;
    double ox = 0, oy = 0;
    wlr_output_layout_output_coords(global_server.wsm_scene->output_layout,
                                    output->wlr_output, &ox, &oy);
    box.x -= ox;
    box.y -= oy;
    return box;
}

struct wlr_box
wsm_output_usable_area_scaled(struct wsm_output *output)
{
    if (!output) {
        return (struct wlr_box){0};
    }
    struct wlr_box usable = wsm_output_usable_area_in_layout_coords(output);
    if (usable.height == output->wlr_output->height
        && output->wlr_output->scale != 1) {
        usable.height /= output->wlr_output->scale;
    }
    if (usable.width == output->wlr_output->width
        && output->wlr_output->scale != 1) {
        usable.width /= output->wlr_output->scale;
    }
    return usable;
}


void
wsm_output_set_enable_adaptive_sync(struct wlr_output *output, bool enabled)
{

}

void wsm_output_power_manager_set_mode(struct wlr_output_power_v1_set_mode_event *event) {

}

bool wsm_output_is_usable(struct wsm_output *output) {
    return output && output->wlr_output->enabled && !output->leased;
}

void output_get_box(struct wsm_output *output, struct wlr_box *box) {
    box->x = output->lx;
    box->y = output->ly;
    box->width = output->width;
    box->height = output->height;
}

struct wsm_workspace *output_get_active_workspace(struct wsm_output *output) {
    struct wsm_seat *seat = input_manager_current_seat();
    struct wsm_node *focus = seat_get_active_tiling_child(seat, &output->node);
    if (!focus) {
        if (!output->workspace_manager->current.workspaces->length) {
            return NULL;
        }
        return output->workspace_manager->current.workspaces->items[0];
    }
    return focus->wsm_workspace;
}

static void handle_destroy_non_desktop(struct wl_listener *listener, void *data) {
    struct wsm_output_non_desktop *output =
        wl_container_of(listener, output, destroy);

    wsm_log(WSM_DEBUG, "Destroying non-desktop output '%s'", output->wlr_output->name);

    int index = list_find(global_server.wsm_scene->non_desktop_outputs, output);
    list_del(global_server.wsm_scene->non_desktop_outputs, index);

    wl_list_remove(&output->destroy.link);

    free(output);
}

struct wsm_output_non_desktop *output_non_desktop_create(struct wlr_output *wlr_output) {
    struct wsm_output_non_desktop *output =
        calloc(1, sizeof(struct wsm_output_non_desktop));

    output->wlr_output = wlr_output;

    output->destroy.notify = handle_destroy_non_desktop;
    wl_signal_add(&wlr_output->events.destroy, &output->destroy);

    return output;
}

void output_for_each_container(struct wsm_output *output,
                               void (*f)(struct wsm_container *con, void *data), void *data) {
    for (int i = 0; i < output->workspace_manager->current.workspaces->length; ++i) {
        struct wsm_workspace *workspace = output->workspace_manager->current.workspaces->items[i];
        workspace_for_each_container(workspace, f, data);
    }
}

static int timer_modeset_handle(void *data) {
    struct wsm_server *server = data;
    wl_event_source_remove(server->delayed_modeset);
    server->delayed_modeset = NULL;

    apply_all_output_configs();
    transaction_commit_dirty();
    update_output_manager_config(server);

    return 0;
}

void request_modeset() {
    if (global_server.delayed_modeset == NULL) {
        global_server.delayed_modeset = wl_event_loop_add_timer(global_server.wl_event_loop,
                                                          timer_modeset_handle, &global_server);
        wl_event_source_timer_update(global_server.delayed_modeset, 10);
    }
}

static struct udev_device* get_udev_device_by_connector_id(int card, const char *name) {
    struct udev *udev;
    struct udev_enumerate *enumerate;
    struct udev_list_entry *devices, *entry;
    struct udev_device *dev;
    char sysfs_path[256];

    udev = udev_new();
    if (!udev) {
        wsm_log(WSM_ERROR, "Cannot create udev object");
        return NULL;
    }

    // 枚举所有 DRM 设备
    enumerate = udev_enumerate_new(udev);
    udev_enumerate_add_match_subsystem(enumerate, "drm");
    udev_enumerate_scan_devices(enumerate);

    devices = udev_enumerate_get_list_entry(enumerate);
    udev_list_entry_foreach(entry, devices) {
        const char *path;
        path = udev_list_entry_get_name(entry);
        dev = udev_device_new_from_syspath(udev, path);

        snprintf(sysfs_path, sizeof(sysfs_path), "card%d-%s", card, name);
        if (strstr(udev_device_get_syspath(dev), sysfs_path) != NULL) {
            udev_device_ref(dev);
            udev_enumerate_unref(enumerate);
            udev_unref(udev);
            return dev;
        }

        udev_device_unref(dev);
    }

    udev_enumerate_unref(enumerate);
    udev_unref(udev);

    return NULL;
}

struct udev_device *wsm_output_get_device_handle(struct wsm_output *output) {
    struct wlr_output *wlr_output = output->wlr_output;
    if (!wlr_output_is_drm(wlr_output))
        return NULL;

    struct wlr_backend *backend = wlr_output->backend;
    if (!backend) {
        wsm_log(WSM_ERROR, "Output has no wlr_backend");
        return NULL;
    }

    int drm_fd = wlr_backend_get_drm_fd(backend);
    char path[256];
    snprintf(path, sizeof(path), "/proc/self/fd/%d", drm_fd);
    char drm_path[256];
    ssize_t len = readlink(path, drm_path, sizeof(drm_path) - 1);
    if (len == -1) {
        wsm_log(WSM_ERROR, "readlink failed!");
        return NULL;
    }

    drm_path[len] = '\0';

    char *card_str = strrchr(drm_path, '/');
    if (!card_str) {
        wsm_log(WSM_ERROR, "Invalid DRM path");
        return NULL;
    }
    card_str++;
    if (strncmp(card_str, "card", 4) != 0) {
        wsm_log(WSM_ERROR, "Invalid DRM card string");
        return NULL;
    }
    int card_number = atoi(card_str + 4);

    return get_udev_device_by_connector_id(card_number, wlr_output->name);
}

struct wsm_output *output_by_name_or_id(const char *name_or_id) {
    struct wsm_scene *root = global_server.wsm_scene;
    for (int i = 0; i < root->outputs->length; ++i) {
        struct wsm_output *output = root->outputs->items[i];
        if (output_match_name_or_id(output, name_or_id)) {
            return output;
        }
    }
    return NULL;
}
