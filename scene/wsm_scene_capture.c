#include "wsm_scene_capture.h"

#include "wsm_log.h"

#include <math.h>
#include <stdlib.h>

#include <drm_fourcc.h>
#include <pixman.h>

#include <wlr/render/allocator.h>
#include <wlr/render/drm_format_set.h>
#include <wlr/render/pass.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/render/wlr_texture.h>
#include <wlr/types/wlr_buffer.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/util/transform.h>

struct capture_render_data {
	struct wlr_render_pass *pass;
	struct wlr_renderer *renderer;
	struct wlr_box bounds;
	float scale;
	struct wlr_texture **textures;
	size_t textures_len;
	bool failed;
};

static void measure_tree(struct wlr_scene_tree *tree, int sx, int sy,
		pixman_region32_t *bounds) {
	struct wlr_scene_node *node;
	wl_list_for_each(node, &tree->children, link) {
		if (!node->enabled) {
			continue;
		}

		int x = sx + node->x;
		int y = sy + node->y;
		if (node->type == WLR_SCENE_NODE_TREE) {
			measure_tree(wlr_scene_tree_from_node(node), x, y, bounds);
			continue;
		}

		int width = 0;
		int height = 0;
		if (node->type == WLR_SCENE_NODE_RECT) {
			struct wlr_scene_rect *rect = wlr_scene_rect_from_node(node);
			width = rect->width;
			height = rect->height;
		} else {
			struct wlr_scene_buffer *buffer =
				wlr_scene_buffer_from_node(node);
			if (buffer->buffer == NULL && buffer->WLR_PRIVATE.texture == NULL) {
				continue;
			}
			if (buffer->dst_width > 0 && buffer->dst_height > 0) {
				width = buffer->dst_width;
				height = buffer->dst_height;
			} else {
				width = buffer->WLR_PRIVATE.buffer_width;
				height = buffer->WLR_PRIVATE.buffer_height;
				wlr_output_transform_coords(
					buffer->transform, &width, &height);
			}
		}
		if (width > 0 && height > 0) {
			pixman_region32_union_rect(bounds, bounds, x, y, width, height);
		}
	}
}

static bool remember_texture(struct capture_render_data *data,
		struct wlr_texture *texture) {
	void *textures = realloc(data->textures,
		(data->textures_len + 1) * sizeof(*data->textures));
	if (textures == NULL) {
		return false;
	}
	data->textures = textures;
	data->textures[data->textures_len++] = texture;
	return true;
}

static struct wlr_box scaled_box(struct capture_render_data *data,
		int x, int y, int width, int height) {
	double left = (x - data->bounds.x) * data->scale;
	double top = (y - data->bounds.y) * data->scale;
	double right = (x + width - data->bounds.x) * data->scale;
	double bottom = (y + height - data->bounds.y) * data->scale;
	return (struct wlr_box) {
		.x = lround(left),
		.y = lround(top),
		.width = lround(right) - lround(left),
		.height = lround(bottom) - lround(top),
	};
}

static void render_tree(struct wlr_scene_tree *tree, int sx, int sy,
		struct capture_render_data *data) {
	struct wlr_scene_node *node;
	wl_list_for_each(node, &tree->children, link) {
		if (!node->enabled || data->failed) {
			continue;
		}

		int x = sx + node->x;
		int y = sy + node->y;
		if (node->type == WLR_SCENE_NODE_TREE) {
			render_tree(wlr_scene_tree_from_node(node), x, y, data);
			continue;
		}

		if (node->type == WLR_SCENE_NODE_RECT) {
			struct wlr_scene_rect *rect = wlr_scene_rect_from_node(node);
			struct wlr_box box = scaled_box(
				data, x, y, rect->width, rect->height);
			if (box.width <= 0 || box.height <= 0) {
				continue;
			}
			wlr_render_pass_add_rect(data->pass,
				&(struct wlr_render_rect_options) {
					.box = box,
					.color = {
						.r = rect->color[0],
						.g = rect->color[1],
						.b = rect->color[2],
						.a = rect->color[3],
					},
				});
			continue;
		}

		struct wlr_scene_buffer *buffer =
			wlr_scene_buffer_from_node(node);
		if (buffer->buffer == NULL && buffer->WLR_PRIVATE.texture == NULL) {
			continue;
		}

		int width;
		int height;
		if (buffer->dst_width > 0 && buffer->dst_height > 0) {
			width = buffer->dst_width;
			height = buffer->dst_height;
		} else {
			width = buffer->WLR_PRIVATE.buffer_width;
			height = buffer->WLR_PRIVATE.buffer_height;
			wlr_output_transform_coords(buffer->transform, &width, &height);
		}
		struct wlr_box box = scaled_box(data, x, y, width, height);
		if (box.width <= 0 || box.height <= 0) {
			continue;
		}

		struct wlr_texture *texture = buffer->WLR_PRIVATE.texture;
		bool texture_owned = false;
		if (texture == NULL) {
			struct wlr_client_buffer *client_buffer =
				wlr_client_buffer_get(buffer->buffer);
			if (client_buffer != NULL) {
				texture = client_buffer->texture;
			}
			if (texture == NULL) {
				texture = wlr_texture_from_buffer(
					data->renderer, buffer->buffer);
				texture_owned = texture != NULL;
			}
		}
		if (texture == NULL) {
			wsm_log(WSM_DEBUG,
				"Scene capture failed: could not import source buffer");
			data->failed = true;
			continue;
		}
		if (texture_owned && !remember_texture(data, texture)) {
			wlr_texture_destroy(texture);
			wsm_log(WSM_DEBUG,
				"Scene capture failed: could not track imported texture");
			data->failed = true;
			continue;
		}

		struct wlr_fbox source_box = buffer->src_box;
		if (wlr_fbox_empty(&source_box)) {
			source_box = (struct wlr_fbox) {
				.width = texture->width,
				.height = texture->height,
			};
		}

		wlr_render_pass_add_texture(data->pass,
			&(struct wlr_render_texture_options) {
				.texture = texture,
				.src_box = source_box,
				.dst_box = box,
				.transform =
					wlr_output_transform_invert(buffer->transform),
				.alpha = &buffer->opacity,
				.filter_mode = buffer->filter_mode,
			});
	}
}

