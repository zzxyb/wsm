#include "wsm_window_animation.h"

#include "wsm_common.h"
#include "wsm_container.h"
#include "wsm_log.h"
#include "wsm_output.h"
#include "wsm_scene.h"
#include "wsm_scene_capture.h"
#include "wsm_server.h"
#include "wsm_view.h"

#include <math.h>
#include <pixman.h>
#include <stdlib.h>
#include <time.h>

#include <drm_fourcc.h>
#include <wayland-server-core.h>

#include <wlr/render/allocator.h>
#include <wlr/render/drm_format_set.h>
#include <wlr/render/pass.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/render/wlr_texture.h>
#include <wlr/types/wlr_buffer.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_scene.h>

#define CLOSE_DOOR_DURATION_MSEC 320
#define CLOSE_DOOR_FRAME_MSEC 16
#define MINIMIZE_DURATION_MSEC 320
#define GEOMETRY_DURATION_MSEC 320

struct wsm_window_animation {
	struct wlr_scene_tree *tree;
	struct wlr_scene_buffer *snapshot;
	struct wlr_scene_buffer *left;
	struct wlr_scene_buffer *right;
	struct wlr_buffer *left_buffer;
	struct wlr_buffer *right_buffer;
	struct wl_event_source *timer;
	struct timespec started_at;
	struct wlr_box box;
	struct wlr_box target_box;
	float scale;
	uint32_t duration_msec;
	enum wsm_window_animation_kind kind;
	struct wsm_view *view;
	struct wl_listener view_unmap;
};

static double ease_out_cubic(double t) {
	double inv = 1.0 - t;
	return 1.0 - inv * inv * inv;
}

static void schedule_animation_frames(void) {
	for (int i = 0; i < global_server.scene->outputs->length; ++i) {
		struct wsm_output *output = global_server.scene->outputs->items[i];
		if (output->enabled && output->wlr_output->enabled) {
			wlr_damage_ring_add_whole(&output->scene_output->damage_ring);
			wlr_output_schedule_frame(output->wlr_output);
		}
	}
}

static void animation_destroy(struct wsm_window_animation *animation) {
	if (animation == NULL) {
		return;
	}
	if (animation->timer) {
		wl_event_source_remove(animation->timer);
	}
	if (animation->tree) {
		wlr_scene_node_destroy(&animation->tree->node);
	}
	if (animation->left_buffer != NULL) {
		wlr_buffer_drop(animation->left_buffer);
	}
	if (animation->right_buffer != NULL) {
		wlr_buffer_drop(animation->right_buffer);
	}
	free(animation);
}

static void handle_animation_view_unmap(struct wl_listener *listener,
		void *data) {
	struct wsm_window_animation *animation =
		wl_container_of(listener, animation, view_unmap);
	if (animation->view != NULL) {
		animation->view->close_animation_pending = false;
		animation->view->open_animation_pending = false;
		animation->view->minimize_animation_pending = false;
		animation->view->maximize_animation_pending = false;
		animation->view = NULL;
	}
	wl_list_remove(&animation->view_unmap.link);
	wl_list_init(&animation->view_unmap.link);
}

