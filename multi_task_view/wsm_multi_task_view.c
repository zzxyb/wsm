#include "wsm_multi_task_view.h"
#include "wsm_titlebar_capture.h"
#include "wsm_window_preview.h"
#include "wsm_workspace_capture.h"
#include "../config.h"

#include "wsm_container.h"
#include "wsm_cursor.h"
#include "wsm_desktop.h"
#include "wsm_list.h"
#include "wsm_log.h"
#include "wsm_output.h"
#include "wsm_arrange.h"
#include "wsm_scene.h"
#include "wsm_seat.h"
#include "wsm_server.h"
#include "wsm_titlebar.h"
#include "wsm_transaction.h"
#include "wsm_view.h"
#include "wsm_workspace.h"
#include "node/wsm_image_node.h"
#include "node/wsm_button_node.h"
#include "node/wsm_node_descriptor.h"
#include "node/wsm_text_node.h"

#include <math.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <libinput.h>
#include <linux/input-event-codes.h>
#include <wayland-server-protocol.h>
#include <wlr/backend/libinput.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_damage_ring.h>
#include <wlr/types/wlr_keyboard.h>
#include <wlr/types/wlr_pointer.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/util/box.h>
#include <wlr/util/transform.h>
#include <xkbcommon/xkbcommon-keysyms.h>

#define OVERVIEW_PADDING 42
#define SPACE_GAP 16
#define SPACE_HEIGHT 118
#define SPACE_MAX_WIDTH 204
#define SPACE_CLOSE_HITBOX_SIZE 28
#define SPACE_CLOSE_ICON_SIZE 18
#define SPACE_ADD_BUTTON_SIZE 56
#define SPACE_ADD_GLYPH_SIZE 18
#define SPACE_ADD_GLYPH_THICKNESS 2
#define SPACE_LABEL_GAP 8
#define WINDOW_GAP 28
#define WINDOW_TOP 202
#define SWIPE_DISTANCE 320.0
#define SWIPE_DIRECTION_LOCK_DISTANCE 12.0
#define SWIPE_DIRECTION_DOMINANCE 1.2
#define SWIPE_PROJECTION_MSEC 160.0
#define HORIZONTAL_SWIPE_THRESHOLD 90.0
#define HORIZONTAL_SWIPE_DISTANCE (HORIZONTAL_SWIPE_THRESHOLD * 2.0)
#define SETTLE_MIN_DURATION_MSEC 100.0
#define SETTLE_MAX_DURATION_MSEC 240.0
#define SPACE_SWIPE_GAP_RATIO 0.055
#define SPACE_SWIPE_GAP_MIN 64
#define SPACE_SWIPE_GAP_MAX 160
#define SPACE_SWIPE_PHYSICAL_FACTOR 1.0
#define SPACE_SWIPE_FINGER_DISTANCE_MIN 300.0
#define SPACE_SWIPE_FINGER_DISTANCE_MAX 520.0
#define SPACE_SWIPE_COMPLETE_THRESHOLD 0.42
#define SPACE_SWIPE_EDGE_MAX_PROGRESS 0.12
#define SPACE_SWIPE_EDGE_RESISTANCE 0.18
#define WINDOW_BORDER_WIDTH 3
#define WINDOW_TITLE_HORIZONTAL_PADDING 10
#define WINDOW_TITLE_VERTICAL_PADDING 4
#define WINDOW_TITLE_CONTENT_OVERLAP 1
#define WINDOW_DRAG_THRESHOLD 8.0

struct wsm_multi_task_buffer {
	struct wlr_scene_buffer *buffer;
	struct wlr_scene_buffer *source;
	struct wsm_container *container;
	double from_x;
	double from_y;
	double from_width;
	double from_height;
	double to_x;
	double to_y;
	double to_width;
	double to_height;
	double crop_x;
	double crop_y;
	double crop_width;
	double crop_height;
	bool cropped;
	bool workspace_preview;
	bool source_synced;
	bool geometry_synced;
	bool snapshot;
};

struct wsm_multi_task_rect {
	struct wlr_scene_rect *rect;
	struct wlr_scene_rect *source;
	struct wsm_container *container;
	double from_x, from_y, from_width, from_height;
	double to_x, to_y, to_width, to_height;
	float color[4];
	bool workspace_preview;
	bool geometry_synced;
};

struct wsm_multi_task_window {
	struct wsm_multi_task_layout *layout;
	struct wsm_container *container;
	struct wl_listener destroy;
	struct wlr_scene_rect *backdrop;
	struct wlr_scene_rect *border;
	struct wlr_scene_rect *title_background;
	struct wsm_text_node *title_label;
	struct wsm_window_preview *preview;
	struct wlr_box hitbox;
	double from_x;
	double from_y;
	double from_width;
	double from_height;
	double to_x;
	double to_y;
	double to_width;
	double to_height;
};

struct wsm_multi_task_source {
	struct wlr_scene_node *node;
	bool enabled;
};

struct wsm_multi_task_workspace_preview {
	struct wsm_workspace *workspace;
	struct wsm_workspace_capture *capture;
	struct wlr_scene_tree *capture_tree;
	struct wlr_scene_rect *drop_border;
	struct wlr_box box;
	struct wlr_box label_hitbox;
	struct wlr_box close_hitbox;
	struct wsm_image_node *close_icon;
	struct wsm_text_node *name_label;
};

struct wsm_multi_task_buffer_observer {
	struct wl_list link;
	struct wsm_multi_task_layout *layout;
	struct wlr_scene_buffer *source;
	struct wlr_surface *surface;
	struct wl_listener source_commit;
	struct wl_listener source_destroy;
	struct wl_listener frame_done;
};

struct wsm_multi_task_layout {
	struct wsm_multi_task_view *view;
	struct wsm_output *output;
	struct wl_listener output_frame;
	struct wl_listener output_disable;
	struct wlr_scene_tree *tree;
	struct wlr_scene_rect *scrim;
	struct wlr_scene_tree *content;
	struct wsm_multi_task_buffer *buffers;
	size_t buffers_len;
	struct wsm_multi_task_rect *rects;
	size_t rects_len;
	struct wsm_multi_task_window *windows;
	size_t windows_len;
	struct wsm_multi_task_source *sources;
	size_t sources_len;
	struct wl_list buffer_observers;
	int hovered_window;
	int hovered_workspace;
	bool workspace_captures_started;
	bool sources_hidden;
	struct wlr_scene_rect *space_background;
	struct wlr_scene_rect **space_borders;
	size_t space_borders_len;
	struct wlr_scene_rect **space_desktops;
	size_t space_desktops_len;
	struct wsm_multi_task_workspace_preview *workspace_previews;
	size_t workspace_previews_len;
	struct wlr_scene_rect *add_workspace_horizontal;
	struct wlr_scene_rect *add_workspace_vertical;
	struct wlr_box add_workspace_hitbox;
};

struct wsm_space_swipe_transition {
	struct wsm_multi_task_view *view;
	struct wsm_output *output;
	struct wsm_workspace *source;
	struct wsm_workspace *target;
	struct wlr_scene_tree *tree;
	struct wlr_scene_tree *source_capture_tree;
	struct wlr_scene_tree *target_capture_tree;
	struct wsm_workspace_capture *source_capture;
	struct wsm_workspace_capture *target_capture;
	struct wl_listener output_frame;
	struct wl_listener output_disable;
	struct wlr_box output_box;
	struct timespec animation_started;
	double progress;
	double animation_from;
	double animation_to;
	uint32_t animation_duration_msec;
	int direction;
	int gap;
	double finger_distance;
	int finish_frames;
	bool animation_running;
	bool switch_applied;
};

static void damage_space_swipe_output(
		struct wsm_space_swipe_transition *transition) {
	if (transition == NULL || transition->output == NULL) {
		return;
	}
	if (transition->output->scene_output != NULL) {
		wlr_damage_ring_add_whole(
			&transition->output->scene_output->damage_ring);
	}
	wlr_output_schedule_frame(transition->output->wlr_output);
}

static struct wsm_output *gesture_target_output(
		struct wsm_multi_task_view *view) {
	if (view == NULL) {
		return NULL;
	}
	struct wsm_workspace *workspace =
		seat_get_focused_workspace(view->seat);
	if (workspace != NULL && workspace->output != NULL &&
			wsm_output_is_usable(workspace->output)) {
		return workspace->output;
	}
	struct wsm_output *output = wsm_output_nearest_to(
		view->seat->cursor->cursor_wlr->x,
		view->seat->cursor->cursor_wlr->y);
	if (output != NULL && wsm_output_is_usable(output)) {
		return output;
	}
	for (int i = 0; i < global_server.scene->outputs->length; ++i) {
		output = global_server.scene->outputs->items[i];
		if (wsm_output_is_usable(output) && output->workspaces != NULL &&
				output->workspaces->length > 0) {
			return output;
		}
	}
	return NULL;
}

struct container_array {
	struct wsm_container **items;
	size_t length;
	size_t capacity;
};

struct clone_data {
	struct wsm_multi_task_layout *layout;
	struct wsm_container *container;
	struct wlr_box target;
	struct wlr_box clip;
	bool clipped;
	bool workspace_preview;
	bool skip_titlebar;
};

static double clamp(double value, double min, double max) {
	return value < min ? min : value > max ? max : value;
}

static double space_swipe_finger_distance(
		struct wsm_space_swipe_transition *transition) {
	if (transition == NULL) {
		return HORIZONTAL_SWIPE_DISTANCE;
	}
	return transition->finger_distance > 0
		? transition->finger_distance : HORIZONTAL_SWIPE_DISTANCE;
}

static double calculate_space_swipe_finger_distance(
		struct wsm_multi_task_view *view, struct wsm_output *output) {
	if (view != NULL && output != NULL &&
			view->swipe_device_width_mm > 0 &&
			output->wlr_output->phys_width > 0) {
		double distance = output->width *
			(view->swipe_device_width_mm /
				(double)output->wlr_output->phys_width) *
			SPACE_SWIPE_PHYSICAL_FACTOR;
		return clamp(distance, SPACE_SWIPE_FINGER_DISTANCE_MIN,
			SPACE_SWIPE_FINGER_DISTANCE_MAX);
	}
	return clamp(output != NULL ? output->width * 0.35 :
		HORIZONTAL_SWIPE_DISTANCE,
		SPACE_SWIPE_FINGER_DISTANCE_MIN,
		SPACE_SWIPE_FINGER_DISTANCE_MAX);
}

static void store_swipe_device_size(
		struct wsm_multi_task_view *view, struct wlr_pointer *pointer) {
	if (view == NULL) {
		return;
	}
	view->swipe_device_width_mm = 0;
	view->swipe_device_height_mm = 0;
	if (pointer == NULL) {
		return;
	}
	struct libinput_device *device =
		wlr_libinput_get_device_handle(&pointer->base);
	if (device == NULL) {
		return;
	}
	double width = 0, height = 0;
	if (libinput_device_get_size(device, &width, &height) == 0) {
		view->swipe_device_width_mm = width;
		view->swipe_device_height_mm = height;
	}
}

static double ease_out_cubic(double progress) {
	double remaining = 1.0 - progress;
	return 1.0 - remaining * remaining * remaining;
}

static void handle_layout_output_frame(
	struct wl_listener *listener, void *data);
static void handle_layout_output_disable(
	struct wl_listener *listener, void *data);
static void close_overview(struct wsm_multi_task_view *view);
static void request_scene_frame(struct wsm_multi_task_view *view);
static void queue_scene_progress(
	struct wsm_multi_task_view *view, double progress);
static void cancel_window_drag(
	struct wsm_multi_task_view *view, bool restore_position);
static void destroy_space_swipe(struct wsm_multi_task_view *view);
static void start_workspace_captures(
	struct wsm_multi_task_layout *layout);
static void start_view_workspace_captures(
	struct wsm_multi_task_view *view);
static void set_layout_progress(
	struct wsm_multi_task_layout *layout, double progress, double position);
static void hide_deferred_workspace_container(
	struct wsm_multi_task_view *view);
static void refresh_deferred_workspace_captures(
	struct wsm_multi_task_view *view);

static bool has_keysym(
	const uint32_t *keysyms, size_t length, uint32_t wanted) {
	for (size_t i = 0; i < length; ++i) {
		if (keysyms[i] == wanted) {
			return true;
		}
	}
	return false;
}

static void collect_container(struct wsm_container *container, void *data) {
	struct container_array *array = data;
	if (container->view == NULL || container->node.destroying) {
		return;
	}
	if (array->length == array->capacity) {
		size_t capacity =
			array->capacity == 0 ? 8 : array->capacity * 2;
		void *items =
			realloc(array->items, capacity * sizeof(*array->items));
		if (items == NULL) {
			return;
		}
		array->items = items;
		array->capacity = capacity;
	}
	array->items[array->length++] = container;
}

static void collect_workspace_windows(
	struct wsm_workspace *workspace, struct container_array *array) {
	for (int i = 0; i < workspace->tiling->length; ++i) {
		struct wsm_container *container = workspace->tiling->items[i];
		collect_container(container, array);
		container_for_each_child(container, collect_container, array);
	}
	for (int i = 0; i < workspace->floating->length; ++i) {
		struct wsm_container *container = workspace->floating->items[i];
		collect_container(container, array);
		container_for_each_child(container, collect_container, array);
	}
}

static bool append_buffer(struct wsm_multi_task_layout *layout,
	const struct wsm_multi_task_buffer *buffer) {
	void *buffers = realloc(layout->buffers,
		(layout->buffers_len + 1) * sizeof(*layout->buffers));
	if (buffers == NULL) {
		return false;
	}
	layout->buffers = buffers;
	layout->buffers[layout->buffers_len++] = *buffer;
	return true;
}

static bool append_rect(struct wsm_multi_task_layout *layout,
	const struct wsm_multi_task_rect *rect) {
	void *rects = realloc(layout->rects,
		(layout->rects_len + 1) * sizeof(*layout->rects));
	if (rects == NULL) {
		return false;
	}
	layout->rects = rects;
	layout->rects[layout->rects_len++] = *rect;
	return true;
}

static void set_window_hovered(
	struct wsm_multi_task_window *window, bool hovered);

