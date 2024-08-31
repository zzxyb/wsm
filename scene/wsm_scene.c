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
#include "wsm_common.h"
#include "wsm_input_manager.h"
#include "node/wsm_node_descriptor.h"
#include "wsm_output_manager.h"
#include "wsm_workspace.h"
#include "wsm_arrange.h"

#include <stdlib.h>
#include <assert.h>

#include <drm_fourcc.h>

#include <wlr/util/log.h>
#include <wlr/util/transform.h>
#include <wlr/util/region.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_output.h>
#include <wlr/render/swapchain.h>
#include <wlr/types/wlr_output_layout.h>

#define HIGHLIGHT_DAMAGE_FADEOUT_TIME 250

struct render_data {
	enum wl_output_transform transform;
	float scale;
	struct wlr_box logical;
	int trans_width, trans_height;

	struct wlr_scene_output *output;

	struct wlr_render_pass *render_pass;
	pixman_region32_t damage;
};

struct render_list_constructor_data {
	struct wlr_box box;
	struct wl_array *render_list;
	bool calculate_visibility;
	bool highlight_transparent_region;
	bool fractional_scale;
};

struct render_list_entry {
	struct wlr_scene_node *node;
	bool sent_dmabuf_feedback;
	bool highlight_transparent_region;
	int x, y;
};

struct highlight_region {
	pixman_region32_t region;
	struct timespec when;
	struct wl_list link;
};

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

bool wsm_scene_output_commit(struct wlr_scene_output *scene_output,
	const struct wlr_scene_output_state_options *options) {
	if (!scene_output->output->needs_frame && !pixman_region32_not_empty(
			&scene_output->damage_ring.current)) {
		return true;
	}

	bool ok = false;
	struct wlr_output_state state;
	wlr_output_state_init(&state);
	if (!wsm_scene_output_build_state(scene_output, &state, options)) {
		goto out;
	}

	ok = wlr_output_commit_state(scene_output->output, &state);
	if (!ok) {
		goto out;
	}

	wlr_damage_ring_rotate(&scene_output->damage_ring);

out:
	wlr_output_state_finish(&state);
	return ok;
}

static void output_pending_resolution(struct wlr_output *output,
	const struct wlr_output_state *state, int *width, int *height) {
	if (state->committed & WLR_OUTPUT_STATE_MODE) {
		switch (state->mode_type) {
		case WLR_OUTPUT_STATE_MODE_FIXED:
			*width = state->mode->width;
			*height = state->mode->height;
			return;
		case WLR_OUTPUT_STATE_MODE_CUSTOM:
			*width = state->custom_mode.width;
			*height = state->custom_mode.height;
			return;
		}
		abort();
	} else {
		*width = output->width;
		*height = output->height;
	}
}

static void scene_node_get_size(struct wlr_scene_node *node,
	int *width, int *height) {
	*width = 0;
	*height = 0;

	switch (node->type) {
	case WLR_SCENE_NODE_TREE:
		return;
	case WLR_SCENE_NODE_RECT:;
		struct wlr_scene_rect *scene_rect = wlr_scene_rect_from_node(node);
		*width = scene_rect->width;
		*height = scene_rect->height;
		break;
	case WLR_SCENE_NODE_BUFFER:;
		struct wlr_scene_buffer *scene_buffer = wlr_scene_buffer_from_node(node);
		if (scene_buffer->dst_width > 0 && scene_buffer->dst_height > 0) {
			*width = scene_buffer->dst_width;
			*height = scene_buffer->dst_height;
		} else {
			*width = scene_buffer->buffer_width;
			*height = scene_buffer->buffer_height;
			wlr_output_transform_coords(scene_buffer->transform, width, height);
		}
		break;
	}
}

typedef bool (*scene_node_box_iterator_func_t)(struct wlr_scene_node *node,
	int sx, int sy, void *data);

