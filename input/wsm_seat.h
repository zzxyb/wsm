#ifndef WSM_SEAT_H
#define WSM_SEAT_H

#include "wsm_input.h"
#include "wsm_text_input.h"

#include <stdbool.h>

#include <pixman.h>

#include <wayland-util.h>
#include <wayland-server-core.h>
#include <wayland-server-protocol.h>

#include <wlr/types/wlr_input_device.h>
#include <wlr/types/wlr_tablet_tool.h>

struct wlr_seat;
struct wlr_drag;
struct wlr_surface;
struct wlr_scene_tree;
struct wlr_layer_surface_v1;
struct wlr_pointer_axis_event;
struct wlr_pointer_hold_end_event;
struct wlr_pointer_pinch_end_event;
struct wlr_pointer_swipe_end_event;
struct wlr_pointer_hold_begin_event;
struct wlr_pointer_pinch_begin_event;
struct wlr_pointer_swipe_begin_event;
struct wlr_pointer_swipe_update_event;
struct wlr_pointer_pinch_update_event;
struct wlr_touch_motion_event;
struct wlr_touch_up_event;
struct wlr_touch_down_event;
struct wlr_touch_cancel_event;

struct wsm_node;
struct wsm_list;
struct wsm_cursor;
struct wsm_output;
struct wsm_keyboard;
struct wsm_switch;
struct wsm_tablet;
struct wsm_pointer;
struct wsm_container;
struct wsm_workspace;
struct wsm_tablet_pad;
struct wsm_input_device;
struct wsm_tablet_tool;
struct wsm_seatop_impl;

enum wsm_input_idle_source {
	IDLE_SOURCE_KEYBOARD = 1 << 0,
	IDLE_SOURCE_POINTER = 1 << 1,
	IDLE_SOURCE_TOUCH = 1 << 2,
	IDLE_SOURCE_TABLET_PAD = 1 << 3,
	IDLE_SOURCE_TABLET_TOOL = 1 << 4,
	IDLE_SOURCE_SWITCH = 1 << 5,
};

struct wsm_seat_device {
	struct wl_list link;
	struct wsm_seat *seat;
	struct wsm_input_device *input_device;
	struct wsm_pointer *pointer;
	struct wsm_keyboard *keyboard;
	struct wsm_switch *_switch;
	struct wsm_tablet *tablet;
	struct wsm_tablet_pad *tablet_pad;
};

struct wsm_seat_node {
	struct wl_listener destroy;
	struct wl_list link; // focus_stack

	struct wsm_seat *seat;
	struct wsm_node *node;
};

struct wsm_drag {
	struct wl_listener destroy;
	struct wsm_seat *seat;
	struct wlr_drag *drag;
};

struct wsm_seat {
	struct wsm_input_method_relay im_relay;

	struct wl_listener focus_destroy;
	struct wl_listener new_node;
	struct wl_listener request_start_drag;
	struct wl_listener start_drag;
	struct wl_listener request_set_selection;
	struct wl_listener request_set_primary_selection;
	struct wl_listener destroy;

	struct wl_list link;

	struct wl_list focus_stack;
	struct wl_list devices;
	struct wl_list keyboard_groups;
	struct wl_list keyboard_shortcuts_inhibitors;

	struct wsm_list *deferred_bindings;
	struct wlr_seat *seat;
	struct wsm_cursor *cursor;

	struct wlr_scene_tree *scene_tree;
	struct wlr_scene_tree *drag_icons;

	struct wsm_workspace *workspace;
	char *prev_workspace_name;
	struct wlr_layer_surface_v1 *focused_layer_wlr;

	struct wl_client *exclusive_client;

	const struct wsm_seatop_impl *seatop_impl;
	void *seatop_data;

	double touch_x, touch_y;
	int32_t touch_id;
	uint32_t last_button_serial;
	uint32_t idle_inhibit_sources, idle_wake_sources;
	bool has_focus;
	bool has_exclusive_layer;
};

struct wsm_seatop_impl {
	void (*button)(struct wsm_seat *seat, uint32_t time_msec,
		struct wlr_input_device *device, uint32_t button, enum wl_pointer_button_state state);
	void (*pointer_motion)(struct wsm_seat *seat, uint32_t time_msec);
	void (*pointer_axis)(struct wsm_seat *seat, struct wlr_pointer_axis_event *event);
	void (*hold_begin)(struct wsm_seat *seat, struct wlr_pointer_hold_begin_event *event);
	void (*hold_end)(struct wsm_seat *seat, struct wlr_pointer_hold_end_event *event);
	void (*pinch_begin)(struct wsm_seat *seat, struct wlr_pointer_pinch_begin_event *event);
	void (*pinch_update)(struct wsm_seat *seat, struct wlr_pointer_pinch_update_event *event);
	void (*pinch_end)(struct wsm_seat *seat, struct wlr_pointer_pinch_end_event *event);
	void (*swipe_begin)(struct wsm_seat *seat, struct wlr_pointer_swipe_begin_event *event);
	void (*swipe_update)(struct wsm_seat *seat, struct wlr_pointer_swipe_update_event *event);
	void (*swipe_end)(struct wsm_seat *seat, struct wlr_pointer_swipe_end_event *event);
	void (*rebase)(struct wsm_seat *seat, uint32_t time_msec);
	void (*touch_motion)(struct wsm_seat *seat,
		struct wlr_touch_motion_event *event, double lx, double ly);
	void (*touch_up)(struct wsm_seat *seat, struct wlr_touch_up_event *event);
	void (*touch_down)(struct wsm_seat *seat,
		struct wlr_touch_down_event *event, double lx, double ly);
	void (*touch_cancel)(struct wsm_seat *seat, struct wlr_touch_cancel_event *event);
	void (*tablet_tool_motion)(struct wsm_seat *seat,
		struct wsm_tablet_tool *tool, uint32_t time_msec);
	void (*tablet_tool_tip)(struct wsm_seat *seat, struct wsm_tablet_tool *tool,
		uint32_t time_msec, enum wlr_tablet_tool_tip_state state);
	void (*end)(struct wsm_seat *seat);
	void (*unref)(struct wsm_seat *seat, struct wsm_container *con);
	bool allow_set_cursor;
};