static bool clip_buffer_item(struct wsm_multi_task_buffer *item,
		const struct wlr_box *clip) {
	double left = fmax(item->to_x, clip->x);
	double top = fmax(item->to_y, clip->y);
	double right = fmin(item->to_x + item->to_width,
		clip->x + clip->width);
	double bottom = fmin(item->to_y + item->to_height,
		clip->y + clip->height);
	if (right <= left || bottom <= top || item->to_width <= 0 ||
			item->to_height <= 0) {
		return false;
	}
	item->crop_x = (left - item->to_x) / item->to_width;
	item->crop_y = (top - item->to_y) / item->to_height;
	item->crop_width = (right - left) / item->to_width;
	item->crop_height = (bottom - top) / item->to_height;
	item->cropped = item->crop_x > 0 || item->crop_y > 0 ||
		item->crop_width < 1 || item->crop_height < 1;
	item->to_x = left;
	item->to_y = top;
	item->to_width = right - left;
	item->to_height = bottom - top;
	return true;
}

static bool clip_rect_item(struct wsm_multi_task_rect *item,
		const struct wlr_box *clip) {
	double left = fmax(item->to_x, clip->x);
	double top = fmax(item->to_y, clip->y);
	double right = fmin(item->to_x + item->to_width,
		clip->x + clip->width);
	double bottom = fmin(item->to_y + item->to_height,
		clip->y + clip->height);
	if (right <= left || bottom <= top) {
		return false;
	}
	item->to_x = left;
	item->to_y = top;
	item->to_width = right - left;
	item->to_height = bottom - top;
	return true;
}

static void sync_buffer(struct wsm_multi_task_buffer *item,
	const pixman_region32_t *damage);

static void handle_source_buffer_commit(
		struct wl_listener *listener, void *data) {
	struct wsm_multi_task_buffer_observer *observer = wl_container_of(
		listener, observer, source_commit);
	if (observer->layout->view->client_updates_suspended) {
		return;
	}
	for (size_t i = 0; i < observer->layout->buffers_len; ++i) {
		struct wsm_multi_task_buffer *item =
			&observer->layout->buffers[i];
		if (item->source == observer->source) {
			sync_buffer(item, &observer->surface->buffer_damage);
		}
	}
}

static void handle_source_buffer_destroy(
		struct wl_listener *listener, void *data) {
	struct wsm_multi_task_buffer_observer *observer = wl_container_of(
		listener, observer, source_destroy);
	for (size_t i = 0; i < observer->layout->buffers_len; ++i) {
		struct wsm_multi_task_buffer *item =
			&observer->layout->buffers[i];
		if (item->source == observer->source) {
			item->source = NULL;
			wlr_scene_buffer_set_buffer(item->buffer, NULL);
		}
	}
	wl_list_remove(&observer->source_commit.link);
	wl_list_remove(&observer->source_destroy.link);
	wl_list_init(&observer->source_commit.link);
	wl_list_init(&observer->source_destroy.link);
	observer->source = NULL;
	observer->surface = NULL;
}

static void handle_preview_frame_done(
		struct wl_listener *listener, void *data) {
	struct wsm_multi_task_buffer_observer *observer = wl_container_of(
		listener, observer, frame_done);
	/* Keep window contents stable for the whole interactive transition,
	 * including the settle animation after the fingers are released. */
	if (observer->surface != NULL &&
			!observer->layout->view->client_updates_suspended) {
		wlr_surface_send_frame_done(observer->surface, data);
	}
}

static void observe_source_buffer(struct wsm_multi_task_layout *layout,
		struct wlr_scene_buffer *source,
		struct wlr_scene_buffer *preview) {
	if (wlr_scene_surface_try_from_buffer(source) == NULL) {
		return;
	}
	struct wsm_multi_task_buffer_observer *observer;
	wl_list_for_each(observer, &layout->buffer_observers, link) {
		if (observer->source == source) {
			return;
		}
	}
	observer = calloc(1, sizeof(*observer));
	if (observer == NULL) {
		return;
	}
	observer->layout = layout;
	observer->source = source;
	observer->surface =
		wlr_scene_surface_try_from_buffer(source)->surface;
	observer->source_commit.notify = handle_source_buffer_commit;
	wl_signal_add(&observer->surface->events.commit,
		&observer->source_commit);
	observer->source_destroy.notify = handle_source_buffer_destroy;
	wl_signal_add(&source->node.events.destroy, &observer->source_destroy);
	observer->frame_done.notify = handle_preview_frame_done;
	wl_signal_add(&preview->events.frame_done, &observer->frame_done);
	wl_list_insert(&layout->buffer_observers, &observer->link);
}

static void finish_buffer_observers(struct wsm_multi_task_layout *layout) {
	struct wsm_multi_task_buffer_observer *observer, *tmp;
	wl_list_for_each_safe(observer, tmp, &layout->buffer_observers, link) {
		if (observer->source != NULL) {
			wl_list_remove(&observer->source_commit.link);
			wl_list_remove(&observer->source_destroy.link);
		}
		wl_list_remove(&observer->frame_done.link);
		wl_list_remove(&observer->link);
		free(observer);
	}
}

static void clone_scene_tree(
	struct wlr_scene_tree *tree, int sx, int sy, struct clone_data *clone) {
	struct wsm_container *container = clone->container;
	int scene_x = 0, scene_y = 0;
	wlr_scene_node_coords(&container->scene_tree->node, &scene_x, &scene_y);
	if (container->current.width <= 0 || container->current.height <= 0) {
		return;
	}
	double scale_x = (double)clone->target.width / container->current.width;
	double scale_y =
		(double)clone->target.height / container->current.height;
	struct wlr_scene_node *node;
	wl_list_for_each(node, &tree->children, link) {
		/* A no-SSD build keeps the titlebar objects alive because the normal
		 * container code owns them, but they must not inflate overview clones. */
		if ((!HAVE_SSD || clone->skip_titlebar) &&
				container->title_bar != NULL &&
				node == &container->title_bar->tree->node) {
			continue;
		}
		if (!node->enabled) {
			if (node->type != WLR_SCENE_NODE_TREE ||
				wsm_scene_descriptor_try_get(
					node, WSM_SCENE_DESC_BUTTON)) {
				continue;
			}
		}
		int x = sx + node->x;
		int y = sy + node->y;
		if (node->type == WLR_SCENE_NODE_TREE) {
			clone_scene_tree(
				wlr_scene_tree_from_node(node), x, y, clone);
			continue;
		}
		if (node->type == WLR_SCENE_NODE_BUFFER) {
			struct wlr_scene_buffer *source =
				wlr_scene_buffer_from_node(node);
			int width = source->dst_width > 0
				? source->dst_width
				: source->buffer_width;
			int height = source->dst_height > 0
				? source->dst_height
				: source->buffer_height;
			if (width <= 0 || height <= 0) {
				continue;
			}
			/* Text and image scene nodes create their backing buffers lazily
			 * after they enter an output. Keep an empty clone for them so SSD
			 * titles, application icons and buttons can be attached by
			 * sync_buffer() as soon as the source buffer becomes available. */
			struct wlr_scene_buffer *copy = wlr_scene_buffer_create(
				clone->layout->content, source->buffer);
			if (copy == NULL) {
				continue;
			}
			wlr_scene_buffer_set_source_box(copy, &source->src_box);
			wlr_scene_buffer_set_transform(copy, source->transform);
			wlr_scene_buffer_set_filter_mode(
				copy, source->filter_mode);
			wlr_scene_buffer_set_opacity(copy, source->opacity);
			wlr_scene_buffer_set_opaque_region(
				copy, &source->opaque_region);
			struct wsm_multi_task_buffer item = {
				.buffer = copy,
				.source = source,
				.container = container,
				.from_x = scene_x + x,
				.from_y = scene_y + y,
				.from_width = width,
				.from_height = height,
				.to_x = clone->target.x + x * scale_x,
				.to_y = clone->target.y + y * scale_y,
				.to_width = width * scale_x,
				.to_height = height * scale_y,
				.workspace_preview = clone->workspace_preview,
			};
			if (clone->clipped &&
					!clip_buffer_item(&item, &clone->clip)) {
				wlr_scene_node_destroy(&copy->node);
				continue;
			}
			if (!append_buffer(clone->layout, &item)) {
				wlr_scene_node_destroy(&copy->node);
			} else {
				observe_source_buffer(clone->layout, source, copy);
			}
			continue;
		}
		struct wlr_scene_rect *source = wlr_scene_rect_from_node(node);
		struct wlr_scene_rect *copy =
			wlr_scene_rect_create(clone->layout->content,
				source->width, source->height, source->color);
		if (copy == NULL) {
			continue;
		}
		struct wsm_multi_task_rect item = {
			.rect = copy,
			.source = source,
			.container = container,
			.from_x = scene_x + x,
			.from_y = scene_y + y,
			.from_width = source->width,
			.from_height = source->height,
			.to_x = clone->target.x + x * scale_x,
			.to_y = clone->target.y + y * scale_y,
			.to_width = source->width * scale_x,
			.to_height = source->height * scale_y,
			.workspace_preview = clone->workspace_preview,
		};
		if (clone->clipped && !clip_rect_item(&item, &clone->clip)) {
			wlr_scene_node_destroy(&copy->node);
			continue;
		}
		for (size_t i = 0; i < 4; ++i) {
			item.color[i] = source->color[i];
		}
		if (!append_rect(clone->layout, &item)) {
			wlr_scene_node_destroy(&copy->node);
		}
	}
}

static bool clone_titlebar_capture(struct wsm_multi_task_layout *layout,
		struct wsm_container *container, const struct wlr_box *target,
		const struct wlr_box *clip, bool workspace_preview) {
	if (!HAVE_SSD || container->title_bar == NULL ||
			container->title_bar->tree == NULL ||
			!container->title_bar->tree->node.enabled) {
		return false;
	}
	struct wsm_titlebar_capture capture;
	float output_scale = layout->output->wlr_output->scale;
	if (!wsm_titlebar_capture(&capture, container->title_bar->tree,
			global_server.wlr_renderer, global_server.wlr_allocator,
			output_scale)) {
		return false;
	}
	struct wlr_box capture_box = capture.box;
	struct wlr_scene_buffer *copy =
		wlr_scene_buffer_create(layout->content, capture.buffer);
	wsm_titlebar_capture_finish(&capture);
	if (copy == NULL) {
		return false;
	}

	int scene_x = 0, scene_y = 0;
	wlr_scene_node_coords(
		&container->scene_tree->node, &scene_x, &scene_y);
	int local_x = container->title_bar->tree->node.x + capture_box.x;
	int local_y = container->title_bar->tree->node.y + capture_box.y;
	double scale_x = (double)target->width / container->current.width;
	double scale_y = (double)target->height / container->current.height;
	struct wsm_multi_task_buffer item = {
		.buffer = copy,
		.container = container,
		.from_x = scene_x + local_x,
		.from_y = scene_y + local_y,
		.from_width = capture_box.width,
		.from_height = capture_box.height + WINDOW_TITLE_CONTENT_OVERLAP,
		.to_x = target->x + local_x * scale_x,
		.to_y = target->y + local_y * scale_y,
		.to_width = capture_box.width * scale_x,
		.to_height = capture_box.height * scale_y +
			WINDOW_TITLE_CONTENT_OVERLAP,
		.workspace_preview = workspace_preview,
		.source_synced = true,
		.snapshot = true,
	};
	if (clip != NULL && !clip_buffer_item(&item, clip)) {
		wlr_scene_node_destroy(&copy->node);
		return true;
	}
	wlr_scene_buffer_set_filter_mode(copy, WLR_SCALE_FILTER_BILINEAR);
	wlr_scene_buffer_set_dest_size(
		copy, capture_box.width, capture_box.height);
	pixman_region32_t empty;
	pixman_region32_init(&empty);
	wlr_scene_buffer_set_opaque_region(copy, &empty);
	pixman_region32_fini(&empty);
	if (!append_buffer(layout, &item)) {
		wlr_scene_node_destroy(&copy->node);
		return false;
	}
	return true;
}

static void clone_titlebar_fallback(struct wsm_multi_task_layout *layout,
		struct wsm_container *container, const struct wlr_box *target) {
	if (!HAVE_SSD || container->title_bar == NULL ||
			container->title_bar->tree == NULL ||
			!container->title_bar->tree->node.enabled) {
		return;
	}
	struct clone_data data = {
		.layout = layout,
		.container = container,
		.target = *target,
	};
	clone_scene_tree(container->title_bar->tree,
		container->title_bar->tree->node.x,
		container->title_bar->tree->node.y, &data);
}

static void clone_container(struct wsm_multi_task_view *view,
	struct wsm_multi_task_layout *layout, struct wsm_container *container,
	const struct wlr_box *target, const struct wlr_box *clip,
	bool workspace_preview) {
	if (container->scene_tree == NULL) {
		return;
	}
	if (container->title_bar && container->title_bar->title_text) {
		wsm_text_node_ensure_buffer(container->title_bar->title_text);
	}
	bool captured_titlebar = clone_titlebar_capture(layout, container,
		target, clip, workspace_preview);
	struct clone_data data = {
		.layout = layout,
		.container = container,
		.target = *target,
		.clipped = clip != NULL,
		.workspace_preview = workspace_preview,
		.skip_titlebar = captured_titlebar,
	};
	if (clip != NULL) {
		data.clip = *clip;
	}
	clone_scene_tree(container->scene_tree, 0, 0, &data);
}