static struct wlr_buffer *create_target_buffer(struct wlr_allocator *allocator,
		int width, int height) {
	uint64_t modifier = DRM_FORMAT_MOD_INVALID;
	struct wlr_drm_format format = {
		.format = DRM_FORMAT_ARGB8888,
		.len = 1,
		.capacity = 1,
		.modifiers = &modifier,
	};
	return wlr_allocator_create_buffer(allocator, width, height, &format);
}

bool wsm_scene_capture_tree(struct wsm_scene_capture *capture,
		struct wlr_scene_tree *tree, struct wlr_renderer *renderer,
		struct wlr_allocator *allocator, float scale) {
	*capture = (struct wsm_scene_capture){0};
	if (tree == NULL || renderer == NULL || allocator == NULL) {
		return false;
	}
	if (scale <= 0) {
		scale = 1;
	}

	pixman_region32_t measured;
	pixman_region32_init(&measured);
	measure_tree(tree, 0, 0, &measured);
	const pixman_box32_t *extents = pixman_region32_extents(&measured);
	struct wlr_box bounds = {
		.x = extents->x1,
		.y = extents->y1,
		.width = extents->x2 - extents->x1,
		.height = extents->y2 - extents->y1,
	};
	pixman_region32_fini(&measured);
	if (bounds.width <= 0 || bounds.height <= 0) {
		return false;
	}

	int buffer_width = (int)ceil(bounds.width * scale);
	int buffer_height = (int)ceil(bounds.height * scale);
	if (buffer_width < 1) {
		buffer_width = 1;
	}
	if (buffer_height < 1) {
		buffer_height = 1;
	}

	struct wlr_buffer *target = create_target_buffer(
		allocator, buffer_width, buffer_height);
	if (target == NULL) {
		wsm_log(WSM_ERROR, "Could not allocate scene capture buffer");
		return false;
	}

	struct wlr_render_pass *pass =
		wlr_renderer_begin_buffer_pass(renderer, target, NULL);
	if (pass == NULL) {
		wlr_buffer_drop(target);
		return false;
	}
	wlr_render_pass_add_rect(pass, &(struct wlr_render_rect_options) {
		.box = { .width = buffer_width, .height = buffer_height },
		.color = {0},
		.blend_mode = WLR_RENDER_BLEND_MODE_NONE,
	});

	struct capture_render_data data = {
		.pass = pass,
		.renderer = renderer,
		.bounds = bounds,
		.scale = scale,
	};
	render_tree(tree, 0, 0, &data);
	bool submitted = wlr_render_pass_submit(pass);
	for (size_t i = 0; i < data.textures_len; ++i) {
		wlr_texture_destroy(data.textures[i]);
	}
	free(data.textures);
	if (!submitted || data.failed) {
		if (!submitted) {
			wsm_log(WSM_DEBUG, "Scene capture failed: render pass submit failed");
		}
		wlr_buffer_drop(target);
		return false;
	}

	capture->buffer = target;
	capture->box = bounds;
	capture->scale = scale;
	return true;
}

void wsm_scene_capture_finish(struct wsm_scene_capture *capture) {
	if (capture == NULL) {
		return;
	}
	wlr_buffer_drop(capture->buffer);
	*capture = (struct wsm_scene_capture){0};
}
