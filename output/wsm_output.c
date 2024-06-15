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
#include "wsm_seat.h"
#include "wsm_cursor.h"
#include "wsm_server.h"
#include "wsm_scene.h"
#include "wsm_output.h"
#include "wsm_workspace.h"
#include "wsm_input_manager.h"
#include "wsm_output_manager.h"
#include "wsm_workspace_manager.h"

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include <pixman.h>

#include <wlr/backend/x11.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_output.h>
#include <wlr/backend/headless.h>
#include <wlr/backend/wayland.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/backend/drm.h>
#include <wlr/types/wlr_drm_lease_v1.h>
#include <wlr/types/wlr_output_power_management_v1.h>

static void handle_destroy(struct wl_listener *listener, void *data) {
    struct wsm_output *output = wl_container_of(listener, output, destroy);

    if (output->enabled) {
        wsm_output_enable(output, false);
    }

    wl_list_remove(&output->link);

    wl_list_remove(&output->destroy.link);
    wl_list_remove(&output->frame.link);

    wlr_scene_node_destroy(&output->background_layers_tree->node);
    wlr_scene_node_destroy(&output->bottom_layers_tree->node);

    wlr_scene_node_destroy(&output->top_layers_tree->node);
    wlr_scene_node_destroy(&output->overlay_layers_tree->node);

    wlr_scene_node_destroy(&output->shell_layer_popups->node);
    wlr_scene_node_destroy(&output->osd_layers_tree->node);

    if (output->water_mark_tree) {
        wlr_scene_node_destroy(&output->water_mark_tree->node);
    }

    if (output->black_screen_tree) {
        wlr_scene_node_destroy(&output->black_screen_tree->node);
    }

    wsm_output_destroy(output);
}

static void handle_frame(struct wl_listener *listener, void *user_data) {
    struct wsm_output *output = wl_container_of(listener, output, frame);
    wsm_scene_output_commit(output->wlr_scene_output, NULL);

    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    wlr_scene_output_send_frame_done(output->wlr_scene_output, &now);
}

static void output_request_state(struct wl_listener *listener, void *data) {
    struct wsm_output *output = wl_container_of(listener, output, request_state);
    const struct wlr_output_event_request_state *event = data;
    wlr_output_commit_state(output->wlr_output, event->state);
}

static unsigned int last_headless_num = 0;

struct wsm_output *wsm_ouput_create(struct wlr_output *wlr_output) {
    struct wsm_output *output = calloc(1, sizeof(struct wsm_output));
    if (!wsm_assert(output, "Could not create wsm_output: allocation failed!")) {
        return NULL;
    }

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

    if (global_server.drm_lease_manager && wlr_output_is_drm(wlr_output)) {
        wlr_drm_lease_v1_manager_offer_output(global_server.drm_lease_manager,
                                              wlr_output);
    }
    wlr_output_enable(wlr_output, true);

    output->wlr_output = wlr_output;
    wlr_output->data = output;
    output->detected_subpixel = wlr_output->subpixel;
    output->scale_filter = SCALE_FILTER_NEAREST;
    output->workspace_manager = wsm_workspace_manager_create(&global_server, output);

    wl_signal_init(&output->events.disable);

    output->background_layers_tree = wlr_scene_tree_create(&global_server.wsm_scene->wlr_scene->tree);
    assert(output->background_layers_tree);

    output->bottom_layers_tree = wlr_scene_tree_create(&global_server.wsm_scene->wlr_scene->tree);
    assert(output->bottom_layers_tree);

    output->top_layers_tree = wlr_scene_tree_create(&global_server.wsm_scene->wlr_scene->tree);
    assert(output->top_layers_tree);

    output->overlay_layers_tree = wlr_scene_tree_create(&global_server.wsm_scene->wlr_scene->tree);
    assert(output->overlay_layers_tree);

    output->shell_layer_popups = wlr_scene_tree_create(&global_server.wsm_scene->wlr_scene->tree);
    assert(output->shell_layer_popups);

    output->osd_layers_tree = wlr_scene_tree_create(&global_server.wsm_scene->wlr_scene->tree);
    assert(output->osd_layers_tree);