static void handle_overview_window_destroy(
		struct wl_listener *listener, void *data) {
	struct wsm_multi_task_window *window =
		wl_container_of(listener, window, destroy);
	struct wsm_multi_task_layout *layout = window->layout;
	struct wsm_container *container = window->container;
	if (layout->view->dragged_container == container) {
		cancel_window_drag(layout->view, false);
	}
	wsm_window_preview_destroy(window->preview);
	window->preview = NULL;
	for (size_t i = 0; i < layout->buffers_len; ++i) {
		if (layout->buffers[i].container == container) {
			layout->buffers[i].container = NULL;
			layout->buffers[i].source = NULL;
			wlr_scene_buffer_set_buffer(
				layout->buffers[i].buffer, NULL);
		}
	}
	for (size_t i = 0; i < layout->rects_len; ++i) {
		if (layout->rects[i].container == container) {
			layout->rects[i].container = NULL;
			layout->rects[i].source = NULL;
			wlr_scene_node_set_enabled(
				&layout->rects[i].rect->node, false);
		}
	}
	for (size_t i = 0; i < layout->sources_len; ++i) {
		if (layout->sources[i].node == &container->scene_tree->node) {
			layout->sources[i].node = NULL;
		}
	}
	wlr_scene_node_set_enabled(&window->backdrop->node, false);
	set_window_hovered(window, false);
	window->container = NULL;
	wl_list_remove(&window->destroy.link);
	wl_list_init(&window->destroy.link);
	if (layout->hovered_window == (int)(window - layout->windows)) {
		layout->hovered_window = -1;
	}
}

static void finish_window_listeners(struct wsm_multi_task_layout *layout) {
	for (size_t i = 0; i < layout->windows_len; ++i) {
		wsm_window_preview_destroy(layout->windows[i].preview);
		layout->windows[i].preview = NULL;
		if (layout->windows[i].container != NULL) {
			wl_list_remove(&layout->windows[i].destroy.link);
		}
	}
}

static void set_sources_hidden(
		struct wsm_multi_task_layout *layout, bool hidden) {
	if (layout == NULL || layout->sources_hidden == hidden) {
		return;
	}
	for (size_t i = 0; i < layout->sources_len; ++i) {
		struct wsm_multi_task_source *source = &layout->sources[i];
		if (source->node == NULL) {
			continue;
		}
		if (hidden) {
			wlr_scene_node_set_enabled(source->node, false);
		} else {
			wlr_scene_node_set_enabled(source->node, source->enabled);
		}
	}
	if (!hidden) {
		/* Re-enabling a scene subtree restores visibility and output tracking, but
		 * doesn't rebuild the subsurface clip. A scale transition may have changed
		 * the surface geometry while the real tree was hidden, leaving the old clip
		 * narrower than the SSD titlebar until the next interactive resize. */
		for (size_t i = 0; i < layout->windows_len; ++i) {
			struct wsm_container *container = layout->windows[i].container;
			if (container != NULL && container->view != NULL &&
					container->view->surface != NULL) {
				view_center_and_clip_surface(container->view);
			}
		}
	}
	layout->sources_hidden = hidden;
}

static void destroy_layout(struct wsm_multi_task_layout *layout) {
	if (layout == NULL) {
		return;
	}
	set_sources_hidden(layout, false);
	if (!wl_list_empty(&layout->output_frame.link)) {
		wl_list_remove(&layout->output_frame.link);
	}
	if (!wl_list_empty(&layout->output_disable.link)) {
		wl_list_remove(&layout->output_disable.link);
	}
	finish_window_listeners(layout);
	finish_buffer_observers(layout);
	for (size_t i = 0; i < layout->workspace_previews_len; ++i) {
		wsm_workspace_capture_destroy(
			layout->workspace_previews[i].capture);
		layout->workspace_previews[i].capture = NULL;
	}
	if (layout->tree != NULL) {
		wlr_scene_node_destroy(&layout->tree->node);
	}
	free(layout->buffers);
	free(layout->rects);
	free(layout->windows);
	free(layout->sources);
	free(layout->space_borders);
	free(layout->space_desktops);
	free(layout->workspace_previews);
	free(layout);
}

static void destroy_layouts(struct wsm_multi_task_view *view) {
	for (size_t i = 0; i < view->layouts_len; ++i) {
		destroy_layout(view->layouts[i]);
	}
	free(view->layouts);
	view->layouts = NULL;
	view->layouts_len = 0;
}

static char *find_workspace_close_icon(void) {
	static const char *names[] = {
		"window-close",
		"window-close-symbolic",
		"view-close",
		NULL,
	};
	for (size_t i = 0; names[i] != NULL; ++i) {
		char *path = find_icon_file_frome_theme(
			global_server.desktop_interface, names[i]);
		if (path != NULL && *path != '\0') {
			return path;
		}
		free(path);
	}
	return NULL;
}

static bool add_workspace_previews(struct wsm_multi_task_view *view,
	struct wsm_multi_task_layout *layout,
	const struct wlr_box *output_box) {
	size_t length = layout->output->workspaces->length;
	if (length == 0) {
		return true;
	}
	layout->space_borders = calloc(length, sizeof(*layout->space_borders));
	layout->space_desktops = calloc(length, sizeof(*layout->space_desktops));
	layout->workspace_previews =
		calloc(length, sizeof(*layout->workspace_previews));
	if (layout->space_borders == NULL ||
			layout->space_desktops == NULL ||
			layout->workspace_previews == NULL) {
		return false;
	}
	float background_color[4] = {0.0f, 0.0f, 0.0f, 1.0f};
	layout->space_background = wlr_scene_rect_create(layout->content,
		output_box->width, WINDOW_TOP - WINDOW_GAP / 2, background_color);
	if (layout->space_background == NULL) {
		return false;
	}
	wlr_scene_node_set_position(&layout->space_background->node,
		output_box->x, output_box->y);
	wlr_scene_node_set_enabled(&layout->space_background->node, false);
	int available = output_box->width - OVERVIEW_PADDING * 2 -
		((int)length - 1) * SPACE_GAP;
	int max_width = available / (int)length;
	if (max_width > SPACE_MAX_WIDTH) {
		max_width = SPACE_MAX_WIDTH;
	}
	int preview_width = fmax(1, max_width - 10);
	int preview_height = lround((double)preview_width * output_box->height /
		output_box->width);
	if (preview_height > SPACE_HEIGHT - 10) {
		preview_height = SPACE_HEIGHT - 10;
		preview_width = lround((double)preview_height * output_box->width /
			output_box->height);
	}
	int width = preview_width + 10;
	int height = preview_height + 10;
	int total_width = (int)length * width + ((int)length - 1) * SPACE_GAP;
	int start_x = output_box->x + (output_box->width - total_width) / 2;
	struct wsm_workspace *active =
		output_get_active_workspace(layout->output);
	char *close_icon_path = length > 1 ? find_workspace_close_icon() : NULL;

	for (size_t i = 0; i < length; ++i) {
		struct wsm_workspace *workspace =
			layout->output->workspaces->items[i];
		struct wlr_box preview = {
			.x = start_x + (int)i * (width + SPACE_GAP) + 5,
			.y = output_box->y + OVERVIEW_PADDING + 5,
			.width = preview_width,
			.height = preview_height,
		};
		struct wsm_multi_task_workspace_preview *preview_item =
			&layout->workspace_previews[layout->workspace_previews_len++];
		preview_item->workspace = workspace;
		preview_item->box = preview;
		char workspace_label[64];
		if (length == 1) {
			snprintf(workspace_label, sizeof(workspace_label), "Workspace");
		} else {
			snprintf(workspace_label, sizeof(workspace_label),
				"Workspace %zu", i + 1);
		}
		float workspace_label_color[4] = {
			0.92f, 0.92f, 0.94f, 1.0f,
		};
		preview_item->name_label = wsm_text_node_create(layout->content,
			global_server.desktop_interface, workspace_label,
			workspace_label_color, false);
		if (preview_item->name_label != NULL) {
			wsm_text_node_set_max_width(
				preview_item->name_label, preview.width);
			wsm_text_node_set_ellipsize(
				preview_item->name_label, true);
			wsm_text_node_ensure_buffer(preview_item->name_label);
			int label_width = preview_item->name_label->width < preview.width
				? preview_item->name_label->width : preview.width;
			wlr_scene_node_set_position(
				preview_item->name_label->node_wlr,
				preview.x + (preview.width - label_width) / 2,
				preview.y + preview.height + SPACE_LABEL_GAP);
			preview_item->label_hitbox = (struct wlr_box) {
				.x = preview.x + (preview.width - label_width) / 2,
				.y = preview.y + preview.height + SPACE_LABEL_GAP,
				.width = label_width,
				.height = preview_item->name_label->height,
			};
			wlr_scene_node_set_enabled(
				preview_item->name_label->node_wlr, false);
		}
		if (workspace == active) {
			float border_color[4] = {0.30f, 0.52f, 0.92f, 1.0f};
			struct wlr_scene_rect *border = wlr_scene_rect_create(
				layout->content, width, height, border_color);
			if (border != NULL) {
				wlr_scene_node_set_position(
					&border->node, preview.x - 5, preview.y - 5);
				wlr_scene_node_set_enabled(&border->node, false);
				layout->space_borders[layout->space_borders_len++] =
					border;
			}
		}
		float drop_border_color[4] = {0.36f, 0.76f, 1.0f, 1.0f};
		preview_item->drop_border = wlr_scene_rect_create(
			layout->content, width, height, drop_border_color);
		if (preview_item->drop_border != NULL) {
			wlr_scene_node_set_position(&preview_item->drop_border->node,
				preview.x - 5, preview.y - 5);
			wlr_scene_node_set_enabled(
				&preview_item->drop_border->node, false);
		}
		float desktop_color[4] = {0.035f, 0.04f, 0.055f, 1.0f};
		struct wlr_scene_rect *desktop =
			wlr_scene_rect_create(layout->content, preview.width,
				preview.height, desktop_color);
		if (desktop != NULL) {
			wlr_scene_node_set_position(
				&desktop->node, preview.x, preview.y);
			wlr_scene_node_set_enabled(&desktop->node, false);
			layout->space_desktops[layout->space_desktops_len++] =
				desktop;
		}
		preview_item->capture_tree =
			wlr_scene_tree_create(layout->content);
		if (preview_item->capture_tree == NULL) {
			free(close_icon_path);
			return false;
		}
		if (desktop != NULL) {
			wlr_scene_node_place_above(
				&preview_item->capture_tree->node, &desktop->node);
		}
		if (close_icon_path != NULL) {
			int corner_x = workspace == active ? preview.x - 5 : preview.x;
			int corner_y = workspace == active ? preview.y - 5 : preview.y;
			preview_item->close_hitbox = (struct wlr_box){
				.x = corner_x - SPACE_CLOSE_HITBOX_SIZE / 2,
				.y = corner_y - SPACE_CLOSE_HITBOX_SIZE / 2,
				.width = SPACE_CLOSE_HITBOX_SIZE,
				.height = SPACE_CLOSE_HITBOX_SIZE,
			};
			preview_item->close_icon = wsm_image_node_create(
				layout->content, SPACE_CLOSE_ICON_SIZE,
				SPACE_CLOSE_ICON_SIZE, close_icon_path, 1.0f);
			if (preview_item->close_icon != NULL) {
				wlr_scene_node_set_position(
					preview_item->close_icon->node_wlr,
					preview_item->close_hitbox.x +
						(SPACE_CLOSE_HITBOX_SIZE -
						 SPACE_CLOSE_ICON_SIZE) / 2,
					preview_item->close_hitbox.y +
						(SPACE_CLOSE_HITBOX_SIZE -
						 SPACE_CLOSE_ICON_SIZE) / 2);
				wlr_scene_node_set_enabled(
					preview_item->close_icon->node_wlr, false);
			}
		}
	}
	free(close_icon_path);
	layout->add_workspace_hitbox = (struct wlr_box){
		.x = output_box->x + output_box->width -
			OVERVIEW_PADDING / 2 - SPACE_ADD_BUTTON_SIZE,
		.y = output_box->y + OVERVIEW_PADDING +
			(height - SPACE_ADD_BUTTON_SIZE) / 2,
		.width = SPACE_ADD_BUTTON_SIZE,
		.height = SPACE_ADD_BUTTON_SIZE,
	};
	float add_color[4] = {0.88f, 0.88f, 0.90f, 1.0f};
	layout->add_workspace_horizontal = wlr_scene_rect_create(layout->content,
		SPACE_ADD_GLYPH_SIZE, SPACE_ADD_GLYPH_THICKNESS, add_color);
	layout->add_workspace_vertical = wlr_scene_rect_create(layout->content,
		SPACE_ADD_GLYPH_THICKNESS, SPACE_ADD_GLYPH_SIZE, add_color);
	if (layout->add_workspace_horizontal != NULL) {
		wlr_scene_node_set_position(
			&layout->add_workspace_horizontal->node,
			layout->add_workspace_hitbox.x +
				(SPACE_ADD_BUTTON_SIZE - SPACE_ADD_GLYPH_SIZE) / 2,
			layout->add_workspace_hitbox.y +
				(SPACE_ADD_BUTTON_SIZE -
				 SPACE_ADD_GLYPH_THICKNESS) / 2);
		wlr_scene_node_set_enabled(
			&layout->add_workspace_horizontal->node, false);
	}
	if (layout->add_workspace_vertical != NULL) {
		wlr_scene_node_set_position(
			&layout->add_workspace_vertical->node,
			layout->add_workspace_hitbox.x +
				(SPACE_ADD_BUTTON_SIZE -
				 SPACE_ADD_GLYPH_THICKNESS) / 2,
			layout->add_workspace_hitbox.y +
				(SPACE_ADD_BUTTON_SIZE - SPACE_ADD_GLYPH_SIZE) / 2);
		wlr_scene_node_set_enabled(
			&layout->add_workspace_vertical->node, false);
	}
	return true;
}

