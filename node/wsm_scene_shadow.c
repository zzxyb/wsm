#include "node/wsm_scene_shadow.h"
#include "scene/wsm_scene_output.h"
#include "pass/shadow.h"

#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <wlr/render/pass.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/render/wlr_texture.h>
#include <wlr/util/region.h>
#include <wlr/util/transform.h>

static const struct wsm_scene_node_impl scene_node_impl;

static void shadow_cache_damage_nodes(struct wsm_scene_shadow_cache *cache) {
	struct wsm_scene_shadow *shadow;
	wl_list_for_each(shadow, &cache->nodes, cache_link) {
		wsm_scene_node_update(&shadow->node, NULL);
	}
}

static void shadow_cache_handle_renderer_destroy(struct wl_listener *listener,
		void *data) {
	struct wsm_scene_shadow_cache *cache =
		wl_container_of(listener, cache, renderer_destroy);

	wl_list_remove(&cache->renderer_destroy.link);
	wl_list_init(&cache->renderer_destroy.link);
	wlr_texture_destroy(cache->texture);
	cache->texture = NULL;
	cache->width = cache->height = 0;
	shadow_cache_damage_nodes(cache);
}

static bool slices_valid(const struct wsm_scene_shadow_slices *slices,
		int width, int height) {
	return slices->left >= 0 && slices->right >= 0 &&
		slices->top >= 0 && slices->bottom >= 0 &&
		(int64_t)slices->left + slices->right <= width &&
		(int64_t)slices->top + slices->bottom <= height;
}

struct wsm_scene_shadow_cache *wsm_scene_shadow_cache_create_from_texture(
		struct wlr_texture *texture,
		const struct wsm_scene_shadow_slices *slices) {
	if (texture == NULL || slices == NULL ||
			!slices_valid(slices, texture->width, texture->height)) {
		return NULL;
	}

	struct wsm_scene_shadow_cache *cache = calloc(1, sizeof(*cache));
	if (cache == NULL) {
		return NULL;
	}

	cache->texture = texture;
	cache->width = texture->width;
	cache->height = texture->height;
	cache->slices = *slices;
	cache->ref_count = 1;
	wl_list_init(&cache->nodes);
	wl_list_init(&cache->renderer_destroy.link);

	cache->renderer_destroy.notify = shadow_cache_handle_renderer_destroy;
	wl_signal_add(&texture->renderer->events.destroy, &cache->renderer_destroy);

	return cache;
}

struct wsm_scene_shadow_cache *wsm_scene_shadow_cache_create(
		struct wlr_renderer *renderer, uint32_t format, uint32_t stride,
		uint32_t width, uint32_t height, const void *data,
		const struct wsm_scene_shadow_slices *slices) {
	if (renderer == NULL || data == NULL || slices == NULL ||
			width == 0 || height == 0 || stride == 0 ||
			width > INT32_MAX || height > INT32_MAX ||
			!slices_valid(slices, width, height) ||
			(size_t)stride > SIZE_MAX / height) {
		return NULL;
	}
	size_t size = (size_t)stride * height;
	void *pixels = malloc(size);
	if (pixels == NULL) {
		return NULL;
	}
	memcpy(pixels, data, size);
	struct wlr_texture *texture = wlr_texture_from_pixels(renderer, format,
		stride, width, height, pixels);
	if (texture == NULL) {
		free(pixels);
		return NULL;
	}

	struct wsm_scene_shadow_cache *cache =
		wsm_scene_shadow_cache_create_from_texture(texture, slices);
	if (cache == NULL) {
		wlr_texture_destroy(texture);
		free(pixels);
	} else {
		cache->owned_pixels = pixels;
	}

	return cache;
}

struct wsm_scene_shadow_cache *wsm_scene_shadow_cache_ref(
		struct wsm_scene_shadow_cache *cache) {
	assert(cache != NULL);
	cache->ref_count++;
	return cache;
}

void wsm_scene_shadow_cache_unref(struct wsm_scene_shadow_cache *cache) {
	if (cache == NULL) {
		return;
	}

	assert(cache->ref_count > 0);
	cache->ref_count--;
	if (cache->ref_count > 0) {
		return;
	}

	assert(wl_list_empty(&cache->nodes));
	wl_list_remove(&cache->renderer_destroy.link);
	wlr_texture_destroy(cache->texture);
	free(cache->owned_pixels);
	free(cache);
}

