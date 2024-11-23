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

/**
 * @brief Enumeration of input idle sources
 */
enum wsm_input_idle_source {
	IDLE_SOURCE_KEYBOARD = 1 << 0, /**< Idle source for keyboard */
	IDLE_SOURCE_POINTER = 1 << 1, /**< Idle source for pointer */
	IDLE_SOURCE_TOUCH = 1 << 2, /**< Idle source for touch */
	IDLE_SOURCE_TABLET_PAD = 1 << 3, /**< Idle source for tablet pad */
	IDLE_SOURCE_TABLET_TOOL = 1 << 4, /**< Idle source for tablet tool */
	IDLE_SOURCE_SWITCH = 1 << 5, /**< Idle source for switch */
};

/**
 * @brief Structure representing a device associated with a seat
 */
struct wsm_seat_device {
	struct wl_list link; /**< Link for managing a list of seat devices */
	struct wsm_seat *seat; /**< Pointer to the associated WSM seat */
	struct wsm_input_device *input_device; /**< Pointer to the associated input device */
	struct wsm_pointer *pointer; /**< Pointer to the associated pointer */
	struct wsm_keyboard *keyboard; /**< Pointer to the associated keyboard */
	struct wsm_switch *_switch; /**< Pointer to the associated switch */
	struct wsm_tablet *tablet; /**< Pointer to the associated tablet */
	struct wsm_tablet_pad *tablet_pad; /**< Pointer to the associated tablet pad */
};

/**
 * @brief Structure representing a node in the seat's focus stack
 */
struct wsm_seat_node {
	struct wl_listener destroy; /**< Listener for destruction events */
	struct wl_list link; /**< Link for managing the focus stack */

	struct wsm_seat *seat; /**< Pointer to the associated WSM seat */
	struct wsm_node *node; /**< Pointer to the associated WSM node */
};

/**
 * @brief Structure representing a seat in the WSM
 */
struct wsm_seat {
	struct wsm_input_method_relay im_relay; /**< Input method relay for the seat */

	struct wl_listener focus_destroy; /**< Listener for focus destruction events */
	struct wl_listener new_node; /**< Listener for new node events */
	struct wl_listener request_start_drag; /**< Listener for drag request events */
	struct wl_listener start_drag; /**< Listener for start drag events */
	struct wl_listener request_set_selection; /**< Listener for selection request events */
	struct wl_listener request_set_primary_selection; /**< Listener for primary selection request events */
	struct wl_listener destroy; /**< Listener for destruction events */

	struct wl_list link; /**< Link for managing a list of seats */

	struct wl_list focus_stack; /**< List of focused nodes */
	struct wl_list devices; /**< List of devices associated with the seat */
	struct wl_list keyboard_groups; /**< List of keyboard groups associated with the seat */
	struct wl_list keyboard_shortcuts_inhibitors; /**< List of keyboard shortcuts inhibitors */

	struct wsm_list *deferred_bindings; /**< List of deferred bindings */
	struct wlr_seat *seat; /**< Pointer to the WLR seat instance */
	struct wsm_cursor *cursor; /**< Pointer to the associated cursor */

	struct wlr_scene_tree *scene_tree; /**< Pointer to the scene tree for the seat */
	struct wlr_scene_tree *drag_icons; /**< Pointer to the scene tree for drag icons */

	struct wsm_workspace *workspace; /**< Pointer to the associated workspace */
	char *prev_workspace_name; /**< Name of the previous workspace */
	struct wlr_layer_surface_v1 *focused_layer_wlr; /**< Pointer to the focused layer surface */

	struct wl_client *exclusive_client; /**< Pointer to the exclusive client for the seat */

	const struct wsm_seatop_impl *seatop_impl; /**< Pointer to the seat operation implementation */
	void *seatop_data; /**< Pointer to additional seat operation data */

	double touch_x, touch_y; /**< Coordinates of the last touch event */
	int32_t touch_id; /**< ID of the last touch event */
	uint32_t last_button_serial; /**< Serial number of the last button event */
	uint32_t idle_inhibit_sources, idle_wake_sources; /**< Sources for idle inhibition and wake */
	bool has_focus; /**< Flag indicating if the seat has focus */
	bool has_exclusive_layer; /**< Flag indicating if the seat has an exclusive layer */
};

