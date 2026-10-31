#include "node/wsm_scene_blur_buffer.h"

#include <assert.h>
#include <math.h>
#include <stdlib.h>

#include <wlr/render/wlr_texture.h>
#include <wlr/types/wlr_buffer.h>
#include <wlr/types/wlr_output.h>
#include "scene/wsm_scene.h"
#include "scene/wsm_scene_output.h"
#include "node/wsm_scene_tree.h"
#include "node/wsm_scene_node.h"
#include <wlr/util/log.h>
#include <wlr/util/region.h>
#include <wlr/util/transform.h>

#define DEFAULT_CORNER_SMOOTHNESS 4.0f

static void handle_pass_destroy(struct wl_listener *listener, void *data) {
	struct wsm_scene_blur_buffer *blur = wl_container_of(listener, blur, pass_destroy);
	wl_list_remove(&blur->pass_destroy.link);
	wl_list_init(&blur->pass_destroy.link);
	blur->pass = NULL;
	wsm_scene_node_update(&blur->buffer.node, NULL);
}

static void scene_node_get_size(struct wsm_scene_node *node, int *width, int *height) {
	struct wsm_scene_blur_buffer *blur = wsm_scene_blur_buffer_from_node(node);
	struct wsm_scene_buffer *buffer = &blur->buffer;

	if (buffer->dst_width > 0 && buffer->dst_height > 0) {
		*width = buffer->dst_width;
		*height = buffer->dst_height;
		return;
	}
	if (buffer->buffer == NULL) {
		*width = 0;
		*height = 0;
		return;
	}
	*width = buffer->buffer->width;
	*height = buffer->buffer->height;
	wlr_output_transform_coords(buffer->transform, width, height);
}

static void scene_node_destroy(struct wsm_scene_node *node) {
	struct wsm_scene_blur_buffer *blur = wsm_scene_blur_buffer_from_node(node);
	wl_list_remove(&blur->pass_destroy.link);
	wsm_scene_buffer_set_buffer(&blur->buffer, NULL);
	pixman_region32_fini(&blur->buffer.opaque_region);
	free(blur);
}

static bool scene_node_at_iterator(struct wsm_scene_node *node, int lx, int ly, void *data) {
	struct node_at_data *at_data = data;
	double rx = at_data->lx - lx;
	double ry = at_data->ly - ly;
	struct wsm_scene_blur_buffer *blur = wsm_scene_blur_buffer_from_node(node);
	struct wsm_scene_buffer *buffer = &blur->buffer;
	if (buffer->point_accepts_input && !buffer->point_accepts_input(buffer, &rx, &ry)) {
		return false;
	}
	at_data->rx = rx;
	at_data->ry = ry;
	at_data->node = node;
	return true;
}

