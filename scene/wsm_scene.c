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
#include "wsm_scene.h"
#include "wsm_server.h"
#include "wsm_seat.h"
#include "wsm_output.h"
#include "wsm_container.h"
#include "wsm_input_manager.h"
#include "node/wsm_node_descriptor.h"
#include "wsm_output_manager.h"
#include "wsm_workspace.h"

#include <stdlib.h>
#include <assert.h>

#include <drm_fourcc.h>

#include <wlr/util/region.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_output.h>
#include <wlr/render/swapchain.h>
#include <wlr/types/wlr_output_layout.h>

#define HIGHLIGHT_DAMAGE_FADEOUT_TIME 250
// static const long NSEC_PER_SEC = 1000000000;

// struct render_data {
//     enum wl_output_transform transform;
//     float scale;
//     struct wlr_box logical;
//     int trans_width, trans_height;

//     struct wlr_scene_output *output;

//     struct wlr_render_pass *render_pass;
//     pixman_region32_t damage;
// };

// struct render_list_constructor_data {
//     struct wlr_box box;
//     struct wl_array *render_list;
//     bool calculate_visibility;
// };

// struct render_list_entry {
//     struct wlr_scene_node *node;
//     bool sent_dmabuf_feedback;
//     int x, y;
// };

// struct highlight_region {
//     pixman_region32_t region;
//     struct timespec when;
//     struct wl_list link;
// };

struct wsm_scene *wsm_scene_create(const struct wsm_server* server) {
    struct wsm_scene *scene = calloc(1, sizeof(struct wsm_scene));
    if (!wsm_assert(scene, "Could not create wsm_scene: allocation failed!")) {
        return NULL;
    }
    struct wlr_scene *root_scene = wlr_scene_create();
    if (!wsm_assert(root_scene, "wlr_scene_create return NULL!")) {
        return NULL;
    }

    node_init(&scene->node, N_ROOT, scene);
    scene->root_scene = root_scene;

    bool failed = false;
    scene->staging = alloc_scene_tree(&root_scene->tree, &failed);
    scene->layer_tree = alloc_scene_tree(&root_scene->tree, &failed);

    scene->layers.shell_background = alloc_scene_tree(scene->layer_tree, &failed);
    scene->layers.shell_bottom = alloc_scene_tree(scene->layer_tree, &failed);
    scene->layers.tiling = alloc_scene_tree(scene->layer_tree, &failed);
    scene->layers.floating = alloc_scene_tree(scene->layer_tree, &failed);
    scene->layers.shell_top = alloc_scene_tree(scene->layer_tree, &failed);
    scene->layers.fullscreen = alloc_scene_tree(scene->layer_tree, &failed);
    scene->layers.fullscreen_global = alloc_scene_tree(scene->layer_tree, &failed);
#if HAVE_XWAYLAND
    scene->layers.unmanaged = alloc_scene_tree(scene->layer_tree, &failed);
#endif
    scene->layers.shell_overlay = alloc_scene_tree(scene->layer_tree, &failed);
    scene->layers.popup = alloc_scene_tree(scene->layer_tree, &failed);
    scene->layers.seat = alloc_scene_tree(scene->layer_tree, &failed);
    scene->layers.session_lock = alloc_scene_tree(scene->layer_tree, &failed);

    if (!failed && !wsm_scene_descriptor_assign(&scene->layers.seat->node,
                                            WSM_SCENE_DESC_NON_INTERACTIVE, (void *)1)) {
        failed = true;
    }

    if (failed) {
        wlr_scene_node_destroy(&root_scene->tree.node);
        free(scene);
        return NULL;
    }

    wlr_scene_node_set_enabled(&scene->staging->node, false);
    scene->output_layout = wlr_output_layout_create(server->wl_display);
    wl_list_init(&scene->all_outputs);
    wl_signal_init(&scene->events.new_node);
    scene->outputs = create_list();
    scene->non_desktop_outputs = create_list();
    scene->scratchpad = create_list();

    return scene;
}

// bool wsm_scene_output_commit(struct wlr_scene_output *scene_output,
//                              const struct wlr_scene_output_state_options *options) {
//     if (!scene_output->output->needs_frame && !pixman_region32_not_empty(
//             &scene_output->damage_ring.current)) {
//         return true;
//     }

//     bool ok = false;
//     struct wlr_output_state state;
//     wlr_output_state_init(&state);
//     if (!wsm_scene_output_build_state(scene_output, &state, options)) {
//         goto out;
//     }

//     ok = wlr_output_commit_state(scene_output->output, &state);
//     if (!ok) {
//         goto out;
//     }

//     wlr_damage_ring_rotate(&scene_output->damage_ring);

// out:
//     wlr_output_state_finish(&state);
//     return ok;
// }

// void output_pending_resolution(struct wlr_output *output,
//                                const struct wlr_output_state *state, int *width, int *height) {
//     if (state->committed & WLR_OUTPUT_STATE_MODE) {
//         switch (state->mode_type) {
//         case WLR_OUTPUT_STATE_MODE_FIXED:
//             *width = state->mode->width;
//             *height = state->mode->height;
//             return;
//         case WLR_OUTPUT_STATE_MODE_CUSTOM:
//             *width = state->custom_mode.width;
//             *height = state->custom_mode.height;
//             return;
//         }
//         abort();
//     } else {
//         *width = output->width;
//         *height = output->height;
//     }
// }

// static void scene_node_get_size(struct wlr_scene_node *node,
//                                 int *width, int *height) {
//     *width = 0;
//     *height = 0;