/**
 * @brief Structure representing the implementation of seat operations
 */
struct wsm_seatop_impl {
	void (*button)(struct wsm_seat *seat, uint32_t time_msec,
		struct wlr_input_device *device, uint32_t button, enum wl_pointer_button_state state); /**< Button event handler */
	void (*pointer_motion)(struct wsm_seat *seat, uint32_t time_msec); /**< Pointer motion event handler */
	void (*pointer_axis)(struct wsm_seat *seat, struct wlr_pointer_axis_event *event); /**< Pointer axis event handler */
	void (*hold_begin)(struct wsm_seat *seat, struct wlr_pointer_hold_begin_event *event); /**< Hold begin event handler */
	void (*hold_end)(struct wsm_seat *seat, struct wlr_pointer_hold_end_event *event); /**< Hold end event handler */
	void (*pinch_begin)(struct wsm_seat *seat, struct wlr_pointer_pinch_begin_event *event); /**< Pinch begin event handler */
	void (*pinch_update)(struct wsm_seat *seat, struct wlr_pointer_pinch_update_event *event); /**< Pinch update event handler */
	void (*pinch_end)(struct wsm_seat *seat, struct wlr_pointer_pinch_end_event *event); /**< Pinch end event handler */
	void (*swipe_begin)(struct wsm_seat *seat, struct wlr_pointer_swipe_begin_event *event); /**< Swipe begin event handler */
	void (*swipe_update)(struct wsm_seat *seat, struct wlr_pointer_swipe_update_event *event); /**< Swipe update event handler */
	void (*swipe_end)(struct wsm_seat *seat, struct wlr_pointer_swipe_end_event *event); /**< Swipe end event handler */
	void (*rebase)(struct wsm_seat *seat, uint32_t time_msec); /**< Rebase event handler */
	void (*touch_motion)(struct wsm_seat *seat,
		struct wlr_touch_motion_event *event, double lx, double ly); /**< Touch motion event handler */
	void (*touch_up)(struct wsm_seat *seat, struct wlr_touch_up_event *event); /**< Touch up event handler */
	void (*touch_down)(struct wsm_seat *seat,
		struct wlr_touch_down_event *event, double lx, double ly); /**< Touch down event handler */
	void (*touch_cancel)(struct wsm_seat *seat, struct wlr_touch_cancel_event *event); /**< Touch cancel event handler */
	void (*tablet_tool_motion)(struct wsm_seat *seat,
		struct wsm_tablet_tool *tool, uint32_t time_msec); /**< Tablet tool motion event handler */
	void (*tablet_tool_tip)(struct wsm_seat *seat, struct wsm_tablet_tool *tool,
		uint32_t time_msec, enum wlr_tablet_tool_tip_state state); /**< Tablet tool tip event handler */
	void (*end)(struct wsm_seat *seat); /**< End event handler */
	void (*unref)(struct wsm_seat *seat, struct wsm_container *con); /**< Unreference event handler */
	bool allow_set_cursor; /**< Flag indicating if setting the cursor is allowed */
};

/**
 * @brief Creates a new seat instance
 * @param seat_name Name of the seat to create
 * @return Pointer to the newly created wsm_seat instance
 */
struct wsm_seat *seat_create(const char *seat_name);

/**
 * @brief Destroys the specified seat instance
 * @param seat Pointer to the wsm_seat instance to destroy
 */
void seat_destroy(struct wsm_seat *seat);

/**
 * @brief Adds a device to the specified seat
 * @param seat Pointer to the wsm_seat instance
 * @param device Pointer to the wsm_input_device to add
 */
void seat_add_device(struct wsm_seat *seat, struct wsm_input_device *device);

/**
 * @brief Configures a device for the specified seat
 * @param seat Pointer to the wsm_seat instance
 * @param device Pointer to the wsm_input_device to configure
 */
void seat_configure_device(struct wsm_seat *seat, struct wsm_input_device *device);

/**
 * @brief Resets a device for the specified seat
 * @param seat Pointer to the wsm_seat instance
 * @param input_device Pointer to the wsm_input_device to reset
 */
void seat_reset_device(struct wsm_seat *seat, struct wsm_input_device *input_device);