static bool _scene_nodes_in_box(struct wlr_scene_node *node, struct wlr_box *box,
	scene_node_box_iterator_func_t iterator, void *user_data, int lx, int ly) {
	if (!node->enabled) {
		return false;
	}

	switch (node->type) {
	case WLR_SCENE_NODE_TREE:;
		struct wlr_scene_tree *scene_tree = wlr_scene_tree_from_node(node);
		struct wlr_scene_node *child;
		wl_list_for_each_reverse(child, &scene_tree->children, link) {
			if (_scene_nodes_in_box(child, box, iterator, user_data, lx + child->x, ly + child->y)) {
				return true;
			}
		}
		break;
	case WLR_SCENE_NODE_RECT:
	case WLR_SCENE_NODE_BUFFER:;
		struct wlr_box node_box = { .x = lx, .y = ly };
		scene_node_get_size(node, &node_box.width, &node_box.height);
		
		if (wlr_box_intersection(&node_box, &node_box, box) &&
			iterator(node, lx, ly, user_data)) {
			return true;
		}
		break;
	}

	return false;
}

static bool scene_nodes_in_box(struct wlr_scene_node *node, struct wlr_box *box,
	scene_node_box_iterator_func_t iterator, void *user_data) {
	int x, y;
	wlr_scene_node_coords(node, &x, &y);

	return _scene_nodes_in_box(node, box, iterator, user_data, x, y);
}

static bool scene_node_invisible(struct wlr_scene_node *node) {
	if (node->type == WLR_SCENE_NODE_TREE) {
		return true;
	} else if (node->type == WLR_SCENE_NODE_RECT) {
		struct wlr_scene_rect *rect = wlr_scene_rect_from_node(node);

		return rect->color[3] == 0.f;
	} else if (node->type == WLR_SCENE_NODE_BUFFER) {
		struct wlr_scene_buffer *buffer = wlr_scene_buffer_from_node(node);

		return buffer->buffer == NULL && buffer->texture == NULL;
	}

	return false;
}

static bool construct_render_list_iterator(struct wlr_scene_node *node,
	int lx, int ly, void *_data) {
	struct render_list_constructor_data *data = _data;

	if (scene_node_invisible(node)) {
		return false;
	}

	if (node->type == WLR_SCENE_NODE_RECT && data->calculate_visibility &&
		(!data->fractional_scale || data->render_list->size == 0)) {
		struct wlr_scene_rect *rect = wlr_scene_rect_from_node(node);
		float *black = (float[4]){ 0.f, 0.f, 0.f, 1.f };
		
		if (memcmp(rect->color, black, sizeof(float) * 4) == 0) {
			return false;
		}
	}

	pixman_region32_t intersection;
	pixman_region32_init(&intersection);
	pixman_region32_intersect_rect(&intersection, &node->visible,
		data->box.x, data->box.y,
		data->box.width, data->box.height);
	if (!pixman_region32_not_empty(&intersection)) {
		pixman_region32_fini(&intersection);
		return false;
	}

	pixman_region32_fini(&intersection);

	struct render_list_entry *entry = wl_array_add(data->render_list, sizeof(*entry));
	if (!entry) {
		return false;
	}

	*entry = (struct render_list_entry){
		.node = node,
		.x = lx,
		.y = ly,
		.highlight_transparent_region = data->highlight_transparent_region,
	};

	return false;
}

static bool array_realloc(struct wl_array *arr, size_t size) {
	size_t alloc;
	if (arr->alloc > 0 && size > arr->alloc / 4) {
		alloc = arr->alloc;
	} else {
		alloc = 16;
	}

	while (alloc < size) {
		alloc *= 2;
	}

	if (alloc == arr->alloc) {
		return true;
	}

	void *data = realloc(arr->data, alloc);
	if (data == NULL) {
		return false;
	}
	arr->data = data;
	arr->alloc = alloc;
	return true;
}

static void scene_buffer_send_dmabuf_feedback(const struct wlr_scene *scene,
	struct wlr_scene_buffer *scene_buffer,
	const struct wlr_linux_dmabuf_feedback_v1_init_options *options) {
	if (!scene->linux_dmabuf_v1) {
		return;
	}

	struct wlr_scene_surface *surface = wlr_scene_surface_try_from_buffer(scene_buffer);
	if (!surface) {
		return;
	}

	if (memcmp(options, &scene_buffer->prev_feedback_options, sizeof(*options)) == 0) {
		return;
	}

	scene_buffer->prev_feedback_options = *options;
	struct wlr_linux_dmabuf_feedback_v1 feedback = {0};
	if (!wlr_linux_dmabuf_feedback_v1_init_with_options(&feedback, options)) {
		return;
	}

	wlr_linux_dmabuf_v1_set_surface_feedback(scene->linux_dmabuf_v1,
		surface->surface, &feedback);

	wlr_linux_dmabuf_feedback_v1_finish(&feedback);
}

