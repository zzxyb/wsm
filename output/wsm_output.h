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

#ifndef WSM_OUTPUT_H
#define WSM_OUTPUT_H

#include <bits/types/struct_timespec.h>

#include <wayland-server-core.h>

#include <wlr/util/box.h>
#include <wlr/types/wlr_damage_ring.h>
#include <wlr/types/wlr_output_layout.h>

struct wlr_output;
struct wlr_output_mode;
struct wlr_scene_output;
struct wlr_output_power_v1_set_mode_event;

struct wsm_workspace;
struct wsm_workspace_manager;

enum scale_filter_mode {
    SCALE_FILTER_DEFAULT, // the default is currently smart
    SCALE_FILTER_LINEAR,
    SCALE_FILTER_NEAREST,
    SCALE_FILTER_SMART,
};

/**
 * @brief The wsm_output class
 */
struct wsm_output {
    struct wl_list link;
    struct wlr_output *wlr_output;
    struct wlr_scene_output *wlr_scene_output;
    struct wsm_workspace_manager* workspace_manager;

    struct wlr_scene_tree *background_layers_tree;
    struct wlr_scene_tree *bottom_layers_tree;
    struct wlr_scene_tree *top_layers_tree;
    struct wlr_scene_tree *overlay_layers_tree;
    struct wlr_scene_tree *shell_layer_popups;
    struct wlr_scene_tree *osd_layers_tree;
    struct wlr_scene_tree *water_mark_tree;
    struct wlr_scene_tree *black_screen_tree;

    struct wlr_box usable_area;

    struct timespec last_frame;
    struct wlr_damage_ring damage_ring;

    int lx, ly; // layout coords
    int width, height; // transformed buffer size
    enum wl_output_subpixel detected_subpixel;
    enum scale_filter_mode scale_filter;
    struct wlr_output_mode *current_mode;

    bool enabled;

    struct wl_listener destroy;
    struct wl_listener frame;
    struct wl_listener request_state;

    struct {
        struct wl_signal disable;
    } events;

    void *data;
    struct timespec last_presentation;
    uint32_t refresh_nsec;

    bool leased;
    bool gamma_lut_changed;
};

struct wsm_output *wsm_ouput_create(struct wlr_output *wlr_output);
void wsm_output_destroy(struct wsm_output *wlr_output);
void wsm_output_enable(struct wsm_output *output, bool enable);
struct wsm_output *wsm_wsm_output_nearest_to_cursor();
struct wsm_output *wsm_output_nearest_to(int lx, int ly);
struct wsm_output *wsm_output_from_wlr_output(struct wlr_output *wlr_output);
struct wsm_output *wsm_output_get_in_direction(struct wsm_output *reference,
                                            enum wlr_direction direction);
void wsm_output_add_workspace(struct wsm_output *output,
                          struct wsm_workspace *workspace);
void wsm_output_damage_whole(struct wsm_output *output);
void wsm_output_damage_surface(struct wsm_output *output, double ox, double oy,
                           struct wlr_surface *surface, bool whole);
void wsm_output_damage_box(struct wsm_output *output, struct wlr_box *box);
struct wsm_workspace *wsm_output_get_active_workspace(struct wsm_output *output);
bool wsm_scene_output_commit(struct wlr_scene_output *scene_output);
struct wlr_box wsm_output_usable_area_in_layout_coords(struct wsm_output *output);
struct wlr_box wsm_output_usable_area_scaled(struct wsm_output *output);
void wsm_output_set_enable_adaptive_sync(struct wlr_output *output, bool enabled);

void wsm_output_power_manager_set_mode(struct wlr_output_power_v1_set_mode_event *event);

/**
 * @brief wsm_output_calculate_usable_area calculate unusable area on output
 * for example, the area occupied by the Dock
 * @param output
 * @param usable_area
 */
void wsm_output_calculate_usable_area(struct wsm_output *output, struct wlr_box *usable_area);
bool wsm_output_is_usable(struct wsm_output *output);
bool wsm_output_can_reuse_mode(struct wsm_output *output);

#endif