static void animation_apply(struct wsm_window_animation *animation,
		double progress) {
	if (animation->kind == WSM_WINDOW_ANIMATION_MINIMIZE ||
			animation->kind == WSM_WINDOW_ANIMATION_RESTORE_MINIMIZE ||
			animation->kind == WSM_WINDOW_ANIMATION_GEOMETRY) {
		progress = ease_out_cubic(progress);
		if (animation->kind == WSM_WINDOW_ANIMATION_RESTORE_MINIMIZE ||
				animation->kind == WSM_WINDOW_ANIMATION_GEOMETRY) {
			progress = 1.0 - progress;
		}
		double inv = 1.0 - progress;
		struct wlr_box box = {
			.x = lround(animation->box.x * inv +
				animation->target_box.x * progress),
			.y = lround(animation->box.y * inv +
				animation->target_box.y * progress),
			.width = lround(animation->box.width * inv +
				animation->target_box.width * progress),
			.height = lround(animation->box.height * inv +
				animation->target_box.height * progress),
		};
		if (box.width < 1) {
			box.width = 1;
		}
		if (box.height < 1) {
			box.height = 1;
		}

		wlr_scene_node_set_position(&animation->snapshot->node,
			box.x - animation->box.x, box.y - animation->box.y);
		wlr_scene_buffer_set_dest_size(animation->snapshot,
			box.width, box.height);

		int lx = 0;
		int ly = 0;
		wlr_scene_node_coords(&animation->tree->node, &lx, &ly);
		pixman_region32_clear(&animation->snapshot->node.visible);
		pixman_region32_union_rect(&animation->snapshot->node.visible,
			&animation->snapshot->node.visible,
			lx + box.x - animation->box.x,
			ly + box.y - animation->box.y,
			box.width, box.height);
		return;
	}

	progress = ease_out_cubic(progress);
	if (animation->kind == WSM_WINDOW_ANIMATION_OPEN_DOOR) {
		progress = 1.0 - progress;
	}

	int width = animation->box.width;
	int height = animation->box.height;
	int left_width = width / 2;
	int right_width = width - left_width;
	int left_offset = lround(left_width * progress);
	int right_offset = lround(right_width * progress);

	wlr_scene_node_set_position(&animation->left->node, -left_offset, 0);
	wlr_scene_node_set_position(&animation->right->node,
		left_width + right_offset, 0);

	int lx = 0;
	int ly = 0;
	wlr_scene_node_coords(&animation->tree->node, &lx, &ly);

	struct wlr_box clip = {
		.x = lx,
		.y = ly,
		.width = width,
		.height = height,
	};
	struct wlr_box left = {
		.x = lx - left_offset,
		.y = ly,
		.width = left_width,
		.height = height,
	};
	struct wlr_box right = {
		.x = lx + left_width + right_offset,
		.y = ly,
		.width = right_width,
		.height = height,
	};
	wlr_box_intersection(&left, &left, &clip);
	wlr_box_intersection(&right, &right, &clip);

	pixman_region32_clear(&animation->left->node.visible);
	if (left.width > 0 && left.height > 0) {
		pixman_region32_union_rect(&animation->left->node.visible,
			&animation->left->node.visible,
			left.x, left.y, left.width, left.height);
	}
	pixman_region32_clear(&animation->right->node.visible);
	if (right.width > 0 && right.height > 0) {
		pixman_region32_union_rect(&animation->right->node.visible,
			&animation->right->node.visible,
			right.x, right.y, right.width, right.height);
	}
}

static int handle_animation_timer(void *data) {
	struct wsm_window_animation *animation = data;

	struct timespec now;
	struct timespec elapsed;
	clock_gettime(CLOCK_MONOTONIC, &now);
	timespec_sub(&elapsed, &now, &animation->started_at);

	double progress = (double)timespec_to_msec(&elapsed) /
		(double)animation->duration_msec;
	if (progress >= 1.0) {
		if (animation->view != NULL) {
			animation->view->close_animation_pending = false;
			animation->view->open_animation_pending = false;
			animation->view->minimize_animation_pending = false;
			animation->view->maximize_animation_pending = false;
			if (animation->kind != WSM_WINDOW_ANIMATION_MINIMIZE &&
					animation->view->container != NULL) {
				wlr_scene_node_set_enabled(
					&animation->view->container->scene_tree->node, true);
			}
			wl_list_remove(&animation->view_unmap.link);
			wl_list_init(&animation->view_unmap.link);
		}
		animation_destroy(animation);
		return 0;
	}

	animation_apply(animation, progress);
	schedule_animation_frames();
	wl_event_source_timer_update(animation->timer, CLOSE_DOOR_FRAME_MSEC);
	return 0;
}

static struct wlr_buffer *create_animation_buffer(int width, int height) {
	uint64_t modifier = DRM_FORMAT_MOD_INVALID;
	struct wlr_drm_format format = {
		.format = DRM_FORMAT_ARGB8888,
		.len = 1,
		.capacity = 1,
		.modifiers = &modifier,
	};
	return wlr_allocator_create_buffer(global_server.wlr_allocator,
		width, height, &format);
}

static bool render_animation_half(struct wlr_buffer *target,
		struct wlr_texture *source, struct wlr_fbox *source_box) {
	struct wlr_render_pass *pass = wlr_renderer_begin_buffer_pass(
		global_server.wlr_renderer, target, NULL);
	if (pass == NULL) {
		return false;
	}

	wlr_render_pass_add_rect(pass, &(struct wlr_render_rect_options) {
		.box = { .width = target->width, .height = target->height },
		.color = {0},
		.blend_mode = WLR_RENDER_BLEND_MODE_NONE,
	});
	wlr_render_pass_add_texture(pass, &(struct wlr_render_texture_options) {
		.texture = source,
		.src_box = *source_box,
		.dst_box = {
			.width = target->width,
			.height = target->height,
		},
	});

	return wlr_render_pass_submit(pass);
}