static void shadow_set_cache(struct wsm_scene_shadow *shadow,
		struct wsm_scene_shadow_cache *cache) {
	if (shadow->cache == cache) {
		return;
	}

	wl_list_remove(&shadow->cache_link);
	wl_list_init(&shadow->cache_link);
	wsm_scene_shadow_cache_unref(shadow->cache);

	shadow->cache = cache != NULL ? wsm_scene_shadow_cache_ref(cache) : NULL;
	if (shadow->cache != NULL) {
		wl_list_insert(&shadow->cache->nodes, &shadow->cache_link);
	}
}

static void scene_node_destroy(struct wsm_scene_node *node) {
	struct wsm_scene_shadow *shadow = wsm_scene_shadow_from_node(node);
	shadow_set_cache(shadow, NULL);
	free(shadow);
}

static void scene_node_get_size(struct wsm_scene_node *node,
		int *width, int *height) {
	struct wsm_scene_shadow *shadow = wsm_scene_shadow_from_node(node);
	*width = shadow->margins.left + shadow->width + shadow->margins.right;
	*height = shadow->margins.top + shadow->height + shadow->margins.bottom;
}

static bool scene_node_invisible(struct wsm_scene_node *node) {
	struct wsm_scene_shadow *shadow = wsm_scene_shadow_from_node(node);
	return shadow->cache == NULL || shadow->cache->texture == NULL ||
		shadow->opacity == 0 || shadow->width <= 0 || shadow->height <= 0;
}

static void scene_node_visibility(struct wsm_scene_node *node,
		pixman_region32_t *visible) {
	if (!node->enabled) {
		return;
	}

	pixman_region32_union(visible, visible, &node->visible);
}

static void scene_node_opaque_region(struct wsm_scene_node *node, int x, int y,
		pixman_region32_t *opaque) {
}

static bool scene_nodes_in_box(struct wsm_scene_node *node,
		struct wlr_box *box, scene_node_box_iterator_func_t iterator,
		void *user_data) {
	if (!node->enabled) {
		return false;
	}

	int x, y;
	wsm_scene_node_coords(node, &x, &y);

	struct wlr_box node_box = { .x = x, .y = y };
	scene_node_get_size(node, &node_box.width, &node_box.height);

	if (wlr_box_intersection(&node_box, &node_box, box) &&
			iterator(node, x, y, user_data)) {
		return true;
	}

	return false;
}

static bool construct_render_list_iterator(struct wsm_scene_node *node,
		int lx, int ly, void *_data) {
	struct wsm_render_list_constructor_data *data = _data;

	if (scene_node_invisible(node)) {
		return false;
	}

	pixman_region32_t intersection;
	pixman_region32_init(&intersection);
	pixman_region32_intersect_rect(&intersection, &node->visible,
		data->box.x, data->box.y, data->box.width, data->box.height);
	if (pixman_region32_empty(&intersection)) {
		pixman_region32_fini(&intersection);
		return false;
	}
	pixman_region32_fini(&intersection);

	struct wsm_render_list_entry *entry =
		wl_array_add(data->render_list, sizeof(*entry));
	if (entry == NULL) {
		return false;
	}

	*entry = (struct wsm_render_list_entry){
		.node = node,
		.x = lx,
		.y = ly,
		.highlight_transparent_region = data->highlight_transparent_region,
	};

	return false;
}

static void scale_region(pixman_region32_t *region, float scale,
		bool round_up) {
	wlr_region_scale(region, region, scale);

	if (round_up && floor(scale) != scale) {
		wlr_region_expand(region, region, 1);
	}
}

static void logical_to_buffer_coords(pixman_region32_t *region,
		const struct wsm_render_data *data, bool round_up) {
	enum wl_output_transform transform = wlr_output_transform_invert(data->transform);
	scale_region(region, data->scale, round_up);
	wlr_region_transform(region, region, transform,
		data->trans_width, data->trans_height);
}

static bool shadow_has_corner_radius(struct wsm_scene_shadow *shadow) {
	return shadow->top_left_radius > 0.f || shadow->top_right_radius > 0.f ||
		shadow->bottom_left_radius > 0.f || shadow->bottom_right_radius > 0.f;
}

static float clamp_radius(float radius, int width, int height) {
	if (radius < 0.f) {
		return 0.f;
	}

	float max_radius = fminf(width, height) / 2.f;
	return radius > max_radius ? max_radius : radius;
}

#define SHADOW_CORNER_SMOOTHNESS 4.0f

