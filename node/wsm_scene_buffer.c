#include "node/wsm_scene_buffer.h"
#include "types/wsm_color.h"
#include "scene/wsm_scene.h"
#include "node/wsm_scene_node.h"
#include "node/wsm_scene_tree.h"
#include "scene/wsm_scene_output.h"
#include "scene/wsm_scene_surface.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include <wayland-server-core.h>
#include <wayland-util.h>

#include <wlr/util/region.h>
#include <wlr/util/transform.h>
#include <wlr/render/drm_syncobj.h>
#include <wlr/types/wlr_output.h>
#include <wlr/render/color.h>
#include <wlr/util/transform.h>

static void scene_node_visibility(struct wsm_scene_node *node,
	pixman_region32_t *visible);
static void scene_node_get_size(struct wsm_scene_node *node,
	int *width, int *height);

static void scene_buffer_handle_buffer_release(struct wl_listener *listener,
		void *data) {
	struct wsm_scene_buffer *scene_buffer =
		wl_container_of(listener, scene_buffer, buffer_release);

	scene_buffer->buffer = NULL;
	wl_list_remove(&scene_buffer->buffer_release.link);
	wl_list_init(&scene_buffer->buffer_release.link);
}

static void scene_buffer_set_buffer(struct wsm_scene_buffer *scene_buffer,
		struct wlr_buffer *buffer) {
	wl_list_remove(&scene_buffer->buffer_release.link);
	wl_list_init(&scene_buffer->buffer_release.link);
	if (scene_buffer->own_buffer) {
		wlr_buffer_unlock(scene_buffer->buffer);
	}

	scene_buffer->buffer = NULL;
	scene_buffer->own_buffer = false;
	scene_buffer->buffer_width = scene_buffer->buffer_height = 0;
	scene_buffer->buffer_is_opaque = false;

	if (!buffer) {
		return;
	}

	scene_buffer->own_buffer = true;
	scene_buffer->buffer = wlr_buffer_lock(buffer);
	scene_buffer->buffer_width = buffer->width;
	scene_buffer->buffer_height = buffer->height;
	scene_buffer->buffer_is_opaque = wlr_buffer_is_opaque(buffer);

	scene_buffer->buffer_release.notify = scene_buffer_handle_buffer_release;
	wl_signal_add(&buffer->events.release, &scene_buffer->buffer_release);
}

static void scene_buffer_set_texture(struct wsm_scene_buffer *scene_buffer,
	struct wlr_texture *texture);

static void scene_buffer_handle_renderer_destroy(struct wl_listener *listener,
		void *data) {
	struct wsm_scene_buffer *scene_buffer = wl_container_of(listener, scene_buffer, renderer_destroy);
	scene_buffer_set_texture(scene_buffer, NULL);
}