static void transform_output_damage(pixman_region32_t *damage, const struct render_data *data) {
	enum wl_output_transform transform = wlr_output_transform_invert(data->transform);
	wlr_region_transform(damage, damage, transform, data->trans_width, data->trans_height);
}

static void output_state_apply_damage(const struct render_data *data,
	struct wlr_output_state *state) {
	struct wlr_scene_output *output = data->output;

	pixman_region32_t frame_damage;
	pixman_region32_init(&frame_damage);
	pixman_region32_copy(&frame_damage, &output->damage_ring.current);
	transform_output_damage(&frame_damage, data);
	pixman_region32_union(&output->pending_commit_damage,
		&output->pending_commit_damage, &frame_damage);
	pixman_region32_fini(&frame_damage);

	wlr_output_state_set_damage(state, &output->pending_commit_damage);
}

static bool scene_entry_try_direct_scanout(struct render_list_entry *entry,
	struct wlr_output_state *state, const struct render_data *data) {
	struct wlr_scene_output *scene_output = data->output;
	struct wlr_scene_node *node = entry->node;

	if (!scene_output->scene->direct_scanout) {
		return false;
	}

	if (node->type != WLR_SCENE_NODE_BUFFER) {
		return false;
	}

	if (state->committed & (WLR_OUTPUT_STATE_MODE |
			WLR_OUTPUT_STATE_ENABLED |
			WLR_OUTPUT_STATE_RENDER_FORMAT)) {
		return false;
	}

	if (!wlr_output_is_direct_scanout_allowed(scene_output->output)) {
		return false;
	}

	struct wlr_scene_buffer *buffer = wlr_scene_buffer_from_node(node);
	if (buffer->buffer == NULL) {
		return false;
	}

	int default_width = buffer->buffer->width;
	int default_height = buffer->buffer->height;
	wlr_output_transform_coords(buffer->transform, &default_width, &default_height);
	struct wlr_fbox default_box = {
		.width = default_width,
		.height = default_height,
	};

	if (!wlr_fbox_empty(&buffer->src_box) &&
		!wlr_fbox_equal(&buffer->src_box, &default_box)) {
		return false;
	}

	if (buffer->transform != data->transform) {
		return false;
	}

	struct wlr_box node_box = { .x = entry->x, .y = entry->y };
	scene_node_get_size(node, &node_box.width, &node_box.height);

	if (!wlr_box_equal(&data->logical, &node_box)) {
		return false;
	}

	if (buffer->primary_output == scene_output) {
		struct wlr_linux_dmabuf_feedback_v1_init_options options = {
			.main_renderer = scene_output->output->renderer,
			.scanout_primary_output = scene_output->output,
		};

		scene_buffer_send_dmabuf_feedback(scene_output->scene, buffer, &options);
		entry->sent_dmabuf_feedback = true;
	}

	struct wlr_output_state pending;
	wlr_output_state_init(&pending);
	if (!wlr_output_state_copy(&pending, state)) {
		return false;
	}

	wlr_output_state_set_buffer(&pending, buffer->buffer);

	if (!wlr_output_test_state(scene_output->output, &pending)) {
		wlr_output_state_finish(&pending);
		return false;
	}

	wlr_output_state_copy(state, &pending);
	wlr_output_state_finish(&pending);

	struct wlr_scene_output_sample_event sample_event = {
		.output = scene_output,
		.direct_scanout = true,
	};
	wl_signal_emit_mutable(&buffer->events.output_sample, &sample_event);
	return true;
}

static void highlight_region_destroy(struct highlight_region *damage) {
	wl_list_remove(&damage->link);
	pixman_region32_fini(&damage->region);
	free(damage);
}

static void scene_node_opaque_region(struct wlr_scene_node *node, int x, int y,
	pixman_region32_t *opaque) {
	int width, height;
	scene_node_get_size(node, &width, &height);

	if (node->type == WLR_SCENE_NODE_RECT) {
		struct wlr_scene_rect *scene_rect = wlr_scene_rect_from_node(node);
		if (scene_rect->color[3] != 1) {
			return;
		}
	} else if (node->type == WLR_SCENE_NODE_BUFFER) {
		struct wlr_scene_buffer *scene_buffer = wlr_scene_buffer_from_node(node);
		
		if (!scene_buffer->buffer) {
			return;
		}

		if (scene_buffer->opacity != 1) {
			return;
		}

		if (!scene_buffer->buffer_is_opaque) {
			pixman_region32_copy(opaque, &scene_buffer->opaque_region);
			pixman_region32_intersect_rect(opaque, opaque, 0, 0, width, height);
			pixman_region32_translate(opaque, x, y);
			return;
		}
	}

	pixman_region32_fini(opaque);
	pixman_region32_init_rect(opaque, x, y, width, height);
}