static float rounded_corner_cut(float radius, float y) {
	if (radius <= 0.f || y >= radius) {
		return 0.f;
	}

	float dy = radius - y - 0.5f;
	if (dy <= 0.f) {
		return 0.f;
	}

	float radius_pow = powf(radius, SHADOW_CORNER_SMOOTHNESS);
	float dy_pow = powf(dy, SHADOW_CORNER_SMOOTHNESS);
	if (dy_pow >= radius_pow) {
		return radius;
	}

	return radius - powf(radius_pow - dy_pow,
		1.f / SHADOW_CORNER_SMOOTHNESS);
}

static void rounded_rect_region(pixman_region32_t *region,
		struct wsm_scene_shadow *shadow, int x, int y) {
	int inset = (int)ceilf(shadow->inner_offset);
	int width = shadow->width - inset * 2;
	int height = shadow->height - inset * 2;

	pixman_region32_init(region);
	if (width <= 0 || height <= 0) {
		return;
	}

	float tl = clamp_radius(shadow->top_left_radius - inset, width, height);
	float tr = clamp_radius(shadow->top_right_radius - inset, width, height);
	float bl = clamp_radius(shadow->bottom_left_radius - inset, width, height);
	float br = clamp_radius(shadow->bottom_right_radius - inset, width, height);
	x += inset;
	y += inset;

	for (int row = 0; row < height; row++) {
		float left_cut = 0.f;
		float right_cut = 0.f;

		left_cut = fmaxf(left_cut, rounded_corner_cut(tl, row));
		right_cut = fmaxf(right_cut, rounded_corner_cut(tr, row));
		left_cut = fmaxf(left_cut,
			rounded_corner_cut(bl, height - row - 1));
		right_cut = fmaxf(right_cut,
			rounded_corner_cut(br, height - row - 1));

		int left = (int)ceilf(left_cut);
		int right = (int)ceilf(right_cut);
		int row_width = width - left - right;
		if (row_width <= 0) {
			continue;
		}

		pixman_region32_union_rect(region, region,
			x + left, y + row, row_width, 1);
	}
}

static void scene_node_render(struct wsm_render_list_entry *entry,
		const struct wsm_render_data *data) {
	struct wsm_scene_shadow *shadow = wsm_scene_shadow_from_node(entry->node);
	if (scene_node_invisible(&shadow->node)) {
		return;
	}

	pixman_region32_t render_region;
	pixman_region32_init(&render_region);
	pixman_region32_copy(&render_region, &shadow->node.visible);
	pixman_region32_translate(&render_region,
		-data->logical.x, -data->logical.y);

	int x = entry->x - data->logical.x;
	int y = entry->y - data->logical.y;
	if (shadow_has_corner_radius(shadow)) {
		pixman_region32_t hole;
		rounded_rect_region(&hole, shadow,
			x + shadow->margins.left, y + shadow->margins.top);
		wlr_region_expand(&hole, &hole, 1);
		pixman_region32_subtract(&render_region, &render_region, &hole);
		pixman_region32_fini(&hole);
	}

	logical_to_buffer_coords(&render_region, data, true);
	pixman_region32_intersect(&render_region, &render_region, &data->damage);
	if (pixman_region32_empty(&render_region)) {
		pixman_region32_fini(&render_region);
		return;
	}

	wsm_render_pass_add_shadow(shadow, data, &render_region, x, y);

	pixman_region32_fini(&render_region);
}

static void get_scene_node_extents(struct wsm_scene_node *node, int lx, int ly,
		int *x_min, int *y_min, int *x_max, int *y_max) {
	struct wlr_box node_box = { .x = lx, .y = ly };
	scene_node_get_size(node, &node_box.width, &node_box.height);

	if (node_box.x < *x_min) {
		*x_min = node_box.x;
	}
	if (node_box.y < *y_min) {
		*y_min = node_box.y;
	}
	if (node_box.x + node_box.width > *x_max) {
		*x_max = node_box.x + node_box.width;
	}
	if (node_box.y + node_box.height > *y_max) {
		*y_max = node_box.y + node_box.height;
	}
}

static void scene_node_cleanup_when_disabled(struct wsm_scene_node *node,
		bool xwayland_restack, struct wl_list *outputs) {
	pixman_region32_clear(&node->visible);
	wsm_scene_node_update_outputs(node, outputs, NULL, NULL);
}

static const struct wsm_scene_node_impl scene_node_impl = {
	.destroy = scene_node_destroy,
	.get_size = scene_node_get_size,
	.in_box = scene_nodes_in_box,
	.opaque_region = scene_node_opaque_region,
	.visibility = scene_node_visibility,
	.invisible = scene_node_invisible,
	.construct_render_list_iterator = construct_render_list_iterator,
	.render = scene_node_render,
	.get_extents = get_scene_node_extents,
	.cleanup_when_disabled = scene_node_cleanup_when_disabled,
};