struct wsm_seat *seat_create(const char *seat_name);
void seat_destroy(struct wsm_seat *seat);
void seat_add_device(struct wsm_seat *seat, struct wsm_input_device *device);
void seat_configure_device(struct wsm_seat *seat, struct wsm_input_device *device);
void seat_reset_device(struct wsm_seat *seat, struct wsm_input_device *input_device);
void seat_remove_device(struct wsm_seat *seat, struct wsm_input_device *device);
void seat_idle_notify_activity(struct wsm_seat *seat, enum wlr_input_device_type source);
void seatop_tablet_tool_tip(struct wsm_seat *seat,
	struct wsm_tablet_tool *tool, uint32_t time_msec, enum wlr_tablet_tool_tip_state state);
void seatop_hold_begin(struct wsm_seat *seat, struct wlr_pointer_hold_begin_event *event);
void seatop_hold_end(struct wsm_seat *seat, struct wlr_pointer_hold_end_event *event);
void seatop_pinch_begin(struct wsm_seat *seat, struct wlr_pointer_pinch_begin_event *event);
void seatop_pinch_update(struct wsm_seat *seat, struct wlr_pointer_pinch_update_event *event);
void seatop_pinch_end(struct wsm_seat *seat, struct wlr_pointer_pinch_end_event *event);
void seatop_swipe_begin(struct wsm_seat *seat, struct wlr_pointer_swipe_begin_event *event);
void seatop_swipe_update(struct wsm_seat *seat, struct wlr_pointer_swipe_update_event *event);
void seatop_swipe_end(struct wsm_seat *seat, struct wlr_pointer_swipe_end_event *event);
void seatop_pointer_motion(struct wsm_seat *seat, uint32_t time_msec);
void seatop_pointer_axis(struct wsm_seat *seat, struct wlr_pointer_axis_event *event);
void seatop_button(struct wsm_seat *seat, uint32_t time_msec,
	struct wlr_input_device *device, uint32_t button, enum wl_pointer_button_state state);
void seatop_touch_motion(struct wsm_seat *seat,
	struct wlr_touch_motion_event *event, double lx, double ly);
void seatop_touch_up(struct wsm_seat *seat, struct wlr_touch_up_event *event);
void seatop_touch_down(struct wsm_seat *seat,
	struct wlr_touch_down_event *event, double lx, double ly);
void seatop_touch_cancel(struct wsm_seat *seat, struct wlr_touch_cancel_event *event);
void seatop_rebase(struct wsm_seat *seat, uint32_t time_msec);
void seatop_end(struct wsm_seat *seat);
void seatop_tablet_tool_motion(struct wsm_seat *seat,
	struct wsm_tablet_tool *tool, uint32_t time_msec);
bool seatop_allows_set_cursor(struct wsm_seat *seat);
void seat_add_device(struct wsm_seat *seat, struct wsm_input_device *device);
void seat_set_focus_surface(struct wsm_seat *seat, struct wlr_surface *surface, bool unfocus);
void seat_set_focus_layer(struct wsm_seat *seat, struct wlr_layer_surface_v1 *layer);
void drag_icons_update_position(struct wsm_seat *seat);
void seat_pointer_notify_button(struct wsm_seat *seat, uint32_t time_msec,
	uint32_t button, enum wl_pointer_button_state state);
void seat_set_focus_workspace(struct wsm_seat *seat, struct wsm_workspace *ws);
void seat_set_focus(struct wsm_seat *seat, struct wsm_node *node);
struct wsm_node *seat_get_focus(struct wsm_seat *seat);
struct wsm_workspace *seat_get_focused_workspace(struct wsm_seat *seat);
struct wsm_node *seat_get_focus_inactive(struct wsm_seat *seat, struct wsm_node *node);
struct wsm_container *seat_get_focus_inactive_view(struct wsm_seat *seat,
	struct wsm_node *ancestor);
struct wsm_workspace *seat_get_last_known_workspace(struct wsm_seat *seat);
struct wsm_node *seat_get_active_tiling_child(struct wsm_seat *seat,
	struct wsm_node *parent);
void seat_set_raw_focus(struct wsm_seat *seat, struct wsm_node *node);
void seat_configure_xcursor(struct wsm_seat *seat);
struct wsm_container *seat_get_focused_container(struct wsm_seat *seat);
void seat_set_focus_container(struct wsm_seat *seat, struct wsm_container *con);
void seatop_unref(struct wsm_seat *seat, struct wsm_container *con);
void seat_consider_warp_to_focus(struct wsm_seat *seat);
struct wsm_container *seat_get_focus_inactive_tiling(struct wsm_seat *seat,
	struct wsm_workspace *workspace);
bool seat_is_input_allowed(struct wsm_seat *seat,
	struct wlr_surface *surface);
void seat_unfocus_unless_client(struct wsm_seat *seat, struct wl_client *client);
void seat_configure_device_mapping(struct wsm_seat *seat,
	struct wsm_input_device *input_device);

#endif