/**
 * @brief Removes a device from the specified seat
 * @param seat Pointer to the wsm_seat instance
 * @param device Pointer to the wsm_input_device to remove
 */
void seat_remove_device(struct wsm_seat *seat, struct wsm_input_device *device);

/**
 * @brief Notifies the seat of activity from an idle source
 * @param seat Pointer to the wsm_seat instance
 * @param source Type of the idle source
 */
void seat_idle_notify_activity(struct wsm_seat *seat, enum wlr_input_device_type source);

/**
 * @brief Handles tablet tool tip events for the specified seat
 * @param seat Pointer to the wsm_seat instance
 * @param tool Pointer to the wsm_tablet_tool instance
 * @param time_msec Time in milliseconds
 * @param state State of the tablet tool tip
 */
void seatop_tablet_tool_tip(struct wsm_seat *seat,
	struct wsm_tablet_tool *tool, uint32_t time_msec, enum wlr_tablet_tool_tip_state state);

/**
 * @brief Handles hold begin events for the specified seat
 * @param seat Pointer to the wsm_seat instance
 * @param event Pointer to the hold begin event
 */
void seatop_hold_begin(struct wsm_seat *seat, struct wlr_pointer_hold_begin_event *event);

/**
 * @brief Handles hold end events for the specified seat
 * @param seat Pointer to the wsm_seat instance
 * @param event Pointer to the hold end event
 */
void seatop_hold_end(struct wsm_seat *seat, struct wlr_pointer_hold_end_event *event);

/**
 * @brief Handles pinch begin events for the specified seat
 * @param seat Pointer to the wsm_seat instance
 * @param event Pointer to the pinch begin event
 */
void seatop_pinch_begin(struct wsm_seat *seat, struct wlr_pointer_pinch_begin_event *event);

/**
 * @brief Handles pinch update events for the specified seat
 * @param seat Pointer to the wsm_seat instance
 * @param event Pointer to the pinch update event
 */
void seatop_pinch_update(struct wsm_seat *seat, struct wlr_pointer_pinch_update_event *event);

/**
 * @brief Handles pinch end events for the specified seat
 * @param seat Pointer to the wsm_seat instance
 * @param event Pointer to the pinch end event
 */
void seatop_pinch_end(struct wsm_seat *seat, struct wlr_pointer_pinch_end_event *event);

/**
 * @brief Handles swipe begin events for the specified seat
 * @param seat Pointer to the wsm_seat instance
 * @param event Pointer to the swipe begin event
 */
void seatop_swipe_begin(struct wsm_seat *seat, struct wlr_pointer_swipe_begin_event *event);

/**
 * @brief Handles swipe update events for the specified seat
 * @param seat Pointer to the wsm_seat instance
 * @param event Pointer to the swipe update event
 */
void seatop_swipe_update(struct wsm_seat *seat, struct wlr_pointer_swipe_update_event *event);

/**
 * @brief Handles swipe end events for the specified seat
 * @param seat Pointer to the wsm_seat instance
 * @param event Pointer to the swipe end event
 */
void seatop_swipe_end(struct wsm_seat *seat, struct wlr_pointer_swipe_end_event *event);

/**
 * @brief Handles pointer motion events for the specified seat
 * @param seat Pointer to the wsm_seat instance
 * @param time_msec Time in milliseconds
 */
void seatop_pointer_motion(struct wsm_seat *seat, uint32_t time_msec);

/**
 * @brief Handles pointer axis events for the specified seat
 * @param seat Pointer to the wsm_seat instance
 * @param event Pointer to the pointer axis event
 */
void seatop_pointer_axis(struct wsm_seat *seat, struct wlr_pointer_axis_event *event);

/**
 * @brief Handles button events for the specified seat
 * @param seat Pointer to the wsm_seat instance
 * @param time_msec Time in milliseconds
 * @param device Pointer to the WLR input device
 * @param button Button identifier
 * @param state State of the button (pressed/released)
 */
void seatop_button(struct wsm_seat *seat, uint32_t time_msec,
	struct wlr_input_device *device, uint32_t button, enum wl_pointer_button_state state);

/**
 * @brief Handles touch motion events for the specified seat
 * @param seat Pointer to the wsm_seat instance
 * @param event Pointer to the touch motion event
 * @param lx X coordinate in layout coordinates
 * @param ly Y coordinate in layout coordinates
 */