static void scene_buffer_set_texture(struct wlr_scene_buffer *scene_buffer,
									 struct wlr_texture *texture);

static void scene_buffer_handle_renderer_destroy(struct wl_listener *listener,
												 void *data) {
	struct wlr_scene_buffer *scene_buffer = wl_container_of(listener, scene_buffer, renderer_destroy);
	scene_buffer_set_texture(scene_buffer, NULL);
}

static void scene_buffer_set_texture(struct wlr_scene_buffer *scene_buffer,
	struct wlr_texture *texture) {
	wl_list_remove(&scene_buffer->renderer_destroy.link);
	wlr_texture_destroy(scene_buffer->texture);
	scene_buffer->texture = texture;

	if (texture != NULL) {
		scene_buffer->renderer_destroy.notify = scene_buffer_handle_renderer_destroy;
		wl_signal_add(&texture->renderer->events.destroy, &scene_buffer->renderer_destroy);
	} else {
		wl_list_init(&scene_buffer->renderer_destroy.link);
	}
}

static void scale_output_damage(pixman_region32_t *damage, float scale) {
	wlr_region_scale(damage, damage, scale);

	if (floor(scale) != scale) {
		wlr_region_expand(damage, damage, 1);
	}
}

static int scale_length(int length, int offset, float scale) {
	return round((offset + length) * scale) - round(offset * scale);
}

static void scale_box(struct wlr_box *box, float scale) {
	box->width = scale_length(box->width, box->x, scale);
	box->height = scale_length(box->height, box->y, scale);
	box->x = round(box->x * scale);
	box->y = round(box->y * scale);
}

static void transform_output_box(struct wlr_box *box, const struct render_data *data) {
	enum wl_output_transform transform = wlr_output_transform_invert(data->transform);
	wlr_box_transform(box, box, transform, data->trans_width, data->trans_height);
}

static struct wlr_texture *scene_buffer_get_texture(
	struct wlr_scene_buffer *scene_buffer, struct wlr_renderer *renderer) {
	if (scene_buffer->buffer == NULL || scene_buffer->texture != NULL) {
		return scene_buffer->texture;
	}

	struct wlr_client_buffer *client_buffer =
		wlr_client_buffer_get(scene_buffer->buffer);
	if (client_buffer != NULL) {
		return client_buffer->texture;
	}

	struct wlr_texture *texture =
		wlr_texture_from_buffer(renderer, scene_buffer->buffer);
	if (texture != NULL && scene_buffer->own_buffer) {
		scene_buffer->own_buffer = false;
		wlr_buffer_unlock(scene_buffer->buffer);
	}
	scene_buffer_set_texture(scene_buffer, texture);
	return texture;
}