//     switch (node->type) {
//     case WLR_SCENE_NODE_TREE:
//         return;
//     case WLR_SCENE_NODE_RECT:;
//         struct wlr_scene_rect *scene_rect = wlr_scene_rect_from_node(node);
//         *width = scene_rect->width;
//         *height = scene_rect->height;
//         break;
//     case WLR_SCENE_NODE_BUFFER:;
//         struct wlr_scene_buffer *scene_buffer = wlr_scene_buffer_from_node(node);
//         if (scene_buffer->dst_width > 0 && scene_buffer->dst_height > 0) {
//             *width = scene_buffer->dst_width;
//             *height = scene_buffer->dst_height;
//         } else if (scene_buffer->buffer) {
//             if (scene_buffer->transform & WL_OUTPUT_TRANSFORM_90) {
//                 *height = scene_buffer->buffer->width;
//                 *width = scene_buffer->buffer->height;
//             } else {
//                 *width = scene_buffer->buffer->width;
//                 *height = scene_buffer->buffer->height;
//             }
//         }
//         break;
//     }
// }

// typedef bool (*scene_node_box_iterator_func_t)(struct wlr_scene_node *node,
//                                                int sx, int sy, void *data);

// static bool _scene_nodes_in_box(struct wlr_scene_node *node, struct wlr_box *box,
//                                 scene_node_box_iterator_func_t iterator, void *user_data, int lx, int ly) {
//     if (!node->enabled) {
//         return false;
//     }

//     switch (node->type) {
//     case WLR_SCENE_NODE_TREE:;
//         struct wlr_scene_tree *scene_tree = wlr_scene_tree_from_node(node);
//         struct wlr_scene_node *child;
//         wl_list_for_each_reverse(child, &scene_tree->children, link) {
//             if (_scene_nodes_in_box(child, box, iterator, user_data, lx + child->x, ly + child->y)) {
//                 return true;
//             }
//         }
//         break;
//     case WLR_SCENE_NODE_RECT:
//     case WLR_SCENE_NODE_BUFFER:;
//         struct wlr_box node_box = { .x = lx, .y = ly };
//         scene_node_get_size(node, &node_box.width, &node_box.height);

//         if (wlr_box_intersection(&node_box, &node_box, box) &&
//             iterator(node, lx, ly, user_data)) {
//             return true;
//         }
//         break;
//     }

//     return false;
// }

// static bool scene_nodes_in_box(struct wlr_scene_node *node, struct wlr_box *box,
//                                scene_node_box_iterator_func_t iterator, void *user_data) {
//     int x, y;
//     wlr_scene_node_coords(node, &x, &y);

//     return _scene_nodes_in_box(node, box, iterator, user_data, x, y);
// }

// static bool scene_node_invisible(struct wlr_scene_node *node) {
//     if (node->type == WLR_SCENE_NODE_TREE) {
//         return true;
//     } else if (node->type == WLR_SCENE_NODE_RECT) {
//         struct wlr_scene_rect *rect = wlr_scene_rect_from_node(node);

//         return rect->color[3] == 0.f;
//     } else if (node->type == WLR_SCENE_NODE_BUFFER) {
//         struct wlr_scene_buffer *buffer = wlr_scene_buffer_from_node(node);

//         return buffer->buffer == NULL;
//     }

//     return false;
// }

// static bool construct_render_list_iterator(struct wlr_scene_node *node,
//                                            int lx, int ly, void *_data) {
//     struct render_list_constructor_data *data = _data;

//     if (scene_node_invisible(node)) {
//         return false;
//     }

//     // while rendering, the background should always be black.
//     // If we see a black rect, we can ignore rendering everything under the rect
//     // and even the rect itself.
//     if (node->type == WLR_SCENE_NODE_RECT && data->calculate_visibility) {
//         struct wlr_scene_rect *rect = wlr_scene_rect_from_node(node);
//         float *black = (float[4]){ 0.f, 0.f, 0.f, 1.f };

//         if (memcmp(rect->color, black, sizeof(float) * 4) == 0) {
//             return false;
//         }
//     }

//     pixman_region32_t intersection;
//     pixman_region32_init(&intersection);
//     pixman_region32_intersect_rect(&intersection, &node->visible,
//                                    data->box.x, data->box.y,
//                                    data->box.width, data->box.height);
//     if (!pixman_region32_not_empty(&intersection)) {
//         pixman_region32_fini(&intersection);
//         return false;
//     }

//     pixman_region32_fini(&intersection);

//     struct render_list_entry *entry = wl_array_add(data->render_list, sizeof(*entry));
//     if (!entry) {
//         return false;
//     }

//     *entry = (struct render_list_entry){
//         .node = node,
//         .x = lx,
//         .y = ly,
//     };

//     return false;
// }

// static bool array_realloc(struct wl_array *arr, size_t size) {
//     // If the size is less than 1/4th of the allocation size, we shrink it.
//     // 1/4th is picked to provide hysteresis, without which an array with size
//     // arr->alloc would constantly reallocate if an element is added and then
//     // removed continously.
//     size_t alloc;
//     if (arr->alloc > 0 && size > arr->alloc / 4) {
//         alloc = arr->alloc;
//     } else {
//         alloc = 16;
//     }

//     while (alloc < size) {
//         alloc *= 2;
//     }

//     if (alloc == arr->alloc) {
//         return true;
//     }

//     void *data = realloc(arr->data, alloc);
//     if (data == NULL) {
//         return false;
//     }
//     arr->data = data;
//     arr->alloc = alloc;
//     return true;
// }

// static void scene_buffer_send_dmabuf_feedback(const struct wlr_scene *scene,
//                                               struct wlr_scene_buffer *scene_buffer,
//                                               const struct wlr_linux_dmabuf_feedback_v1_init_options *options) {
//     if (!scene->linux_dmabuf_v1) {
//         return;
//     }

