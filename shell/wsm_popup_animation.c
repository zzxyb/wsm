#include "wsm_popup_animation.h"

#include "wsm_common.h"
#include "wsm_log.h"
#include "wsm_output.h"
#include "wsm_scene.h"
#include "wsm_scene_capture.h"
#include "wsm_server.h"

#include <math.h>
#include <stdlib.h>
#include <time.h>

#include <wayland-server-core.h>

#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/util/box.h>

#define POPUP_SPRING_DURATION_MSEC 3600
#define POPUP_SPRING_FRAME_MSEC 16

struct wsm_popup_animation {
	struct wlr_scene_tree *source_tree;
	struct wlr_scene_tree *tree;
	struct wlr_scene_buffer *snapshot;
	struct wl_event_source *timer;
	struct wl_listener source_tree_destroy;
	struct timespec started_at;
	int final_x;
	int final_y;
	int width;
	int height;
	int start_dx;
	int start_dy;
	float scale;
	uint32_t duration_msec;
};

static void schedule_animation_frames(void) {
	for (int i = 0; i < global_server.scene->outputs->length; ++i) {
		struct wsm_output *output = global_server.scene->outputs->items[i];
		if (output->enabled && output->wlr_output->enabled) {
			wlr_damage_ring_add_whole(&output->scene_output->damage_ring);
			wlr_output_schedule_frame(output->wlr_output);
		}
	}
}

static void animation_destroy(struct wsm_popup_animation *animation) {
	if (animation == NULL) {
		return;
	}
	if (animation->timer != NULL) {
		wl_event_source_remove(animation->timer);
	}
	if (!wl_list_empty(&animation->source_tree_destroy.link)) {
		wl_list_remove(&animation->source_tree_destroy.link);
	}
	if (animation->source_tree != NULL) {
		wlr_scene_node_set_enabled(&animation->source_tree->node, true);
	}
	if (animation->tree != NULL) {
		wlr_scene_node_destroy(&animation->tree->node);
	}
	free(animation);
}

static void handle_source_tree_destroy(struct wl_listener *listener, void *data) {
	struct wsm_popup_animation *animation =
		wl_container_of(listener, animation, source_tree_destroy);
	wl_list_remove(&animation->source_tree_destroy.link);
	wl_list_init(&animation->source_tree_destroy.link);
	animation->source_tree = NULL;
	animation_destroy(animation);
}

static double ease_out_cubic(double t) {
	if (t <= 0.0) {
		return 0.0;
	}
	if (t >= 1.0) {
		return 1.0;
	}
	double inv = 1.0 - t;
	return 1.0 - inv * inv * inv;
}

static void animation_apply(struct wsm_popup_animation *animation,
		double progress) {
	double p = ease_out_cubic(progress);
	int dx = lround(animation->start_dx * (1.0 - p));
	int dy = lround(animation->start_dy * (1.0 - p));

	struct wlr_box moved = {
		.x = animation->final_x + dx,
		.y = animation->final_y + dy,
		.width = animation->width,
		.height = animation->height,
	};
	struct wlr_box clip = {
		.x = animation->final_x,
		.y = animation->final_y,
		.width = animation->width,
		.height = animation->height,
	};
	struct wlr_box visible;
	if (!wlr_box_intersection(&visible, &moved, &clip)) {
		wlr_scene_node_set_enabled(&animation->snapshot->node, false);
		return;
	}

	wlr_scene_node_set_enabled(&animation->snapshot->node, true);
	wlr_scene_node_set_position(&animation->snapshot->node,
		visible.x - animation->final_x, visible.y - animation->final_y);
	wlr_scene_buffer_set_dest_size(animation->snapshot,
		visible.width, visible.height);

	struct wlr_fbox source_box = {
		.x = (visible.x - moved.x) * animation->scale,
		.y = (visible.y - moved.y) * animation->scale,
		.width = visible.width * animation->scale,
		.height = visible.height * animation->scale,
	};
	wlr_scene_buffer_set_source_box(animation->snapshot, &source_box);
}