static void scene_entry_render(struct render_list_entry *entry, const struct render_data *data) {
	struct wlr_scene_node *node = entry->node;

	pixman_region32_t render_region;
	pixman_region32_init(&render_region);
	pixman_region32_copy(&render_region, &node->visible);
	pixman_region32_translate(&render_region, -data->logical.x, -data->logical.y);
	scale_output_damage(&render_region, data->scale);
	pixman_region32_intersect(&render_region, &render_region, &data->damage);
	if (!pixman_region32_not_empty(&render_region)) {
		pixman_region32_fini(&render_region);
		return;
	}

	int x = entry->x - data->logical.x;
	int y = entry->y - data->logical.y;

	struct wlr_box dst_box = {
		.x = x,
		.y = y,
	};
	scene_node_get_size(node, &dst_box.width, &dst_box.height);
	scale_box(&dst_box, data->scale);

	pixman_region32_t opaque;
	pixman_region32_init(&opaque);
	scene_node_opaque_region(node, x, y, &opaque);
	scale_output_damage(&opaque, data->scale);
	pixman_region32_subtract(&opaque, &render_region, &opaque);

	transform_output_box(&dst_box, data);
	transform_output_damage(&render_region, data);

	switch (node->type) {
	case WLR_SCENE_NODE_TREE:
		assert(false);
		break;
	case WLR_SCENE_NODE_RECT:;
		struct wlr_scene_rect *scene_rect = wlr_scene_rect_from_node(node);

		wlr_render_pass_add_rect(data->render_pass, &(struct wlr_render_rect_options){
				.box = dst_box,
				.color = {
					.r = scene_rect->color[0],
					.g = scene_rect->color[1],
					.b = scene_rect->color[2],
					.a = scene_rect->color[3],
				},
				.clip = &render_region,
		});
		break;
	case WLR_SCENE_NODE_BUFFER:;
		struct wlr_scene_buffer *scene_buffer = wlr_scene_buffer_from_node(node);

		struct wlr_texture *texture = scene_buffer_get_texture(scene_buffer,
			data->output->output->renderer);
		if (texture == NULL) {
			wlr_damage_ring_add(&data->output->damage_ring, &render_region);
			break;
		}

		enum wl_output_transform transform =
			wlr_output_transform_invert(scene_buffer->transform);
		transform = wlr_output_transform_compose(transform, data->transform);

		wlr_render_pass_add_texture(data->render_pass, &(struct wlr_render_texture_options) {
			.texture = texture,
			.src_box = scene_buffer->src_box,
			.dst_box = dst_box,
			.transform = transform,
			.clip = &render_region,
			.alpha = &scene_buffer->opacity,
			.filter_mode = scene_buffer->filter_mode,
			.blend_mode = pixman_region32_not_empty(&opaque) ?
				WLR_RENDER_BLEND_MODE_PREMULTIPLIED : WLR_RENDER_BLEND_MODE_NONE,
		});

		struct wlr_scene_output_sample_event sample_event = {
			.output = data->output,
			.direct_scanout = false,
		};
		wl_signal_emit_mutable(&scene_buffer->events.output_sample, &sample_event);

		if (entry->highlight_transparent_region) {
			wlr_render_pass_add_rect(data->render_pass, &(struct wlr_render_rect_options){
				.box = dst_box,
				.color = { .r = 0, .g = 0.3, .b = 0, .a = 0.3 },
				.clip = &opaque,
			});
		}

		break;
	}

	pixman_region32_fini(&opaque);
	pixman_region32_fini(&render_region);
}