static bool create_door_buffers(struct wsm_window_animation *animation,
		struct wlr_buffer *buffer) {
	int width = animation->box.width;
	int height = animation->box.height;
	int left_width = width / 2;
	int right_width = width - left_width;
	int left_buffer_width = lround(left_width * animation->scale);
	int right_buffer_width = buffer->width - left_buffer_width;
	int buffer_height = buffer->height;

	if (left_buffer_width <= 0 || right_buffer_width <= 0 ||
			buffer_height <= 0) {
		return false;
	}

	struct wlr_texture *source =
		wlr_texture_from_buffer(global_server.wlr_renderer, buffer);
	if (source == NULL) {
		return false;
	}

	animation->left_buffer =
		create_animation_buffer(left_buffer_width, buffer_height);
	animation->right_buffer =
		create_animation_buffer(right_buffer_width, buffer_height);
	if (animation->left_buffer == NULL || animation->right_buffer == NULL) {
		wlr_texture_destroy(source);
		return false;
	}

	struct wlr_fbox left_source = {
		.x = 0,
		.y = 0,
		.width = left_buffer_width,
		.height = buffer_height,
	};
	struct wlr_fbox right_source = {
		.x = left_buffer_width,
		.y = 0,
		.width = right_buffer_width,
		.height = buffer_height,
	};
	bool ok = render_animation_half(animation->left_buffer,
		source, &left_source) &&
		render_animation_half(animation->right_buffer, source, &right_source);
	wlr_texture_destroy(source);
	if (!ok) {
		return false;
	}

	animation->left = wlr_scene_buffer_create(animation->tree,
		animation->left_buffer);
	animation->right = wlr_scene_buffer_create(animation->tree,
		animation->right_buffer);
	if (animation->left == NULL || animation->right == NULL) {
		return false;
	}

	wlr_scene_node_set_position(&animation->right->node, left_width, 0);
	wlr_scene_buffer_set_dest_size(animation->left, left_width, height);
	wlr_scene_buffer_set_dest_size(animation->right, right_width, height);
	return true;
}