static bool add_active_windows(struct wsm_multi_task_view *view,
	struct wsm_multi_task_layout *layout,
	const struct wlr_box *output_box) {
	struct wsm_workspace *workspace =
		output_get_active_workspace(layout->output);
	if (workspace == NULL) {
		return true;
	}
	struct container_array windows = {0};
	collect_workspace_windows(workspace, &windows);
	if (windows.length == 0) {
		free(windows.items);
		return true;
	}
	layout->windows = calloc(windows.length, sizeof(*layout->windows));
	layout->sources = calloc(windows.length, sizeof(*layout->sources));
	if (layout->windows == NULL || layout->sources == NULL) {
		free(windows.items);
		return false;
	}
	for (size_t i = 0; i < windows.length; ++i) {
		struct wlr_scene_node *node = &windows.items[i]->scene_tree->node;
		layout->sources[layout->sources_len++] =
			(struct wsm_multi_task_source){
				.node = node,
				.enabled = windows.items[i]->view->enabled,
			};
	}
	int columns = (int)ceil(sqrt((double)windows.length));
	int rows = ((int)windows.length + columns - 1) / columns;
	int available_width = output_box->width - OVERVIEW_PADDING * 2 -
		(columns - 1) * WINDOW_GAP;
	int available_height = output_box->height - WINDOW_TOP -
		OVERVIEW_PADDING - (rows - 1) * WINDOW_GAP;
	int cell_width = available_width / columns;
	int cell_height = available_height / rows;
	int start_x = output_box->x + OVERVIEW_PADDING;
	int start_y = output_box->y + WINDOW_TOP;

	for (size_t i = 0; i < windows.length; ++i) {
		struct wsm_container *container = windows.items[i];
		if (container->current.width <= 0 ||
				container->current.height <= 0) {
			continue;
		}
		/* Overview previews may shrink windows to fit their cells, but must
		 * never enlarge them. Upscaling a small client buffer only makes the
		 * preview softer and provides no additional visual information. */
		double scale = fmin(1.0,
			fmin((double)cell_width / container->current.width,
				(double)cell_height / container->current.height));
		int width = fmax(1, floor(container->current.width * scale));
		int height = fmax(1, floor(container->current.height * scale));
		int column = (int)i % columns;
		int row = (int)i / columns;
		struct wlr_box target = {
			.x = start_x + column * (cell_width + WINDOW_GAP) +
				(cell_width - width) / 2,
			.y = start_y + row * (cell_height + WINDOW_GAP) +
				(cell_height - height) / 2,
			.width = width,
			.height = height,
		};
		float shadow_color[4] = {0};
		struct wlr_scene_rect *backdrop = wlr_scene_rect_create(
			layout->content, container->current.width + 16,
			container->current.height + 16, shadow_color);
		if (backdrop == NULL) {
			continue;
		}
		wlr_scene_node_lower_to_bottom(&backdrop->node);
		wlr_scene_node_set_enabled(&backdrop->node, false);
		int scene_x = 0, scene_y = 0;
		wlr_scene_node_coords(
			&container->scene_tree->node, &scene_x, &scene_y);
		struct wsm_multi_task_window *item =
			&layout->windows[layout->windows_len++];
		float border_color[4] = {0.20f, 0.55f, 0.95f, 1.0f};
		struct wlr_scene_rect *border = wlr_scene_rect_create(
			layout->content, target.width + WINDOW_BORDER_WIDTH * 2,
			target.height + WINDOW_BORDER_WIDTH * 2, border_color);
		*item = (struct wsm_multi_task_window){
			.layout = layout,
			.container = container,
			.backdrop = backdrop,
			.border = border,
			.from_x = scene_x - 8,
			.from_y = scene_y - 8,
			.from_width = container->current.width + 16,
			.from_height = container->current.height + 16,
			.to_x = target.x - 8,
			.to_y = target.y - 8,
			.to_width = target.width + 16,
			.to_height = target.height + 16,
		};
		item->destroy.notify = handle_overview_window_destroy;
		wl_signal_add(&container->node.events.destroy, &item->destroy);
		if (border != NULL) {
			wlr_scene_node_set_enabled(&border->node, false);
			wlr_scene_node_place_above(&border->node, &backdrop->node);
		}
		int content_x = container->view->scene_tree->node.x +
			container->view->content_tree->node.x;
		int content_y = container->view->scene_tree->node.y +
			container->view->content_tree->node.y;
		item->preview = wsm_window_preview_create(layout->content,
			container->view->content_tree,
			container->current.width, container->current.height,
			content_x, content_y);
		if (item->preview == NULL) {
			/* Preserve compatibility for unusual view scene layouts. */
			clone_container(view, layout, container, &target, NULL, false);
		} else if (!clone_titlebar_capture(
				layout, container, &target, NULL, false)) {
			clone_titlebar_fallback(layout, container, &target);
		}

		const char *title = view_get_title(container->view);
		float title_background_color[4] = {0.12f, 0.12f, 0.14f, 1.0f};
		if (container->title_bar != NULL &&
				container->title_bar->background != NULL) {
			for (size_t channel = 0; channel < 4; ++channel) {
				title_background_color[channel] =
					container->title_bar->background->color[channel];
			}
		}
		item->title_background = wlr_scene_rect_create(layout->content,
			1, 1, title_background_color);
		if (item->title_background != NULL) {
			wlr_scene_node_set_enabled(
				&item->title_background->node, false);
		}
		float title_color[4] = {0.95f, 0.95f, 0.97f, 1.0f};
		item->title_label = wsm_text_node_create(layout->content,
			global_server.desktop_interface,
			(char *)(title != NULL ? title : ""), title_color, false);
		if (item->title_label != NULL) {
			int max_title_width = target.width -
				WINDOW_TITLE_HORIZONTAL_PADDING * 2;
			wsm_text_node_set_max_width(item->title_label,
				max_title_width > 0 ? max_title_width : 1);
			wsm_text_node_set_ellipsize(item->title_label, true);
			wsm_text_node_ensure_buffer(item->title_label);
			wlr_scene_node_set_enabled(
				item->title_label->node_wlr, false);
		}
	}
	free(windows.items);
	return true;
}

static struct wsm_multi_task_layout *create_layout(
		struct wsm_multi_task_view *view, struct wsm_output *output) {
	struct wsm_multi_task_layout *layout = calloc(1, sizeof(*layout));
	if (layout == NULL) {
		return NULL;
	}
	wl_list_init(&layout->buffer_observers);
	wl_list_init(&layout->output_frame.link);
	wl_list_init(&layout->output_disable.link);
	layout->view = view;
	layout->hovered_window = -1;
	layout->hovered_workspace = -1;
	layout->output = output;
	layout->tree = wlr_scene_tree_create(view->tree);
	if (layout->tree == NULL) {
		destroy_layout(layout);
		return NULL;
	}
	float clear[4] = {0};
	layout->scrim = wlr_scene_rect_create(layout->tree, 0, 0, clear);
	layout->content = wlr_scene_tree_create(layout->tree);
	if (layout->scrim == NULL || layout->content == NULL) {
		destroy_layout(layout);
		return NULL;
	}
	struct wlr_box output_box;
	output_get_box(output, &output_box);
	wlr_scene_rect_set_size(
		layout->scrim, output_box.width, output_box.height);
	wlr_scene_node_set_position(
		&layout->scrim->node, output_box.x, output_box.y);
	if (!add_workspace_previews(view, layout, &output_box) ||
		!add_active_windows(view, layout, &output_box)) {
		destroy_layout(layout);
		return NULL;
	}
	wlr_scene_node_raise_to_top(&layout->content->node);
	layout->output_frame.notify = handle_layout_output_frame;
	wl_signal_add(&output->events.frame, &layout->output_frame);
	layout->output_disable.notify = handle_layout_output_disable;
	wl_signal_add(&output->events.disable, &layout->output_disable);
	return layout;
}

static bool prepare_layouts(struct wsm_multi_task_view *view) {
	cancel_window_drag(view, false);
	destroy_layouts(view);
	for (int i = 0; i < global_server.scene->outputs->length; ++i) {
		struct wsm_output *output =
			global_server.scene->outputs->items[i];
		if (!wsm_output_is_usable(output) || output->workspaces == NULL) {
			continue;
		}
		struct wsm_multi_task_layout *layout =
			create_layout(view, output);
		if (layout == NULL) {
			destroy_layouts(view);
			return false;
		}
		void *layouts = realloc(view->layouts,
			(view->layouts_len + 1) * sizeof(*view->layouts));
		if (layouts == NULL) {
			destroy_layout(layout);
			destroy_layouts(view);
			return false;
		}
		view->layouts = layouts;
		view->layouts[view->layouts_len++] = layout;
	}
	if (view->workspace_captures_started) {
		view->workspace_captures_started = false;
		start_view_workspace_captures(view);
	}
	hide_deferred_workspace_container(view);
	return view->layouts_len > 0;
}

static void sync_buffer(struct wsm_multi_task_buffer *item,
		const pixman_region32_t *damage) {
	struct wlr_scene_buffer *source = item->source;
	if (source == NULL) {
		return;
	}
	/* The container is a read-only source for the overview. Keep its geometry
	 * captured at layout time and only mirror changing buffer properties. */
	if (damage != NULL) {
		wlr_scene_buffer_set_buffer_with_damage(
			item->buffer, source->buffer, damage);
	} else {
		wlr_scene_buffer_set_buffer(item->buffer, source->buffer);
	}
	if (item->cropped && source->buffer_width > 0 &&
			source->buffer_height > 0) {
		struct wlr_fbox source_box = source->src_box;
		if (wlr_fbox_empty(&source_box)) {
			source_box = (struct wlr_fbox){
				.width = source->buffer_width,
				.height = source->buffer_height,
			};
		}
		struct wlr_fbox transformed;
		wlr_fbox_transform(&transformed, &source_box, source->transform,
			source->buffer_width, source->buffer_height);
		double width = transformed.width;
		double height = transformed.height;
		transformed.x += width * item->crop_x;
		transformed.y += height * item->crop_y;
		transformed.width = width * item->crop_width;
		transformed.height = height * item->crop_height;
		int transformed_width = source->buffer_width;
		int transformed_height = source->buffer_height;
		if (source->transform & 1) {
			transformed_width = source->buffer_height;
			transformed_height = source->buffer_width;
		}
		wlr_fbox_transform(&source_box, &transformed,
			wlr_output_transform_invert(source->transform),
			transformed_width, transformed_height);
		wlr_scene_buffer_set_source_box(item->buffer, &source_box);
	} else {
		wlr_scene_buffer_set_source_box(item->buffer, &source->src_box);
	}
	/* The custom scene renderer consumes opaque regions in destination-space,
	 * but the source region is expressed in the unscaled buffer coordinates.
	 * Copying it to a moving, scaled clone incorrectly culls the background
	 * beneath CSD alpha edges and leaves visible edge remnants. Overview
	 * previews are short-lived, so render their background unconditionally. */
	pixman_region32_t empty_opaque_region;
	pixman_region32_init(&empty_opaque_region);
	wlr_scene_buffer_set_opaque_region(item->buffer, &empty_opaque_region);
	pixman_region32_fini(&empty_opaque_region);
	wlr_scene_buffer_set_transform(item->buffer, source->transform);
	wlr_scene_buffer_set_filter_mode(item->buffer, source->filter_mode);
	item->source_synced = true;
}

static void resume_client_updates(struct wsm_multi_task_view *view) {
	if (!view->client_updates_suspended) {
		return;
	}
	view->client_updates_suspended = false;
	/* Commits made during the transition were deliberately skipped. Apply the
	 * latest buffer state once, after all preview geometry has settled. */
	for (size_t i = 0; i < view->layouts_len; ++i) {
		struct wsm_multi_task_layout *layout = view->layouts[i];
		for (size_t j = 0; j < layout->buffers_len; ++j) {
			if (layout->buffers[j].source != NULL) {
				sync_buffer(&layout->buffers[j], NULL);
			}
		}
	}
}

static void sync_rect(struct wsm_multi_task_rect *item) {
	for (size_t i = 0; i < 4; ++i) {
		item->color[i] = item->source->color[i];
	}
}

static struct wlr_box rounded_box(
		double x, double y, double width, double height) {
	int left = lround(x);
	int top = lround(y);
	int right = lround(x + width);
	int bottom = lround(y + height);
	return (struct wlr_box){
		.x = left,
		.y = top,
		.width = right > left ? right - left : 1,
		.height = bottom > top ? bottom - top : 1,
	};
}

static void set_window_hovered(
		struct wsm_multi_task_window *window, bool hovered) {
	if (window->border != NULL) {
		wlr_scene_node_set_enabled(&window->border->node, hovered);
	}
	if (window->title_label != NULL) {
		wlr_scene_node_set_enabled(
			window->title_label->node_wlr, hovered);
	}
	if (window->title_background != NULL) {
		wlr_scene_node_set_enabled(
			&window->title_background->node, hovered);
	}
}

static void set_workspace_preview_hovered(
		struct wsm_multi_task_workspace_preview *preview, bool hovered) {
	if (preview->close_icon != NULL) {
		wlr_scene_node_set_enabled(
			preview->close_icon->node_wlr, hovered);
	}
}

static void start_workspace_captures(
		struct wsm_multi_task_layout *layout) {
	if (layout->workspace_captures_started) {
		return;
	}
	layout->workspace_captures_started = true;
	struct wlr_box source_box;
	output_get_box(layout->output, &source_box);
	for (size_t i = 0; i < layout->workspace_previews_len; ++i) {
		struct wsm_multi_task_workspace_preview *preview =
			&layout->workspace_previews[i];
		preview->capture = wsm_workspace_capture_create_options(
			preview->capture_tree, layout->output, preview->workspace,
			&source_box, &preview->box, global_server.wlr_renderer,
			global_server.wlr_allocator, true,
			layout->output->wlr_output->scale);
		wsm_workspace_capture_set_opacity(
			preview->capture, (float)layout->view->progress);
		if (!wsm_workspace_capture_render(preview->capture)) {
			wsm_log(WSM_ERROR,
				"Could not render initial capture for workspace '%s'",
				preview->workspace->name);
		}
	}
	for (size_t i = 0; i < layout->space_desktops_len; ++i) {
		wlr_scene_node_set_enabled(
			&layout->space_desktops[i]->node, true);
	}
	for (size_t i = 0; i < layout->space_borders_len; ++i) {
		wlr_scene_node_set_enabled(
			&layout->space_borders[i]->node, true);
	}
	if (layout->add_workspace_horizontal != NULL) {
		wlr_scene_node_set_enabled(
			&layout->add_workspace_horizontal->node, true);
	}
	if (layout->add_workspace_vertical != NULL) {
		wlr_scene_node_set_enabled(
			&layout->add_workspace_vertical->node, true);
	}
}