//     struct wlr_scene_surface *surface = wlr_scene_surface_try_from_buffer(scene_buffer);
//     if (!surface) {
//         return;
//     }

//     // compare to the previous options so that we don't send
//     // duplicate feedback events.
//     if (memcmp(options, &scene_buffer->prev_feedback_options, sizeof(*options)) == 0) {
//         return;
//     }

//     scene_buffer->prev_feedback_options = *options;

//     struct wlr_linux_dmabuf_feedback_v1 feedback = {0};
//     if (!wlr_linux_dmabuf_feedback_v1_init_with_options(&feedback, options)) {
//         return;
//     }

//     wlr_linux_dmabuf_v1_set_surface_feedback(scene->linux_dmabuf_v1,
//                                              surface->surface, &feedback);

//     wlr_linux_dmabuf_feedback_v1_finish(&feedback);
// }

// static void transform_output_damage(pixman_region32_t *damage, const struct render_data *data) {
//     enum wl_output_transform transform = wlr_output_transform_invert(data->transform);
//     wlr_region_transform(damage, damage, transform, data->trans_width, data->trans_height);
// }

// static void output_state_apply_damage(const struct render_data *data,
//                                       struct wlr_output_state *state) {
//     pixman_region32_t frame_damage;
//     pixman_region32_init(&frame_damage);
//     pixman_region32_copy(&frame_damage, &data->output->damage_ring.current);
//     transform_output_damage(&frame_damage, data);
//     wlr_output_state_set_damage(state, &frame_damage);
//     pixman_region32_fini(&frame_damage);
// }

// static bool scene_entry_try_direct_scanout(struct render_list_entry *entry,
//                                            struct wlr_output_state *state, const struct render_data *data) {
//     struct wlr_scene_output *scene_output = data->output;
//     struct wlr_scene_node *node = entry->node;

//     if (!scene_output->scene->direct_scanout) {
//         return false;
//     }

//     if (scene_output->scene->debug_damage_option ==
//         WLR_SCENE_DEBUG_DAMAGE_HIGHLIGHT) {
//         // We don't want to enter direct scan out if we have highlight regions
//         // enabled. Otherwise, we won't be able to render the damage regions.
//         return false;
//     }

//     if (node->type != WLR_SCENE_NODE_BUFFER) {
//         return false;
//     }

//     if (state->committed & (WLR_OUTPUT_STATE_MODE |
//                             WLR_OUTPUT_STATE_ENABLED |
//                             WLR_OUTPUT_STATE_RENDER_FORMAT)) {
//         // Legacy DRM will explode if we try to modeset with a direct scanout buffer
//         return false;
//     }

//     if (!wlr_output_is_direct_scanout_allowed(scene_output->output)) {
//         return false;
//     }

//     struct wlr_scene_buffer *buffer = wlr_scene_buffer_from_node(node);

//     struct wlr_fbox default_box = {0};
//     if (buffer->transform & WL_OUTPUT_TRANSFORM_90) {
//         default_box.width = buffer->buffer->height;
//         default_box.height = buffer->buffer->width;
//     } else {
//         default_box.width = buffer->buffer->width;
//         default_box.height = buffer->buffer->height;
//     }

//     if (!wlr_fbox_empty(&buffer->src_box) &&
//         !wlr_fbox_equal(&buffer->src_box, &default_box)) {
//         return false;
//     }

//     if (buffer->transform != data->transform) {
//         return false;
//     }

//     struct wlr_box node_box = { .x = entry->x, .y = entry->y };
//     scene_node_get_size(node, &node_box.width, &node_box.height);

//     if (!wlr_box_equal(&data->logical, &node_box)) {
//         return false;
//     }

//     if (buffer->primary_output == scene_output) {
//         struct wlr_linux_dmabuf_feedback_v1_init_options options = {
//             .main_renderer = scene_output->output->renderer,
//             .scanout_primary_output = scene_output->output,
//         };

//         scene_buffer_send_dmabuf_feedback(scene_output->scene, buffer, &options);
//         entry->sent_dmabuf_feedback = true;
//     }

//     struct wlr_output_state pending;
//     wlr_output_state_init(&pending);
//     if (!wlr_output_state_copy(&pending, state)) {
//         return false;
//     }

//     wlr_output_state_set_buffer(&pending, buffer->buffer);
//     output_state_apply_damage(data, &pending);

//     if (!wlr_output_test_state(scene_output->output, &pending)) {
//         wlr_output_state_finish(&pending);
//         return false;
//     }

//     wlr_output_state_copy(state, &pending);
//     wlr_output_state_finish(&pending);

//     struct wlr_scene_output_sample_event sample_event = {
//         .output = scene_output,
//         .direct_scanout = true,
//     };
//     wl_signal_emit_mutable(&buffer->events.output_sample, &sample_event);
//     return true;
// }

// static int64_t timespec_to_msec(const struct timespec *a) {
//     return (int64_t)a->tv_sec * 1000 + a->tv_nsec / 1000000;
// }

// static void timespec_sub(struct timespec *r, const struct timespec *a,
//                          const struct timespec *b) {
//     r->tv_sec = a->tv_sec - b->tv_sec;
//     r->tv_nsec = a->tv_nsec - b->tv_nsec;
//     if (r->tv_nsec < 0) {
//         r->tv_sec--;
//         r->tv_nsec += NSEC_PER_SEC;
//     }
// }

// static int64_t timespec_to_nsec(const struct timespec *a) {
//     return (int64_t)a->tv_sec * NSEC_PER_SEC + a->tv_nsec;
// }

