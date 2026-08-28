#include "node/wsm_scene_node.h"
#include "node/wsm_scene_tree.h"
#include "scene/wsm_scene.h"
#include "scene/wsm_scene_output.h"

#include <sched.h>
#include <stdlib.h>
#include <assert.h>

#include <wayland-server-core.h>
#include <wayland-util.h>

#include <wlr/config.h>
#include <wlr/types/wlr_output.h>
#include <wlr/util/region.h>
#include <wlr/util/transform.h>

void wsm_scene_node_init(struct wsm_scene_node *node,
		const struct wsm_scene_node_impl *impl,
		struct wsm_scene_tree *parent) {
	assert(impl->destroy);
	*node = (struct wsm_scene_node){
		.impl = impl,
		.parent = parent,
		.enabled = true,
	};

	wl_list_init(&node->link);

	wl_signal_init(&node->events.destroy);
	pixman_region32_init(&node->visible);

	if (parent != NULL) {
		wl_list_insert(parent->children.prev, &node->link);
		node->scene = parent->node.scene;
	}

	wlr_addon_set_init(&node->addons);
}

void wsm_scene_node_destroy(struct wsm_scene_node *node) {
	if (node == NULL) {
		return;
	}

	wl_signal_emit_mutable(&node->events.destroy, NULL);
	assert(wl_list_empty(&node->events.destroy.listener_list));
	wlr_addon_set_finish(&node->addons);

	wsm_scene_node_set_enabled(node, false);
	wl_list_remove(&node->link);
	pixman_region32_fini(&node->visible);
	if (node->impl->destroy != NULL) {
		node->impl->destroy(node);
	}
}

void wsm_scene_node_set_enabled(struct wsm_scene_node *node, bool enabled) {
	if (node->impl->set_enabled == NULL) {
		if (node->enabled == enabled) {
			return;
		}

		int x, y;
		pixman_region32_t visible;
		pixman_region32_init(&visible);
		if (wsm_scene_node_coords(node, &x, &y)) {
			wsm_scene_node_visibility(node, &visible);
		}

		node->enabled = enabled;
		wsm_scene_node_update(node, &visible);
		return;
	}

	node->impl->set_enabled(node, enabled);
}

void wsm_scene_node_set_position(struct wsm_scene_node *node, int x, int y) {
	if (node->impl->set_position == NULL) {
		if (node->x == x && node->y == y) {
			return;
		}
	
		node->x = x;
		node->y = y;
		wsm_scene_node_update(node, NULL);
		return;
	}

	node->impl->set_position(node, x, y);
}

void wsm_scene_node_place_above(struct wsm_scene_node *node,
		struct wsm_scene_node *sibling) {
	assert(node != sibling);
	assert(node->parent == sibling->parent);

	if (node->link.prev == &sibling->link) {
		return;
	}

	wl_list_remove(&node->link);
	wl_list_insert(&sibling->link, &node->link);
	wsm_scene_node_update(node, NULL);
}

void wsm_scene_node_place_below(struct wsm_scene_node *node,
		struct wsm_scene_node *sibling) {
	assert(node != sibling);
	assert(node->parent == sibling->parent);

	if (node->link.next == &sibling->link) {
		return;
	}

	wl_list_remove(&node->link);
	wl_list_insert(sibling->link.prev, &node->link);
	wsm_scene_node_update(node, NULL);
}

void wsm_scene_node_raise_to_top(struct wsm_scene_node *node) {
	struct wsm_scene_node *current_top = wl_container_of(
		node->parent->children.prev, current_top, link);
	if (node == current_top) {
		return;
	}
	wsm_scene_node_place_above(node, current_top);
}

void wsm_scene_node_lower_to_bottom(struct wsm_scene_node *node) {
	struct wsm_scene_node *current_bottom = wl_container_of(
		node->parent->children.next, current_bottom, link);
	if (node == current_bottom) {
		return;
	}
	wsm_scene_node_place_below(node, current_bottom);
}

void wsm_scene_node_reparent(struct wsm_scene_node *node,
		struct wsm_scene_tree *new_parent) {
	assert(new_parent != NULL);

	if (node->parent == new_parent) {
		return;
	}

	/* Ensure that a node cannot become its own ancestor */
	for (struct wsm_scene_tree *ancestor = new_parent; ancestor != NULL;
			ancestor = ancestor->node.parent) {
		assert(&ancestor->node != node);
	}

	int x, y;
	pixman_region32_t visible;
	pixman_region32_init(&visible);
	if (wsm_scene_node_coords(node, &x, &y)) {
		wsm_scene_node_visibility(node, &visible);
	}

	wl_list_remove(&node->link);
	node->parent = new_parent;
	wl_list_insert(new_parent->children.prev, &node->link);
	wsm_scene_node_update(node, &visible);
}