static void start_view_workspace_captures(
		struct wsm_multi_task_view *view) {
	if (view == NULL || view->workspace_captures_started ||
			view->workspace_captures_deferred) {
		return;
	}
	view->workspace_captures_started = true;
	for (size_t i = 0; i < view->layouts_len; ++i) {
		start_workspace_captures(view->layouts[i]);
	}
}

static void hide_deferred_workspace_container(
		struct wsm_multi_task_view *view) {
	if (view == NULL || !view->workspace_captures_deferred) {
		return;
	}
	struct wsm_container *container = view->deferred_capture_container;
	if (container != NULL && !container->node.destroying &&
			container->scene_tree != NULL) {
		wlr_scene_node_set_enabled(&container->scene_tree->node, false);
	}
}

static void refresh_deferred_workspace_captures(
		struct wsm_multi_task_view *view) {
	if (view == NULL || !view->workspace_captures_deferred) {
		return;
	}
	struct wsm_container *container = view->deferred_capture_container;
	struct wsm_workspace *target = view->deferred_capture_target;
	if (container != NULL && !container->node.destroying &&
			container->current.workspace != target) {
		hide_deferred_workspace_container(view);
		return;
	}
	hide_deferred_workspace_container(view);
	view->workspace_captures_deferred = false;
	view->deferred_capture_container = NULL;
	view->deferred_capture_target = NULL;
	start_view_workspace_captures(view);
	for (size_t i = 0; i < view->layouts_len; ++i) {
		set_layout_progress(view->layouts[i],
			view->progress, view->progress);
	}
}

static void update_layout_pointer_hover(struct wsm_multi_task_view *view,
		struct wsm_multi_task_layout *layout) {
	int hovered_window = -1;
	int hovered_workspace = -1;
	if (view->progress > 0.999) {
		double lx = view->seat->cursor->cursor_wlr->x;
		double ly = view->seat->cursor->cursor_wlr->y;
		for (size_t i = layout->windows_len; i > 0; --i) {
			if (layout->windows[i - 1].container != NULL &&
					wlr_box_contains_point(
					&layout->windows[i - 1].hitbox, lx, ly)) {
				hovered_window = (int)i - 1;
				break;
			}
		}
		for (size_t i = layout->workspace_previews_len; i > 0; --i) {
			struct wsm_multi_task_workspace_preview *preview =
				&layout->workspace_previews[i - 1];
			if (wlr_box_contains_point(&preview->box, lx, ly) ||
					wlr_box_contains_point(
						&preview->label_hitbox, lx, ly) ||
					wlr_box_contains_point(
						&preview->close_hitbox, lx, ly)) {
				hovered_workspace = (int)i - 1;
				break;
			}
		}
		if (hovered_workspace >= 0) {
			start_view_workspace_captures(view);
		}
	}
	if (hovered_window != layout->hovered_window) {
		if (layout->hovered_window >= 0) {
			set_window_hovered(&layout->windows[
				layout->hovered_window], false);
		}
		layout->hovered_window = hovered_window;
		if (hovered_window >= 0) {
			set_window_hovered(
				&layout->windows[hovered_window], true);
		}
	}
	if (hovered_workspace != layout->hovered_workspace) {
		if (layout->hovered_workspace >= 0) {
			set_workspace_preview_hovered(
				&layout->workspace_previews[
					layout->hovered_workspace], false);
		}
		layout->hovered_workspace = hovered_workspace;
		if (hovered_workspace >= 0) {
			set_workspace_preview_hovered(
				&layout->workspace_previews[hovered_workspace], true);
		}
	}
}

static void update_pointer_hover(struct wsm_multi_task_view *view) {
	for (size_t i = 0; i < view->layouts_len; ++i) {
		update_layout_pointer_hover(view, view->layouts[i]);
	}
}

static void set_layout_progress(struct wsm_multi_task_layout *layout,
		double progress, double position) {
	float scrim[4] = {0};
	wlr_scene_rect_set_color(layout->scrim, scrim);
	set_sources_hidden(layout, progress > 0.001);
	if (layout->space_background != NULL) {
		wlr_scene_node_set_enabled(&layout->space_background->node,
			progress > 0.001);
	}

	for (size_t i = 0; i < layout->buffers_len; ++i) {
		struct wsm_multi_task_buffer *item = &layout->buffers[i];
		if (item->source == NULL && !item->snapshot) {
			continue;
		}
		/* Surface commits synchronize live content through the observer. The
		 * fallback comparison also catches lazily-created text/image buffers.
		 * Avoid reapplying unchanged buffer state for every gesture event: that
		 * work scales with every scene buffer in every preview. */
		if (item->source != NULL &&
				!layout->view->client_updates_suspended &&
				(!item->source_synced ||
				item->buffer->buffer != item->source->buffer)) {
			sync_buffer(item, NULL);
		}
		if (item->workspace_preview && item->geometry_synced) {
			wlr_scene_buffer_set_opacity(item->buffer,
				(item->snapshot ? 1.0f : item->source->opacity) *
					progress);
			continue;
		}
		double p = item->workspace_preview ? 1.0 : position;
		double x = item->workspace_preview
			? item->to_x
			: item->from_x + (item->to_x - item->from_x) * p;
		double y = item->workspace_preview
			? item->to_y
			: item->from_y + (item->to_y - item->from_y) * p;
		double width = item->workspace_preview ? item->to_width
						       : item->from_width +
				(item->to_width - item->from_width) * p;
		double height = item->workspace_preview ? item->to_height
							: item->from_height +
				(item->to_height - item->from_height) * p;
		struct wlr_box box = rounded_box(x, y, width, height);
		wlr_scene_node_set_position(
			&item->buffer->node, box.x, box.y);
		wlr_scene_buffer_set_dest_size(item->buffer,
			box.width, box.height);
		item->geometry_synced = item->workspace_preview;
		wlr_scene_buffer_set_opacity(item->buffer,
			(item->snapshot ? 1.0f : item->source->opacity) *
				(item->workspace_preview ? progress : 1.0f));
	}
	for (size_t i = 0; i < layout->rects_len; ++i) {
		struct wsm_multi_task_rect *item = &layout->rects[i];
		if (item->source == NULL) {
			continue;
		}
		sync_rect(item);
		double p = item->workspace_preview ? 1.0 : position;
		double x = item->workspace_preview
			? item->to_x
			: item->from_x + (item->to_x - item->from_x) * p;
		double y = item->workspace_preview
			? item->to_y
			: item->from_y + (item->to_y - item->from_y) * p;
		double width = item->workspace_preview ? item->to_width
						       : item->from_width +
				(item->to_width - item->from_width) * p;
		double height = item->workspace_preview ? item->to_height
							: item->from_height +
				(item->to_height - item->from_height) * p;
		float color[4];
		for (size_t channel = 0; channel < 4; ++channel) {
			color[channel] = item->color[channel];
		}
		if (item->workspace_preview) {
			for (size_t channel = 0; channel < 4; ++channel) {
				color[channel] *= progress;
			}
		}
		wlr_scene_rect_set_color(item->rect, color);
		if (item->workspace_preview && item->geometry_synced) {
			continue;
		}
		struct wlr_box box = rounded_box(x, y, width, height);
		wlr_scene_node_set_position(
			&item->rect->node, box.x, box.y);
		wlr_scene_rect_set_size(item->rect, box.width, box.height);
		item->geometry_synced = item->workspace_preview;
	}
	for (size_t i = 0; i < layout->windows_len; ++i) {
		struct wsm_multi_task_window *item = &layout->windows[i];
		if (item->container == NULL) {
			continue;
		}
		double x =
			item->from_x + (item->to_x - item->from_x) * position;
		double y =
			item->from_y + (item->to_y - item->from_y) * position;
		double width = item->from_width +
			(item->to_width - item->from_width) * position;
		double height = item->from_height +
			(item->to_height - item->from_height) * position;
		struct wlr_box box = rounded_box(x, y, width, height);
		wlr_scene_node_set_position(
			&item->backdrop->node, box.x, box.y);
		wlr_scene_rect_set_size(item->backdrop, box.width, box.height);
		item->hitbox = (struct wlr_box){
			.x = box.x + 8,
			.y = box.y + 8,
			.width = box.width > 16 ? box.width - 16 : 1,
			.height = box.height > 16 ? box.height - 16 : 1,
		};
		wsm_window_preview_set_geometry(item->preview, &item->hitbox);
		if (item->border != NULL) {
			wlr_scene_node_set_position(&item->border->node,
				item->hitbox.x - WINDOW_BORDER_WIDTH,
				item->hitbox.y - WINDOW_BORDER_WIDTH);
			wlr_scene_rect_set_size(item->border,
				item->hitbox.width + WINDOW_BORDER_WIDTH * 2,
				item->hitbox.height + WINDOW_BORDER_WIDTH * 2);
		}
		if (item->title_label != NULL) {
			int title_width = item->title_label->width < item->hitbox.width
				? item->title_label->width : item->hitbox.width;
			int background_width = title_width +
				WINDOW_TITLE_HORIZONTAL_PADDING * 2;
			if (background_width > item->hitbox.width) {
				background_width = item->hitbox.width;
			}
			int background_height = item->title_label->height +
				WINDOW_TITLE_VERTICAL_PADDING * 2;
			int background_x = item->hitbox.x +
				(item->hitbox.width - background_width) / 2;
			int background_y = item->hitbox.y +
				(item->hitbox.height - background_height) / 2;
			if (item->title_background != NULL) {
				if (item->container->title_bar != NULL &&
						item->container->title_bar->background != NULL) {
					wlr_scene_rect_set_color(item->title_background,
						item->container->title_bar->background->color);
				}
				wlr_scene_node_set_position(
					&item->title_background->node,
					background_x, background_y);
				wlr_scene_rect_set_size(item->title_background,
					background_width, background_height);
			}
			wlr_scene_node_set_position(item->title_label->node_wlr,
				background_x +
					(background_width - title_width) / 2,
				background_y + WINDOW_TITLE_VERTICAL_PADDING);
		}
	}
	float desktop_color[4] = {
		0.035f * progress, 0.04f * progress,
		0.055f * progress, (float)progress,
	};
	for (size_t i = 0; i < layout->space_desktops_len; ++i) {
		wlr_scene_rect_set_color(
			layout->space_desktops[i], desktop_color);
	}
	float border_color[4] = {
		0.30f * progress, 0.52f * progress,
		0.92f * progress, (float)progress,
	};
	for (size_t i = 0; i < layout->space_borders_len; ++i) {
		wlr_scene_rect_set_color(layout->space_borders[i], border_color);
		wlr_scene_node_set_enabled(
			&layout->space_borders[i]->node,
			layout->workspace_captures_started && progress > 0.001);
	}
	for (size_t i = 0; i < layout->workspace_previews_len; ++i) {
		struct wsm_multi_task_workspace_preview *preview =
			&layout->workspace_previews[i];
		if (preview->drop_border != NULL) {
			float drop_color[4] = {
				0.36f * progress, 0.76f * progress,
				1.0f * progress, (float)progress,
			};
			wlr_scene_rect_set_color(preview->drop_border, drop_color);
			wlr_scene_node_set_enabled(&preview->drop_border->node,
				layout->view->dragging &&
				layout->view->drag_target_workspace == preview->workspace &&
				progress > 0.001);
		}
		wsm_workspace_capture_set_opacity(preview->capture, progress);
		struct wsm_text_node *label = preview->name_label;
		if (label != NULL) {
			wlr_scene_buffer_set_opacity(
				wlr_scene_buffer_from_node(label->node_wlr), progress);
			wlr_scene_node_set_enabled(
				label->node_wlr, progress > 0.001);
		}
		if (preview->close_icon != NULL) {
			wlr_scene_buffer_set_opacity(wlr_scene_buffer_from_node(
				preview->close_icon->node_wlr), progress);
		}
	}
	float add_color[4] = {
		0.88f * progress, 0.88f * progress,
		0.90f * progress, (float)progress,
	};
	if (layout->add_workspace_horizontal != NULL) {
		wlr_scene_rect_set_color(
			layout->add_workspace_horizontal, add_color);
		wlr_scene_node_set_enabled(
			&layout->add_workspace_horizontal->node,
			layout->workspace_captures_started && progress > 0.001);
	}
	if (layout->add_workspace_vertical != NULL) {
		wlr_scene_rect_set_color(
			layout->add_workspace_vertical, add_color);
		wlr_scene_node_set_enabled(
			&layout->add_workspace_vertical->node,
			layout->workspace_captures_started && progress > 0.001);
	}
}

static void set_progress(struct wsm_multi_task_view *view, double progress) {
	progress = clamp(progress, 0.0, 1.0);
	view->progress = progress;
	wlr_scene_node_set_enabled(&view->tree->node, progress > 0.001);
	for (size_t i = 0; i < view->layouts_len; ++i) {
		set_layout_progress(view->layouts[i], progress, progress);
	}
	update_pointer_hover(view);
}

static void handle_layout_output_frame(
		struct wl_listener *listener, void *data) {
	struct wsm_multi_task_layout *layout = wl_container_of(
		listener, layout, output_frame);
	struct wsm_multi_task_view *view = layout->view;
	hide_deferred_workspace_container(view);
	refresh_deferred_workspace_captures(view);
	for (size_t i = 0; i < layout->workspace_previews_len; ++i) {
		struct wsm_workspace_capture *capture =
			layout->workspace_previews[i].capture;
		if (wsm_workspace_capture_is_dirty(capture) &&
				!wsm_workspace_capture_render(capture)) {
			wlr_output_schedule_frame(layout->output->wlr_output);
		}
	}
	/* Progress is global. Use one output as the animation clock so multiple
	 * output frame signals cannot traverse every preview layout more than once
	 * for a single compositor frame. Scene mutations damage all outputs and the
	 * remaining outputs consume that damage through their normal repaint path. */
	if (view->layouts_len == 0 || layout != view->layouts[0]) {
		return;
	}

	if (view->animation_running) {
		struct timespec now;
		clock_gettime(CLOCK_MONOTONIC, &now);
		double elapsed_msec =
			(now.tv_sec - view->animation_started.tv_sec) * 1000.0 +
			(now.tv_nsec - view->animation_started.tv_nsec) / 1000000.0;
		double time_progress = clamp(
			elapsed_msec / view->animation_duration_msec, 0.0, 1.0);
		double position = ease_out_cubic(time_progress);
		set_progress(view, view->animation_from +
			(view->animation_to - view->animation_from) * position);
		if (time_progress >= 1.0) {
			view->animation_running = false;
			view->animation_completion_pending = true;
		} else {
			request_scene_frame(view);
		}
	} else if (view->gesture_update_pending) {
		view->gesture_update_pending = false;
		set_progress(view, view->pending_gesture_progress);
	}

	if (view->animation_completion_pending) {
		view->animation_completion_pending = false;
		if (view->animation_to <= 0.0) {
			close_overview(view);
		} else {
			view->active = true;
			resume_client_updates(view);
		}
	}
}