bool wsm_scene_output_build_state(struct wlr_scene_output *scene_output,
	struct wlr_output_state *state, const struct wlr_scene_output_state_options *options) {
	struct wlr_scene_output_state_options default_options = {0};
	if (!options) {
		options = &default_options;
	}
	struct wlr_scene_timer *timer = options->timer;
	struct timespec start_time;
	if (timer) {
		clock_gettime(CLOCK_MONOTONIC, &start_time);
		wlr_scene_timer_finish(timer);
		*timer = (struct wlr_scene_timer){0};
	}

	if ((state->committed & WLR_OUTPUT_STATE_ENABLED) && !state->enabled) {
		return true;
	}

	struct wlr_output *output = scene_output->output;
	enum wlr_scene_debug_damage_option debug_damage =
		scene_output->scene->debug_damage_option;

	struct render_data render_data = {
		.transform = output->transform,
		.scale = output->scale,
		.logical = { .x = scene_output->x, .y = scene_output->y },
		.output = scene_output,
	};

	int resolution_width, resolution_height;
	output_pending_resolution(output, state,
		&resolution_width, &resolution_height);

	if (state->committed & WLR_OUTPUT_STATE_TRANSFORM) {
		if (render_data.transform != state->transform) {
			wlr_damage_ring_add_whole(&scene_output->damage_ring);
		}

		render_data.transform = state->transform;
	}

	if (state->committed & WLR_OUTPUT_STATE_SCALE) {
		if (render_data.scale != state->scale) {
			wlr_damage_ring_add_whole(&scene_output->damage_ring);
		}

		render_data.scale = state->scale;
	}

	render_data.trans_width = resolution_width;
	render_data.trans_height = resolution_height;
	wlr_output_transform_coords(render_data.transform,
		&render_data.trans_width, &render_data.trans_height);

	render_data.logical.width = render_data.trans_width / render_data.scale;
	render_data.logical.height = render_data.trans_height / render_data.scale;

	struct render_list_constructor_data list_con = {
		.box = render_data.logical,
		.render_list = &scene_output->render_list,
		.calculate_visibility = scene_output->scene->calculate_visibility,
		.highlight_transparent_region = scene_output->scene->highlight_transparent_region,
		.fractional_scale = floor(render_data.scale) != render_data.scale,
	};

	list_con.render_list->size = 0;
	scene_nodes_in_box(&scene_output->scene->tree.node, &list_con.box,
		construct_render_list_iterator, &list_con);
	array_realloc(list_con.render_list, list_con.render_list->size);

	struct render_list_entry *list_data = list_con.render_list->data;
	int list_len = list_con.render_list->size / sizeof(*list_data);

	wlr_damage_ring_set_bounds(&scene_output->damage_ring,
		render_data.trans_width, render_data.trans_height);

	if (debug_damage == WLR_SCENE_DEBUG_DAMAGE_RERENDER) {
		wlr_damage_ring_add_whole(&scene_output->damage_ring);
	}

	struct timespec now;
	if (debug_damage == WLR_SCENE_DEBUG_DAMAGE_HIGHLIGHT) {
		struct wl_list *regions = &scene_output->damage_highlight_regions;
		clock_gettime(CLOCK_MONOTONIC, &now);
		if (pixman_region32_not_empty(&scene_output->damage_ring.current)) {
			struct highlight_region *current_damage = calloc(1, sizeof(*current_damage));
			if (current_damage) {
				pixman_region32_init(&current_damage->region);
				pixman_region32_copy(&current_damage->region,
					&scene_output->damage_ring.current);
				current_damage->when = now;
				wl_list_insert(regions, &current_damage->link);
			}
		}

		pixman_region32_t acc_damage;
		pixman_region32_init(&acc_damage);
		struct highlight_region *damage, *tmp_damage;
		wl_list_for_each_safe(damage, tmp_damage, regions, link) {
			pixman_region32_subtract(&damage->region, &damage->region, &acc_damage);
			pixman_region32_union(&acc_damage, &acc_damage, &damage->region);
			struct timespec time_diff;
			timespec_sub(&time_diff, &now, &damage->when);
			if (timespec_to_msec(&time_diff) >= HIGHLIGHT_DAMAGE_FADEOUT_TIME ||
				!pixman_region32_not_empty(&damage->region)) {
				highlight_region_destroy(damage);
			}
		}

		wlr_damage_ring_add(&scene_output->damage_ring, &acc_damage);
		pixman_region32_fini(&acc_damage);
	}

	output_state_apply_damage(&render_data, state);
	bool scanout = options->color_transform == NULL &&
		list_len == 1 && debug_damage != WLR_SCENE_DEBUG_DAMAGE_HIGHLIGHT &&
		scene_entry_try_direct_scanout(&list_data[0], state, &render_data);

	if (scene_output->prev_scanout != scanout) {
		scene_output->prev_scanout = scanout;
		wlr_log(WLR_DEBUG, "Direct scan-out %s",
			scanout ? "enabled" : "disabled");
	}

	if (scanout) {
		if (timer) {
			struct timespec end_time, duration;
			clock_gettime(CLOCK_MONOTONIC, &end_time);
			timespec_sub(&duration, &end_time, &start_time);
			timer->pre_render_duration = timespec_to_nsec(&duration);
		}
		return true;
	}

	struct wlr_swapchain *swapchain = options->swapchain;
	if (!swapchain) {
		if (!wlr_output_configure_primary_swapchain(output, state, &output->swapchain)) {
			return false;
		}

		swapchain = output->swapchain;
	}

	struct wlr_buffer *buffer = wlr_swapchain_acquire(swapchain, NULL);
	if (buffer == NULL) {
		return false;
	}

	assert(buffer->width == resolution_width && buffer->height == resolution_height);

	if (timer) {
		timer->render_timer = wlr_render_timer_create(output->renderer);

		struct timespec end_time, duration;
		clock_gettime(CLOCK_MONOTONIC, &end_time);
		timespec_sub(&duration, &end_time, &start_time);
		timer->pre_render_duration = timespec_to_nsec(&duration);
	}

	struct wlr_render_pass *render_pass = wlr_renderer_begin_buffer_pass(output->renderer, buffer,
		&(struct wlr_buffer_pass_options){
			.timer = timer ? timer->render_timer : NULL,
			.color_transform = options->color_transform,
	});
	if (render_pass == NULL) {
		wlr_buffer_unlock(buffer);
		return false;
	}

	render_data.render_pass = render_pass;

	pixman_region32_init(&render_data.damage);
	wlr_damage_ring_rotate_buffer(&scene_output->damage_ring, buffer,
		&render_data.damage);

	pixman_region32_t background;
	pixman_region32_init(&background);
	pixman_region32_copy(&background, &render_data.damage);

	if (scene_output->scene->calculate_visibility) {
		for (int i = list_len - 1; i >= 0; i--) {
			struct render_list_entry *entry = &list_data[i];
			pixman_region32_t opaque;
			pixman_region32_init(&opaque);
			scene_node_opaque_region(entry->node, entry->x, entry->y, &opaque);
			pixman_region32_intersect(&opaque, &opaque, &entry->node->visible);

			pixman_region32_translate(&opaque, -scene_output->x, -scene_output->y);
			wlr_region_scale(&opaque, &opaque, render_data.scale);
			pixman_region32_subtract(&background, &background, &opaque);
			pixman_region32_fini(&opaque);
		}

		if (floor(render_data.scale) != render_data.scale) {
			wlr_region_expand(&background, &background, 1);
			pixman_region32_intersect(&background, &background, &render_data.damage);
		}
	}

	transform_output_damage(&background, &render_data);
	wlr_render_pass_add_rect(render_pass, &(struct wlr_render_rect_options){
		.box = { .width = buffer->width, .height = buffer->height },
		.color = { .r = 0, .g = 0, .b = 0, .a = 1 },
		.clip = &background,
	});
	pixman_region32_fini(&background);

	for (int i = list_len - 1; i >= 0; i--) {
		struct render_list_entry *entry = &list_data[i];
		scene_entry_render(entry, &render_data);

		if (entry->node->type == WLR_SCENE_NODE_BUFFER) {
			struct wlr_scene_buffer *buffer = wlr_scene_buffer_from_node(entry->node);

			if (buffer->primary_output == scene_output && !entry->sent_dmabuf_feedback) {
				struct wlr_linux_dmabuf_feedback_v1_init_options options = {
					.main_renderer = output->renderer,
					.scanout_primary_output = NULL,
				};

				scene_buffer_send_dmabuf_feedback(scene_output->scene, buffer, &options);
			}
		}
	}

	if (debug_damage == WLR_SCENE_DEBUG_DAMAGE_HIGHLIGHT) {
		struct highlight_region *damage;
		wl_list_for_each(damage, &scene_output->damage_highlight_regions, link) {
			struct timespec time_diff;
			timespec_sub(&time_diff, &now, &damage->when);
			int64_t time_diff_ms = timespec_to_msec(&time_diff);
			float alpha = 1.0 - (double)time_diff_ms / HIGHLIGHT_DAMAGE_FADEOUT_TIME;

			wlr_render_pass_add_rect(render_pass, &(struct wlr_render_rect_options){
				.box = { .width = buffer->width, .height = buffer->height },
				.color = { .r = alpha * 0.5, .g = 0, .b = 0, .a = alpha * 0.5 },
				.clip = &damage->region,
			});
		}
	}

	wlr_output_add_software_cursors_to_render_pass(output, render_pass, &render_data.damage);

	pixman_region32_fini(&render_data.damage);

	if (!wlr_render_pass_submit(render_pass)) {
		wlr_buffer_unlock(buffer);
		wlr_damage_ring_add_whole(&scene_output->damage_ring);
		return false;
	}

	wlr_output_state_set_buffer(state, buffer);
	wlr_buffer_unlock(buffer);

	return true;
}

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
	if (new_ws->fullscreen) {
		container_fullscreen_disable(new_ws->fullscreen);
	}

	if (global_server.wsm_scene->fullscreen_global) {
		container_fullscreen_disable(global_server.wsm_scene->fullscreen_global);
	}

	if (old_ws) {
		container_detach(con);
		struct wsm_node *node = seat_get_focus_inactive(seat, &old_ws->node);
		seat_set_raw_focus(seat, node);
	} else {
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

	wsm_arrange_workspace_auto(new_ws);
	seat_set_focus(seat, seat_get_focus_inactive(seat, &con->node));
	if (old_ws) {
		workspace_consider_destroy(old_ws);
	}
}
