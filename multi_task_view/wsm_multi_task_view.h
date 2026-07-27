#ifndef WSM_MULTI_TASK_VIEW_H
#define WSM_MULTI_TASK_VIEW_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <time.h>

#include <wayland-server-protocol.h>

struct wlr_pointer_swipe_begin_event;
struct wlr_pointer_swipe_end_event;
struct wlr_pointer_swipe_update_event;
struct wlr_scene_rect;
struct wlr_scene_tree;
struct wsm_container;
struct wsm_multi_task_layout;
struct wsm_space_swipe_transition;
struct wsm_seat;
struct wsm_workspace;

enum wsm_multi_task_swipe_axis {
	WSM_MULTI_TASK_SWIPE_NONE,
	WSM_MULTI_TASK_SWIPE_UNDECIDED,
	WSM_MULTI_TASK_SWIPE_HORIZONTAL,
	WSM_MULTI_TASK_SWIPE_VERTICAL,
};

struct wsm_multi_task_view {
	struct wsm_seat *seat;
	struct wlr_scene_tree *tree;
	struct wsm_multi_task_layout **layouts;
	size_t layouts_len;

	double progress;
	double swipe_start_progress;
	double swipe_cancel_target;
	double swipe_dx;
	double swipe_dy;
	double swipe_velocity_x;
	double swipe_velocity_y;
	double swipe_device_width_mm;
	double swipe_device_height_mm;
	uint32_t swipe_last_time_msec;
	uint32_t swipe_fingers;
	enum wsm_multi_task_swipe_axis swipe_axis;
	bool swipe_claimed;
	struct wsm_space_swipe_transition *space_swipe;

	struct timespec animation_started;
	double animation_from;
	double animation_to;
	uint32_t animation_duration_msec;
	bool animation_running;
	bool animation_completion_pending;
	double pending_gesture_progress;
	bool gesture_update_pending;
	bool client_updates_suspended;
	bool workspace_captures_started;
	bool workspace_captures_deferred;
	struct wsm_container *deferred_capture_container;
	struct wsm_workspace *deferred_capture_target;

	struct wsm_multi_task_layout *drag_layout;
	struct wsm_container *dragged_container;
	struct wsm_workspace *drag_target_workspace;
	double drag_start_x;
	double drag_start_y;
	double drag_last_x;
	double drag_last_y;
	bool drag_pending;
	bool dragging;

	bool active;
	bool super_w_down;
};

struct wsm_multi_task_view *wsm_multi_task_view_create(struct wsm_seat *seat);
void wsm_multi_task_view_destroy(struct wsm_multi_task_view *view);
bool wsm_multi_task_view_handle_key(struct wsm_multi_task_view *view,
	const uint32_t *keysyms, size_t keysyms_len, uint32_t modifiers,
	uint32_t state);
void wsm_multi_task_view_handle_pointer_motion(
	struct wsm_multi_task_view *view);
bool wsm_multi_task_view_blocks_pointer(
	const struct wsm_multi_task_view *view);
bool wsm_multi_task_view_handle_button(struct wsm_multi_task_view *view,
	uint32_t button, enum wl_pointer_button_state state);
bool wsm_multi_task_view_handle_swipe_begin(struct wsm_multi_task_view *view,
	struct wlr_pointer_swipe_begin_event *event);
bool wsm_multi_task_view_handle_swipe_update(struct wsm_multi_task_view *view,
	struct wlr_pointer_swipe_update_event *event);
bool wsm_multi_task_view_handle_swipe_end(struct wsm_multi_task_view *view,
	struct wlr_pointer_swipe_end_event *event);

#endif