// static void highlight_region_destroy(struct highlight_region *damage) {
//     wl_list_remove(&damage->link);
//     pixman_region32_fini(&damage->region);
//     free(damage);
// }

// struct wlr_pixel_format_info {
//     uint32_t drm_format;

//     /* Equivalent of the format if it has an alpha channel,
//      * DRM_FORMAT_INVALID (0) if NA
//      */
//     uint32_t opaque_substitute;

//     /* Bytes per block (including padding) */
//     uint32_t bytes_per_block;
//     /* Size of a block in pixels (zero for 1×1) */
//     uint32_t block_width, block_height;

//     /* True if the format has an alpha channel */
//     bool has_alpha;
// };

// static const struct wlr_pixel_format_info pixel_format_info[] = {
//     {
//         .drm_format = DRM_FORMAT_XRGB8888,
//         .bytes_per_block = 4,
//     },
//     {
//         .drm_format = DRM_FORMAT_ARGB8888,
//         .opaque_substitute = DRM_FORMAT_XRGB8888,
//         .bytes_per_block = 4,
//         .has_alpha = true,
//     },
//     {
//         .drm_format = DRM_FORMAT_XBGR8888,
//         .bytes_per_block = 4,
//     },
//     {
//         .drm_format = DRM_FORMAT_ABGR8888,
//         .opaque_substitute = DRM_FORMAT_XBGR8888,
//         .bytes_per_block = 4,
//         .has_alpha = true,
//     },
//     {
//         .drm_format = DRM_FORMAT_RGBX8888,
//         .bytes_per_block = 4,
//     },
//     {
//         .drm_format = DRM_FORMAT_RGBA8888,
//         .opaque_substitute = DRM_FORMAT_RGBX8888,
//         .bytes_per_block = 4,
//         .has_alpha = true,
//     },
//     {
//         .drm_format = DRM_FORMAT_BGRX8888,
//         .bytes_per_block = 4,
//     },
//     {
//         .drm_format = DRM_FORMAT_BGRA8888,
//         .opaque_substitute = DRM_FORMAT_BGRX8888,
//         .bytes_per_block = 4,
//         .has_alpha = true,
//     },
//     {
//         .drm_format = DRM_FORMAT_R8,
//         .bytes_per_block = 1,
//     },
//     {
//         .drm_format = DRM_FORMAT_GR88,
//         .bytes_per_block = 2,
//     },
//     {
//         .drm_format = DRM_FORMAT_RGB888,
//         .bytes_per_block = 3,
//     },
//     {
//         .drm_format = DRM_FORMAT_BGR888,
//         .bytes_per_block = 3,
//     },
//     {
//         .drm_format = DRM_FORMAT_RGBX4444,
//         .bytes_per_block = 2,
//     },
//     {
//         .drm_format = DRM_FORMAT_RGBA4444,
//         .opaque_substitute = DRM_FORMAT_RGBX4444,
//         .bytes_per_block = 2,
//         .has_alpha = true,
//     },
//     {
//         .drm_format = DRM_FORMAT_BGRX4444,
//         .bytes_per_block = 2,
//     },
//     {
//         .drm_format = DRM_FORMAT_BGRA4444,
//         .opaque_substitute = DRM_FORMAT_BGRX4444,
//         .bytes_per_block = 2,
//         .has_alpha = true,
//     },
//     {
//         .drm_format = DRM_FORMAT_RGBX5551,
//         .bytes_per_block = 2,
//     },
//     {
//         .drm_format = DRM_FORMAT_RGBA5551,
//         .opaque_substitute = DRM_FORMAT_RGBX5551,
//         .bytes_per_block = 2,
//         .has_alpha = true,
//     },
//     {
//         .drm_format = DRM_FORMAT_BGRX5551,
//         .bytes_per_block = 2,
//     },
//     {
//         .drm_format = DRM_FORMAT_BGRA5551,
//         .opaque_substitute = DRM_FORMAT_BGRX5551,
//         .bytes_per_block = 2,
//         .has_alpha = true,
//     },
//     {
//         .drm_format = DRM_FORMAT_XRGB1555,
//         .bytes_per_block = 2,
//     },
//     {
//         .drm_format = DRM_FORMAT_ARGB1555,
//         .opaque_substitute = DRM_FORMAT_XRGB1555,
//         .bytes_per_block = 2,
//         .has_alpha = true,
//     },
//     {
//         .drm_format = DRM_FORMAT_RGB565,
//         .bytes_per_block = 2,
//     },
//     {
//         .drm_format = DRM_FORMAT_BGR565,
//         .bytes_per_block = 2,
//     },
//     {
//         .drm_format = DRM_FORMAT_XRGB2101010,
//         .bytes_per_block = 4,
//     },
//     {
//         .drm_format = DRM_FORMAT_ARGB2101010,
//         .opaque_substitute = DRM_FORMAT_XRGB2101010,
//         .bytes_per_block = 4,
//         .has_alpha = true,
//     },
//     {
//         .drm_format = DRM_FORMAT_XBGR2101010,
//         .bytes_per_block = 4,
//     },
//     {
//         .drm_format = DRM_FORMAT_ABGR2101010,
//         .opaque_substitute = DRM_FORMAT_XBGR2101010,
//         .bytes_per_block = 4,
//         .has_alpha = true,
//     },
//     {
//         .drm_format = DRM_FORMAT_XBGR16161616F,
//         .bytes_per_block = 8,
//     },
//     {
//         .drm_format = DRM_FORMAT_ABGR16161616F,
//         .opaque_substitute = DRM_FORMAT_XBGR16161616F,
//         .bytes_per_block = 8,
//         .has_alpha = true,
//     },
//     {
//         .drm_format = DRM_FORMAT_XBGR16161616,
//         .bytes_per_block = 8,
//     },
//     {
//         .drm_format = DRM_FORMAT_ABGR16161616,
//         .opaque_substitute = DRM_FORMAT_XBGR16161616,
//         .bytes_per_block = 8,
//         .has_alpha = true,
//     },
//     {
//         .drm_format = DRM_FORMAT_YVYU,
//         .bytes_per_block = 4,
//         .block_width = 2,
//         .block_height = 1,
//     },
//     {
//         .drm_format = DRM_FORMAT_VYUY,
//         .bytes_per_block = 4,
//         .block_width = 2,
//         .block_height = 1,
//     },
// };