static void handle_layout_output_disable(
		struct wl_listener *listener, void *data) {
	struct wsm_multi_task_layout *layout = wl_container_of(
		listener, layout, output_disable);
	close_overview(layout->view);
}

static void request_scene_frame(struct wsm_multi_task_view *view) {
	for (size_t i = 0; i < view->layouts_len; ++i) {
		struct wsm_output *output = view->layouts[i]->output;
		if (output->enabled && output->wlr_output->enabled) {
			wlr_output_schedule_frame(output->wlr_output);
		}
	}
}

static void queue_scene_progress(
		struct wsm_multi_task_view *view, double progress) {
	/* Input only publishes the newest state. Any number of input events before
	 * the next output frame collapse into one scene traversal. */
	view->pending_gesture_progress = clamp(progress, 0.0, 1.0);
	view->gesture_update_pending = true;
	request_scene_frame(view);
}

static void stop_settle_animation(struct wsm_multi_task_view *view) {
	view->animation_running = false;
	view->animation_completion_pending = false;
}

static bool begin_transition(struct wsm_multi_task_view *view) {
	if (view->layouts_len == 0 && !prepare_layouts(view)) {
		return false;
	}
	view->active = true;
	wlr_seat_pointer_notify_clear_focus(view->seat->seat);
	cursor_set_image(view->seat->cursor, "default", NULL);
	wlr_scene_node_raise_to_top(&view->tree->node);
	return true;
}

static void close_overview(struct wsm_multi_task_view *view) {
	stop_settle_animation(view);
	cancel_window_drag(view, false);
	view->gesture_update_pending = false;
	view->progress = 0.0;
	wlr_scene_node_set_enabled(&view->tree->node, false);
	view->client_updates_suspended = false;
	view->active = false;
	view->workspace_captures_started = false;
	view->workspace_captures_deferred = false;
	view->deferred_capture_container = NULL;
	view->deferred_capture_target = NULL;
	destroy_layouts(view);
	cursor_rebase(view->seat->cursor);
}

static void start_settle_animation(
		struct wsm_multi_task_view *view, double target) {
	target = clamp(target, 0.0, 1.0);
	stop_settle_animation(view);
	/* The latest input may not have reached an output frame yet. Settle from
	 * the published value without synchronously mutating the scene here. */
	double from = view->gesture_update_pending
		? view->pending_gesture_progress : view->progress;
	view->gesture_update_pending = false;
	double distance = fabs(target - from);
	view->animation_from = from;
	view->animation_to = target;
	if (distance <= 0.001) {
		view->pending_gesture_progress = target;
		view->gesture_update_pending = true;
		view->animation_completion_pending = true;
		request_scene_frame(view);
		return;
	}

	view->animation_duration_msec = clamp(
		SETTLE_MIN_DURATION_MSEC + distance * 140.0,
		SETTLE_MIN_DURATION_MSEC, SETTLE_MAX_DURATION_MSEC);
	clock_gettime(CLOCK_MONOTONIC, &view->animation_started);
	view->animation_running = true;
	request_scene_frame(view);
}

static struct wsm_multi_task_layout *layout_for_output(
		struct wsm_multi_task_view *view, struct wsm_output *output) {
	for (size_t i = 0; i < view->layouts_len; ++i) {
		if (view->layouts[i]->output == output) {
			return view->layouts[i];
		}
	}
	return NULL;
}

static struct wsm_multi_task_layout *current_layout(
		struct wsm_multi_task_view *view) {
	struct wsm_output *output = wsm_output_nearest_to(
		view->seat->cursor->cursor_wlr->x,
		view->seat->cursor->cursor_wlr->y);
	struct wsm_multi_task_layout *layout =
		layout_for_output(view, output);
	if (layout != NULL) {
		return layout;
	}
	struct wsm_workspace *workspace =
		seat_get_focused_workspace(view->seat);
	return workspace != NULL
		? layout_for_output(view, workspace->output) : NULL;
}

static bool workspace_name_exists(
		struct wsm_output *output, const char *name) {
	for (int i = 0; i < output->workspaces->length; ++i) {
		struct wsm_workspace *workspace = output->workspaces->items[i];
		if (strcmp(workspace->name, name) == 0) {
			return true;
		}
	}
	return false;
}

static bool add_overview_workspace(struct wsm_multi_task_view *view,
		struct wsm_multi_task_layout *layout) {
	if (layout == NULL || layout->output == NULL) {
		return false;
	}
	struct wsm_output *output = layout->output;
	unsigned int next_index = 0;
	for (int i = 0; i < output->workspaces->length; ++i) {
		const char *workspace_name =
			((struct wsm_workspace *)output->workspaces->items[i])->name;
		char *end = NULL;
		unsigned long index = strtoul(workspace_name, &end, 10);
		if (end != workspace_name && *end == '\0' && index < UINT_MAX &&
				index >= next_index) {
			next_index = (unsigned int)index + 1;
		}
	}
	char name[32];
	for (;; ++next_index) {
		snprintf(name, sizeof(name), "%u", next_index);
		if (!workspace_name_exists(output, name)) {
			break;
		}
	}
	if (workspace_create(output, name) == NULL) {
		return false;
	}
	output_sort_workspaces(output);
	transaction_commit_dirty();
	if (!prepare_layouts(view)) {
		close_overview(view);
		return false;
	}
	queue_scene_progress(view, 1.0);
	return true;
}

static bool remove_overview_workspace(struct wsm_multi_task_view *view,
		struct wsm_multi_task_layout *layout,
		struct wsm_workspace *workspace) {
	if (layout == NULL || layout->output == NULL ||
			workspace == NULL) {
		return false;
	}
	struct wsm_output *output = layout->output;
	if (output->workspaces->length <= 1) {
		return false;
	}
	int index = wsm_list_find(output->workspaces, workspace);
	if (index < 0) {
		return false;
	}
	struct wsm_workspace *target = output->workspaces->items[
		index > 0 ? index - 1 : index + 1];
	if (output_get_active_workspace(output) == workspace) {
		struct wsm_node *focus =
			seat_get_focus_inactive(view->seat, &target->node);
		seat_set_focus(view->seat,
			focus != NULL ? focus : &target->node);
		if (output_get_active_workspace(output) == workspace) {
			return false;
		}
	}
	while (workspace->tiling->length > 0) {
		workspace_add_tiling(
			target, workspace->tiling->items[0]);
	}
	while (workspace->floating->length > 0) {
		workspace_add_floating(
			target, workspace->floating->items[0]);
	}
	workspace_begin_destroy(workspace);
	wsm_arrange_workspace_auto(target);
	transaction_commit_dirty();
	if (!prepare_layouts(view)) {
		close_overview(view);
		return false;
	}
	queue_scene_progress(view, 1.0);
	return true;
}

static void activate_window(struct wsm_multi_task_view *view,
		struct wsm_container *container) {
	if (container == NULL || container->node.destroying ||
			container->view == NULL) {
		return;
	}
	if (!container->view->enabled) {
		view_minimize(container->view, false);
	}
	if (container_is_scratchpad_hidden_or_child(container)) {
		root_scratchpad_show(container);
	}
	seat_set_focus(view->seat, &container->node);
	container_raise(container);
	transaction_commit_dirty();
	close_overview(view);
}

static struct wsm_multi_task_window *find_overview_window(
		struct wsm_multi_task_layout *layout,
		struct wsm_container *container) {
	if (layout == NULL || container == NULL) {
		return NULL;
	}
	for (size_t i = 0; i < layout->windows_len; ++i) {
		if (layout->windows[i].container == container) {
			return &layout->windows[i];
		}
	}
	return NULL;
}

static void clear_overview_window_hover(
		struct wsm_multi_task_layout *layout,
		struct wsm_container *container) {
	struct wsm_multi_task_window *window =
		find_overview_window(layout, container);
	if (window != NULL) {
		set_window_hovered(window, false);
	}
}

static void translate_overview_window(struct wsm_multi_task_view *view,
		double dx, double dy) {
	struct wsm_multi_task_layout *layout = view->drag_layout;
	struct wsm_container *container = view->dragged_container;
	struct wsm_multi_task_window *window =
		find_overview_window(layout, container);
	if (window == NULL || (dx == 0.0 && dy == 0.0)) {
		return;
	}
	window->from_x += dx;
	window->from_y += dy;
	window->to_x += dx;
	window->to_y += dy;
	for (size_t i = 0; i < layout->buffers_len; ++i) {
		struct wsm_multi_task_buffer *item = &layout->buffers[i];
		if (item->container != container || item->workspace_preview) {
			continue;
		}
		item->from_x += dx;
		item->from_y += dy;
		item->to_x += dx;
		item->to_y += dy;
		item->geometry_synced = false;
	}
	for (size_t i = 0; i < layout->rects_len; ++i) {
		struct wsm_multi_task_rect *item = &layout->rects[i];
		if (item->container != container || item->workspace_preview) {
			continue;
		}
		item->from_x += dx;
		item->from_y += dy;
		item->to_x += dx;
		item->to_y += dy;
		item->geometry_synced = false;
	}
	set_layout_progress(layout, view->progress, view->progress);
}

static void set_workspace_drop_target(
		struct wsm_multi_task_view *view,
		struct wsm_workspace *target) {
	view->drag_target_workspace = target;
	for (size_t i = 0; i < view->layouts_len; ++i) {
		struct wsm_multi_task_layout *layout = view->layouts[i];
		for (size_t j = 0; j < layout->workspace_previews_len; ++j) {
			struct wsm_multi_task_workspace_preview *preview =
				&layout->workspace_previews[j];
			if (preview->close_icon != NULL) {
				wlr_scene_node_set_enabled(
					preview->close_icon->node_wlr, false);
			}
			if (preview->drop_border != NULL) {
				wlr_scene_node_set_enabled(&preview->drop_border->node,
					view->dragging && layout == view->drag_layout &&
					preview->workspace == target);
			}
		}
	}
}

static void update_window_drag_target(struct wsm_multi_task_view *view) {
	struct wsm_multi_task_layout *layout = view->drag_layout;
	struct wsm_container *container = view->dragged_container;
	if (layout == NULL || container == NULL) {
		set_workspace_drop_target(view, NULL);
		return;
	}
	double lx = view->seat->cursor->cursor_wlr->x;
	double ly = view->seat->cursor->cursor_wlr->y;
	struct wsm_workspace *source = container->pending.workspace;
	struct wsm_workspace *target = NULL;
	for (size_t i = layout->workspace_previews_len; i > 0; --i) {
		struct wsm_multi_task_workspace_preview *preview =
			&layout->workspace_previews[i - 1];
		if (preview->workspace != source &&
				!preview->workspace->node.destroying &&
				(wlr_box_contains_point(&preview->box, lx, ly) ||
				 wlr_box_contains_point(&preview->label_hitbox, lx, ly))) {
			target = preview->workspace;
			break;
		}
	}
	if (target != NULL) {
		start_view_workspace_captures(view);
	}
	set_workspace_drop_target(view, target);
}

static void cancel_window_drag(
		struct wsm_multi_task_view *view, bool restore_position) {
	if (view == NULL) {
		return;
	}
	if (restore_position && view->dragging &&
			view->drag_layout != NULL &&
			view->dragged_container != NULL) {
		translate_overview_window(view,
			view->drag_start_x - view->drag_last_x,
			view->drag_start_y - view->drag_last_y);
	}
	set_workspace_drop_target(view, NULL);
	view->drag_layout = NULL;
	view->dragged_container = NULL;
	view->drag_pending = false;
	view->dragging = false;
	view->drag_start_x = 0.0;
	view->drag_start_y = 0.0;
	view->drag_last_x = 0.0;
	view->drag_last_y = 0.0;
	if (view->active) {
		cursor_set_image(view->seat->cursor, "default", NULL);
	}
}

static bool move_overview_window_to_workspace(
		struct wsm_multi_task_view *view,
		struct wsm_container *container,
		struct wsm_workspace *target) {
	if (container == NULL || container->node.destroying ||
			container->view == NULL || target == NULL ||
			target->node.destroying) {
		return false;
	}
	struct wsm_workspace *source = container->pending.workspace;
	if (source == NULL || source == target || source->output != target->output) {
		return false;
	}
	bool floating = container_is_floating(container);
	if (floating) {
		workspace_add_floating(target, container);
	} else {
		workspace_add_tiling(target, container);
	}

	/* Moving the keyboard-focused window must not implicitly switch Spaces.
	 * Mission Control keeps showing the source Space after a drop. */
	struct wsm_node *focus =
		seat_get_focus_inactive(view->seat, &source->node);
	if (focus == &container->node ||
			(focus != NULL && focus->type == N_CONTAINER &&
			 container_has_ancestor(focus->container, container))) {
		focus = NULL;
	}
	seat_set_focus(view->seat, focus != NULL ? focus : &source->node);
	wsm_arrange_workspace_auto(source);
	wsm_arrange_workspace_auto(target);
	wsm_log(WSM_DEBUG,
		"Moved overview window '%s' from workspace '%s' to '%s' on output '%s'",
		view_get_title(container->view) != NULL
			? view_get_title(container->view) : "(untitled)",
		source->name, target->name,
		target->output->wlr_output->name);
	transaction_commit_dirty();
	view->workspace_captures_started = false;
	view->workspace_captures_deferred = true;
	view->deferred_capture_container = container;
	view->deferred_capture_target = target;
	if (!prepare_layouts(view)) {
		close_overview(view);
		return false;
	}
	queue_scene_progress(view, 1.0);
	return true;
}