void seatop_touch_motion(struct wsm_seat *seat,
	struct wlr_touch_motion_event *event, double lx, double ly);

/**
 * @brief Handles touch up events for the specified seat
 * @param seat Pointer to the wsm_seat instance
 * @param event Pointer to the touch up event
 */
void seatop_touch_up(struct wsm_seat *seat, struct wlr_touch_up_event *event);

/**
 * @brief Handles touch down events for the specified seat
 * @param seat Pointer to the wsm_seat instance
 * @param event Pointer to the touch down event
 * @param lx X coordinate in layout coordinates
 * @param ly Y coordinate in layout coordinates
 */
void seatop_touch_down(struct wsm_seat *seat,
	struct wlr_touch_down_event *event, double lx, double ly);

/**
 * @brief Handles touch cancel events for the specified seat
 * @param seat Pointer to the wsm_seat instance
 * @param event Pointer to the touch cancel event
 */
void seatop_touch_cancel(struct wsm_seat *seat, struct wlr_touch_cancel_event *event);

/**
 * @brief Rebase the seat for the specified time
 * @param seat Pointer to the wsm_seat instance
 * @param time_msec Time in milliseconds
 */
void seatop_rebase(struct wsm_seat *seat, uint32_t time_msec);

/**
 * @brief Ends the seat operation
 * @param seat Pointer to the wsm_seat instance
 */
void seatop_end(struct wsm_seat *seat);

/**
 * @brief Handles tablet tool motion events for the specified seat
 * @param seat Pointer to the wsm_seat instance
 * @param tool Pointer to the wsm_tablet_tool instance
 * @param time_msec Time in milliseconds
 */
void seatop_tablet_tool_motion(struct wsm_seat *seat,
	struct wsm_tablet_tool *tool, uint32_t time_msec);

/**
 * @brief Checks if setting the cursor is allowed for the specified seat
 * @param seat Pointer to the wsm_seat instance
 * @return true if setting the cursor is allowed, false otherwise
 */
bool seatop_allows_set_cursor(struct wsm_seat *seat);

/**
 * @brief Adds a device to the specified seat
 * @param seat Pointer to the wsm_seat instance
 * @param device Pointer to the wsm_input_device to add
 */
void seat_add_device(struct wsm_seat *seat, struct wsm_input_device *device);

/**
 * @brief Sets focus to the specified surface for the seat
 * @param seat Pointer to the wsm_seat instance
 * @param surface Pointer to the WLR surface to focus
 * @param unfocus Boolean indicating if the surface should be unfocused
 */
void seat_set_focus_surface(struct wsm_seat *seat, struct wlr_surface *surface, bool unfocus);

/**
 * @brief Sets focus to the specified layer for the seat
 * @param seat Pointer to the wsm_seat instance
 * @param layer Pointer to the WLR layer surface to focus
 */
void seat_set_focus_layer(struct wsm_seat *seat, struct wlr_layer_surface_v1 *layer);

/**
 * @brief Notifies the seat of a pointer button event
 * @param seat Pointer to the wsm_seat instance
 * @param time_msec Time in milliseconds
 * @param button Button identifier
 * @param state State of the button (pressed/released)
 */
void seat_pointer_notify_button(struct wsm_seat *seat, uint32_t time_msec,
	uint32_t button, enum wl_pointer_button_state state);

/**
 * @brief Sets focus to the specified workspace for the seat
 * @param seat Pointer to the wsm_seat instance
 * @param ws Pointer to the wsm_workspace to focus
 */
void seat_set_focus_workspace(struct wsm_seat *seat, struct wsm_workspace *ws);

/**
 * @brief Sets focus to the specified node for the seat
 * @param seat Pointer to the wsm_seat instance
 * @param node Pointer to the wsm_node to focus
 */
void seat_set_focus(struct wsm_seat *seat, struct wsm_node *node);

/**
 * @brief Retrieves the currently focused node for the specified seat
 * @param seat Pointer to the wsm_seat instance
 * @return Pointer to the currently focused wsm_node
 */
struct wsm_node *seat_get_focus(struct wsm_seat *seat);

