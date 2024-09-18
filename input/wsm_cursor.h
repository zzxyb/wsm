#ifndef WSM_CURSOR_H
#define WSM_CURSOR_H

#include <stdbool.h>

#include <pixman.h>

#include <wayland-util.h>
#include <wayland-server-core.h>
#include <wayland-server-protocol.h>

#include <wlr/types/wlr_input_device.h>

#define WSM_SCROLL_UP KEY_MAX + 1
#define WSM_SCROLL_DOWN KEY_MAX + 2
#define WSM_SCROLL_LEFT KEY_MAX + 3
#define WSM_SCROLL_RIGHT KEY_MAX + 4

struct wlr_cursor;
struct wsm_seat;
struct wlr_surface;
struct wlr_input_device;
struct wlr_xcursor_manager;
struct wlr_pointer_constraint_v1;
struct wlr_pointer_gestures_v1;
struct wlr_pointer_axis_event;

struct wsm_node;
struct wsm_server;
struct wsm_container;
struct wsm_workspace;

enum wsm_cursor_mode {
	WSM_CURSOR_PASSTHROUGH,
	WSM_CURSOR_MOVE,
	WSM_CURSOR_RESIZE,
};

enum seat_config_hide_cursor_when_typing {
	HIDE_WHEN_TYPING_DEFAULT,
	HIDE_WHEN_TYPING_ENABLE,
	HIDE_WHEN_TYPING_DISABLE,
};

struct wsm_cursor {
	struct wl_listener request_cursor;
	struct wl_listener request_set_shape;

	struct wl_listener hold_begin;
	struct wl_listener hold_end;
	struct wl_listener pinch_begin;
	struct wl_listener pinch_update;
	struct wl_listener pinch_end;
	struct wl_listener swipe_begin;
	struct wl_listener swipe_update;
	struct wl_listener swipe_end;

	struct wl_listener motion;
	struct wl_listener motion_absolute;
	struct wl_listener button;
	struct wl_listener axis;
	struct wl_listener frame;

	struct wl_listener touch_down;
	struct wl_listener touch_up;
	struct wl_listener touch_cancel;
	struct wl_listener touch_motion;
	struct wl_listener touch_frame;

	struct wl_listener tool_axis;
	struct wl_listener tool_tip;
	struct wl_listener tool_proximity;
	struct wl_listener tool_button;

	struct wl_listener request_set_cursor;
	struct wl_listener image_surface_destroy;

	struct wl_listener constraint_commit;

	pixman_region32_t confine; // invalid if active_constraint == NULL

	struct wl_list tablets;
	struct wl_list tablet_pads;
	struct {
		double x, y;
	} previous;

	struct wsm_seat *wsm_seat;
	struct wlr_cursor *wlr_cursor;

	const char *image;
	struct wl_client *image_client;
	struct wlr_surface *image_surface;

	struct wlr_pointer_constraint_v1 *active_constraint;

	struct wlr_pointer_gestures_v1 *pointer_gestures;

	struct wl_event_source *hide_source;

	size_t pressed_button_count;
	int32_t pointer_touch_id;
	uint32_t tool_buttons;
	int hotspot_x, hotspot_y;
	enum seat_config_hide_cursor_when_typing hide_when_typing;
	bool active_confine_requires_warp;
	bool simulating_pointer_from_touch;
	bool pointer_touch_up;
	bool simulating_pointer_from_tool_tip;
	bool simulating_pointer_from_tool_button;
	bool hidden;
};

struct wsm_cursor *wsm_cursor_create(const struct wsm_server* server, struct wsm_seat *seat);
void wsm_cursor_destroy(struct wsm_cursor *cursor);
void cursor_rebase_all(void);
void cursor_set_image(struct wsm_cursor *cursor, const char *image,
	struct wl_client *client);
void cursor_set_image_surface(struct wsm_cursor *cursor, struct wlr_surface *surface,
	int32_t hotspot_x, int32_t hotspot_y, struct wl_client *client);
void cursor_rebase(struct wsm_cursor *cursor);
void cursor_handle_activity_from_device(struct wsm_cursor *cursor,
	struct wlr_input_device *device);
void cursor_handle_activity_from_idle_source(struct wsm_cursor *cursor,
	enum wlr_input_device_type idle_source);
void cursor_unhide(struct wsm_cursor *cursor);
void pointer_motion(struct wsm_cursor *cursor, uint32_t time_msec,
	struct wlr_input_device *device, double dx, double dy,
	double dx_unaccel, double dy_unaccel);
void dispatch_cursor_axis(struct wsm_cursor *cursor,
	struct wlr_pointer_axis_event *event);
void cursor_update_image(struct wsm_cursor *cursor, struct wsm_node *node);
void cursor_warp_to_container(struct wsm_cursor *cursor,
	struct wsm_container *container, bool force);
void cursor_warp_to_workspace(struct wsm_cursor *cursor,
	struct wsm_workspace *workspace);
void wsm_cursor_constrain(struct wsm_cursor *cursor,
	struct wlr_pointer_constraint_v1 *constraint);
void warp_to_constraint_cursor_hint(struct wsm_cursor *cursor);
struct wsm_node *node_at_coords(
	struct wsm_seat *seat, double lx, double ly,
	struct wlr_surface **surface, double *sx, double *sy);
void dispatch_cursor_button(struct wsm_cursor *cursor,
	struct wlr_input_device *device, uint32_t time_msec, uint32_t button,
	enum wl_pointer_button_state state);

#endif
