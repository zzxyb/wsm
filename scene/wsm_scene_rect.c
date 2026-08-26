#include "scene/wsm_scene_rect.h"
#include "scene/wsm_scene.h"

#include <sched.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include <wayland-server-core.h>
#include <wayland-util.h>

#include <wlr/config.h>
#include <wlr/util/region.h>
#include <wlr/util/transform.h>
#include <wlr/render/interface.h>

static void scene_node_visibility(struct wsm_scene_node *node,
	pixman_region32_t *visible);
static void scene_node_get_size(struct wsm_scene_node *node,
	int *width, int *height);

static void scene_node_destroy(struct wsm_scene_node *node) {
	struct wsm_scene_rect *scene_rect = wsm_scene_rect_from_node(node);

	free(scene_rect);
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
	struct wsm_scene_rect *scene_rect = wsm_scene_rect_from_node(node);
	*width = scene_rect->width;
	*height = scene_rect->height;
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

	struct wsm_scene_rect *scene_rect = wsm_scene_rect_from_node(node);
	if (scene_rect->color[3] != 1) {
		return;
	}

	pixman_region32_fini(opaque);
	pixman_region32_init_rect(opaque, x, y, width, height);
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
	struct wsm_scene_rect *rect = wsm_scene_rect_from_node(node);

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
		struct wsm_scene_rect *rect = wsm_scene_rect_from_node(node);
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

	pixman_region32_t opaque;
	pixman_region32_init(&opaque);
	scene_node_opaque_region(node, x, y, &opaque);
	logical_to_buffer_coords(&opaque, data, false);
	pixman_region32_subtract(&opaque, &render_region, &opaque);

	struct wsm_scene_rect *scene_rect = wsm_scene_rect_from_node(node);

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

	pixman_region32_fini(&opaque);
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

struct wsm_scene_rect *wsm_scene_rect_create(struct wsm_scene_tree *parent,
		int width, int height, const float color[static 4]) {
	assert(parent);
	assert(width >= 0 && height >= 0);

	struct wsm_scene_rect *scene_rect = calloc(1, sizeof(*scene_rect));
	if (scene_rect == NULL) {
		return NULL;
	}
	wsm_scene_node_init(&scene_rect->node, &scene_node_impl, parent);

	scene_rect->width = width;
	scene_rect->height = height;
	memcpy(scene_rect->color, color, sizeof(scene_rect->color));

	wsm_scene_node_update(&scene_rect->node, NULL);

	return scene_rect;
}

void wsm_scene_rect_set_size(struct wsm_scene_rect *rect, int width, int height) {
	if (rect->width == width && rect->height == height) {
		return;
	}

	assert(width >= 0 && height >= 0);

	rect->width = width;
	rect->height = height;
	wsm_scene_node_update(&rect->node, NULL);
}

void wsm_scene_rect_set_color(struct wsm_scene_rect *rect, const float color[static 4]) {
	if (memcmp(rect->color, color, sizeof(rect->color)) == 0) {
		return;
	}

	memcpy(rect->color, color, sizeof(rect->color));
	wsm_scene_node_update(&rect->node, NULL);
}

bool wsm_scene_node_is_rect(const struct wsm_scene_node *node) {
	return node != NULL && node->impl == &scene_node_impl;
}

struct wsm_scene_rect *wsm_scene_rect_from_node(struct wsm_scene_node *node) {
	assert(node->impl == &scene_node_impl);

	struct wsm_scene_rect *scene_rect =
		wl_container_of(node, scene_rect, node);

	return scene_rect;
}