struct wsm_scene_shadow *wsm_scene_shadow_create(struct wsm_scene_tree *parent,
		struct wsm_scene_shadow_cache *cache, int width, int height,
		const struct wsm_scene_shadow_margins *margins,
		float opacity) {
	assert(parent != NULL);
	assert(width >= 0 && height >= 0);
	assert(margins != NULL);
	assert(opacity >= 0.f && opacity <= 1.f);

	struct wsm_scene_shadow *shadow = calloc(1, sizeof(*shadow));
	if (shadow == NULL) {
		return NULL;
	}

	wl_list_init(&shadow->cache_link);
	wsm_scene_node_init(&shadow->node, &scene_node_impl, parent);
	shadow->width = width;
	shadow->height = height;
	shadow->margins = *margins;
	shadow->opacity = opacity;
	shadow_set_cache(shadow, cache);

	wsm_scene_node_update(&shadow->node, NULL);
	return shadow;
}

void wsm_scene_shadow_set_size(struct wsm_scene_shadow *shadow,
		int width, int height) {
	assert(width >= 0 && height >= 0);
	if (shadow->width == width && shadow->height == height) {
		return;
	}

	shadow->width = width;
	shadow->height = height;
	shadow->top_left_radius = clamp_radius(shadow->top_left_radius, width, height);
	shadow->top_right_radius = clamp_radius(shadow->top_right_radius, width, height);
	shadow->bottom_left_radius = clamp_radius(shadow->bottom_left_radius, width, height);
	shadow->bottom_right_radius = clamp_radius(shadow->bottom_right_radius, width, height);
	wsm_scene_node_update(&shadow->node, NULL);
}

void wsm_scene_shadow_set_margins(struct wsm_scene_shadow *shadow,
		const struct wsm_scene_shadow_margins *margins) {
	if (memcmp(&shadow->margins, margins, sizeof(*margins)) == 0) {
		return;
	}

	shadow->margins = *margins;
	wsm_scene_node_update(&shadow->node, NULL);
}

void wsm_scene_shadow_set_corner_radius(struct wsm_scene_shadow *shadow,
		float top_left, float top_right,
		float bottom_left, float bottom_right) {
	top_left = clamp_radius(top_left, shadow->width, shadow->height);
	top_right = clamp_radius(top_right, shadow->width, shadow->height);
	bottom_left = clamp_radius(bottom_left, shadow->width, shadow->height);
	bottom_right = clamp_radius(bottom_right, shadow->width, shadow->height);

	if (shadow->top_left_radius == top_left &&
			shadow->top_right_radius == top_right &&
			shadow->bottom_left_radius == bottom_left &&
			shadow->bottom_right_radius == bottom_right) {
		return;
	}

	shadow->top_left_radius = top_left;
	shadow->top_right_radius = top_right;
	shadow->bottom_left_radius = bottom_left;
	shadow->bottom_right_radius = bottom_right;
	wsm_scene_node_update(&shadow->node, NULL);
}

void wsm_scene_shadow_set_inner_offset(struct wsm_scene_shadow *shadow,
		float inner_offset) {
	assert(inner_offset >= 0.f);
	if (shadow->inner_offset == inner_offset) {
		return;
	}

	shadow->inner_offset = inner_offset;
	wsm_scene_node_update(&shadow->node, NULL);
}

void wsm_scene_shadow_set_opacity(struct wsm_scene_shadow *shadow,
		float opacity) {
	assert(opacity >= 0.f && opacity <= 1.f);
	if (shadow->opacity == opacity) {
		return;
	}

	shadow->opacity = opacity;
	wsm_scene_node_update(&shadow->node, NULL);
}

void wsm_scene_shadow_set_cache(struct wsm_scene_shadow *shadow,
		struct wsm_scene_shadow_cache *cache) {
	if (shadow->cache == cache) {
		return;
	}

	wsm_scene_node_update(&shadow->node, NULL);
	shadow_set_cache(shadow, cache);
	wsm_scene_node_update(&shadow->node, NULL);
}

bool wsm_scene_node_is_shadow(const struct wsm_scene_node *node) {
	return node->impl == &scene_node_impl;
}

struct wsm_scene_shadow *wsm_scene_shadow_from_node(
		struct wsm_scene_node *node) {
	assert(node->impl == &scene_node_impl);

	struct wsm_scene_shadow *shadow = wl_container_of(node, shadow, node);
	return shadow;
}