static void begin_window_drag(struct wsm_multi_task_view *view,
		struct wsm_multi_task_layout *layout,
		struct wsm_container *container, double lx, double ly) {
	view->drag_layout = layout;
	view->dragged_container = container;
	view->drag_start_x = lx;
	view->drag_start_y = ly;
	view->drag_last_x = lx;
	view->drag_last_y = ly;
	view->drag_pending = true;
	view->dragging = false;
}

static void finish_window_drag(struct wsm_multi_task_view *view) {
	struct wsm_container *container = view->dragged_container;
	struct wsm_workspace *target = view->drag_target_workspace;
	bool dragging = view->dragging;
	if (dragging && target != NULL) {
		cancel_window_drag(view, false);
		if (!move_overview_window_to_workspace(view, container, target) &&
				view->active) {
			prepare_layouts(view);
			queue_scene_progress(view, 1.0);
		}
		return;
	}
	if (dragging) {
		cancel_window_drag(view, true);
		update_pointer_hover(view);
		return;
	}
	cancel_window_drag(view, false);
	activate_window(view, container);
}

void wsm_multi_task_view_handle_pointer_motion(
		struct wsm_multi_task_view *view) {
	if (view == NULL || !view->active) {
		return;
	}
	if (view->drag_pending) {
		double lx = view->seat->cursor->cursor_wlr->x;
		double ly = view->seat->cursor->cursor_wlr->y;
		if (!view->dragging) {
			double dx = lx - view->drag_start_x;
			double dy = ly - view->drag_start_y;
			if (hypot(dx, dy) < WINDOW_DRAG_THRESHOLD) {
				return;
			}
			view->dragging = true;
			clear_overview_window_hover(
				view->drag_layout, view->dragged_container);
			cursor_set_image(view->seat->cursor, "grab", NULL);
		}
		translate_overview_window(view,
			lx - view->drag_last_x, ly - view->drag_last_y);
		view->drag_last_x = lx;
		view->drag_last_y = ly;
		update_window_drag_target(view);
		return;
	}
	update_pointer_hover(view);
}

bool wsm_multi_task_view_blocks_pointer(
		const struct wsm_multi_task_view *view) {
	return view != NULL && (view->active || view->space_swipe != NULL);
}

bool wsm_multi_task_view_handle_button(struct wsm_multi_task_view *view,
		uint32_t button, enum wl_pointer_button_state state) {
	if (view == NULL || !view->active) {
		return false;
	}
	if (button != BTN_LEFT) {
		return true;
	}
	if (state == WL_POINTER_BUTTON_STATE_RELEASED) {
		if (view->drag_pending) {
			finish_window_drag(view);
		}
		return true;
	}
	if (state != WL_POINTER_BUTTON_STATE_PRESSED) {
		return true;
	}
	struct wsm_multi_task_layout *layout = current_layout(view);
	if (layout == NULL) {
		return true;
	}
	double lx = view->seat->cursor->cursor_wlr->x;
	double ly = view->seat->cursor->cursor_wlr->y;
	update_pointer_hover(view);
	if (layout->workspace_captures_started && wlr_box_contains_point(
			&layout->add_workspace_hitbox, lx, ly)) {
		add_overview_workspace(view, layout);
		return true;
	}
	for (size_t i = 0; i < layout->workspace_previews_len; ++i) {
		struct wsm_multi_task_workspace_preview *preview =
			&layout->workspace_previews[i];
		if ((int)i == layout->hovered_workspace &&
				preview->close_icon != NULL && wlr_box_contains_point(
				&preview->close_hitbox, lx, ly)) {
			remove_overview_workspace(view, layout, preview->workspace);
			return true;
		}
	}
	if (layout->hovered_window < 0) {
		return true;
	}
	struct wsm_multi_task_window *window = &layout->windows[
		layout->hovered_window];
	begin_window_drag(view, layout, window->container, lx, ly);
	return true;
}

static bool switch_workspace(struct wsm_multi_task_view *view,
		struct wsm_multi_task_layout *layout, int direction) {
	struct wsm_output *output = layout != NULL ? layout->output :
		gesture_target_output(view);
	if (output == NULL || output->workspaces == NULL ||
		output->workspaces->length < 2) {
		return false;
	}
	struct wsm_workspace *current = output_get_active_workspace(output);
	int index = wsm_list_find(output->workspaces, current);
	int next = index + direction;
	if (index < 0 || next < 0 || next >= output->workspaces->length) {
		return false;
	}
	struct wsm_workspace *workspace = output->workspaces->items[next];
	wsm_log(WSM_DEBUG,
		"Switching output '%s' from workspace '%s' to '%s'",
		output->wlr_output->name, current->name, workspace->name);
	struct wsm_node *focus =
		seat_get_focus_inactive(view->seat, &workspace->node);
	seat_set_focus(view->seat, focus != NULL ? focus : &workspace->node);
	transaction_commit_dirty();
	if (view->active) {
		prepare_layouts(view);
		queue_scene_progress(view, 1.0);
	}
	return true;
}

static void set_space_swipe_progress(
		struct wsm_space_swipe_transition *transition, double progress) {
	if (transition == NULL) {
		return;
	}
	transition->progress = clamp(progress, 0.0, 1.0);
	double travel = transition->progress *
		(transition->output_box.width + transition->gap);
	int source_offset = -lround(transition->direction * travel);
	int target_offset = transition->direction *
		(transition->output_box.width + transition->gap) -
		lround(transition->direction * travel);
	struct wsm_workspace *workspaces[] = {
		transition->source,
		transition->target,
	};
	struct wlr_scene_tree *capture_trees[] = {
		transition->source_capture_tree,
		transition->target_capture_tree,
	};
	int offsets[] = {
		source_offset,
		target_offset,
	};
	for (size_t i = 0; i < sizeof(workspaces) / sizeof(workspaces[0]); ++i) {
		struct wsm_workspace *workspace = workspaces[i];
		if (workspace == NULL) {
			continue;
		}
		if (capture_trees[i] != NULL) {
			wlr_scene_node_set_position(
				&capture_trees[i]->node,
				transition->output_box.x + offsets[i],
				transition->output_box.y);
		}
	}
	wlr_output_schedule_frame(transition->output->wlr_output);
}

static void hide_workspace_for_space_swipe(struct wsm_workspace *workspace) {
	if (workspace == NULL) {
		return;
	}
	wlr_scene_node_set_enabled(&workspace->layers.non_fullscreen->node, false);
	wlr_scene_node_set_enabled(&workspace->layers.fullscreen->node, false);
	for (int i = 0; i < workspace->current.floating->length; ++i) {
		struct wsm_container *floater = workspace->current.floating->items[i];
		wlr_scene_node_set_enabled(&floater->scene_tree->node, false);
	}
}

static void hide_shell_lower_layers_for_space_swipe(struct wsm_output *output) {
	if (output == NULL) {
		return;
	}
	wlr_scene_node_set_enabled(&output->layers.shell_background->node, false);
	wlr_scene_node_set_enabled(&output->layers.shell_bottom->node, false);
}

static struct wsm_workspace_capture *create_space_swipe_capture(
		struct wsm_space_swipe_transition *transition,
		struct wsm_workspace *workspace,
		struct wlr_scene_tree **capture_tree) {
	if (transition == NULL || workspace == NULL || capture_tree == NULL) {
		return NULL;
	}
	*capture_tree = wlr_scene_tree_create(transition->tree);
	if (*capture_tree == NULL) {
		return NULL;
	}
	struct wlr_box source_box = transition->output_box;
	struct wlr_box destination = {
		.width = transition->output_box.width,
		.height = transition->output_box.height,
	};
	struct wsm_workspace_capture *capture =
		wsm_workspace_capture_create_layer_options(
		*capture_tree, transition->output, workspace, &source_box,
		&destination, global_server.wlr_renderer,
		global_server.wlr_allocator, true, false, true,
		transition->output->wlr_output->scale);
	if (capture == NULL) {
		wlr_scene_node_destroy(&(*capture_tree)->node);
		*capture_tree = NULL;
		return NULL;
	}
	wsm_workspace_capture_set_opacity(capture, 1.0f);
	if (!wsm_workspace_capture_render(capture)) {
		wsm_workspace_capture_destroy(capture);
		wlr_scene_node_destroy(&(*capture_tree)->node);
		*capture_tree = NULL;
		return NULL;
	}
	return capture;
}

static void render_space_swipe_capture(
		struct wsm_space_swipe_transition *transition,
		struct wsm_workspace_capture *capture) {
	if (transition == NULL || capture == NULL ||
			!wsm_workspace_capture_is_dirty(capture)) {
		return;
	}
	if (!wsm_workspace_capture_render(capture)) {
		wlr_output_schedule_frame(transition->output->wlr_output);
	}
}

static void render_space_swipe_captures(
		struct wsm_space_swipe_transition *transition) {
	if (transition == NULL) {
		return;
	}
	render_space_swipe_capture(transition, transition->source_capture);
	render_space_swipe_capture(transition, transition->target_capture);
}

static void prepare_workspace_for_space_swipe(struct wsm_workspace *workspace) {
	if (workspace == NULL || workspace->output == NULL) {
		return;
	}
	struct wsm_output *output = workspace->output;
	wlr_scene_node_reparent(&workspace->layers.non_fullscreen->node,
		output->layers.tiling);
	wlr_scene_node_reparent(&workspace->layers.fullscreen->node,
		output->layers.fullscreen);
	if (workspace->current.fullscreen != NULL) {
		wlr_scene_node_set_enabled(
			&workspace->layers.non_fullscreen->node, false);
		wlr_scene_node_set_enabled(&workspace->layers.fullscreen->node, true);
		wlr_scene_rect_set_size(output->fullscreen_background,
			output->width, output->height);
		wsm_arrange_fullscreen(workspace->layers.fullscreen,
			workspace->current.fullscreen, workspace,
			output->width, output->height);
	} else {
		wlr_scene_node_set_enabled(
			&workspace->layers.non_fullscreen->node, true);
		wlr_scene_node_set_enabled(&workspace->layers.fullscreen->node, false);
		struct wlr_box *area = &output->usable_area;
		struct side_gaps *gaps = &workspace->current_gaps;
		arrange_workspace_tiling(workspace,
			area->width - gaps->left - gaps->right,
			area->height - gaps->top - gaps->bottom);
	}
	arrange_workspace_floating(workspace);
}

static void apply_space_swipe_target(
		struct wsm_space_swipe_transition *transition) {
	if (transition == NULL || transition->switch_applied ||
			transition->target == NULL) {
		return;
	}
	struct wsm_node *focus = seat_get_focus_inactive(
		transition->view->seat, &transition->target->node);
	wsm_log(WSM_DEBUG,
		"Completing interactive Space swipe on output '%s': '%s' -> '%s'",
		transition->output->wlr_output->name,
		transition->source->name, transition->target->name);
	seat_set_focus(transition->view->seat,
		focus != NULL ? focus : &transition->target->node);
	transaction_commit_dirty();
	transition->switch_applied = true;
}

static void destroy_space_swipe(struct wsm_multi_task_view *view) {
	if (view == NULL || view->space_swipe == NULL) {
		return;
	}
	struct wsm_space_swipe_transition *transition = view->space_swipe;
	view->space_swipe = NULL;
	if (!wl_list_empty(&transition->output_frame.link)) {
		wl_list_remove(&transition->output_frame.link);
	}
	if (!wl_list_empty(&transition->output_disable.link)) {
		wl_list_remove(&transition->output_disable.link);
	}
	wsm_workspace_capture_destroy(transition->source_capture);
	wsm_workspace_capture_destroy(transition->target_capture);
	if (transition->tree != NULL) {
		wlr_scene_node_destroy(&transition->tree->node);
	}
	arrange_root_scene(global_server.scene);
	damage_space_swipe_output(transition);
	free(transition);
	if (!view->active && view->layouts_len == 0) {
		wlr_scene_node_set_enabled(&view->tree->node, false);
	}
}

static void handle_space_swipe_output_disable(
		struct wl_listener *listener, void *data) {
	struct wsm_space_swipe_transition *transition = wl_container_of(
		listener, transition, output_disable);
	destroy_space_swipe(transition->view);
}

static void handle_space_swipe_output_frame(
		struct wl_listener *listener, void *data) {
	struct wsm_space_swipe_transition *transition = wl_container_of(
		listener, transition, output_frame);
	render_space_swipe_captures(transition);
	if (transition->finish_frames > 0) {
		if (--transition->finish_frames == 0) {
			destroy_space_swipe(transition->view);
		}
		return;
	}
	if (!transition->animation_running) {
		return;
	}
	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);
	double elapsed_msec =
		(now.tv_sec - transition->animation_started.tv_sec) * 1000.0 +
		(now.tv_nsec - transition->animation_started.tv_nsec) / 1000000.0;
	double time_progress = clamp(
		elapsed_msec / transition->animation_duration_msec, 0.0, 1.0);
	double position = ease_out_cubic(time_progress);
	set_space_swipe_progress(transition,
		transition->animation_from +
			(transition->animation_to - transition->animation_from) * position);
	if (time_progress < 1.0) {
		return;
	}
	transition->animation_running = false;
	if (transition->animation_to >= 0.999 && transition->target != NULL) {
		apply_space_swipe_target(transition);
		/* Let the workspace transaction settle before normal arrangement
		 * takes ownership of the real scene trees again. */
		transition->finish_frames = 1;
		damage_space_swipe_output(transition);
	} else {
		destroy_space_swipe(transition->view);
	}
}