/**
 * @brief Retrieves the currently focused workspace for the specified seat
 * @param seat Pointer to the wsm_seat instance
 * @return Pointer to the currently focused wsm_workspace
 */
struct wsm_workspace *seat_get_focused_workspace(struct wsm_seat *seat);

/**
 * @brief Retrieves the inactive focus node for the specified seat
 * @param seat Pointer to the wsm_seat instance
 * @param node Pointer to the wsm_node to check
 * @return Pointer to the inactive focus wsm_node
 */
struct wsm_node *seat_get_focus_inactive(struct wsm_seat *seat, struct wsm_node *node);

/**
 * @brief Retrieves the inactive focus view for the specified seat
 * @param seat Pointer to the wsm_seat instance
 * @param ancestor Pointer to the ancestor wsm_node
 * @return Pointer to the inactive focus wsm_container
 */
struct wsm_container *seat_get_focus_inactive_view(struct wsm_seat *seat,
	struct wsm_node *ancestor);

/**
 * @brief Retrieves the last known workspace for the specified seat
 * @param seat Pointer to the wsm_seat instance
 * @return Pointer to the last known wsm_workspace
 */
struct wsm_workspace *seat_get_last_known_workspace(struct wsm_seat *seat);

/**
 * @brief Retrieves the active tiling child for the specified seat
 * @param seat Pointer to the wsm_seat instance
 * @param parent Pointer to the parent wsm_node
 * @return Pointer to the active tiling child wsm_node
 */
struct wsm_node *seat_get_active_tiling_child(struct wsm_seat *seat,
	struct wsm_node *parent);

/**
 * @brief Sets raw focus to the specified node for the seat
 * @param seat Pointer to the wsm_seat instance
 * @param node Pointer to the wsm_node to focus
 */
void seat_set_raw_focus(struct wsm_seat *seat, struct wsm_node *node);

/**
 * @brief Configures the X cursor for the specified seat
 * @param seat Pointer to the wsm_seat instance
 */
void seat_configure_xcursor(struct wsm_seat *seat);

/**
 * @brief Retrieves the currently focused container for the specified seat
 * @param seat Pointer to the wsm_seat instance
 * @return Pointer to the currently focused wsm_container
 */
struct wsm_container *seat_get_focused_container(struct wsm_seat *seat);

/**
 * @brief Sets focus to the specified container for the seat
 * @param seat Pointer to the wsm_seat instance
 * @param con Pointer to the wsm_container to focus
 */
void seat_set_focus_container(struct wsm_seat *seat, struct wsm_container *con);

/**
 * @brief Unreferences the specified seat and container
 * @param seat Pointer to the wsm_seat instance
 * @param con Pointer to the wsm_container to unreference
 */
void seatop_unref(struct wsm_seat *seat, struct wsm_container *con);

/**
 * @brief Considers warping to the focus for the specified seat
 * @param seat Pointer to the wsm_seat instance
 */
void seat_consider_warp_to_focus(struct wsm_seat *seat);

/**
 * @brief Retrieves the inactive tiling focus for the specified seat
 * @param seat Pointer to the wsm_seat instance
 * @param workspace Pointer to the wsm_workspace to check
 * @return Pointer to the inactive tiling focus wsm_container
 */
struct wsm_container *seat_get_focus_inactive_tiling(struct wsm_seat *seat,
	struct wsm_workspace *workspace);

/**
 * @brief Checks if input is allowed for the specified seat and surface
 * @param seat Pointer to the wsm_seat instance
 * @param surface Pointer to the WLR surface to check
 * @return true if input is allowed, false otherwise
 */
bool seat_is_input_allowed(struct wsm_seat *seat,
	struct wlr_surface *surface);

/**
 * @brief Unfocuses the seat unless the specified client is active
 * @param seat Pointer to the wsm_seat instance
 * @param client Pointer to the Wayland client to check
 */
void seat_unfocus_unless_client(struct wsm_seat *seat, struct wl_client *client);

/**
 * @brief Configures the device mapping for the specified seat
 * @param seat Pointer to the wsm_seat instance
 * @param input_device Pointer to the wsm_input_device to configure
 */
void seat_configure_device_mapping(struct wsm_seat *seat,
	struct wsm_input_device *input_device);

#endif