void wsm_scene_node_get_size(struct wsm_scene_node *node,
		int *width, int *height) {
	if (node->impl->get_size == NULL) {
		*width = 0;
		*height = 0;
		return;
	}

	node->impl->get_size(node, width, height);
}

void wsm_scene_node_bounds(struct wsm_scene_node *node,
		int x, int y, pixman_region32_t *visible) {
	if (node->impl->bounds == NULL) {
		if (!node->enabled) {
			return;
		}

		int width, height;
		wsm_scene_node_get_size(node, &width, &height);
		pixman_region32_union_rect(visible, visible, x, y, width, height);
		return;
	}

	node->impl->bounds(node, x, y, visible);
}

bool wsm_scene_node_coords(struct wsm_scene_node *node, int *lx_ptr, int *ly_ptr) {
	if (node->impl->coords == NULL) {
		int lx = 0, ly = 0;
		bool enabled = true;
		while (true) {
			lx += node->x;
			ly += node->y;
			enabled = enabled && node->enabled;
			if (node->parent == NULL) {
				break;
			}
	
			node = &node->parent->node;
		}

		*lx_ptr = lx;
		*ly_ptr = ly;
		return enabled;
	}

	return node->impl->coords(node, lx_ptr, ly_ptr);
}

struct wsm_scene_node *wsm_scene_node_at(struct wsm_scene_node *node,
		double lx, double ly, double *nx, double *ny) {

	if (node->impl->at == NULL) {
		return NULL;
	}

	return node->impl->at(node, lx, ly, nx, ny);
}

bool wsm_scene_node_nodes_in_box(struct wsm_scene_node *node, struct wlr_box *box,
		scene_node_box_iterator_func_t iterator, void *user_data) {
	if (node->impl->in_box == NULL) {
		return false;
	}

	return node->impl->in_box(node, box, iterator, user_data);
}

void wsm_scene_node_opaque_region(struct wsm_scene_node *node, int x, int y,
		pixman_region32_t *opaque) {
	if (node->impl->opaque_region == NULL) {
		int width, height;
		wsm_scene_node_get_size(node, &width, &height);
		pixman_region32_fini(opaque);
		pixman_region32_init_rect(opaque, x, y, width, height);
		return;
	}

	return node->impl->opaque_region(node, x, y, opaque);
}

void wsm_scene_node_update_outputs(struct wsm_scene_node *node,
		struct wl_list *outputs, struct wsm_scene_output *ignore,
		struct wsm_scene_output *force) {

	if (node->impl->update_outputs == NULL) {
		return;
	}

	return node->impl->update_outputs(node, outputs, ignore, force);
}

static void scale_region(pixman_region32_t *region, float scale, bool round_up) {
	wlr_region_scale(region, region, scale);

	if (round_up && floor(scale) != scale) {
		wlr_region_expand(region, region, 1);
	}
}

static bool scene_node_update_iterator(struct wsm_scene_node *node,
		int lx, int ly, void *_data) {
	struct wsm_scene_update_data *data = _data;

	struct wlr_box box = { .x = lx, .y = ly };
	wsm_scene_node_get_size(node, &box.width, &box.height);

	pixman_region32_subtract(&node->visible, &node->visible, data->update_region);
	pixman_region32_union(&node->visible, &node->visible, data->visible);
	pixman_region32_intersect_rect(&node->visible, &node->visible,
		lx, ly, box.width, box.height);

	if (data->calculate_visibility) {
		pixman_region32_t opaque;
		pixman_region32_init(&opaque);
		wsm_scene_node_opaque_region(node, lx, ly, &opaque);
		pixman_region32_subtract(data->visible, data->visible, &opaque);
		pixman_region32_fini(&opaque);
	}

	wsm_scene_node_update_outputs(node, data->outputs, NULL, NULL);
#if WLR_HAS_XWAYLAND
	if (data->restack_xwayland_surfaces) {
		wsm_scene_node_restack_xwayland_surface(node, &box, data);
	}
#endif

	return false;
}

static void scene_update_region(struct wsm_scene *scene,
		const pixman_region32_t *update_region) {
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
		.restack_xwayland_surfaces = scene->restack_xwayland_surfaces,
	};

	// update node visibility and output enter/leave events
	wsm_scene_node_nodes_in_box(&scene->tree->node, &data.update_box, scene_node_update_iterator, &data);

	pixman_region32_fini(&visible);
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

static void output_to_buffer_coords(pixman_region32_t *damage, struct wlr_output *output) {
	int width, height;
	wlr_output_transformed_resolution(output, &width, &height);

	wlr_region_transform(damage, damage,
		wlr_output_transform_invert(output->transform), width, height);
}