// static const size_t pixel_format_info_size =
//     sizeof(pixel_format_info) / sizeof(pixel_format_info[0]);

// static const struct wlr_pixel_format_info *drm_get_pixel_format_info(uint32_t fmt) {
//     for (size_t i = 0; i < pixel_format_info_size; ++i) {
//         if (pixel_format_info[i].drm_format == fmt) {
//             return &pixel_format_info[i];
//         }
//     }

//     return NULL;
// }

// static bool buffer_is_opaque(struct wlr_buffer *buffer) {
//     void *data;
//     uint32_t format;
//     size_t stride;
//     struct wlr_dmabuf_attributes dmabuf;
//     struct wlr_shm_attributes shm;
//     if (wlr_buffer_get_dmabuf(buffer, &dmabuf)) {
//         format = dmabuf.format;
//     } else if (wlr_buffer_get_shm(buffer, &shm)) {
//         format = shm.format;
//     } else if (wlr_buffer_begin_data_ptr_access(buffer,
//                                                 WLR_BUFFER_DATA_PTR_ACCESS_READ, &data, &format, &stride)) {
//         wlr_buffer_end_data_ptr_access(buffer);
//     } else {
//         return false;
//     }

//     const struct wlr_pixel_format_info *format_info =
//         drm_get_pixel_format_info(format);
//     if (format_info == NULL) {
//         return false;
//     }

//     return !format_info->has_alpha;
// }

// static void scene_node_opaque_region(struct wlr_scene_node *node, int x, int y,
//                                      pixman_region32_t *opaque) {
//     int width, height;
//     scene_node_get_size(node, &width, &height);

//     if (node->type == WLR_SCENE_NODE_RECT) {
//         struct wlr_scene_rect *scene_rect = wlr_scene_rect_from_node(node);
//         if (scene_rect->color[3] != 1) {
//             return;
//         }
//     } else if (node->type == WLR_SCENE_NODE_BUFFER) {
//         struct wlr_scene_buffer *scene_buffer = wlr_scene_buffer_from_node(node);

//         if (!scene_buffer->buffer) {
//             return;
//         }

//         if (scene_buffer->opacity != 1) {
//             return;
//         }

//         if (!buffer_is_opaque(scene_buffer->buffer)) {
//             pixman_region32_copy(opaque, &scene_buffer->opaque_region);
//             pixman_region32_intersect_rect(opaque, opaque, 0, 0, width, height);
//             pixman_region32_translate(opaque, x, y);
//             return;
//         }
//     }

//     pixman_region32_fini(opaque);
//     pixman_region32_init_rect(opaque, x, y, width, height);
// }

// static void scale_output_damage(pixman_region32_t *damage, float scale) {
//     wlr_region_scale(damage, damage, scale);

//     if (floor(scale) != scale) {
//         wlr_region_expand(damage, damage, 1);
//     }
// }

// static int scale_length(int length, int offset, float scale) {
//     return round((offset + length) * scale) - round(offset * scale);
// }

// static void scale_box(struct wlr_box *box, float scale) {
//     box->width = scale_length(box->width, box->x, scale);
//     box->height = scale_length(box->height, box->y, scale);
//     box->x = round(box->x * scale);
//     box->y = round(box->y * scale);
// }

// static void transform_output_box(struct wlr_box *box, const struct render_data *data) {
//     enum wl_output_transform transform = wlr_output_transform_invert(data->transform);
//     wlr_box_transform(box, box, transform, data->trans_width, data->trans_height);
// }

// static struct wlr_texture *scene_buffer_get_texture(
//     struct wlr_scene_buffer *scene_buffer, struct wlr_renderer *renderer) {
//     struct wlr_client_buffer *client_buffer =
//         wlr_client_buffer_get(scene_buffer->buffer);
//     if (client_buffer != NULL) {
//         return client_buffer->texture;
//     }

//     if (scene_buffer->texture != NULL) {
//         return scene_buffer->texture;
//     }

//     scene_buffer->texture =
//         wlr_texture_from_buffer(renderer, scene_buffer->buffer);
//     return scene_buffer->texture;
// }

// static void scene_entry_render(struct render_list_entry *entry, const struct render_data *data) {
//     struct wlr_scene_node *node = entry->node;

//     pixman_region32_t render_region;
//     pixman_region32_init(&render_region);
//     pixman_region32_copy(&render_region, &node->visible);
//     pixman_region32_translate(&render_region, -data->logical.x, -data->logical.y);
//     scale_output_damage(&render_region, data->scale);
//     pixman_region32_intersect(&render_region, &render_region, &data->damage);
//     if (!pixman_region32_not_empty(&render_region)) {
//         pixman_region32_fini(&render_region);
//         return;
//     }