static bool window_animation_start(struct wsm_container *container,
		const struct wsm_window_animation_options *options,
		enum wsm_window_animation_kind kind) {
	if (container == NULL || container->scene_tree == NULL ||
			global_server.scene == NULL ||
			global_server.scene->layers.animation == NULL) {
		return false;
	}

	struct wsm_window_animation_options opts = {
		.kind = kind,
		.duration_msec = (kind == WSM_WINDOW_ANIMATION_MINIMIZE ||
			kind == WSM_WINDOW_ANIMATION_RESTORE_MINIMIZE) ?
			MINIMIZE_DURATION_MSEC :
			(kind == WSM_WINDOW_ANIMATION_GEOMETRY ?
				GEOMETRY_DURATION_MSEC : CLOSE_DOOR_DURATION_MSEC),
		.scale = 1.0f,
		.target_box = { .x = 0, .y = 0, .width = 100, .height = 100 },
	};
	if (options != NULL) {
		opts = *options;
	}
	if (opts.kind != WSM_WINDOW_ANIMATION_CLOSE_DOOR &&
			opts.kind != WSM_WINDOW_ANIMATION_OPEN_DOOR &&
			opts.kind != WSM_WINDOW_ANIMATION_MINIMIZE &&
			opts.kind != WSM_WINDOW_ANIMATION_RESTORE_MINIMIZE &&
			opts.kind != WSM_WINDOW_ANIMATION_GEOMETRY) {
		return false;
	}
	if (opts.duration_msec == 0) {
		opts.duration_msec = (opts.kind == WSM_WINDOW_ANIMATION_MINIMIZE ||
			opts.kind == WSM_WINDOW_ANIMATION_RESTORE_MINIMIZE) ?
			MINIMIZE_DURATION_MSEC :
			(opts.kind == WSM_WINDOW_ANIMATION_GEOMETRY ?
				GEOMETRY_DURATION_MSEC : CLOSE_DOOR_DURATION_MSEC);
	}
	if (opts.scale <= 0) {
		opts.scale = 1.0f;
	}

	struct wsm_scene_capture capture;
	if (!wsm_scene_capture_tree(&capture, container->scene_tree,
			global_server.wlr_renderer, global_server.wlr_allocator,
			opts.scale)) {
		wsm_log(WSM_DEBUG, "Could not capture window for close animation");
		return false;
	}

	int lx = 0;
	int ly = 0;
	wlr_scene_node_coords(&container->scene_tree->node, &lx, &ly);

	struct wsm_window_animation *animation =
		calloc(1, sizeof(struct wsm_window_animation));
	if (animation == NULL) {
		wsm_scene_capture_finish(&capture);
		return false;
	}
	animation->box = (struct wlr_box) {
		.x = lx + capture.box.x,
		.y = ly + capture.box.y,
		.width = capture.box.width,
		.height = capture.box.height,
	};
	animation->target_box = opts.target_box;
	animation->scale = capture.scale;
	animation->duration_msec = opts.duration_msec;
	animation->kind = opts.kind;
	animation->view = container->view;
	wl_list_init(&animation->view_unmap.link);
	animation->tree = wlr_scene_tree_create(global_server.scene->layers.animation);
	if (animation->tree == NULL) {
		wsm_log(WSM_DEBUG, "Could not allocate close animation scene tree");
		free(animation);
		wsm_scene_capture_finish(&capture);
		return false;
	}

	wlr_scene_node_set_position(&animation->tree->node,
		animation->box.x, animation->box.y);
	if (animation->kind == WSM_WINDOW_ANIMATION_MINIMIZE ||
			animation->kind == WSM_WINDOW_ANIMATION_RESTORE_MINIMIZE ||
			animation->kind == WSM_WINDOW_ANIMATION_GEOMETRY) {
		animation->snapshot =
			wlr_scene_buffer_create(animation->tree, capture.buffer);
		if (animation->snapshot == NULL) {
			wsm_log(WSM_DEBUG,
				"Could not allocate minimize animation buffer");
			animation_destroy(animation);
			wsm_scene_capture_finish(&capture);
			return false;
		}
		wlr_scene_buffer_set_dest_size(animation->snapshot,
			capture.box.width, capture.box.height);
	} else if (!create_door_buffers(animation, capture.buffer)) {
		wsm_log(WSM_DEBUG, "Could not allocate close animation buffers");
		animation_destroy(animation);
		wsm_scene_capture_finish(&capture);
		return false;
	}
	wsm_scene_capture_finish(&capture);

	clock_gettime(CLOCK_MONOTONIC, &animation->started_at);
	animation->timer = wl_event_loop_add_timer(global_server.wl_event_loop,
		handle_animation_timer, animation);
	if (animation->timer == NULL) {
		wsm_log(WSM_DEBUG, "Could not allocate close animation timer");
		animation_destroy(animation);
		return false;
	}
	if (animation->view != NULL) {
		animation->view_unmap.notify = handle_animation_view_unmap;
		wl_signal_add(&animation->view->events.unmap, &animation->view_unmap);
		if (animation->kind == WSM_WINDOW_ANIMATION_CLOSE_DOOR) {
			animation->view->close_animation_pending = true;
		} else if (animation->kind == WSM_WINDOW_ANIMATION_OPEN_DOOR) {
			animation->view->open_animation_pending = true;
		} else if (animation->kind == WSM_WINDOW_ANIMATION_MINIMIZE ||
				animation->kind == WSM_WINDOW_ANIMATION_RESTORE_MINIMIZE) {
			animation->view->minimize_animation_pending = true;
		} else if (animation->kind == WSM_WINDOW_ANIMATION_GEOMETRY) {
			animation->view->maximize_animation_pending = true;
		}
	}
	wlr_scene_node_set_enabled(&container->scene_tree->node, false);
	animation_apply(animation, 0.0);
	schedule_animation_frames();
	wl_event_source_timer_update(animation->timer, CLOSE_DOOR_FRAME_MSEC);
	return true;
}

bool wsm_window_animation_start_close(struct wsm_container *container,
		const struct wsm_window_animation_options *options) {
	return window_animation_start(container, options,
		WSM_WINDOW_ANIMATION_CLOSE_DOOR);
}

bool wsm_window_animation_start_open(struct wsm_container *container,
		const struct wsm_window_animation_options *options) {
	return window_animation_start(container, options,
		WSM_WINDOW_ANIMATION_OPEN_DOOR);
}

bool wsm_window_animation_start_minimize(struct wsm_container *container,
		const struct wsm_window_animation_options *options) {
	return window_animation_start(container, options,
		WSM_WINDOW_ANIMATION_MINIMIZE);
}

bool wsm_window_animation_start_restore_minimize(struct wsm_container *container,
		const struct wsm_window_animation_options *options) {
	return window_animation_start(container, options,
		WSM_WINDOW_ANIMATION_RESTORE_MINIMIZE);
}

bool wsm_window_animation_start_geometry(struct wsm_container *container,
		const struct wsm_window_animation_options *options) {
	return window_animation_start(container, options,
		WSM_WINDOW_ANIMATION_GEOMETRY);
}