static void scene_damage_outputs(struct wsm_scene *scene, const pixman_region32_t *damage) {
	if (pixman_region32_empty(damage)) {
		return;
	}

	struct wsm_scene_output *scene_output;
	wl_list_for_each(scene_output, &scene->outputs, link) {
		pixman_region32_t output_damage;
		pixman_region32_init(&output_damage);
		pixman_region32_copy(&output_damage, damage);
		pixman_region32_translate(&output_damage,
			-scene_output->x, -scene_output->y);
		scale_region(&output_damage, scene_output->output->scale, true);
		output_to_buffer_coords(&output_damage, scene_output->output);
		scene_output_damage(scene_output, &output_damage);
		pixman_region32_fini(&output_damage);
	}
}

void wsm_scene_node_update(struct wsm_scene_node *node,
		pixman_region32_t *damage) {
	if (node->impl->update == NULL) {
		struct wsm_scene *scene = node->scene;
	
		int x, y;
		if (!wsm_scene_node_coords(node, &x, &y)) {
			// We assume explicit damage on a disabled tree means the node was just
			// disabled.
			if (damage) {
				wsm_scene_node_cleanup_when_disabled(node, scene->restack_xwayland_surfaces, &scene->outputs);
	
				scene_update_region(scene, damage);
				scene_damage_outputs(scene, damage);
				pixman_region32_fini(damage);
			}
	
			return;
		}

		pixman_region32_t visible;
		if (!damage) {
			pixman_region32_init(&visible);
			wsm_scene_node_visibility(node, &visible);
			damage = &visible;
		}
	
		pixman_region32_t update_region;
		pixman_region32_init(&update_region);
		pixman_region32_copy(&update_region, damage);
		wsm_scene_node_bounds(node, x, y, &update_region);

		scene_update_region(scene, &update_region);
		pixman_region32_fini(&update_region);
	
		wsm_scene_node_visibility(node, damage);
		scene_damage_outputs(scene, damage);
		pixman_region32_fini(damage);
		return;
	}

	node->impl->update(node, damage);
}

void wsm_scene_node_visibility(struct wsm_scene_node *node,
		pixman_region32_t *visible) {
	if (node->impl->visibility == NULL) {
		return;
	}

	node->impl->visibility(node, visible);
}

void wsm_scene_node_send_frame_done(struct wsm_scene_node *node,
		struct wsm_scene_output *scene_output, struct timespec *now) {
	if (!node->enabled) {
		return;
	}

	if (node->impl->frame_done == NULL) {
		return;
	}

	node->impl->frame_done(node, scene_output, now);
}

bool wsm_scene_node_invisible(struct wsm_scene_node *node) {
	if (node->impl->invisible == NULL) {
		return false;
	}

	return node->impl->invisible(node);
}

bool wsm_scene_node_construct_render_list_iterator(struct wsm_scene_node *node,
		int lx, int ly, void *_data) {
	if (node->impl->construct_render_list_iterator == NULL) {
		return false;
	}

	return node->impl->construct_render_list_iterator(node, lx, ly, _data);
}

void wsm_scene_node_render(struct wsm_render_list_entry *entry, const struct wsm_render_data *data) {
	if (entry->node->impl->render == NULL) {
		return;
	}

	entry->node->impl->render(entry, data);
}

void wsm_scene_node_dmabuf_feedback(struct wsm_render_list_entry *entry,
		struct wsm_scene_output *scene_output) {
	if (entry->node->impl->dmabuf_feedback == NULL) {
		return;
	}

	entry->node->impl->dmabuf_feedback(entry, scene_output);
}

void wsm_scene_node_get_extents(struct wsm_scene_node *node, int lx, int ly,
		int *x_min, int *y_min, int *x_max, int *y_max) {
	if (node->impl->get_extents == NULL) {
		return;
	}

	node->impl->get_extents(node, lx, ly, x_min, y_min, x_max, y_max);
}

struct wl_list *wsm_scene_node_get_children(struct wsm_scene_node *node) {
	if (node->impl->get_children == NULL) {
		return NULL;
	}

	return node->impl->get_children(node);
}

void wsm_scene_node_restack_xwayland_surface(struct wsm_scene_node *node,
		struct wlr_box *box, struct wsm_scene_update_data *data) {
	if (node->impl->restack_xwayland_surface == NULL) {
		return;
	}

	node->impl->restack_xwayland_surface(node, box, data);
}

void wsm_scene_node_cleanup_when_disabled(struct wsm_scene_node *node,
		bool xwayland_restack, struct wl_list *outputs) {
	if (node->impl->cleanup_when_disabled == NULL) {
		return;
	}

	node->impl->cleanup_when_disabled(node, xwayland_restack, outputs);
}