//     struct wlr_box dst_box = {
//         .x = entry->x - data->logical.x,
//         .y = entry->y - data->logical.y,
//     };
//     scene_node_get_size(node, &dst_box.width, &dst_box.height);
//     scale_box(&dst_box, data->scale);

//     pixman_region32_t opaque;
//     pixman_region32_init(&opaque);
//     scene_node_opaque_region(node, dst_box.x, dst_box.y, &opaque);
//     scale_output_damage(&opaque, data->scale);
//     pixman_region32_subtract(&opaque, &render_region, &opaque);

//     transform_output_box(&dst_box, data);
//     transform_output_damage(&render_region, data);

//     switch (node->type) {
//     case WLR_SCENE_NODE_TREE:
//         assert(false);
//         break;
//     case WLR_SCENE_NODE_RECT:;
//         struct wlr_scene_rect *scene_rect = wlr_scene_rect_from_node(node);

//         wlr_render_pass_add_rect(data->render_pass, &(struct wlr_render_rect_options){
//                                                         .box = dst_box,
//                                                         .color = {
//                                                             .r = scene_rect->color[0],
//                                                             .g = scene_rect->color[1],
//                                                             .b = scene_rect->color[2],
//                                                             .a = scene_rect->color[3],
//                                                         },
//                                                         .clip = &render_region,
//                                                     });
//         break;
//     case WLR_SCENE_NODE_BUFFER:;
//         struct wlr_scene_buffer *scene_buffer = wlr_scene_buffer_from_node(node);
//         assert(scene_buffer->buffer);

//         struct wlr_texture *texture = scene_buffer_get_texture(scene_buffer,
//                                                                data->output->output->renderer);
//         if (texture == NULL) {
//             break;
//         }

//         enum wl_output_transform transform =
//             wlr_output_transform_invert(scene_buffer->transform);
//         transform = wlr_output_transform_compose(transform, data->transform);

//         wlr_render_pass_add_texture(data->render_pass, &(struct wlr_render_texture_options) {
//                                                            .texture = texture,
//                                                            .src_box = scene_buffer->src_box,
//                                                            .dst_box = dst_box,
//                                                            .transform = transform,
//                                                            .clip = &render_region,
//                                                            .alpha = &scene_buffer->opacity,
//                                                            .filter_mode = scene_buffer->filter_mode,
//                                                            .blend_mode = pixman_region32_not_empty(&opaque) ?
//                                                                              WLR_RENDER_BLEND_MODE_PREMULTIPLIED : WLR_RENDER_BLEND_MODE_NONE,
//                                                        });

//         struct wlr_scene_output_sample_event sample_event = {
//             .output = data->output,
//             .direct_scanout = false,
//         };
//         wl_signal_emit_mutable(&scene_buffer->events.output_sample, &sample_event);
//         break;
//     }

//     pixman_region32_fini(&opaque);
//     pixman_region32_fini(&render_region);
// }

// bool wsm_scene_output_build_state(struct wlr_scene_output *scene_output,
//                                   struct wlr_output_state *state, const struct wlr_scene_output_state_options *options) {
//     struct wlr_scene_output_state_options default_options = {0};
//     if (!options) {
//         options = &default_options;
//     }
//     struct wlr_scene_timer *timer = options->timer;
//     struct timespec start_time;
//     if (timer) {
//         clock_gettime(CLOCK_MONOTONIC, &start_time);
//         wlr_scene_timer_finish(timer);
//         *timer = (struct wlr_scene_timer){0};
//     }

//     if ((state->committed & WLR_OUTPUT_STATE_ENABLED) && !state->enabled) {
//         // if the state is being disabled, do nothing.
//         return true;
//     }

//     struct wlr_output *output = scene_output->output;
//     enum wlr_scene_debug_damage_option debug_damage =
//         scene_output->scene->debug_damage_option;

//     struct render_data render_data = {
//                                       .transform = output->transform,
//                                       .scale = output->scale,
//                                       .logical = { .x = scene_output->x, .y = scene_output->y },
//                                       .output = scene_output,
//                                       };

//     output_pending_resolution(output, state,
//                               &render_data.trans_width, &render_data.trans_height);

//     if (state->committed & WLR_OUTPUT_STATE_TRANSFORM) {
//         if (render_data.transform != state->transform) {
//             wlr_damage_ring_add_whole(&scene_output->damage_ring);
//         }

//         render_data.transform = state->transform;
//     }

//     if (state->committed & WLR_OUTPUT_STATE_SCALE) {
//         if (render_data.scale != state->scale) {
//             wlr_damage_ring_add_whole(&scene_output->damage_ring);
//         }

//         render_data.scale = state->scale;
//     }

//     if (render_data.transform & WL_OUTPUT_TRANSFORM_90) {
//         int tmp = render_data.trans_width;
//         render_data.trans_width = render_data.trans_height;
//         render_data.trans_height = tmp;
//     }

//     render_data.logical.width = render_data.trans_width / render_data.scale;
//     render_data.logical.height = render_data.trans_height / render_data.scale;

//     struct render_list_constructor_data list_con = {
//         .box = render_data.logical,
//         .render_list = &scene_output->render_list,
//         .calculate_visibility = scene_output->scene->calculate_visibility,
//     };

//     list_con.render_list->size = 0;
//     scene_nodes_in_box(&scene_output->scene->tree.node, &list_con.box,
//                        construct_render_list_iterator, &list_con);
//     array_realloc(list_con.render_list, list_con.render_list->size);

//     struct render_list_entry *list_data = list_con.render_list->data;
//     int list_len = list_con.render_list->size / sizeof(*list_data);

//     bool scanout = list_len == 1 &&
//                    scene_entry_try_direct_scanout(&list_data[0], state, &render_data);

