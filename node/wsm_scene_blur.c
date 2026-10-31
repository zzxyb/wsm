#include "node/wsm_scene_blur.h"
#include "scene/wsm_scene.h"
#include "scene/wsm_scene_output.h"
#include "node/wsm_scene_tree.h"
#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <wlr/util/region.h>
#include <wlr/util/transform.h>
#include <wlr/types/wlr_output.h>
#define DEFAULT_CORNER_SMOOTHNESS 4.f
/* A node may span outputs with different scale, transform, renderer and buffer
 * age. Keep an independent unfiltered background for each output. */
struct blur_capture {
	struct wl_list link;
	struct wsm_scene_output *output;
	struct wl_listener output_destroy;
	struct wlr_texture *texture;
	struct wlr_texture *filtered;
	struct wlr_box filtered_box;
	bool filtered_valid;
	struct wlr_box box, sample_box;
	int width, height;
	float scale;
	enum wl_output_transform transform;
	pixman_region32_t pending_capture_damage;
	bool capture_needs_full_update;
};

static void destroy_capture(struct blur_capture *capture) {
	wl_list_remove(&capture->link);
	wl_list_remove(&capture->output_destroy.link);
	wlr_texture_destroy(capture->texture);
	wlr_texture_destroy(capture->filtered);
	pixman_region32_fini(&capture->pending_capture_damage);
	free(capture);
}

static void output_destroyed(struct wl_listener *listener, void *data) {
	struct blur_capture *capture = wl_container_of(listener, capture, output_destroy);
	destroy_capture(capture);
}

static void clear_captures(struct wsm_scene_blur *blur) {
	struct blur_capture *capture, *tmp;
	wl_list_for_each_safe(capture, tmp, &blur->captures, link) destroy_capture(capture);
}

static void scene_node_get_size(struct wsm_scene_node *node, int *width, int *height) {
	struct wsm_scene_blur *blur = wsm_scene_blur_from_node(node);
	*width = blur->width;
	*height = blur->height;
}

static void pass_destroyed(struct wl_listener *listener, void *data) {
	struct wsm_scene_blur *blur = wl_container_of(listener, blur, pass_destroy);
	wl_list_remove(&blur->pass_destroy.link);
	wl_list_init(&blur->pass_destroy.link);
	clear_captures(blur);
	blur->pass = NULL;
	wsm_scene_node_update(&blur->node, NULL);
}

static void scene_node_destroy(struct wsm_scene_node *node) {
	struct wsm_scene_blur *blur = wsm_scene_blur_from_node(node);
	wl_list_remove(&blur->pass_destroy.link);
	clear_captures(blur);
	free(blur);
}

static struct wsm_scene_node *scene_node_at(
	struct wsm_scene_node *node, double lx, double ly, double *nx, double *ny) {
	return NULL;
}

static bool scene_nodes_in_box(struct wsm_scene_node *node, struct wlr_box *box,
	scene_node_box_iterator_func_t iterator, void *user_data) {
	if (!node->enabled) {
		return false;
	}

	int x, y;
	wsm_scene_node_coords(node, &x, &y);
	struct wlr_box node_box = {.x = x, .y = y};
	scene_node_get_size(node, &node_box.width, &node_box.height);
	return wlr_box_intersection(&node_box, &node_box, box) && iterator(node, x, y, user_data);
}

static void scene_node_bounds(
	struct wsm_scene_node *node, int x, int y, pixman_region32_t *visible) {
	int width, height;
	scene_node_get_size(node, &width, &height);
	pixman_region32_union_rect(visible, visible, x, y, width, height);
}

static void scene_node_opaque_region(
	struct wsm_scene_node *node, int x, int y, pixman_region32_t *opaque) {}

static void scene_node_visibility(struct wsm_scene_node *node, pixman_region32_t *visible) {
	if (node->enabled) {
		pixman_region32_union(visible, visible, &node->visible);
	}
}

static bool scene_node_invisible(struct wsm_scene_node *node) {
	struct wsm_scene_blur *blur = wsm_scene_blur_from_node(node);
	return blur->width <= 0 || blur->height <= 0 || blur->opacity == 0.f;
}