static bool begin_space_swipe(
		struct wsm_multi_task_view *view, int direction) {
	if (view == NULL) {
		wsm_log(WSM_DEBUG, "Cannot start real Space swipe: no view");
		return false;
	}
	if (view->active || direction == 0) {
		wsm_log(WSM_DEBUG,
			"Cannot start real Space swipe: active=%s direction=%d",
			view->active ? "yes" : "no", direction);
		return false;
	}
	struct wsm_output *output = gesture_target_output(view);
	if (output == NULL || output->workspaces == NULL ||
			output->workspaces->length == 0) {
		wsm_log(WSM_DEBUG,
			"Cannot start real Space swipe: no usable output/workspaces");
		return false;
	}
	struct wsm_workspace *focused = seat_get_focused_workspace(view->seat);
	struct wsm_workspace *source =
		focused != NULL && focused->output == output
			? focused : output_get_active_workspace(output);
	int index = wsm_list_find(output->workspaces, source);
	if (index < 0) {
		wsm_log(WSM_DEBUG,
			"Cannot start real Space swipe: source workspace missing on output '%s'",
			output->wlr_output->name);
		return false;
	}
	int target_index = index + direction;
	struct wsm_workspace *target = target_index >= 0 &&
		target_index < output->workspaces->length
			? output->workspaces->items[target_index] : NULL;

	destroy_space_swipe(view);
	struct wsm_space_swipe_transition *transition =
		calloc(1, sizeof(*transition));
	if (transition == NULL) {
		wsm_log(WSM_DEBUG,
			"Cannot start real Space swipe: transition allocation failed");
		return false;
	}
	transition->view = view;
	transition->output = output;
	transition->source = source;
	transition->target = target;
	transition->direction = direction;
	wl_list_init(&transition->output_frame.link);
	wl_list_init(&transition->output_disable.link);
	output_get_box(output, &transition->output_box);
	transition->gap = (int)clamp(
		lround(transition->output_box.width * SPACE_SWIPE_GAP_RATIO),
		SPACE_SWIPE_GAP_MIN, SPACE_SWIPE_GAP_MAX);
	transition->finger_distance =
		calculate_space_swipe_finger_distance(view, output);
	view->space_swipe = transition;
	transition->tree = wlr_scene_tree_create(global_server.scene->layers.tiling);
	if (transition->tree == NULL) {
		wsm_log(WSM_DEBUG,
			"Cannot start real Space swipe: transition tree allocation failed");
		destroy_space_swipe(view);
		return false;
	}
	wlr_scene_node_place_below(&transition->tree->node,
		&output->layers.tiling->node);
	wsm_button_node_set_hovered(NULL, false);
	prepare_workspace_for_space_swipe(source);
	prepare_workspace_for_space_swipe(target);
	transition->source_capture = create_space_swipe_capture(
		transition, source, &transition->source_capture_tree);
	if (transition->source_capture == NULL) {
		wsm_log(WSM_DEBUG,
			"Cannot start real Space swipe: source capture failed");
		destroy_space_swipe(view);
		return false;
	}
	if (target != NULL) {
		transition->target_capture = create_space_swipe_capture(
			transition, target, &transition->target_capture_tree);
		if (transition->target_capture == NULL) {
			wsm_log(WSM_DEBUG,
				"Cannot start real Space swipe: target capture failed");
			destroy_space_swipe(view);
			return false;
		}
	}
	hide_workspace_for_space_swipe(source);
	hide_workspace_for_space_swipe(target);
	hide_shell_lower_layers_for_space_swipe(output);
	transition->output_frame.notify = handle_space_swipe_output_frame;
	wl_signal_add(&output->events.frame, &transition->output_frame);
	transition->output_disable.notify = handle_space_swipe_output_disable;
	wl_signal_add(&output->events.disable, &transition->output_disable);
	set_space_swipe_progress(transition, 0.0);
	wsm_log(WSM_DEBUG,
		"Starting real Space swipe on output '%s': source='%s' target='%s' gap=%d finger_distance=%.1f touchpad=%.1fx%.1fmm output=%dx%dmm",
		output->wlr_output->name, source->name,
		target != NULL ? target->name : "(edge)", transition->gap,
		space_swipe_finger_distance(transition),
		view->swipe_device_width_mm, view->swipe_device_height_mm,
		output->wlr_output->phys_width, output->wlr_output->phys_height);
	return true;
}

static void update_space_swipe(struct wsm_multi_task_view *view) {
	struct wsm_space_swipe_transition *transition = view->space_swipe;
	if (transition == NULL) {
		return;
	}
	double directional_distance =
		-view->swipe_dx * transition->direction;
	double travel = space_swipe_finger_distance(transition);
	double progress = fmax(0.0, directional_distance / travel);
	if (transition->target == NULL) {
		progress = fmin(SPACE_SWIPE_EDGE_MAX_PROGRESS,
			progress * SPACE_SWIPE_EDGE_RESISTANCE);
	}
	set_space_swipe_progress(transition, progress);
}

static void settle_space_swipe(
		struct wsm_multi_task_view *view, double target) {
	struct wsm_space_swipe_transition *transition = view->space_swipe;
	if (transition == NULL) {
		return;
	}
	if (transition->target == NULL) {
		target = 0.0;
	}
	target = target >= 0.5 ? 1.0 : 0.0;
	double distance = fabs(target - transition->progress);
	wsm_log(WSM_DEBUG,
		"Settling real Space swipe on output '%s': progress=%.3f target=%.0f velocity_x=%.3f",
		transition->output->wlr_output->name, transition->progress,
		target, view->swipe_velocity_x);
	transition->animation_from = transition->progress;
	transition->animation_to = target;
	transition->animation_duration_msec = (uint32_t)clamp(
		SETTLE_MIN_DURATION_MSEC + distance * 140.0,
		SETTLE_MIN_DURATION_MSEC, SETTLE_MAX_DURATION_MSEC);
	clock_gettime(CLOCK_MONOTONIC, &transition->animation_started);
	transition->animation_running = true;
	wlr_output_schedule_frame(transition->output->wlr_output);
}

struct wsm_multi_task_view *wsm_multi_task_view_create(struct wsm_seat *seat) {
	struct wsm_multi_task_view *view = calloc(1, sizeof(*view));
	if (view == NULL) {
		return NULL;
	}
	view->seat = seat;
	view->tree = wlr_scene_tree_create(seat->scene_tree);
	if (view->tree == NULL) {
		free(view);
		return NULL;
	}
	wlr_scene_node_set_enabled(&view->tree->node, false);
	return view;
}

void wsm_multi_task_view_destroy(struct wsm_multi_task_view *view) {
	if (view == NULL) {
		return;
	}
	cancel_window_drag(view, false);
	destroy_space_swipe(view);
	destroy_layouts(view);
	wlr_scene_node_destroy(&view->tree->node);
	free(view);
}

bool wsm_multi_task_view_handle_key(struct wsm_multi_task_view *view,
	const uint32_t *keysyms, size_t keysyms_len, uint32_t modifiers,
	uint32_t state) {
	if (view == NULL) {
		return false;
	}
	bool pressed = state == WL_KEYBOARD_KEY_STATE_PRESSED;
	bool w = has_keysym(keysyms, keysyms_len, XKB_KEY_w) ||
		has_keysym(keysyms, keysyms_len, XKB_KEY_W);
	if (w && (modifiers & WLR_MODIFIER_LOGO)) {
		if (pressed && !view->super_w_down) {
			view->super_w_down = true;
			if (view->active) {
				close_overview(view);
			} else if (begin_transition(view)) {
				queue_scene_progress(view, 1.0);
			}
		}
		if (!pressed) {
			view->super_w_down = false;
		}
		return true;
	}
	if (!pressed && w && view->super_w_down) {
		view->super_w_down = false;
		return true;
	}
	if (!view->active) {
		return false;
	}
	if (pressed && has_keysym(keysyms, keysyms_len, XKB_KEY_Escape)) {
		close_overview(view);
		return true;
	}
	if (pressed && has_keysym(keysyms, keysyms_len, XKB_KEY_Left)) {
		switch_workspace(view, current_layout(view), -1);
		return true;
	}
	if (pressed && has_keysym(keysyms, keysyms_len, XKB_KEY_Right)) {
		switch_workspace(view, current_layout(view), 1);
		return true;
	}
	/* The overview owns keyboard input while it is visible. Preview buffers
	 * deliberately have no wlr_scene_surface or container descriptor, so
	 * pointer and touch input cannot reach the previewed client either. */
	return true;
}

bool wsm_multi_task_view_handle_swipe_begin(struct wsm_multi_task_view *view,
	struct wlr_pointer_swipe_begin_event *event) {
	if (view == NULL || event->fingers != 4) {
		return false;
	}
	destroy_space_swipe(view);

	view->swipe_cancel_target = view->animation_running
		? view->animation_to
		: view->progress >= 0.5 ? 1.0 : 0.0;
	stop_settle_animation(view);
	view->swipe_start_progress = view->progress;
	view->swipe_dx = 0;
	view->swipe_dy = 0;
	view->swipe_velocity_x = 0;
	view->swipe_velocity_y = 0;
	store_swipe_device_size(view, event->pointer);
	view->swipe_last_time_msec = event->time_msec;
	view->swipe_fingers = 4;
	view->swipe_axis = WSM_MULTI_TASK_SWIPE_UNDECIDED;
	view->swipe_claimed = true;
	return true;
}

bool wsm_multi_task_view_handle_swipe_update(struct wsm_multi_task_view *view,
	struct wlr_pointer_swipe_update_event *event) {
	if (view == NULL || !view->swipe_claimed) {
		return false;
	}
	view->swipe_dx += event->dx;
	view->swipe_dy += event->dy;
	uint32_t elapsed = event->time_msec - view->swipe_last_time_msec;
	if (elapsed > 0 && elapsed < 100) {
		double velocity_x = event->dx / elapsed;
		double velocity = event->dy / elapsed;
		view->swipe_velocity_x = view->swipe_velocity_x == 0
			? velocity_x
			: view->swipe_velocity_x * 0.65 + velocity_x * 0.35;
		view->swipe_velocity_y = view->swipe_velocity_y == 0
			? velocity
			: view->swipe_velocity_y * 0.65 + velocity * 0.35;
	}
	view->swipe_last_time_msec = event->time_msec;

	if (view->swipe_axis == WSM_MULTI_TASK_SWIPE_UNDECIDED) {
		double abs_x = fabs(view->swipe_dx);
		double abs_y = fabs(view->swipe_dy);
		if (abs_y >= SWIPE_DIRECTION_LOCK_DISTANCE &&
				abs_y >= abs_x * SWIPE_DIRECTION_DOMINANCE) {
			view->swipe_axis = WSM_MULTI_TASK_SWIPE_VERTICAL;
			if (!view->active && !begin_transition(view)) {
				view->swipe_axis = WSM_MULTI_TASK_SWIPE_NONE;
				return true;
			}
			/* Capture and fully initialize the opening snapshot first, then
			 * keep that image unchanged until the gesture settles. */
			view->client_updates_suspended = true;
		} else if (abs_x >= SWIPE_DIRECTION_LOCK_DISTANCE &&
				abs_x >= abs_y * SWIPE_DIRECTION_DOMINANCE) {
			view->swipe_axis = WSM_MULTI_TASK_SWIPE_HORIZONTAL;
			if (!view->active) {
				int direction = view->swipe_dx < 0 ? 1 : -1;
				if (!begin_space_swipe(view, direction)) {
					wsm_log(WSM_DEBUG,
						"Failed to start real Space swipe: direction=%d dx=%.2f dy=%.2f active=%s",
						direction, view->swipe_dx, view->swipe_dy,
						view->active ? "yes" : "no");
				}
			}
		}
	}

	if (view->swipe_axis == WSM_MULTI_TASK_SWIPE_VERTICAL) {
		queue_scene_progress(view,
			view->swipe_start_progress -
				view->swipe_dy / SWIPE_DISTANCE);
	} else if (view->swipe_axis == WSM_MULTI_TASK_SWIPE_HORIZONTAL) {
		update_space_swipe(view);
	}
	return true;
}

bool wsm_multi_task_view_handle_swipe_end(struct wsm_multi_task_view *view,
	struct wlr_pointer_swipe_end_event *event) {
	if (view == NULL || !view->swipe_claimed) {
		return false;
	}
	uint32_t fingers = view->swipe_fingers;
	enum wsm_multi_task_swipe_axis axis = view->swipe_axis;
	view->swipe_claimed = false;
	view->swipe_axis = WSM_MULTI_TASK_SWIPE_NONE;
	if (event->cancelled) {
		if (axis == WSM_MULTI_TASK_SWIPE_HORIZONTAL &&
				view->space_swipe != NULL) {
			settle_space_swipe(view, 0.0);
		} else if (axis == WSM_MULTI_TASK_SWIPE_VERTICAL ||
				view->progress > 0.001) {
			start_settle_animation(view, view->swipe_cancel_target);
		}
		return true;
	}
	if (fingers == 4 && axis == WSM_MULTI_TASK_SWIPE_VERTICAL) {
		if (event->time_msec - view->swipe_last_time_msec > 80) {
			view->swipe_velocity_y = 0;
		}
		double current_progress = view->gesture_update_pending
			? view->pending_gesture_progress : view->progress;
		double projected_progress = current_progress -
			view->swipe_velocity_y * SWIPE_PROJECTION_MSEC /
				SWIPE_DISTANCE;
		start_settle_animation(
			view, projected_progress >= 0.5 ? 1.0 : 0.0);
	} else if (fingers == 4 && axis == WSM_MULTI_TASK_SWIPE_HORIZONTAL &&
			view->space_swipe != NULL) {
		if (event->time_msec - view->swipe_last_time_msec > 80) {
			view->swipe_velocity_x = 0;
		}
		struct wsm_space_swipe_transition *transition = view->space_swipe;
		double travel = space_swipe_finger_distance(transition);
		double projected_progress = transition->progress +
			(-view->swipe_velocity_x * transition->direction) *
				SWIPE_PROJECTION_MSEC / travel;
		settle_space_swipe(view,
			projected_progress >= SPACE_SWIPE_COMPLETE_THRESHOLD ? 1.0 : 0.0);
	} else if (fingers == 4 && axis == WSM_MULTI_TASK_SWIPE_HORIZONTAL &&
			fabs(view->swipe_dx) >= HORIZONTAL_SWIPE_THRESHOLD) {
		/* Overview mode and allocation failures retain the discrete fallback. */
		switch_workspace(view, current_layout(view),
			view->swipe_dx < 0 ? 1 : -1);
	} else if (view->progress > 0.001 && view->progress < 0.999) {
		start_settle_animation(view, view->swipe_cancel_target);
	}
	return true;
}