//     if (scene_output->prev_scanout != scanout) {
//         scene_output->prev_scanout = scanout;
//         wsm_log(WSM_DEBUG, "Direct scan-out %s",
//                 scanout ? "enabled" : "disabled");
//         if (!scanout) {
//             // When exiting direct scan-out, damage everything
//             wlr_damage_ring_add_whole(&scene_output->damage_ring);
//         }
//     }

//     if (scanout) {
//         if (timer) {
//             struct timespec end_time, duration;
//             clock_gettime(CLOCK_MONOTONIC, &end_time);
//             timespec_sub(&duration, &end_time, &start_time);
//             timer->pre_render_duration = timespec_to_nsec(&duration);
//         }
//         return true;
//     }

//     if (debug_damage == WLR_SCENE_DEBUG_DAMAGE_RERENDER) {
//         wlr_damage_ring_add_whole(&scene_output->damage_ring);
//     }

//     struct timespec now;
//     if (debug_damage == WLR_SCENE_DEBUG_DAMAGE_HIGHLIGHT) {
//         struct wl_list *regions = &scene_output->damage_highlight_regions;
//         clock_gettime(CLOCK_MONOTONIC, &now);

//         // add the current frame's damage if there is damage
//         if (pixman_region32_not_empty(&scene_output->damage_ring.current)) {
//             struct highlight_region *current_damage = calloc(1, sizeof(*current_damage));
//             if (current_damage) {
//                 pixman_region32_init(&current_damage->region);
//                 pixman_region32_copy(&current_damage->region,
//                                      &scene_output->damage_ring.current);
//                 current_damage->when = now;
//                 wl_list_insert(regions, &current_damage->link);
//             }
//         }

//         pixman_region32_t acc_damage;
//         pixman_region32_init(&acc_damage);
//         struct highlight_region *damage, *tmp_damage;
//         wl_list_for_each_safe(damage, tmp_damage, regions, link) {
//             // remove overlaping damage regions
//             pixman_region32_subtract(&damage->region, &damage->region, &acc_damage);
//             pixman_region32_union(&acc_damage, &acc_damage, &damage->region);

//             // if this damage is too old or has nothing in it, get rid of it
//             struct timespec time_diff;
//             timespec_sub(&time_diff, &now, &damage->when);
//             if (timespec_to_msec(&time_diff) >= HIGHLIGHT_DAMAGE_FADEOUT_TIME ||
//                 !pixman_region32_not_empty(&damage->region)) {
//                 highlight_region_destroy(damage);
//             }
//         }

//         wlr_damage_ring_add(&scene_output->damage_ring, &acc_damage);
//         pixman_region32_fini(&acc_damage);
//     }

//     wlr_damage_ring_set_bounds(&scene_output->damage_ring,
//                                render_data.trans_width, render_data.trans_height);

//     if (!wlr_output_configure_primary_swapchain(output, state, &output->swapchain)) {
//         return false;
//     }

//     int buffer_age;
//     struct wlr_buffer *buffer = wlr_swapchain_acquire(output->swapchain, &buffer_age);
//     if (buffer == NULL) {
//         return false;
//     }

//     if (timer) {
//         timer->render_timer = wlr_render_timer_create(output->renderer);

//         struct timespec end_time, duration;
//         clock_gettime(CLOCK_MONOTONIC, &end_time);
//         timespec_sub(&duration, &end_time, &start_time);
//         timer->pre_render_duration = timespec_to_nsec(&duration);
//     }

//     struct wlr_render_pass *render_pass = wlr_renderer_begin_buffer_pass(output->renderer, buffer,
//                                                                          &(struct wlr_buffer_pass_options){
//                                                                              .timer = timer ? timer->render_timer : NULL,
//                                                                          });
//     if (render_pass == NULL) {
//         wlr_buffer_unlock(buffer);
//         return false;
//     }

//     render_data.render_pass = render_pass;
//     pixman_region32_init(&render_data.damage);
//     wlr_damage_ring_get_buffer_damage(&scene_output->damage_ring,
//                                       buffer_age, &render_data.damage);

//     pixman_region32_t background;
//     pixman_region32_init(&background);
//     pixman_region32_copy(&background, &render_data.damage);

//     // Cull areas of the background that are occluded by opaque regions of
//     // scene nodes above. Those scene nodes will just render atop having us
//     // never see the background.
//     if (scene_output->scene->calculate_visibility) {
//         for (int i = list_len - 1; i >= 0; i--) {
//             struct render_list_entry *entry = &list_data[i];

//             // We must only cull opaque regions that are visible by the node.
//             // The node's visibility will have the knowledge of a black rect
//             // that may have been omitted from the render list via the black
//             // rect optimization. In order to ensure we don't cull background
//             // rendering in that black rect region, consider the node's visibility.
//             pixman_region32_t opaque;
//             pixman_region32_init(&opaque);
//             scene_node_opaque_region(entry->node, entry->x, entry->y, &opaque);
//             pixman_region32_intersect(&opaque, &opaque, &entry->node->visible);

//             pixman_region32_translate(&opaque, -scene_output->x, -scene_output->y);
//             wlr_region_scale(&opaque, &opaque, render_data.scale);
//             pixman_region32_subtract(&background, &background, &opaque);
//             pixman_region32_fini(&opaque);
//         }

//         if (floor(render_data.scale) != render_data.scale) {
//             wlr_region_expand(&background, &background, 1);

//             // reintersect with the damage because we never want to render
//             // outside of the damage region
//             pixman_region32_intersect(&background, &background, &render_data.damage);
//         }
//     }