    output->water_mark_tree = wlr_scene_tree_create(&global_server.wsm_scene->wlr_scene->tree);
    assert(output->water_mark_tree);

    output->black_screen_tree = wlr_scene_tree_create(&global_server.wsm_scene->wlr_scene->tree);
    assert(output->black_screen_tree);

    wlr_scene_node_lower_to_bottom(&output->bottom_layers_tree->node);
    wlr_scene_node_lower_to_bottom(&output->background_layers_tree->node);
    wlr_scene_node_raise_to_top(&output->top_layers_tree->node);
    wlr_scene_node_raise_to_top(&global_server.wsm_scene->xdg_popup_tree->node);
    wlr_scene_node_raise_to_top(&output->overlay_layers_tree->node);
    wlr_scene_node_raise_to_top(&output->osd_layers_tree->node);
    wlr_scene_node_raise_to_top(&output->shell_layer_popups->node);
    wlr_scene_node_raise_to_top(&global_server.wsm_scene->osd_overlay_layers_tree->node);
    wlr_scene_node_raise_to_top(&output->water_mark_tree->node);
    wlr_scene_node_raise_to_top(&output->black_screen_tree->node);

    output->destroy.notify = handle_destroy;
    wl_signal_add(&wlr_output->events.destroy, &output->destroy);
    output->frame.notify = handle_frame;
    wl_signal_add(&wlr_output->events.frame, &output->frame);
    output->request_state.notify = output_request_state;
    wl_signal_add(&wlr_output->events.request_state, &output->request_state);

    return output;
}

void wsm_output_destroy(struct wsm_output *wlr_output) {

}

void wsm_output_enable(struct wsm_output *output, bool enable) {
    wlr_output_enable(output->wlr_output, enable);
}

struct wsm_workspace *wsm_output_get_active_workspace(struct wsm_output *output) {
    return output->workspace_manager->active_workspace;
}

struct wsm_output *wsm_wsm_output_nearest_to_cursor() {
    struct wsm_seat *seat = input_manager_get_default_seat();
    return wsm_output_nearest_to(seat->wsm_cursor->wlr_cursor->x,
                             seat->wsm_cursor->wlr_cursor->y);
}

struct wsm_output *wsm_output_nearest_to(int lx, int ly) {
    double closest_x, closest_y;
    wlr_output_layout_closest_point(global_server.wsm_output_manager->wlr_output_layout, NULL, lx, ly,
                                    &closest_x, &closest_y);

    return wsm_output_from_wlr_output(wlr_output_layout_output_at(global_server.wsm_output_manager->wlr_output_layout,
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
    wlr_output_layout_output_coords(global_server.wsm_output_manager->wlr_output_layout,
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
    wlr_output_enable_adaptive_sync(output, enabled);
    if (!wlr_output_test(output)) {
        wlr_output_enable_adaptive_sync(output, false);
        wsm_log(WSM_DEBUG,
                "failed to enable adaptive sync for output %s", output->name);
    } else {
        wsm_log(WSM_INFO, "adaptive sync %sabled for output %s",
                enabled ? "en" : "dis", output->name);
    }
}

void wsm_output_power_manager_set_mode(struct wlr_output_power_v1_set_mode_event *event) {
    switch (event->mode) {
    case ZWLR_OUTPUT_POWER_V1_MODE_OFF:
        wlr_output_enable(event->output, false);
        wlr_output_commit(event->output);
        break;
    case ZWLR_OUTPUT_POWER_V1_MODE_ON:
        wlr_output_enable(event->output, true);
        if (!wlr_output_test(event->output)) {
            wlr_output_rollback(event->output);
        }
        wlr_output_commit(event->output);
        cursor_update_image(input_manager_get_default_seat());
        break;
    }
}

bool wsm_output_is_usable(struct wsm_output *output) {
    return output && output->wlr_output->enabled && !output->leased;
}

bool wsm_output_can_reuse_mode(struct wsm_output *output) {
    struct wlr_output *wlr_output = output->wlr_output;
    return wlr_output->current_mode && wlr_output_test(wlr_output);
}