static int handle_animation_timer(void *data) {
	struct wsm_popup_animation *animation = data;

	struct timespec now;
	struct timespec elapsed;
	clock_gettime(CLOCK_MONOTONIC, &now);
	timespec_sub(&elapsed, &now, &animation->started_at);

	double progress = (double)timespec_to_msec(&elapsed) /
		(double)animation->duration_msec;
	if (progress >= 1.0) {
		if (animation->source_tree != NULL) {
			wlr_scene_node_set_enabled(&animation->source_tree->node, true);
			animation->source_tree = NULL;
		}
		schedule_animation_frames();
		animation_destroy(animation);
		return 0;
	}

	animation_apply(animation, progress);
	schedule_animation_frames();
	wl_event_source_timer_update(animation->timer, POPUP_SPRING_FRAME_MSEC);
	return 0;
}

bool wsm_popup_animation_start(struct wlr_scene_tree *tree,
		const struct wsm_popup_animation_options *options) {
	if (tree == NULL || global_server.scene == NULL) {
		return false;
	}

	struct wsm_popup_animation_options opts = {
		.direction = WSM_POPUP_ANIMATION_FROM_TOP,
		.duration_msec = POPUP_SPRING_DURATION_MSEC,
		.scale = 1.0f,
	};
	if (options != NULL) {
		opts = *options;
	}
	if (opts.duration_msec == 0) {
		opts.duration_msec = POPUP_SPRING_DURATION_MSEC;
	}
	if (opts.scale <= 0) {
		opts.scale = 1.0f;
	}
	struct wsm_popup_animation *animation =
		calloc(1, sizeof(*animation));
	if (animation == NULL) {
		return false;
	}
	wl_list_init(&animation->source_tree_destroy.link);

	int lx = 0;
	int ly = 0;
	wlr_scene_node_coords(&tree->node, &lx, &ly);

	struct wsm_scene_capture capture;
	if (!wsm_scene_capture_tree(&capture, tree, global_server.wlr_renderer,
			global_server.wlr_allocator, opts.scale)) {
		wsm_log(WSM_DEBUG, "Could not capture popup for animation");
		free(animation);
		return false;
	}

	animation->source_tree = tree;
	animation->duration_msec = opts.duration_msec;
	animation->final_x = lx + capture.box.x;
	animation->final_y = ly + capture.box.y;
	animation->width = capture.box.width;
	animation->height = capture.box.height;
	animation->scale = capture.scale;
	animation->tree =
		wlr_scene_tree_create(global_server.scene->layers.animation);
	if (animation->tree == NULL) {
		wsm_scene_capture_finish(&capture);
		free(animation);
		return false;
	}
	wlr_scene_node_set_position(&animation->tree->node,
		animation->final_x, animation->final_y);
	animation->snapshot =
		wlr_scene_buffer_create(animation->tree, capture.buffer);
	wsm_scene_capture_finish(&capture);
	if (animation->snapshot == NULL) {
		animation_destroy(animation);
		return false;
	}

	switch (opts.direction) {
	case WSM_POPUP_ANIMATION_FROM_LEFT:
		if (opts.travel <= 0) {
			opts.travel = animation->width;
		}
		animation->start_dx = -opts.travel;
		break;
	case WSM_POPUP_ANIMATION_FROM_RIGHT:
		if (opts.travel <= 0) {
			opts.travel = animation->width;
		}
		animation->start_dx = opts.travel;
		break;
	case WSM_POPUP_ANIMATION_FROM_TOP:
		if (opts.travel <= 0) {
			opts.travel = animation->height;
		}
		animation->start_dy = -opts.travel;
		break;
	case WSM_POPUP_ANIMATION_FROM_BOTTOM:
		if (opts.travel <= 0) {
			opts.travel = animation->height;
		}
		animation->start_dy = opts.travel;
		break;
	}

	animation->timer = wl_event_loop_add_timer(global_server.wl_event_loop,
		handle_animation_timer, animation);
	if (animation->timer == NULL) {
		animation_destroy(animation);
		return false;
	}

	animation->source_tree_destroy.notify = handle_source_tree_destroy;
	wl_signal_add(&tree->node.events.destroy, &animation->source_tree_destroy);

	wlr_scene_node_set_enabled(&tree->node, false);
	clock_gettime(CLOCK_MONOTONIC, &animation->started_at);
	animation_apply(animation, 0.0);
	schedule_animation_frames();
	wl_event_source_timer_update(animation->timer, POPUP_SPRING_FRAME_MSEC);
	return true;
}
