#include "node/wsm_scene_round_rect.h"
#include "node/wsm_scene_tree.h"
#include "scene/wsm_scene.h"
#include "scene/wsm_scene_output.h"
#include "pass/round_rect.h"

#include <sched.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <math.h>

#include <wayland-server-core.h>
#include <wayland-util.h>

#include <wlr/config.h>
#include <wlr/util/region.h>
#include <wlr/util/transform.h>
#include <wlr/render/interface.h>
#include <wlr/types/wlr_damage_ring.h>
#include <wlr/types/wlr_output.h>

static void scene_node_visibility(struct wsm_scene_node *node,
	pixman_region32_t *visible);
static void scene_node_get_size(struct wsm_scene_node *node,
	int *width, int *height);

static float clamp_radius(float radius, int width, int height) {
	if (!isfinite(radius) || radius <= 0) {
		return 0;
	}
	return fminf(radius, fminf(width, height) / 2.f);
}

static bool square_corners(const struct wsm_scene_round_rect *rect) {
	return rect->top_left_radius == 0 && rect->top_right_radius == 0 &&
		rect->bottom_left_radius == 0 && rect->bottom_right_radius == 0;
}

static void scene_node_destroy(struct wsm_scene_node *node) {
	struct wsm_scene_round_rect *round_rect = wsm_scene_round_rect_from_node(node);

	free(round_rect);
}