static void scene_buffer_set_texture(struct wsm_scene_buffer *scene_buffer,
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

static void scene_node_destroy(struct wsm_scene_node *node) {
	struct wsm_scene_buffer *scene_buffer = wsm_scene_buffer_from_node(node);

	scene_buffer_set_buffer(scene_buffer, NULL);
	scene_buffer_set_texture(scene_buffer, NULL);
	pixman_region32_fini(&scene_buffer->opaque_region);
	wlr_drm_syncobj_timeline_unref(scene_buffer->wait_timeline);

	assert(wl_list_empty(&scene_buffer->events.outputs_update.listener_list));
	assert(wl_list_empty(&scene_buffer->events.output_sample.listener_list));
	assert(wl_list_empty(&scene_buffer->events.frame_done.listener_list));

	free(scene_buffer);
}

static bool scene_node_at_iterator(struct wsm_scene_node *node,
		int lx, int ly, void *data) {
	struct node_at_data *at_data = data;

	double rx = at_data->lx - lx;
	double ry = at_data->ly - ly;

	struct wsm_scene_buffer *scene_buffer = wsm_scene_buffer_from_node(node);

	if (scene_buffer->point_accepts_input &&
			!scene_buffer->point_accepts_input(scene_buffer, &rx, &ry)) {
		return false;
	}

	at_data->rx = rx;
	at_data->ry = ry;
	at_data->node = node;
	return true;
}

static struct wsm_scene_node *scene_node_at(struct wsm_scene_node *node,
		double lx, double ly, double *nx, double *ny) {
	struct wlr_box box = {
		.x = floor(lx),
		.y = floor(ly),
		.width = 1,
		.height = 1
	};

	struct node_at_data data = {
		.lx = lx,
		.ly = ly
	};

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

static void scene_node_get_size(struct wsm_scene_node *node,
		int *width, int *height) {
	struct wsm_scene_buffer *scene_buffer = wsm_scene_buffer_from_node(node);
	if (scene_buffer->dst_width > 0 && scene_buffer->dst_height > 0) {
		*width = scene_buffer->dst_width;
		*height = scene_buffer->dst_height;
	} else {
		*width = scene_buffer->buffer_width;
		*height = scene_buffer->buffer_height;
		wlr_output_transform_coords(scene_buffer->transform, width, height);
	}
}

static bool _scene_nodes_in_box(struct wsm_scene_node *node, struct wlr_box *box,
		scene_node_box_iterator_func_t iterator, void *user_data, int lx, int ly) {
	if (!node->enabled) {
		return false;
	}

	struct wlr_box node_box = { .x = lx, .y = ly };
	scene_node_get_size(node, &node_box.width, &node_box.height);

	if (wlr_box_intersection(&node_box, &node_box, box) &&
			iterator(node, lx, ly, user_data)) {
		return true;
	}

	return false;
}

static bool scene_nodes_in_box(struct wsm_scene_node *node, struct wlr_box *box,
		scene_node_box_iterator_func_t iterator, void *user_data) {
	int x, y;
	wsm_scene_node_coords(node, &x, &y);

	return _scene_nodes_in_box(node, box, iterator, user_data, x, y);
}

static void scene_node_opaque_region(struct wsm_scene_node *node, int x, int y,
		pixman_region32_t *opaque) {
	int width, height;
	scene_node_get_size(node, &width, &height);

	struct wsm_scene_buffer *scene_buffer = wsm_scene_buffer_from_node(node);

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

	pixman_region32_fini(opaque);
	pixman_region32_init_rect(opaque, x, y, width, height);
}

#if WLR_HAS_XWAYLAND
static struct wlr_xwayland_surface *scene_node_try_get_managed_xwayland_surface(
		struct wsm_scene_node *node) {
	struct wsm_scene_buffer *buffer_node = wsm_scene_buffer_from_node(node);
	struct wsm_scene_surface *surface_node = wsm_scene_surface_try_from_buffer(buffer_node);
	if (!surface_node) {
		return NULL;
	}

	struct wlr_xwayland_surface *xwayland_surface =
		wlr_xwayland_surface_try_from_wlr_surface(surface_node->surface);
	if (!xwayland_surface || xwayland_surface->override_redirect) {
		return NULL;
	}

	return xwayland_surface;
}

static void restack_xwayland_surface(struct wsm_scene_node *node,
		struct wlr_box *box, struct wsm_scene_update_data *data) {
	struct wlr_xwayland_surface *xwayland_surface =
		scene_node_try_get_managed_xwayland_surface(node);
	if (!xwayland_surface) {
		return;
	}

	// ensure this node is entirely inside the update region. If not, we can't
	// restack this node since we're not considering the whole thing.
	if (wlr_box_contains_box(&data->update_box, box)) {
		if (data->restack_above) {
			wlr_xwayland_surface_restack(xwayland_surface, data->restack_above, XCB_STACK_MODE_BELOW);
		} else {
			wlr_xwayland_surface_restack(xwayland_surface, NULL, XCB_STACK_MODE_ABOVE);
		}
	}

	data->restack_above = xwayland_surface;
}
#endif

static uint32_t region_area(pixman_region32_t *region) {
	uint32_t area = 0;

	int nrects;
	pixman_box32_t *rects = pixman_region32_rectangles(region, &nrects);
	for (int i = 0; i < nrects; ++i) {
		area += (rects[i].x2 - rects[i].x1) * (rects[i].y2 - rects[i].y1);
	}

	return area;
}

static void update_node_update_outputs(struct wsm_scene_node *node,
		struct wl_list *outputs, struct wsm_scene_output *ignore,
		struct wsm_scene_output *force) {
	struct wsm_scene_buffer *scene_buffer = wsm_scene_buffer_from_node(node);

	uint32_t largest_overlap = 0;
	struct wsm_scene_output *old_primary_output = scene_buffer->primary_output;
	scene_buffer->primary_output = NULL;

	size_t count = 0;
	uint64_t active_outputs = 0;

	// let's update the outputs in two steps:
	//  - the primary outputs
	//  - the enter/leave signals
	// This ensures that the enter/leave signals can rely on the primary output
	// to have a reasonable value. Otherwise, they may get a value that's in
	// the middle of a calculation.
	struct wsm_scene_output *scene_output;
	wl_list_for_each(scene_output, outputs, link) {
		if (scene_output == ignore) {
			continue;
		}

		if (!scene_output->output->enabled) {
			continue;
		}

		struct wlr_box output_box = {
			.x = scene_output->x,
			.y = scene_output->y,
		};
		wlr_output_effective_resolution(scene_output->output,
			&output_box.width, &output_box.height);

		pixman_region32_t intersection;
		pixman_region32_init(&intersection);
		pixman_region32_intersect_rect(&intersection, &node->visible,
			output_box.x, output_box.y, output_box.width, output_box.height);

		if (!pixman_region32_empty(&intersection)) {
			uint32_t overlap = region_area(&intersection);
			if (overlap >= largest_overlap) {
				largest_overlap = overlap;
				scene_buffer->primary_output = scene_output;
			}

			active_outputs |= 1ull << scene_output->index;
			count++;
		}

		pixman_region32_fini(&intersection);
	}

	if (old_primary_output != scene_buffer->primary_output) {
		scene_buffer->prev_feedback_options =
			(struct wlr_linux_dmabuf_feedback_v1_init_options){0};
	}

	uint64_t old_active = scene_buffer->active_outputs;
	scene_buffer->active_outputs = active_outputs;

	wl_list_for_each(scene_output, outputs, link) {
		uint64_t mask = 1ull << scene_output->index;
		bool intersects = active_outputs & mask;
		bool intersects_before = old_active & mask;

		if (intersects && !intersects_before) {
			wl_signal_emit_mutable(&scene_buffer->events.output_enter, scene_output);
		} else if (!intersects && intersects_before) {
			wl_signal_emit_mutable(&scene_buffer->events.output_leave, scene_output);
		}
	}

	// if there are active outputs on this node, we should always have a primary
	// output
	assert(!scene_buffer->active_outputs || scene_buffer->primary_output);

	// Skip output update event if nothing was updated
	if (old_active == active_outputs &&
			(!force || ((1ull << force->index) & ~active_outputs)) &&
			old_primary_output == scene_buffer->primary_output) {
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

	wl_signal_emit_mutable(&scene_buffer->events.outputs_update, &event);
}

static bool scene_node_update_iterator(struct wsm_scene_node *node,
		int lx, int ly, void *_data) {
	struct wsm_scene_update_data *data = _data;

	struct wlr_box box = { .x = lx, .y = ly };
	scene_node_get_size(node, &box.width, &box.height);

	pixman_region32_subtract(&node->visible, &node->visible, data->update_region);
	pixman_region32_union(&node->visible, &node->visible, data->visible);
	pixman_region32_intersect_rect(&node->visible, &node->visible,
		lx, ly, box.width, box.height);

	if (data->calculate_visibility) {
		pixman_region32_t opaque;
		pixman_region32_init(&opaque);
		scene_node_opaque_region(node, lx, ly, &opaque);
		pixman_region32_subtract(data->visible, data->visible, &opaque);
		pixman_region32_fini(&opaque);
	}

	update_node_update_outputs(node, data->outputs, NULL, NULL);
#if WLR_HAS_XWAYLAND
	restack_xwayland_surface(node, &box, data);
#endif

	return false;
}

static void scale_region(pixman_region32_t *region, float scale, bool round_up) {
	wlr_region_scale(region, region, scale);

	if (round_up && floor(scale) != scale) {
		wlr_region_expand(region, region, 1);
	}
}

static void output_to_buffer_coords(pixman_region32_t *damage, struct wlr_output *output) {
	int width, height;
	wlr_output_transformed_resolution(output, &width, &height);

	wlr_region_transform(damage, damage,
		wlr_output_transform_invert(output->transform), width, height);
}

static void scene_output_damage(struct wsm_scene_output *scene_output,
		const pixman_region32_t *damage) {
	struct wlr_output *output = scene_output->output;

	pixman_region32_t clipped;
	pixman_region32_init(&clipped);
	pixman_region32_intersect_rect(&clipped, damage, 0, 0, output->width, output->height);

	if (!pixman_region32_empty(&clipped)) {
		wlr_output_schedule_frame(scene_output->output);
		wlr_damage_ring_add(&scene_output->damage_ring, &clipped);

		pixman_region32_union(&scene_output->pending_commit_damage,
			&scene_output->pending_commit_damage, &clipped);
	}

	pixman_region32_fini(&clipped);
}

static void scene_update_region(struct wsm_scene *scene,
		pixman_region32_t *update_region) {
	pixman_region32_t visible;
	pixman_region32_init(&visible);
	pixman_region32_copy(&visible, update_region);

	struct pixman_box32 *region_box = pixman_region32_extents(update_region);
	struct wsm_scene_update_data data = {
		.visible = &visible,
		.update_region = update_region,
		.update_box = {
			.x = region_box->x1,
			.y = region_box->y1,
			.width = region_box->x2 - region_box->x1,
			.height = region_box->y2 - region_box->y1,
		},
		.outputs = &scene->outputs,
		.calculate_visibility = scene->calculate_visibility,
	};

	// update node visibility and output enter/leave events
	wsm_scene_node_nodes_in_box(&scene->tree->node, &data.update_box, scene_node_update_iterator, &data);

	pixman_region32_fini(&visible);
}

static void scene_node_visibility(struct wsm_scene_node *node,
		pixman_region32_t *visible) {
	if (!node->enabled) {
		return;
	}

	pixman_region32_union(visible, visible, &node->visible);
}

static void scene_node_send_frame_done(struct wsm_scene_node *node,
		struct wsm_scene_output *scene_output, struct timespec *now) {
	if (!node->enabled) {
		return;
	}

	struct wsm_scene_buffer *scene_buffer =
		wsm_scene_buffer_from_node(node);
	struct wlr_scene_frame_done_event event = {
		.output = scene_output,
		.when = *now,
	};
	wsm_scene_buffer_send_frame_done(scene_buffer, &event);
}

static bool scene_node_invisible(struct wsm_scene_node *node) {
	struct wsm_scene_buffer *buffer = wsm_scene_buffer_from_node(node);

	return buffer->buffer == NULL && buffer->texture == NULL;
}

static bool scene_buffer_is_black_opaque(struct wsm_scene_buffer *scene_buffer) {
	return scene_buffer->is_single_pixel_buffer &&
		scene_buffer->single_pixel_buffer_color[0] == 0 &&
		scene_buffer->single_pixel_buffer_color[1] == 0 &&
		scene_buffer->single_pixel_buffer_color[2] == 0 &&
		scene_buffer->single_pixel_buffer_color[3] == UINT32_MAX &&
		scene_buffer->opacity == 1.0;
}

static bool construct_render_list_iterator(struct wsm_scene_node *node,
		int lx, int ly, void *_data) {
	struct wsm_render_list_constructor_data *data = _data;

	if (wsm_scene_node_invisible(node)) {
		return false;
	}

	// Apply the same special-case to black opaque single-pixel buffers
	if (data->calculate_visibility &&
			(!data->fractional_scale || data->render_list->size == 0)) {
		struct wsm_scene_buffer *scene_buffer = wsm_scene_buffer_from_node(node);

		if (scene_buffer_is_black_opaque(scene_buffer)) {
			return false;
		}
	}

	pixman_region32_t intersection;
	pixman_region32_init(&intersection);
	pixman_region32_intersect_rect(&intersection, &node->visible,
			data->box.x, data->box.y,
			data->box.width, data->box.height);
	if (pixman_region32_empty(&intersection)) {
		pixman_region32_fini(&intersection);
		return false;
	}

	pixman_region32_fini(&intersection);

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

static void logical_to_buffer_coords(pixman_region32_t *region, const struct wsm_render_data *data,
		bool round_up) {
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

static struct wlr_texture *scene_buffer_get_texture(
		struct wsm_scene_buffer *scene_buffer, struct wlr_renderer *renderer) {
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

static float get_luminance_multiplier(const struct wlr_color_luminances *src_lum,
		const struct wlr_color_luminances *dst_lum) {
	return (dst_lum->reference / src_lum->reference) * (src_lum->max / dst_lum->max);
}

static void scene_node_render(struct wsm_render_list_entry *entry, const struct wsm_render_data *data) {
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

	struct wlr_box dst_box = {
		.x = x,
		.y = y,
	};
	scene_node_get_size(node, &dst_box.width, &dst_box.height);
	transform_output_box(&dst_box, data);

	pixman_region32_t opaque;
	pixman_region32_init(&opaque);
	scene_node_opaque_region(node, x, y, &opaque);
	logical_to_buffer_coords(&opaque, data, false);
	pixman_region32_subtract(&opaque, &render_region, &opaque);

	struct wsm_scene_buffer *scene_buffer = wsm_scene_buffer_from_node(node);

	if (scene_buffer->is_single_pixel_buffer) {
		// Render the buffer as a rect, this is likely to be more efficient
		wlr_render_pass_add_rect(data->render_pass, &(struct wlr_render_rect_options){
			.box = dst_box,
			.color = {
				.r = (float)scene_buffer->single_pixel_buffer_color[0] / (float)UINT32_MAX,
				.g = (float)scene_buffer->single_pixel_buffer_color[1] / (float)UINT32_MAX,
				.b = (float)scene_buffer->single_pixel_buffer_color[2] / (float)UINT32_MAX,
				.a = (float)scene_buffer->single_pixel_buffer_color[3] /
					(float)UINT32_MAX * scene_buffer->opacity,
			},
			.clip = &render_region,
		});
		goto done;
	}

	struct wlr_texture *texture = scene_buffer_get_texture(scene_buffer,
		data->output->output->renderer);
	if (texture == NULL) {
		scene_output_damage(data->output, &render_region);
		goto done;
	}

	enum wl_output_transform transform =
		wlr_output_transform_invert(scene_buffer->transform);
	transform = wlr_output_transform_compose(transform, data->transform);

	struct wlr_color_primaries primaries = {0};
	if (scene_buffer->primaries != 0) {
		wlr_color_primaries_from_named(&primaries, scene_buffer->primaries);
	}

	struct wlr_color_luminances src_lum, srgb_lum;
	wsm_color_transfer_function_get_default_luminance(
		scene_buffer->transfer_function, &src_lum);
	wsm_color_transfer_function_get_default_luminance(
		WLR_COLOR_TRANSFER_FUNCTION_SRGB, &srgb_lum);
	float luminance_multiplier = get_luminance_multiplier(&src_lum, &srgb_lum);

	wlr_render_pass_add_texture(data->render_pass, &(struct wlr_render_texture_options) {
	.texture = texture,
		.src_box = scene_buffer->src_box,
		.dst_box = dst_box,
		.transform = transform,
		.clip = &render_region,
		.alpha = &scene_buffer->opacity,
		.filter_mode = scene_buffer->filter_mode,
		.blend_mode = !data->output->scene->calculate_visibility ||
				!pixman_region32_empty(&opaque) ?
			WLR_RENDER_BLEND_MODE_PREMULTIPLIED : WLR_RENDER_BLEND_MODE_NONE,
		.transfer_function = scene_buffer->transfer_function,
		.primaries = scene_buffer->primaries != 0 ? &primaries : NULL,
		.color_encoding = scene_buffer->color_encoding,
		.color_range = scene_buffer->color_range,
		.luminance_multiplier = &luminance_multiplier,
		.wait_timeline = scene_buffer->wait_timeline,
		.wait_point = scene_buffer->wait_point,
	});

	struct wsm_scene_output_sample_event sample_event = {
		.output = data->output,
		.direct_scanout = false,
		.release_timeline = data->output->in_timeline,
		.release_point = data->output->in_point,
	};
	wl_signal_emit_mutable(&scene_buffer->events.output_sample, &sample_event);
	if (entry->highlight_transparent_region) {
		wlr_render_pass_add_rect(data->render_pass, &(struct wlr_render_rect_options){
			.box = dst_box,
			.color = { .r = 0, .g = 0.3, .b = 0, .a = 0.3 },
			.clip = &opaque,
		});
	}

	goto done;

done:
	pixman_region32_fini(&opaque);
	pixman_region32_fini(&render_region);
}

static void scene_buffer_send_dmabuf_feedback(const struct wsm_scene *scene,
		struct wsm_scene_buffer *scene_buffer,
		const struct wlr_linux_dmabuf_feedback_v1_init_options *options) {
	if (!scene->linux_dmabuf_v1) {
		return;
	}

	struct wsm_scene_surface *surface = wsm_scene_surface_try_from_buffer(scene_buffer);
	if (!surface) {
		return;
	}

	// compare to the previous options so that we don't send
	// duplicate feedback events.
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

static void node_scene_buffer_send_dmabuf_feedback(struct wsm_render_list_entry *entry,
		struct wsm_scene_output *scene_output) {
	struct wsm_scene_buffer *buffer = wsm_scene_buffer_from_node(entry->node);

		// Direct scanout counts up to DMABUF_FEEDBACK_DEBOUNCE_FRAMES before sending new dmabuf
		// feedback, and on composition we wait until it hits zero again. If we knew that an
		// entry could never be a scanout candidate, we could send feedback to it
		// unconditionally without debounce, but for now it is all or nothing
		if (scene_output->dmabuf_feedback_debounce == 0 && buffer->primary_output == scene_output) {
			struct wlr_linux_dmabuf_feedback_v1_init_options options = {
				.main_renderer = scene_output->output->renderer,
				.scanout_primary_output = NULL,
			};

		scene_buffer_send_dmabuf_feedback(scene_output->scene, buffer, &options);
	}
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
	update_node_update_outputs(node, outputs, NULL, NULL);

#if WLR_HAS_XWAYLAND
	if (xwayland_restack) {
		struct wlr_xwayland_surface *xwayland_surface =
			scene_node_try_get_managed_xwayland_surface(node);
		if (!xwayland_surface) {
			return;
		}

		wlr_xwayland_surface_restack(xwayland_surface, NULL, XCB_STACK_MODE_BELOW);
	}
#endif
}

static const struct wsm_scene_node_impl scene_node_impl = {
	.destroy = scene_node_destroy,
	.set_enabled = NULL,
	.set_position = NULL,
	.bounds = NULL,
	.get_size = scene_node_get_size,
	.coords = NULL,
	.at = scene_node_at,
	.in_box = scene_nodes_in_box,
	.opaque_region = scene_node_opaque_region,
	.update_outputs = update_node_update_outputs,
	.update = NULL,
	.visibility = scene_node_visibility,
	.frame_done = scene_node_send_frame_done,
	.invisible = scene_node_invisible,
	.construct_render_list_iterator = construct_render_list_iterator,
	.render = scene_node_render,
	.dmabuf_feedback = node_scene_buffer_send_dmabuf_feedback,
	.get_extents = get_scene_node_extents,
	.get_children = NULL,
#if WLR_HAS_XWAYLAND
	.restack_xwayland_surface = restack_xwayland_surface,
#else
	.restack_xwayland_surface = NULL,
#endif
	.cleanup_when_disabled = scene_node_cleanup_when_disabled,
};

static void scene_buffer_set_wait_timeline(struct wsm_scene_buffer *scene_buffer,
		struct wlr_drm_syncobj_timeline *timeline, uint64_t point) {
	wlr_drm_syncobj_timeline_unref(scene_buffer->wait_timeline);
	if (timeline != NULL) {
		scene_buffer->wait_timeline = wlr_drm_syncobj_timeline_ref(timeline);
		scene_buffer->wait_point = point;
	} else {
		scene_buffer->wait_timeline = NULL;
		scene_buffer->wait_point = 0;
	}
}

struct wsm_scene_buffer *wsm_scene_buffer_create(struct wsm_scene_tree *parent,
		struct wlr_buffer *buffer) {
	struct wsm_scene_buffer *scene_buffer = calloc(1, sizeof(*scene_buffer));
	if (scene_buffer == NULL) {
		return NULL;
	}
	assert(parent);
	wsm_scene_node_init(&scene_buffer->node, &scene_node_impl, parent);

	wl_signal_init(&scene_buffer->events.outputs_update);
	wl_signal_init(&scene_buffer->events.output_enter);
	wl_signal_init(&scene_buffer->events.output_leave);
	wl_signal_init(&scene_buffer->events.output_sample);
	wl_signal_init(&scene_buffer->events.frame_done);

	pixman_region32_init(&scene_buffer->opaque_region);
	wl_list_init(&scene_buffer->buffer_release.link);
	wl_list_init(&scene_buffer->renderer_destroy.link);
	scene_buffer->opacity = 1;

	scene_buffer_set_buffer(scene_buffer, buffer);
	wsm_scene_node_update(&scene_buffer->node, NULL);

	return scene_buffer;
}

void wsm_scene_buffer_set_buffer(struct wsm_scene_buffer *scene_buffer,
		struct wlr_buffer *buffer) {
	wsm_scene_buffer_set_buffer_with_options(scene_buffer, buffer, NULL);
}

void wsm_scene_buffer_set_buffer_with_damage(struct wsm_scene_buffer *scene_buffer,
		struct wlr_buffer *buffer, const pixman_region32_t *damage) {
	const struct wsm_scene_buffer_set_buffer_options options = {
		.damage = damage,
	};
	wsm_scene_buffer_set_buffer_with_options(scene_buffer, buffer, &options);
}

void wsm_scene_buffer_set_buffer_with_options(struct wsm_scene_buffer *scene_buffer,
		struct wlr_buffer *buffer, const struct wsm_scene_buffer_set_buffer_options *options) {
	const struct wsm_scene_buffer_set_buffer_options default_options = {0};
	if (options == NULL) {
		options = &default_options;
	}

	// specifying a region for a NULL buffer doesn't make sense. We need to know
	// about the buffer to scale the buffer local coordinates down to scene
	// coordinates.
	assert(buffer || !options->damage);

	bool mapped = buffer != NULL;
	bool prev_mapped = scene_buffer->buffer != NULL || scene_buffer->texture != NULL;

	if (!mapped && !prev_mapped) {
		// unmapping already unmapped buffer - noop
		return;
	}

	// if this node used to not be mapped or its previous displayed
	// buffer region will be different from what the new buffer would
	// produce we need to update the node.
	bool update = mapped != prev_mapped;
	if (buffer != NULL && scene_buffer->dst_width == 0 && scene_buffer->dst_height == 0) {
		update = update || scene_buffer->buffer_width != buffer->width ||
			scene_buffer->buffer_height != buffer->height;
	}

	// If this is a buffer change, check if it's a single pixel buffer.
	// Cache that so we can still apply rendering optimisations even when
	// the original buffer has been freed after texture upload.
	if (buffer != scene_buffer->buffer) {
		scene_buffer->is_single_pixel_buffer = false;
		struct wlr_client_buffer *client_buffer = NULL;
		if (buffer != NULL) {
			client_buffer = wlr_client_buffer_get(buffer);
		}
		if (client_buffer != NULL && client_buffer->source != NULL) {
			struct wlr_single_pixel_buffer_v1 *single_pixel_buffer =
				wlr_single_pixel_buffer_v1_try_from_buffer(client_buffer->source);
			if (single_pixel_buffer != NULL) {
				scene_buffer->is_single_pixel_buffer = true;
				scene_buffer->single_pixel_buffer_color[0] = single_pixel_buffer->r;
				scene_buffer->single_pixel_buffer_color[1] = single_pixel_buffer->g;
				scene_buffer->single_pixel_buffer_color[2] = single_pixel_buffer->b;
				scene_buffer->single_pixel_buffer_color[3] = single_pixel_buffer->a;
			}
		}
	}

	scene_buffer_set_buffer(scene_buffer, buffer);
	scene_buffer_set_texture(scene_buffer, NULL);
	scene_buffer_set_wait_timeline(scene_buffer,
		options->wait_timeline, options->wait_point);

	if (update) {
		wsm_scene_node_update(&scene_buffer->node, NULL);
		// updating the node will already damage the whole node for us. Return
		// early to not damage again
		return;
	}

	int lx, ly;
	if (!wsm_scene_node_coords(&scene_buffer->node, &lx, &ly)) {
		return;
	}

	pixman_region32_t fallback_damage;
	pixman_region32_init_rect(&fallback_damage, 0, 0, buffer->width, buffer->height);
	const pixman_region32_t *damage = options->damage;
	if (!damage) {
		damage = &fallback_damage;
	}

	struct wlr_fbox box = scene_buffer->src_box;
	if (wlr_fbox_empty(&box)) {
		box.x = 0;
		box.y = 0;
		box.width = buffer->width;
		box.height = buffer->height;
	}

	wlr_fbox_transform(&box, &box, scene_buffer->transform,
		buffer->width, buffer->height);

	float scale_x, scale_y;
	if (scene_buffer->dst_width || scene_buffer->dst_height) {
		scale_x = scene_buffer->dst_width / box.width;
		scale_y = scene_buffer->dst_height / box.height;
	} else {
		scale_x = buffer->width / box.width;
		scale_y = buffer->height / box.height;
	}

	pixman_region32_t trans_damage;
	pixman_region32_init(&trans_damage);
	wlr_region_transform(&trans_damage, damage,
		scene_buffer->transform, buffer->width, buffer->height);
	pixman_region32_intersect_rect(&trans_damage, &trans_damage,
		box.x, box.y, box.width, box.height);
	pixman_region32_translate(&trans_damage, -box.x, -box.y);

	struct wsm_scene *scene = scene_buffer->node.scene;
	struct wsm_scene_output *scene_output;
	wl_list_for_each(scene_output, &scene->outputs, link) {
		float output_scale = scene_output->output->scale;
		float output_scale_x = output_scale * scale_x;
		float output_scale_y = output_scale * scale_y;
		pixman_region32_t output_damage;
		pixman_region32_init(&output_damage);
		wlr_region_scale_xy(&output_damage, &trans_damage,
			output_scale_x, output_scale_y);

		// One output pixel will match (buffer_scale_x)x(buffer_scale_y) buffer pixels.
		// If the buffer is upscaled on the given axis (output_scale_* > 1.0,
		// buffer_scale_* < 1.0), its contents will bleed into adjacent
		// (ceil(output_scale_* / 2)) output pixels because of linear filtering.
		// Additionally, if the buffer is downscaled (output_scale_* < 1.0,
		// buffer_scale_* > 1.0), and one output pixel matches a non-integer number of
		// buffer pixels, its contents will bleed into neighboring output pixels.
		// Handle both cases by computing buffer_scale_{x,y} and checking if they are
		// integer numbers; ceilf() is used to ensure that the distance is at least 1.
		float buffer_scale_x = 1.0f / output_scale_x;
		float buffer_scale_y = 1.0f / output_scale_y;
		int dist_x = floor(buffer_scale_x) != buffer_scale_x ?
			(int)ceilf(output_scale_x / 2.0f) : 0;
		int dist_y = floor(buffer_scale_y) != buffer_scale_y ?
			(int)ceilf(output_scale_y / 2.0f) : 0;
		// TODO: expand with per-axis distances
		wlr_region_expand(&output_damage, &output_damage,
			dist_x >= dist_y ? dist_x : dist_y);

		pixman_region32_t cull_region;
		pixman_region32_init(&cull_region);
		pixman_region32_copy(&cull_region, &scene_buffer->node.visible);
		scale_region(&cull_region, output_scale, true);
		pixman_region32_translate(&cull_region, -lx * output_scale, -ly * output_scale);
		pixman_region32_intersect(&output_damage, &output_damage, &cull_region);
		pixman_region32_fini(&cull_region);

		pixman_region32_translate(&output_damage,
			(int)round((lx - scene_output->x) * output_scale),
			(int)round((ly - scene_output->y) * output_scale));
		output_to_buffer_coords(&output_damage, scene_output->output);
		scene_output_damage(scene_output, &output_damage);
		pixman_region32_fini(&output_damage);
	}

	pixman_region32_fini(&trans_damage);
	pixman_region32_fini(&fallback_damage);
}

void wsm_scene_buffer_set_opaque_region(struct wsm_scene_buffer *scene_buffer,
		const pixman_region32_t *region) {
	if (pixman_region32_equal(&scene_buffer->opaque_region, region)) {
		return;
	}

	pixman_region32_copy(&scene_buffer->opaque_region, region);

	int x, y;
	if (!wsm_scene_node_coords(&scene_buffer->node, &x, &y)) {
		return;
	}

	pixman_region32_t update_region;
	pixman_region32_init(&update_region);
	wsm_scene_node_bounds(&scene_buffer->node, x, y, &update_region);
	scene_update_region(scene_buffer->node.scene, &update_region);
	pixman_region32_fini(&update_region);
}

void wsm_scene_buffer_set_source_box(struct wsm_scene_buffer *scene_buffer,
		const struct wlr_fbox *box) {
	if (wlr_fbox_equal(&scene_buffer->src_box, box)) {
		return;
	}

	if (box != NULL) {
		assert(box->x >= 0 && box->y >= 0 && box->width >= 0 && box->height >= 0);
		scene_buffer->src_box = *box;
	} else {
		scene_buffer->src_box = (struct wlr_fbox){0};
	}

	wsm_scene_node_update(&scene_buffer->node, NULL);
}

void wsm_scene_buffer_set_dest_size(struct wsm_scene_buffer *scene_buffer,
		int width, int height) {
	if (scene_buffer->dst_width == width && scene_buffer->dst_height == height) {
		return;
	}

	assert(width >= 0 && height >= 0);
	scene_buffer->dst_width = width;
	scene_buffer->dst_height = height;
	wsm_scene_node_update(&scene_buffer->node, NULL);
}

void wsm_scene_buffer_set_transform(struct wsm_scene_buffer *scene_buffer,
		enum wl_output_transform transform) {
	if (scene_buffer->transform == transform) {
		return;
	}

	scene_buffer->transform = transform;
	wsm_scene_node_update(&scene_buffer->node, NULL);
}

void wsm_scene_buffer_set_opacity(struct wsm_scene_buffer *scene_buffer,
		float opacity) {
	if (scene_buffer->opacity == opacity) {
		return;
	}

	assert(opacity >= 0 && opacity <= 1);
	scene_buffer->opacity = opacity;
	wsm_scene_node_update(&scene_buffer->node, NULL);
}

void wsm_scene_buffer_set_filter_mode(struct wsm_scene_buffer *scene_buffer,
		enum wlr_scale_filter_mode filter_mode) {
	if (scene_buffer->filter_mode == filter_mode) {
		return;
	}

	scene_buffer->filter_mode = filter_mode;
	wsm_scene_node_update(&scene_buffer->node, NULL);
}

void wsm_scene_buffer_send_frame_done(struct wsm_scene_buffer *scene_buffer,
		struct wlr_scene_frame_done_event *event) {
	if (!pixman_region32_empty(&scene_buffer->node.visible)) {
		wl_signal_emit_mutable(&scene_buffer->events.frame_done, event);
	}
}

bool wsm_scene_node_is_buffer(const struct wsm_scene_node *node) {
	return node != NULL && node->impl == &scene_node_impl;
}

struct wsm_scene_buffer *wsm_scene_buffer_from_node(struct wsm_scene_node *node) {
	assert(node->impl == &scene_node_impl);

	struct wsm_scene_buffer *scene_buffer =
		wl_container_of(node, scene_buffer, node);

	return scene_buffer;
}

void wsm_scene_buffer_set_transfer_function(struct wsm_scene_buffer *scene_buffer,
		enum wlr_color_transfer_function transfer_function) {
	if (scene_buffer->transfer_function == transfer_function) {
		return;
	}

	scene_buffer->transfer_function = transfer_function;
	wsm_scene_node_update(&scene_buffer->node, NULL);
}

void wsm_scene_buffer_set_primaries(struct wsm_scene_buffer *scene_buffer,
		enum wlr_color_named_primaries primaries) {
	if (scene_buffer->primaries == primaries) {
		return;
	}

	scene_buffer->primaries = primaries;
	wsm_scene_node_update(&scene_buffer->node, NULL);
}

void wsm_scene_buffer_set_color_encoding(struct wsm_scene_buffer *scene_buffer,
		enum wlr_color_encoding color_encoding) {
	if (scene_buffer->color_encoding == color_encoding) {
		return;
	}

	scene_buffer->color_encoding = color_encoding;
	wsm_scene_node_update(&scene_buffer->node, NULL);
}

void wsm_scene_buffer_set_color_range(struct wsm_scene_buffer *scene_buffer,
		enum wlr_color_range color_range) {
	if (scene_buffer->color_range == color_range) {
		return;
	}

	scene_buffer->color_range = color_range;
	wsm_scene_node_update(&scene_buffer->node, NULL);
}

static void scene_node_for_each_scene_buffer(struct wsm_scene_node *node,
		int lx, int ly, wsm_scene_buffer_iterator_func_t user_iterator,
		void *user_data) {
	if (!node->enabled) {
		return;
	}

	lx += node->x;
	ly += node->y;

	if (wsm_scene_node_is_buffer(node)) {
		struct wsm_scene_buffer *scene_buffer = wsm_scene_buffer_from_node(node);
		user_iterator(scene_buffer, lx, ly, user_data);
	} else if (wsm_scene_node_get_children(node) != NULL) {
		struct wl_list *childrens = wsm_scene_node_get_children(node);
		struct wsm_scene_node *child;
		wl_list_for_each(child, childrens, link) {
			scene_node_for_each_scene_buffer(child, lx, ly, user_iterator, user_data);
		}
	}
}

void wsm_scene_node_for_each_buffer(struct wsm_scene_node *node,
		wsm_scene_buffer_iterator_func_t user_iterator, void *user_data) {
	scene_node_for_each_scene_buffer(node, 0, 0, user_iterator, user_data);
}