//     transform_output_damage(&background, &render_data);
//     wlr_render_pass_add_rect(render_pass, &(struct wlr_render_rect_options){
//                                                                             .box = { .width = buffer->width, .height = buffer->height },
//                                                                             .color = { .r = 0, .g = 0, .b = 0, .a = 1 },
//                                                                             .clip = &background,
//                                                                             });
//     pixman_region32_fini(&background);

//     for (int i = list_len - 1; i >= 0; i--) {
//         struct render_list_entry *entry = &list_data[i];
//         scene_entry_render(entry, &render_data);

//         if (entry->node->type == WLR_SCENE_NODE_BUFFER) {
//             struct wlr_scene_buffer *buffer = wlr_scene_buffer_from_node(entry->node);

//             if (buffer->primary_output == scene_output && !entry->sent_dmabuf_feedback) {
//                 struct wlr_linux_dmabuf_feedback_v1_init_options options = {
//                     .main_renderer = output->renderer,
//                     .scanout_primary_output = NULL,
//                 };

//                 scene_buffer_send_dmabuf_feedback(scene_output->scene, buffer, &options);
//             }
//         }
//     }

//     if (debug_damage == WLR_SCENE_DEBUG_DAMAGE_HIGHLIGHT) {
//         struct highlight_region *damage;
//         wl_list_for_each(damage, &scene_output->damage_highlight_regions, link) {
//             struct timespec time_diff;
//             timespec_sub(&time_diff, &now, &damage->when);
//             int64_t time_diff_ms = timespec_to_msec(&time_diff);
//             float alpha = 1.0 - (double)time_diff_ms / HIGHLIGHT_DAMAGE_FADEOUT_TIME;

//             wlr_render_pass_add_rect(render_pass, &(struct wlr_render_rect_options){
//                                                                                     .box = { .width = buffer->width, .height = buffer->height },
//                                                                                     .color = { .r = alpha * 0.5, .g = 0, .b = 0, .a = alpha * 0.5 },
//                                                                                     .clip = &damage->region,
//                                                                                     });
//         }
//     }

//     wlr_output_add_software_cursors_to_render_pass(output, render_pass, &render_data.damage);

//     pixman_region32_fini(&render_data.damage);

//     if (!wlr_render_pass_submit(render_pass)) {
//         wlr_buffer_unlock(buffer);
//         return false;
//     }

//     wlr_output_state_set_buffer(state, buffer);
//     wlr_buffer_unlock(buffer);
//     output_state_apply_damage(&render_data, state);

//     return true;
// }

void root_get_box(struct wsm_scene *root, struct wlr_box *box) {
    box->x = root->x;
    box->y = root->y;
    box->width = root->width;
    box->height = root->height;
}

static void set_container_transform(struct wsm_workspace *ws,
                                    struct wsm_container *con) {
    struct wsm_output *output = ws->output;
    struct wlr_box box = {0};
    if (output) {
        output_get_box(output, &box);
    }
    con->transform = box;
}

void root_scratchpad_show(struct wsm_container *con) {
    struct wsm_seat *seat = input_manager_current_seat();
    struct wsm_workspace *new_ws = seat_get_focused_workspace(seat);
    if (!new_ws) {
        wsm_log(WSM_DEBUG, "No focused workspace to show scratchpad on");
        return;
    }
    struct wsm_workspace *old_ws = con->pending.workspace;

    // If the current con or any of its parents are in fullscreen mode, we
    // first need to disable it before showing the scratchpad con.
    if (new_ws->fullscreen) {
        container_fullscreen_disable(new_ws->fullscreen);
    }
    if (global_server.wsm_scene->fullscreen_global) {
        container_fullscreen_disable(global_server.wsm_scene->fullscreen_global);
    }

    // Show the container
    if (old_ws) {
        container_detach(con);
        // Make sure the last inactive container on the old workspace is above
        // the workspace itself in the focus stack.
        struct wsm_node *node = seat_get_focus_inactive(seat, &old_ws->node);
        seat_set_raw_focus(seat, node);
    } else {
        // Act on the ancestor of scratchpad hidden split containers
        while (con->pending.parent) {
            con = con->pending.parent;
        }
    }
    workspace_add_floating(new_ws, con);

    if (new_ws->output) {
        struct wlr_box output_box;
        output_get_box(new_ws->output, &output_box);
        floating_fix_coordinates(con, &con->transform, &output_box);
    }
    set_container_transform(new_ws, con);

    arrange_workspace(new_ws);
    seat_set_focus(seat, seat_get_focus_inactive(seat, &con->node));
    if (old_ws) {
        workspace_consider_destroy(old_ws);
    }
}

void arrange_root(void) {
    struct wlr_box layout_box;
    wlr_output_layout_get_box(global_server.wsm_scene->output_layout, NULL, &layout_box);
    global_server.wsm_scene->x = layout_box.x;
    global_server.wsm_scene->y = layout_box.y;
    global_server.wsm_scene->width = layout_box.width;
    global_server.wsm_scene->height = layout_box.height;

    if (global_server.wsm_scene->fullscreen_global) {
        struct wsm_container *fs = global_server.wsm_scene->fullscreen_global;
        fs->pending.x = global_server.wsm_scene->x;
        fs->pending.y = global_server.wsm_scene->y;
        fs->pending.width = global_server.wsm_scene->width;
        fs->pending.height = global_server.wsm_scene->height;
        arrange_container(fs);
    } else {
        for (int i = 0; i < global_server.wsm_scene->outputs->length; ++i) {
            struct wsm_output *output = global_server.wsm_scene->outputs->items[i];
            arrange_output(output);
        }
    }
}