static bool scene_node_at_iterator(struct wsm_scene_node *node,
		int lx, int ly, void *data) {
	struct node_at_data *at_data = data;

	double rx = at_data->lx - lx;
	double ry = at_data->ly - ly;

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
	struct wsm_scene_round_rect *round_rect = wsm_scene_round_rect_from_node(node);
	*width = round_rect->width;
	*height = round_rect->height;
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

static void solid_region(pixman_region32_t *region, const struct wlr_box *box,
		const float radii[static 4]) {
	pixman_region32_union_rect(region, region, box->x, box->y, box->width, box->height);
	for (int corner = 0; corner < 4; corner++) {
		if (radii[corner] == 0) {
			continue;
		}
		/* Leave the corner and its antialiasing fringe to the rounded pass. */
		int extent = (int)ceilf(radii[corner]) + 1;
		int width = extent < box->width ? extent : box->width;
		int height = extent < box->height ? extent : box->height;
		pixman_region32_t cutout;
		pixman_region32_init_rect(&cutout,
			box->x + ((corner & 1) ? box->width - width : 0),
			box->y + ((corner & 2) ? box->height - height : 0), width, height);
		pixman_region32_subtract(region, region, &cutout);
		pixman_region32_fini(&cutout);
	}
}

static void scene_node_opaque_region(struct wsm_scene_node *node, int x, int y,
		pixman_region32_t *opaque) {
	struct wsm_scene_round_rect *rect = wsm_scene_round_rect_from_node(node);
	pixman_region32_clear(opaque);
	if (rect->color[3] != 1.f) {
		return;
	}
	const float radii[4] = { rect->top_left_radius, rect->top_right_radius,
		rect->bottom_left_radius, rect->bottom_right_radius };
	solid_region(opaque, &(struct wlr_box){x, y, rect->width, rect->height}, radii);
}

static void scale_region(pixman_region32_t *region, float scale, bool round_up) {
	wlr_region_scale(region, region, scale);

	if (round_up && floor(scale) != scale) {
		wlr_region_expand(region, region, 1);
	}
}

static void scene_node_visibility(struct wsm_scene_node *node,
		pixman_region32_t *visible) {
	if (!node->enabled) {
		return;
	}

	pixman_region32_union(visible, visible, &node->visible);
}

static bool scene_node_invisible(struct wsm_scene_node *node) {
	struct wsm_scene_round_rect *rect = wsm_scene_round_rect_from_node(node);

	return rect->color[3] == 0.f;
}

static bool construct_render_list_iterator(struct wsm_scene_node *node,
		int lx, int ly, void *_data) {
	struct wsm_render_list_constructor_data *data = _data;

	if (wsm_scene_node_invisible(node)) {
		return false;
	}

	// While rendering, the background should always be black. If we see a
	// black rect, we can ignore rendering everything under the rect, and
	// unless fractional scale is used even the rect itself (to avoid running
	// into issues regarding damage region expansion).
	if (data->calculate_visibility &&
			(!data->fractional_scale || data->render_list->size == 0)) {
		struct wsm_scene_round_rect *rect = wsm_scene_round_rect_from_node(node);
		float *black = (float[4]){ 0.f, 0.f, 0.f, 1.f };

		if (square_corners(rect) && memcmp(rect->color, black, sizeof(float) * 4) == 0) {
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

	struct wsm_scene_round_rect *round_rect = wsm_scene_round_rect_from_node(node);
	float source_radii[4] = {
		round_rect->top_left_radius, round_rect->top_right_radius,
		round_rect->bottom_left_radius, round_rect->bottom_right_radius,
	};
	float radii[4] = { 0 };
	for (int corner = 0; corner < 4; corner++) {
		struct wlr_box point = {
			.x = x + ((corner & 1) ? round_rect->width - 1 : 0),
			.y = y + ((corner & 2) ? round_rect->height - 1 : 0),
			.width = 1, .height = 1,
		};
		transform_output_box(&point, data);
		int transformed_corner = (point.x >= dst_box.x + dst_box.width / 2) |
			((point.y >= dst_box.y + dst_box.height / 2) << 1);
		radii[transformed_corner] = clamp_radius(source_radii[corner] * data->scale,
			dst_box.width, dst_box.height);
	}
	/* Draw the straight portions with full coverage, including on GLES.
	 * Only the corner rectangles need the rounded shader and blending. */
	pixman_region32_t solid;
	pixman_region32_init(&solid);
	solid_region(&solid, &dst_box, radii);
	pixman_region32_intersect(&solid, &solid, &render_region);
	wlr_render_pass_add_rect(data->render_pass, &(struct wlr_render_rect_options){
		.box = dst_box,
		.color = { round_rect->color[0], round_rect->color[1],
			round_rect->color[2], round_rect->color[3] },
		.clip = &solid,
	});
	pixman_region32_subtract(&render_region, &render_region, &solid);
	pixman_region32_fini(&solid);
	if (!pixman_region32_empty(&render_region)) {
		wsm_render_pass_add_round_rect(data->render_pass,
			&(struct wsm_render_round_rect_options){
				.box = dst_box,
				.color = {
					.r = round_rect->color[0], .g = round_rect->color[1],
					.b = round_rect->color[2], .a = round_rect->color[3],
				},
				.radii = { radii[0], radii[1], radii[2], radii[3] },
				.corner_smoothness = 4.f,
				.clip = &render_region,
			});
	}
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
	.set_enabled = NULL,
	.set_position = NULL,
	.bounds = NULL,
	.get_size = scene_node_get_size,
	.coords = NULL,
	.at = scene_node_at,
	.in_box = scene_nodes_in_box,
	.opaque_region = scene_node_opaque_region,
	.update_outputs = NULL,
	.update = NULL,
	.visibility = scene_node_visibility,
	.frame_done = NULL,
	.invisible = scene_node_invisible,
	.construct_render_list_iterator = construct_render_list_iterator,
	.render = scene_node_render,
	.get_extents = get_scene_node_extents,
	.get_children = NULL,
	.restack_xwayland_surface = NULL,
	.cleanup_when_disabled = scene_node_cleanup_when_disabled,
};

struct wsm_scene_round_rect *wsm_scene_round_rect_create(struct wsm_scene_tree *parent,
		int width, int height, float top_left_radius, float top_right_radius,
		float bottom_left_radius, float bottom_right_radius, const float color[static 4]) {
	assert(parent);
	assert(width >= 0 && height >= 0);

	struct wsm_scene_round_rect *round_rect = calloc(1, sizeof(*round_rect));
	if (round_rect == NULL) {
		return NULL;
	}
	wsm_scene_node_init(&round_rect->node, &scene_node_impl, parent);

	round_rect->width = width;
	round_rect->height = height;
	round_rect->top_left_radius = clamp_radius(top_left_radius, width, height);
	round_rect->top_right_radius = clamp_radius(top_right_radius, width, height);
	round_rect->bottom_left_radius = clamp_radius(bottom_left_radius, width, height);
	round_rect->bottom_right_radius = clamp_radius(bottom_right_radius, width, height);
	memcpy(round_rect->color, color, sizeof(round_rect->color));

	wsm_scene_node_update(&round_rect->node, NULL);

	return round_rect;
}

void wsm_scene_round_rect_set_size(struct wsm_scene_round_rect *rect, int width, int height) {
	if (rect->width == width && rect->height == height) {
		return;
	}

	assert(width >= 0 && height >= 0);

	rect->width = width;
	rect->height = height;
	rect->top_left_radius = clamp_radius(rect->top_left_radius, width, height);
	rect->top_right_radius = clamp_radius(rect->top_right_radius, width, height);
	rect->bottom_left_radius = clamp_radius(rect->bottom_left_radius, width, height);
	rect->bottom_right_radius = clamp_radius(rect->bottom_right_radius, width, height);
	wsm_scene_node_update(&rect->node, NULL);
}

void wsm_scene_round_rect_set_color(struct wsm_scene_round_rect *rect, const float color[static 4]) {
	if (memcmp(rect->color, color, sizeof(rect->color)) == 0) {
		return;
	}

	memcpy(rect->color, color, sizeof(rect->color));
	wsm_scene_node_update(&rect->node, NULL);
}

static bool update_corner_visibility(struct wsm_scene_node *node,
		int x, int y, void *user_data) {
	struct wsm_scene_update_data *data = user_data;
	int width, height;
	wsm_scene_node_get_size(node, &width, &height);
	pixman_region32_subtract(&node->visible, &node->visible, data->update_region);
	pixman_region32_union(&node->visible, &node->visible, data->visible);
	pixman_region32_intersect_rect(&node->visible, &node->visible, x, y, width, height);
	if (data->calculate_visibility) {
		pixman_region32_t opaque;
		pixman_region32_init(&opaque);
		wsm_scene_node_opaque_region(node, x, y, &opaque);
		pixman_region32_subtract(data->visible, data->visible, &opaque);
		pixman_region32_fini(&opaque);
	}
	wsm_scene_node_update_outputs(node, data->outputs, NULL, NULL);
	return false;
}

static void damage_corner(struct wsm_scene_round_rect *rect,
		float old_radius, float new_radius, bool right, bool bottom) {
	int lx, ly;
	if (rect->node.scene == NULL || !wsm_scene_node_coords(&rect->node, &lx, &ly)) {
		return;
	}

	/* Include both shapes and a pixel of coverage for antialiasing. */
	int extent = (int)ceilf(fmaxf(old_radius, new_radius)) + 2;
	int width = extent < rect->width ? extent : rect->width;
	int height = extent < rect->height ? extent : rect->height;
	pixman_region32_t logical_damage;
	pixman_region32_init_rect(&logical_damage,
		lx + (right ? rect->width - width : 0),
		ly + (bottom ? rect->height - height : 0), width, height);

	/* A radius change also exposes or occludes the nodes underneath it. */
	pixman_region32_t visible;
	pixman_region32_init(&visible);
	pixman_region32_copy(&visible, &logical_damage);
	struct wsm_scene_update_data update = {
		.visible = &visible,
		.update_region = &logical_damage,
		.outputs = &rect->node.scene->outputs,
		.calculate_visibility = rect->node.scene->calculate_visibility,
	};
	const pixman_box32_t *box = pixman_region32_extents(&logical_damage);
	struct wlr_box update_box = { box->x1, box->y1,
		box->x2 - box->x1, box->y2 - box->y1 };
	wsm_scene_node_nodes_in_box(&rect->node.scene->tree->node, &update_box,
		update_corner_visibility, &update);
	pixman_region32_fini(&visible);
	pixman_region32_intersect(&logical_damage, &logical_damage, &rect->node.visible);

	/* Convert this corner to each output's buffer coordinates. */
	struct wsm_scene_output *scene_output;
	wl_list_for_each(scene_output, &rect->node.scene->outputs, link) {
		struct wlr_output *output = scene_output->output;
		pixman_region32_t damage;
		pixman_region32_init(&damage);
		pixman_region32_copy(&damage, &logical_damage);
		pixman_region32_translate(&damage, -scene_output->x, -scene_output->y);
		scale_region(&damage, output->scale, true);
		int transformed_width, transformed_height;
		wlr_output_transformed_resolution(output, &transformed_width, &transformed_height);
		wlr_region_transform(&damage, &damage,
			wlr_output_transform_invert(output->transform),
			transformed_width, transformed_height);
		pixman_region32_intersect_rect(&damage, &damage,
			0, 0, output->width, output->height);
		if (!pixman_region32_empty(&damage)) {
			wlr_output_schedule_frame(output);
			wlr_damage_ring_add(&scene_output->damage_ring, &damage);
			pixman_region32_union(&scene_output->pending_commit_damage,
				&scene_output->pending_commit_damage, &damage);
		}
		pixman_region32_fini(&damage);
	}
	pixman_region32_fini(&logical_damage);
}

bool wsm_scene_round_rect_set_top_left_radius(struct wsm_scene_round_rect *rect, float radius) {
	radius = clamp_radius(radius, rect->width, rect->height);
	if (rect->top_left_radius == radius) {
		return false;
	}
	float old_radius = rect->top_left_radius;
	rect->top_left_radius = radius;
	damage_corner(rect, old_radius, radius, false, false);
	return true;
}

bool wsm_scene_round_rect_set_top_right_radius(struct wsm_scene_round_rect *rect, float radius) {
	radius = clamp_radius(radius, rect->width, rect->height);
	if (rect->top_right_radius == radius) {
		return false;
	}
	float old_radius = rect->top_right_radius;
	rect->top_right_radius = radius;
	damage_corner(rect, old_radius, radius, true, false);
	return true;
}

bool wsm_scene_round_rect_set_bottom_left_radius(struct wsm_scene_round_rect *rect, float radius) {
	radius = clamp_radius(radius, rect->width, rect->height);
	if (rect->bottom_left_radius == radius) {
		return false;
	}
	float old_radius = rect->bottom_left_radius;
	rect->bottom_left_radius = radius;
	damage_corner(rect, old_radius, radius, false, true);
	return true;
}

bool wsm_scene_round_rect_set_bottom_right_radius(struct wsm_scene_round_rect *rect, float radius) {
	radius = clamp_radius(radius, rect->width, rect->height);
	if (rect->bottom_right_radius == radius) {
		return false;
	}
	float old_radius = rect->bottom_right_radius;
	rect->bottom_right_radius = radius;
	damage_corner(rect, old_radius, radius, true, true);
	return true;
}

bool wsm_scene_node_is_round_rect(const struct wsm_scene_node *node) {
	return node != NULL && node->impl == &scene_node_impl;
}

struct wsm_scene_round_rect *wsm_scene_round_rect_from_node(struct wsm_scene_node *node) {
	assert(node->impl == &scene_node_impl);

	struct wsm_scene_round_rect *round_rect =
		wl_container_of(node, round_rect, node);

	return round_rect;
}