static bool construct_render_list_iterator(
	struct wsm_scene_node *node, int lx, int ly, void *_data) {
	struct wsm_render_list_constructor_data *data = _data;
	if (wsm_scene_node_invisible(node)) {
		return false;
	}

	pixman_region32_t intersection;
	pixman_region32_init(&intersection);
	pixman_region32_intersect_rect(
		&intersection, &node->visible, data->box.x, data->box.y, data->box.width, data->box.height);
	bool visible = !pixman_region32_empty(&intersection);
	pixman_region32_fini(&intersection);
	if (!visible) {
		return false;
	}

	struct wsm_render_list_entry *entry = wl_array_add(data->render_list, sizeof(*entry));
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

static void scale_region(pixman_region32_t *region, float scale, bool round_up) {
	wlr_region_scale(region, region, scale);
	if (round_up && floor(scale) != scale) {
		wlr_region_expand(region, region, 1);
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

static void logical_to_buffer_coords(
	pixman_region32_t *region, const struct wsm_render_data *data, bool round_up) {
	enum wl_output_transform transform = wlr_output_transform_invert(data->transform);
	scale_region(region, data->scale, round_up);
	wlr_region_transform(region, region, transform, data->trans_width, data->trans_height);
}

static void transform_output_box(struct wlr_box *box, const struct wsm_render_data *data) {
	enum wl_output_transform transform = wlr_output_transform_invert(data->transform);
	scale_box(box, data->scale);
	wlr_box_transform(box, box, transform, data->trans_width, data->trans_height);
}

static int capture_expansion(const struct wsm_scene_blur *blur, float scale) {
	/* The first bilinear downsample can fetch beyond the cropped source box.
	 * Preserve that sampler gutter as well as the reference kernel support. */
	return wsm_scene_blur_get_damage_expansion(blur, scale) +
		   (int)ceilf(blur->offset * scale * .5f) + 2;
}

static struct blur_capture *get_capture(
	struct wsm_render_list_entry *entry, const struct wsm_render_data *data) {
	struct wsm_scene_blur *blur = wsm_scene_blur_from_node(entry->node);
	struct blur_capture *capture = NULL, *candidate;
	wl_list_for_each(candidate, &blur->captures, link) {
		if (candidate->output == data->output) {
			capture = candidate;
			break;
		}
	}
	if (!capture) {
		capture = calloc(1, sizeof(*capture));
		if (!capture) {
			return NULL;
		}
		capture->output = data->output;
		wl_list_init(&capture->output_destroy.link);
		if (data->output) {
			capture->output_destroy.notify = output_destroyed;
			wl_signal_add(&data->output->events.destroy, &capture->output_destroy);
		}
		pixman_region32_init(&capture->pending_capture_damage);
		capture->capture_needs_full_update = true;
		wl_list_insert(&blur->captures, &capture->link);
	}
	struct wlr_box box = {
		entry->x - data->logical.x, entry->y - data->logical.y, blur->width, blur->height};
	transform_output_box(&box, data);
	int width = data->trans_width, height = data->trans_height;
	wlr_output_transform_coords(data->transform, &width, &height);
	int padding = capture_expansion(blur, data->scale);
	struct wlr_box sample_box = {
		box.x - padding, box.y - padding, box.width + 2 * padding, box.height + 2 * padding};
	wlr_box_intersection(&sample_box, &sample_box, &(struct wlr_box){0, 0, width, height});
	if (capture->width != width || capture->height != height ||
		(capture->texture && (capture->texture->width != (uint32_t)sample_box.width ||
								 capture->texture->height != (uint32_t)sample_box.height))) {
		wlr_texture_destroy(capture->texture);
		capture->texture = NULL;
		capture->capture_needs_full_update = true;
	}
	if (!wlr_box_equal(&capture->box, &box) || !wlr_box_equal(&capture->sample_box, &sample_box) ||
		capture->scale != data->scale || capture->transform != data->transform) {
		capture->capture_needs_full_update = true;
	}
	struct wlr_box filtered_box;
	wlr_box_intersection(&filtered_box, &box, &(struct wlr_box){0, 0, width, height});
	if (capture->capture_needs_full_update ||
		!wlr_box_equal(&capture->filtered_box, &filtered_box)) {
		capture->filtered_valid = false;
	}
	if (capture->filtered && (capture->filtered->width != (uint32_t)filtered_box.width ||
								 capture->filtered->height != (uint32_t)filtered_box.height)) {
		wlr_texture_destroy(capture->filtered);
		capture->filtered = NULL;
	}
	capture->filtered_box = filtered_box;
	capture->box = box;
	capture->sample_box = sample_box;
	capture->width = width;
	capture->height = height;
	capture->scale = data->scale;
	capture->transform = data->transform;
	return capture;
}

bool wsm_scene_blur_prepare(struct wsm_render_list_entry *entry, const struct wsm_render_data *data,
	pixman_region32_t *damage) {
	struct wsm_scene_blur *blur = wsm_scene_blur_from_node(entry->node);
	if (!blur->pass) {
		return true;
	}
	struct blur_capture *capture = get_capture(entry, data);
	if (!capture) {
		return false;
	}
	pixman_region32_t affected;
	pixman_region32_init(&affected);
	pixman_region32_intersect_rect(&affected, damage, capture->sample_box.x, capture->sample_box.y,
		capture->sample_box.width, capture->sample_box.height);
	pixman_region32_union(
		&capture->pending_capture_damage, &capture->pending_capture_damage, &affected);
	if (capture->capture_needs_full_update) {
		pixman_region32_union_rect(&capture->pending_capture_damage,
			&capture->pending_capture_damage, capture->sample_box.x, capture->sample_box.y,
			capture->sample_box.width, capture->sample_box.height);
	}
	/* Captures are refreshed from freshly repainted preceding content, never
	 * from pixels which still contain this node's blur from an older frame. */
	pixman_region32_union(damage, damage, &capture->pending_capture_damage);
	wlr_region_expand(
		&affected, &capture->pending_capture_damage, capture_expansion(blur, data->scale));
	pixman_region32_intersect_rect(&affected, &affected, capture->box.x, capture->box.y,
		capture->box.width, capture->box.height);
	pixman_region32_union(damage, damage, &affected);
	pixman_region32_intersect_rect(damage, damage, 0, 0, capture->width, capture->height);
	pixman_region32_fini(&affected);
	return true;
}

bool wsm_scene_blur_prepare_output(struct wsm_scene_blur *blur, struct wsm_scene_output *output) {
	if (!blur || !blur->pass || wsm_scene_node_invisible(&blur->node)) {
		return true;
	}
	int x, y;
	if (!wsm_scene_node_coords(&blur->node, &x, &y)) {
		return true;
	}
	struct wsm_render_data data = {
		.output = output,
		.transform = output->output->transform,
		.scale = output->output->scale,
		.logical = {.x = output->x, .y = output->y},
	};
	wlr_output_transformed_resolution(output->output, &data.trans_width, &data.trans_height);
	wlr_output_effective_resolution(output->output, &data.logical.width, &data.logical.height);
	struct wlr_box intersection;
	if (!wlr_box_intersection(
			&intersection, &data.logical, &(struct wlr_box){x, y, blur->width, blur->height})) {
		return true;
	}
	struct wsm_render_list_entry entry = {.node = &blur->node, .x = x, .y = y};
	pixman_region32_t damage;
	pixman_region32_init(&damage);
	pixman_region32_copy(&damage, &output->pending_commit_damage);
	bool ok = wsm_scene_blur_prepare(&entry, &data, &damage);
	if (ok) {
		wlr_damage_ring_add(&output->damage_ring, &damage);
		pixman_region32_union(
			&output->pending_commit_damage, &output->pending_commit_damage, &damage);
	}
	pixman_region32_fini(&damage);
	return ok;
}

static void scene_node_render(
	struct wsm_render_list_entry *entry, const struct wsm_render_data *data) {
	struct wsm_scene_blur *blur = wsm_scene_blur_from_node(entry->node);
	if (!blur->pass) {
		return;
	}
	pixman_region32_t region;
	pixman_region32_init(&region);
	pixman_region32_copy(&region, &blur->node.visible);
	pixman_region32_translate(&region, -data->logical.x, -data->logical.y);
	logical_to_buffer_coords(&region, data, true);
	pixman_region32_intersect(&region, &region, &data->damage);
	if (pixman_region32_empty(&region)) {
		pixman_region32_fini(&region);
		return;
	}
	struct blur_capture *capture = get_capture(entry, data);
	if (!capture) {
		pixman_region32_fini(&region);
		return;
	}
	struct wlr_box box = capture->box;
	/* Buffer-age repair can intersect an unchanged blur. Recompose its cached
	 * result without repeating capture or down/up filtering. */
	if (capture->filtered_valid && !capture->capture_needs_full_update &&
		pixman_region32_empty(&capture->pending_capture_damage)) {
		wlr_render_pass_add_texture(data->render_pass, &(struct wlr_render_texture_options){
														   .texture = capture->filtered,
														   .dst_box = capture->filtered_box,
														   .clip = &region,
														   .filter_mode = WLR_SCALE_FILTER_NEAREST,
														   .blend_mode = WLR_RENDER_BLEND_MODE_NONE,
													   });
		pixman_region32_fini(&region);
		return;
	}
	bool filtered_was_valid = capture->filtered_valid;
	capture->filtered_valid = false;

	pixman_region32_t capture_region;
	pixman_region32_init(&capture_region);
	if (capture->capture_needs_full_update) {
		pixman_region32_union_rect(&capture->pending_capture_damage,
			&capture->pending_capture_damage, capture->sample_box.x, capture->sample_box.y,
			capture->sample_box.width, capture->sample_box.height);
	}
	pixman_region32_intersect(&capture_region, &capture->pending_capture_damage, &data->damage);
	if (wsm_render_blur_buffer_pass_update_capture(data->render_pass, blur->pass, &capture->texture,
			&capture->sample_box, &capture_region)) {
		pixman_region32_subtract(
			&capture->pending_capture_damage, &capture->pending_capture_damage, &capture_region);
		capture->capture_needs_full_update =
			!pixman_region32_empty(&capture->pending_capture_damage);
		int padding = wsm_scene_blur_get_damage_expansion(blur, data->scale);
		float radii[4] = {blur->top_left_radius, blur->top_right_radius, blur->bottom_left_radius,
			blur->bottom_right_radius};
		float transformed[4];
		/* Map logical corners into the transformed output buffer. */
		for (int i = 0; i < 4; i++) {
			struct wlr_box corner = {.x = i & 1, .y = (i >> 1) & 1};
			wlr_box_transform(&corner, &corner, wlr_output_transform_invert(data->transform), 1, 1);
			transformed[(corner.x != 0) + 2 * (corner.y != 0)] = radii[i] * data->scale;
		}
		wsm_render_blur_buffer_pass_render(data->render_pass, blur->pass,
			&(struct wsm_render_blur_buffer_options){
				.texture = capture->texture,
				.padding = padding,
				.src_box = {box.x - padding - capture->sample_box.x,
					box.y - padding - capture->sample_box.y, box.width + 2 * padding,
					box.height + 2 * padding},
				.dst_box = box,
				.clip = &region,
				.alpha = &blur->opacity,
				.blend_mode = WLR_RENDER_BLEND_MODE_PREMULTIPLIED,
				.top_left_radius = transformed[0],
				.top_right_radius = transformed[1],
				.bottom_left_radius = transformed[2],
				.bottom_right_radius = transformed[3],
				.corner_smoothness = 4,
				.iterations = blur->iterations,
				.offset = blur->offset * data->scale,
				.saturation = blur->saturation,
				.brightness = blur->brightness,
				.noise_amount = blur->noise_amount,
				.material_contrast = blur->material_contrast,
				.tint = {blur->tint[0], blur->tint[1], blur->tint[2], blur->tint[3]},
			});
		/* Cache the composed panel, including its transparent rounded edge's
		 * underlying pixels. Final composition uses SRC rather than blending
		 * this already composed image a second time. */
		if (wsm_render_blur_buffer_pass_update_capture(data->render_pass, blur->pass,
				&capture->filtered, &capture->filtered_box, &region)) {
			/* Existing valid pixels outside a partial update are preserved. */
			pixman_box32_t box = {capture->filtered_box.x, capture->filtered_box.y,
				capture->filtered_box.x + capture->filtered_box.width,
				capture->filtered_box.y + capture->filtered_box.height};
			capture->filtered_valid =
				!capture->capture_needs_full_update &&
				(filtered_was_valid ||
					pixman_region32_contains_rectangle(&region, &box) == PIXMAN_REGION_IN);
		}
	}
	pixman_region32_fini(&capture_region);
	pixman_region32_fini(&region);
}

static void get_scene_node_extents(
	struct wsm_scene_node *node, int lx, int ly, int *x_min, int *y_min, int *x_max, int *y_max) {
	struct wlr_box node_box = {.x = lx, .y = ly};
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

static void scene_node_cleanup_when_disabled(
	struct wsm_scene_node *node, bool xwayland_restack, struct wl_list *outputs) {
	struct wsm_scene_blur *blur = wsm_scene_blur_from_node(node);
	struct blur_capture *capture;
	wl_list_for_each(capture, &blur->captures, link) capture->capture_needs_full_update = true;
	pixman_region32_clear(&node->visible);
	wsm_scene_node_update_outputs(node, outputs, NULL, NULL);
}

static const struct wsm_scene_node_impl scene_node_impl = {
	.destroy = scene_node_destroy,
	.bounds = scene_node_bounds,
	.get_size = scene_node_get_size,
	.at = scene_node_at,
	.in_box = scene_nodes_in_box,
	.opaque_region = scene_node_opaque_region,
	.visibility = scene_node_visibility,
	.invisible = scene_node_invisible,
	.construct_render_list_iterator = construct_render_list_iterator,
	.render = scene_node_render,
	.get_extents = get_scene_node_extents,
	.cleanup_when_disabled = scene_node_cleanup_when_disabled,
};

struct wsm_scene_blur *wsm_scene_blur_create(struct wsm_scene_tree *parent, int width, int height,
	float top_left_radius, float top_right_radius, float bottom_left_radius,
	float bottom_right_radius) {
	assert(parent);
	assert(width >= 0 && height >= 0);
	struct wsm_scene_blur *blur = calloc(1, sizeof(*blur));
	if (blur == NULL) {
		return NULL;
	}

	wsm_scene_node_init(&blur->node, &scene_node_impl, parent);
	wl_list_init(&blur->pass_destroy.link);
	wl_list_init(&blur->captures);
	blur->width = width;
	blur->height = height;
	blur->top_left_radius = top_left_radius;
	blur->top_right_radius = top_right_radius;
	blur->bottom_left_radius = bottom_left_radius;
	blur->bottom_right_radius = bottom_right_radius;
	blur->iterations = 4;
	blur->offset = 1.8f;
	blur->saturation = 1.35f;
	blur->brightness = 1.06f;
	blur->noise_amount = 0.02f;
	blur->material_contrast = 0.04f;
	blur->opacity = 1.f;
	blur->tint[0] = 1.f;
	blur->tint[1] = 1.f;
	blur->tint[2] = 1.f;
	blur->tint[3] = 0.14f;

	wsm_scene_node_update(&blur->node.scene->tree->node, NULL);
	return blur;
}

void wsm_scene_blur_set_pass(
	struct wsm_scene_blur *blur, struct wsm_render_blur_buffer_pass *pass) {
	wl_list_remove(&blur->pass_destroy.link);
	wl_list_init(&blur->pass_destroy.link);
	clear_captures(blur);
	blur->pass = pass;
	if (pass) {
		blur->pass_destroy.notify = pass_destroyed;
		wl_signal_add(&pass->events.destroy, &blur->pass_destroy);
	}
	wsm_scene_node_update(&blur->node, NULL);
}

void wsm_scene_blur_set_blur(struct wsm_scene_blur *blur, int iterations, float offset) {
	assert(isfinite(offset) && offset >= 0);
	blur->iterations =
		iterations > 0
			? (iterations > WSM_BLUR_BUFFER_MAX_PASSES ? WSM_BLUR_BUFFER_MAX_PASSES : iterations)
			: 4;
	blur->offset = offset > 0 ? offset : 1.8f;
	wsm_scene_node_update(&blur->node, NULL);
}

void wsm_scene_blur_set_appearance(
	struct wsm_scene_blur *blur, float saturation, float brightness, const float tint[4]) {
	blur->saturation = saturation;
	blur->brightness = brightness;
	for (size_t i = 0; i < 4; i++) {
		blur->tint[i] = tint[i];
	}
	wsm_scene_node_update(&blur->node, NULL);
}

void wsm_scene_blur_set_noise(struct wsm_scene_blur *blur, float noise_amount) {
	blur->noise_amount = noise_amount < 0.f ? 0.f : noise_amount;
	wsm_scene_node_update(&blur->node, NULL);
}

void wsm_scene_blur_set_material_contrast(struct wsm_scene_blur *blur, float contrast) {
	blur->material_contrast = contrast < 0.f ? 0.f : contrast;
	wsm_scene_node_update(&blur->node, NULL);
}

void wsm_scene_blur_add_capture_damage(
	struct wsm_scene_blur *blur, const pixman_region32_t *damage) {
	struct blur_capture *capture;
	wl_list_for_each(capture, &blur->captures, link) {
		pixman_region32_t clipped;
		pixman_region32_init(&clipped);
		pixman_region32_intersect_rect(&clipped, damage, capture->sample_box.x,
			capture->sample_box.y, capture->sample_box.width, capture->sample_box.height);
		pixman_region32_union(
			&capture->pending_capture_damage, &capture->pending_capture_damage, &clipped);
		pixman_region32_fini(&clipped);
	}
}

int wsm_scene_blur_get_damage_expansion(const struct wsm_scene_blur *blur, float scale) {
	if (blur->iterations <= 0 || blur->offset <= 0.f || scale <= 0.f) {
		return 0;
	}

	int iterations = blur->iterations > WSM_BLUR_BUFFER_MAX_PASSES ? WSM_BLUR_BUFFER_MAX_PASSES
																   : blur->iterations;
	int multiplier = (1 << (iterations + 2)) - 1;
	return fminf(4096, ceilf(blur->offset * scale * multiplier) + 8);
}

bool wsm_scene_node_is_blur(const struct wsm_scene_node *node) {
	return node != NULL && node->impl == &scene_node_impl;
}

struct wsm_scene_blur *wsm_scene_blur_from_node(struct wsm_scene_node *node) {
	assert(node->impl == &scene_node_impl);
	struct wsm_scene_blur *blur = wl_container_of(node, blur, node);
	return blur;
}

void wsm_scene_blur_set_size(struct wsm_scene_blur *blur, int width, int height) {
	assert(width >= 0 && height >= 0);
	if (blur->width == width && blur->height == height) {
		return;
	}
	wsm_scene_node_update(&blur->node, NULL);
	blur->width = width;
	blur->height = height;
	wsm_scene_node_update(&blur->node, NULL);
}

void wsm_scene_blur_set_corner_radius(struct wsm_scene_blur *blur, float top_left, float top_right,
	float bottom_left, float bottom_right) {
	assert(isfinite(top_left) && top_left >= 0 && isfinite(top_right) && top_right >= 0);
	assert(
		isfinite(bottom_left) && bottom_left >= 0 && isfinite(bottom_right) && bottom_right >= 0);
	blur->top_left_radius = top_left;
	blur->top_right_radius = top_right;
	blur->bottom_left_radius = bottom_left;
	blur->bottom_right_radius = bottom_right;
	wsm_scene_node_update(&blur->node, NULL);
}