static struct wsm_scene_node *scene_node_at(
	struct wsm_scene_node *node, double lx, double ly, double *nx, double *ny) {
	struct wlr_box box = {.x = floor(lx), .y = floor(ly), .width = 1, .height = 1};
	struct node_at_data data = {.lx = lx, .ly = ly};
	if (wsm_scene_node_nodes_in_box(node, &box, scene_node_at_iterator, &data)) {
		if (nx) {
			*nx = data.rx;
		}
		if (ny) {
			*ny = data.ry;
		}
		return data.node;
	}
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

static uint32_t region_area(pixman_region32_t *region) {
	uint32_t area = 0;
	int nrects;
	pixman_box32_t *rects = pixman_region32_rectangles(region, &nrects);
	for (int i = 0; i < nrects; ++i) {
		area += (rects[i].x2 - rects[i].x1) * (rects[i].y2 - rects[i].y1);
	}
	return area;
}

static void update_node_update_outputs(struct wsm_scene_node *node, struct wl_list *outputs,
	struct wsm_scene_output *ignore, struct wsm_scene_output *force) {
	struct wsm_scene_blur_buffer *blur = wsm_scene_blur_buffer_from_node(node);
	struct wsm_scene_buffer *buffer = &blur->buffer;
	uint32_t largest_overlap = 0;
	struct wsm_scene_output *old_primary_output = buffer->primary_output;
	buffer->primary_output = NULL;
	size_t count = 0;
	uint64_t active_outputs = 0;

	struct wsm_scene_output *scene_output;
	wl_list_for_each(scene_output, outputs, link) {
		if (scene_output == ignore || !scene_output->output->enabled) {
			continue;
		}
		struct wlr_box output_box = {.x = scene_output->x, .y = scene_output->y};
		wlr_output_effective_resolution(
			scene_output->output, &output_box.width, &output_box.height);
		pixman_region32_t intersection;
		pixman_region32_init(&intersection);
		pixman_region32_intersect_rect(&intersection, &node->visible, output_box.x, output_box.y,
			output_box.width, output_box.height);
		if (!pixman_region32_empty(&intersection)) {
			uint32_t overlap = region_area(&intersection);
			if (overlap >= largest_overlap) {
				largest_overlap = overlap;
				buffer->primary_output = scene_output;
			}
			active_outputs |= 1ull << scene_output->index;
			count++;
		}
		pixman_region32_fini(&intersection);
	}

	if (old_primary_output != buffer->primary_output) {
		buffer->prev_feedback_options = (struct wlr_linux_dmabuf_feedback_v1_init_options){0};
	}
	uint64_t old_active = buffer->active_outputs;
	buffer->active_outputs = active_outputs;
	wl_list_for_each(scene_output, outputs, link) {
		uint64_t mask = 1ull << scene_output->index;
		bool intersects = active_outputs & mask;
		bool intersects_before = old_active & mask;
		if (intersects && !intersects_before) {
			wl_signal_emit_mutable(&buffer->events.output_enter, scene_output);
		} else if (!intersects && intersects_before) {
			wl_signal_emit_mutable(&buffer->events.output_leave, scene_output);
		}
	}

	assert(!buffer->active_outputs || buffer->primary_output);
	if (old_active == active_outputs && (!force || ((1ull << force->index) & ~active_outputs)) &&
		old_primary_output == buffer->primary_output) {
		return;
	}

	struct wsm_scene_output *outputs_array[64];
	struct wsm_scene_outputs_update_event event = {
		.active = outputs_array,
		.size = count,
	};
	size_t i = 0;
	wl_list_for_each(scene_output, outputs, link) {
		if (~active_outputs & (1ull << scene_output->index)) {
			continue;
		}
		assert(i < count);
		outputs_array[i++] = scene_output;
	}
	wl_signal_emit_mutable(&buffer->events.outputs_update, &event);
}

static bool scene_node_invisible(struct wsm_scene_node *node) {
	struct wsm_scene_blur_buffer *blur = wsm_scene_blur_buffer_from_node(node);
	return blur->pass == NULL || blur->buffer.buffer == NULL || blur->buffer.opacity == 0.f;
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

static void logical_to_buffer_coords(
	pixman_region32_t *region, const struct wsm_render_data *data, bool round_up) {
	enum wl_output_transform transform = wlr_output_transform_invert(data->transform);
	scale_region(region, data->scale, round_up);
	wlr_region_transform(region, region, transform, data->trans_width, data->trans_height);
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

static void transform_output_box(struct wlr_box *box, const struct wsm_render_data *data) {
	enum wl_output_transform transform = wlr_output_transform_invert(data->transform);
	scale_box(box, data->scale);
	wlr_box_transform(box, box, transform, data->trans_width, data->trans_height);
}

static struct wlr_texture *buffer_get_texture(
	struct wsm_scene_buffer *buffer, struct wlr_renderer *renderer, bool *must_destroy) {
	*must_destroy = false;
	struct wlr_client_buffer *client_buffer = wlr_client_buffer_get(buffer->buffer);
	if (client_buffer != NULL) {
		return client_buffer->texture;
	}
	struct wlr_texture *texture = wlr_texture_from_buffer(renderer, buffer->buffer);
	if (texture != NULL) {
		*must_destroy = true;
	}
	return texture;
}

static void scene_node_render(
	struct wsm_render_list_entry *entry, const struct wsm_render_data *data) {
	struct wsm_scene_node *node = entry->node;
	pixman_region32_t render_region;
	pixman_region32_init(&render_region);
	pixman_region32_copy(&render_region, &node->visible);
	pixman_region32_translate(&render_region, -data->logical.x, -data->logical.y);
	logical_to_buffer_coords(&render_region, data, true);
	pixman_region32_intersect(&render_region, &render_region, &data->damage);
	if (pixman_region32_empty(&render_region)) {
		pixman_region32_fini(&render_region);
		return;
	}

	int x = entry->x - data->logical.x;
	int y = entry->y - data->logical.y;
	struct wlr_box dst_box = {.x = x, .y = y};
	scene_node_get_size(node, &dst_box.width, &dst_box.height);
	transform_output_box(&dst_box, data);

	struct wsm_scene_blur_buffer *blur = wsm_scene_blur_buffer_from_node(node);
	struct wsm_scene_buffer *buffer = &blur->buffer;
	if (blur->pass == NULL) {
		wlr_log(WLR_ERROR, "No blur buffer pass is available for this renderer");
		pixman_region32_fini(&render_region);
		return;
	}

	bool destroy_texture = false;
	struct wlr_texture *texture =
		buffer_get_texture(buffer, data->output->output->renderer, &destroy_texture);
	if (texture == NULL) {
		pixman_region32_fini(&render_region);
		return;
	}

	enum wl_output_transform transform = wlr_output_transform_invert(buffer->transform);
	transform = wlr_output_transform_compose(transform, data->transform);
	float radius_scale = data->scale;
	float logical_radii[4] = {blur->top_left_radius, blur->top_right_radius,
		blur->bottom_left_radius, blur->bottom_right_radius};
	float radii[4];
	for (int i = 0; i < 4; i++) {
		struct wlr_box corner = {.x = i & 1, .y = (i >> 1) & 1};
		wlr_box_transform(&corner, &corner, wlr_output_transform_invert(data->transform), 1, 1);
		radii[(corner.x != 0) + 2 * (corner.y != 0)] = logical_radii[i] * radius_scale;
	}
	wsm_render_blur_buffer_pass_render(data->render_pass, blur->pass,
		&(struct wsm_render_blur_buffer_options){
			.texture = texture,
			.src_box = buffer->src_box,
			.dst_box = dst_box,
			.transform = transform,
			.clip = &render_region,
			.alpha = &buffer->opacity,
			.filter_mode = buffer->filter_mode,
			.blend_mode = WLR_RENDER_BLEND_MODE_PREMULTIPLIED,
			.top_left_radius = radii[0],
			.top_right_radius = radii[1],
			.bottom_left_radius = radii[2],
			.bottom_right_radius = radii[3],
			.corner_smoothness = DEFAULT_CORNER_SMOOTHNESS,
			.iterations = blur->iterations,
			.offset = blur->offset,
			.saturation = blur->saturation,
			.brightness = blur->brightness,
			.noise_amount = blur->noise_amount,
			.material_contrast = blur->material_contrast,
			.tint = {blur->tint[0], blur->tint[1], blur->tint[2], blur->tint[3]},
			.wait_timeline = buffer->wait_timeline,
			.wait_point = buffer->wait_point,
		});

	struct wsm_scene_output_sample_event sample_event = {
		.output = data->output,
		.direct_scanout = false,
		.release_timeline = data->output->in_timeline,
		.release_point = data->output->in_point,
	};
	wl_signal_emit_mutable(&buffer->events.output_sample, &sample_event);

	if (destroy_texture) {
		wlr_texture_destroy(texture);
	}
	pixman_region32_fini(&render_region);
}

static void scene_node_send_frame_done(
	struct wsm_scene_node *node, struct wsm_scene_output *scene_output, struct timespec *now) {
	if (!node->enabled) {
		return;
	}
	struct wsm_scene_blur_buffer *blur = wsm_scene_blur_buffer_from_node(node);
	struct wlr_scene_frame_done_event event = {
		.output = scene_output,
		.when = *now,
	};
	wsm_scene_buffer_send_frame_done(&blur->buffer, &event);
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
	.update_outputs = update_node_update_outputs,
	.visibility = scene_node_visibility,
	.frame_done = scene_node_send_frame_done,
	.invisible = scene_node_invisible,
	.construct_render_list_iterator = construct_render_list_iterator,
	.render = scene_node_render,
	.get_extents = get_scene_node_extents,
	.cleanup_when_disabled = scene_node_cleanup_when_disabled,
};

struct wsm_scene_blur_buffer *wsm_scene_blur_buffer_create(struct wsm_scene_tree *parent,
	struct wlr_buffer *buffer, struct wsm_render_blur_buffer_pass *pass,
	const struct wsm_render_blur_buffer_options *options) {
	assert(parent);
	struct wsm_scene_blur_buffer *blur = calloc(1, sizeof(*blur));
	if (!blur) {
		return NULL;
	}
	wsm_scene_node_init(&blur->buffer.node, &scene_node_impl, parent);
	wl_signal_init(&blur->buffer.events.outputs_update);
	wl_signal_init(&blur->buffer.events.output_enter);
	wl_signal_init(&blur->buffer.events.output_leave);
	wl_signal_init(&blur->buffer.events.output_sample);
	wl_signal_init(&blur->buffer.events.frame_done);
	pixman_region32_init(&blur->buffer.opaque_region);
	wl_list_init(&blur->buffer.buffer_release.link);
	wl_list_init(&blur->buffer.renderer_destroy.link);
	wl_list_init(&blur->pass_destroy.link);
	blur->buffer.opacity = 1.f;
	const struct wsm_render_blur_buffer_options defaults = {
		.iterations = 4,
		.offset = 1.8f,
		.saturation = 1.35f,
		.brightness = 1.06f,
		.noise_amount = .02f,
		.material_contrast = .04f,
		.tint = {1, 1, 1, .14f},
	};
	const struct wsm_render_blur_buffer_options *o = options ? options : &defaults;
	blur->top_left_radius = o->top_left_radius;
	blur->top_right_radius = o->top_right_radius;
	blur->bottom_left_radius = o->bottom_left_radius;
	blur->bottom_right_radius = o->bottom_right_radius;
	blur->iterations = o->iterations;
	blur->offset = o->offset;
	blur->saturation = o->saturation;
	blur->brightness = o->brightness;
	blur->noise_amount = o->noise_amount;
	blur->material_contrast = o->material_contrast;
	for (int i = 0; i < 4; i++) {
		blur->tint[i] = o->tint[i];
	}
	wsm_scene_buffer_set_buffer(&blur->buffer, buffer);
	wsm_scene_blur_buffer_set_pass(blur, pass);
	return blur;
}

void wsm_scene_blur_buffer_set_buffer(
	struct wsm_scene_blur_buffer *blur, struct wlr_buffer *buffer) {
	wsm_scene_buffer_set_buffer(&blur->buffer, buffer);
	wsm_scene_node_update(&blur->buffer.node, NULL);
}

void wsm_scene_blur_buffer_set_pass(
	struct wsm_scene_blur_buffer *buffer, struct wsm_render_blur_buffer_pass *pass) {
	wl_list_remove(&buffer->pass_destroy.link);
	wl_list_init(&buffer->pass_destroy.link);
	buffer->pass = pass;
	if (pass) {
		buffer->pass_destroy.notify = handle_pass_destroy;
		wl_signal_add(&pass->events.destroy, &buffer->pass_destroy);
	}
	wsm_scene_node_update(&buffer->buffer.node, NULL);
}

void wsm_scene_blur_buffer_set_blur(
	struct wsm_scene_blur_buffer *buffer, int iterations, float offset) {
	buffer->iterations = iterations;
	buffer->offset = offset;
	wsm_scene_node_update(&buffer->buffer.node, NULL);
}

void wsm_scene_blur_buffer_set_appearance(
	struct wsm_scene_blur_buffer *buffer, float saturation, float brightness, const float tint[4]) {
	buffer->saturation = saturation;
	buffer->brightness = brightness;
	for (size_t i = 0; i < 4; i++) {
		buffer->tint[i] = tint[i];
	}
	wsm_scene_node_update(&buffer->buffer.node, NULL);
}

void wsm_scene_blur_buffer_set_noise(struct wsm_scene_blur_buffer *buffer, float noise_amount) {
	buffer->noise_amount = noise_amount < 0.f ? 0.f : noise_amount;
	wsm_scene_node_update(&buffer->buffer.node, NULL);
}

void wsm_scene_blur_buffer_set_material_contrast(
	struct wsm_scene_blur_buffer *buffer, float contrast) {
	buffer->material_contrast = contrast < 0.f ? 0.f : contrast;
	wsm_scene_node_update(&buffer->buffer.node, NULL);
}

bool wsm_scene_node_is_blur_buffer(const struct wsm_scene_node *node) {
	return node != NULL && node->impl == &scene_node_impl;
}

struct wsm_scene_blur_buffer *wsm_scene_blur_buffer_from_node(struct wsm_scene_node *node) {
	assert(node->impl == &scene_node_impl);
	struct wsm_scene_blur_buffer *blur = wl_container_of(node, blur, buffer.node);
	return blur;
}
