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

/**
 * @brief Enumeration of cursor modes in the WSM
 */
enum wsm_cursor_mode {
	WSM_CURSOR_PASSTHROUGH, /**< Cursor mode where the cursor passes through to the underlying application */
	WSM_CURSOR_MOVE, /**< Cursor mode for moving windows or objects */
	WSM_CURSOR_RESIZE, /**< Cursor mode for resizing windows or objects */
};

/**
 * @brief Enumeration for seat configuration regarding cursor visibility when typing
 */
enum seat_config_hide_cursor_when_typing {
	HIDE_WHEN_TYPING_DEFAULT, /**< Default behavior for hiding the cursor when typing */
	HIDE_WHEN_TYPING_ENABLE, /**< Enable hiding the cursor when typing */
	HIDE_WHEN_TYPING_DISABLE, /**< Disable hiding the cursor when typing */
};

/**
 * @brief Structure representing a cursor in the WSM
 */
struct wsm_cursor {
	struct wl_listener request_cursor; /**< Listener for cursor request events */
	struct wl_listener request_set_shape; /**< Listener for setting cursor shape */

	struct wl_listener hold_begin; /**< Listener for hold begin events */
	struct wl_listener hold_end; /**< Listener for hold end events */
	struct wl_listener pinch_begin; /**< Listener for pinch begin events */
	struct wl_listener pinch_update; /**< Listener for pinch update events */
	struct wl_listener pinch_end; /**< Listener for pinch end events */
	struct wl_listener swipe_begin; /**< Listener for swipe begin events */
	struct wl_listener swipe_update; /**< Listener for swipe update events */
	struct wl_listener swipe_end; /**< Listener for swipe end events */

	struct wl_listener motion; /**< Listener for motion events */
	struct wl_listener motion_absolute; /**< Listener for absolute motion events */
	struct wl_listener button; /**< Listener for button events */
	struct wl_listener axis; /**< Listener for axis events */
	struct wl_listener frame; /**< Listener for frame events */

	struct wl_listener touch_down; /**< Listener for touch down events */
	struct wl_listener touch_up; /**< Listener for touch up events */
	struct wl_listener touch_cancel; /**< Listener for touch cancel events */
	struct wl_listener touch_motion; /**< Listener for touch motion events */
	struct wl_listener touch_frame; /**< Listener for touch frame events */

	struct wl_listener tool_axis; /**< Listener for tool axis events */
	struct wl_listener tool_tip; /**< Listener for tool tip events */
	struct wl_listener tool_proximity; /**< Listener for tool proximity events */
	struct wl_listener tool_button; /**< Listener for tool button events */

	struct wl_listener request_set_cursor; /**< Listener for setting the cursor */
	struct wl_listener image_surface_destroy; /**< Listener for image surface destruction events */

	struct wl_listener constraint_commit; /**< Listener for constraint commit events */

	pixman_region32_t confine; /**< Region to confine the cursor; invalid if active_constraint is NULL */

	struct wl_list tablets; /**< List of associated tablets */
	struct wl_list tablet_pads; /**< List of associated tablet pads */
	struct {
		double x, y; /**< Previous cursor coordinates */
	} previous;

	struct wsm_seat *seat_wsm; /**< Pointer to the associated WSM seat */
	struct wlr_cursor *cursor_wlr; /**< Pointer to the WLR cursor instance */

	const char *image; /**< Pointer to the image associated with the cursor */
	struct wl_client *image_client_wl; /**< Pointer to the associated Wayland client */
	struct wlr_surface *image_surface_wlr; /**< Pointer to the WLR surface for the image */

	struct wlr_pointer_constraint_v1 *active_constraint_wlr; /**< Pointer to the active pointer constraint */

	struct wlr_pointer_gestures_v1 *pointer_gestures; /**< Pointer to the pointer gestures instance */

	struct wl_event_source *hide_source; /**< Pointer to the event source for hiding the cursor */

	size_t pressed_button_count; /**< Count of currently pressed buttons */
	int32_t pointer_touch_id; /**< Touch ID for pointer events */
	uint32_t tool_buttons; /**< State of tool buttons */
	int hotspot_x, hotspot_y; /**< Hotspot coordinates for the cursor */
	enum seat_config_hide_cursor_when_typing hide_when_typing; /**< Configuration for hiding the cursor when typing */
	bool active_confine_requires_warp; /**< Flag indicating if active confine requires warping */
	bool simulating_pointer_from_touch; /**< Flag indicating if simulating pointer from touch */
	bool pointer_touch_up; /**< Flag indicating if the pointer touch is up */
	bool simulating_pointer_from_tool_tip; /**< Flag indicating if simulating pointer from tool tip */
	bool simulating_pointer_from_tool_button; /**< Flag indicating if simulating pointer from tool button */
	bool hidden; /**< Flag indicating if the cursor is hidden */
};

/**
 * @brief Creates a new wsm_cursor instance
 * @param server Pointer to the WSM server that will manage the cursor
 * @param seat Pointer to the associated WSM seat
 * @return Pointer to the newly created wsm_cursor instance
 */
struct wsm_cursor *wsm_cursor_create(const struct wsm_server* server, struct wsm_seat *seat);

/**
 * @brief Destroys the specified wsm_cursor instance
 * @param cursor Pointer to the wsm_cursor instance to be destroyed
 */
void wsm_cursor_destroy(struct wsm_cursor *cursor);

/**
 * @brief Rebases all cursors
 */
void cursor_rebase_all(void);

/**
 * @brief Sets the image for the specified cursor
 * @param cursor Pointer to the wsm_cursor instance
 * @param image Pointer to the image string
 * @param client Pointer to the Wayland client requesting the image
 */
void cursor_set_image(struct wsm_cursor *cursor, const char *image,
	struct wl_client *client);

/**
 * @brief Sets the image surface for the specified cursor
 * @param cursor Pointer to the wsm_cursor instance
 * @param surface Pointer to the WLR surface for the image
 * @param hotspot_x X coordinate of the hotspot
 * @param hotspot_y Y coordinate of the hotspot
 * @param client Pointer to the Wayland client requesting the image surface
 */
void cursor_set_image_surface(struct wsm_cursor *cursor, struct wlr_surface *surface,
	int32_t hotspot_x, int32_t hotspot_y, struct wl_client *client);

/**
 * @brief Rebases the specified cursor
 * @param cursor Pointer to the wsm_cursor instance to be rebased
 */
void cursor_rebase(struct wsm_cursor *cursor);

/**
 * @brief Handles activity from the specified input device
 * @param cursor Pointer to the wsm_cursor instance
 * @param device Pointer to the WLR input device
 */
void cursor_handle_activity_from_device(struct wsm_cursor *cursor,
	struct wlr_input_device *device);

/**
 * @brief Handles activity from an idle source
 * @param cursor Pointer to the wsm_cursor instance
 * @param idle_source Type of the idle source
 */
void cursor_handle_activity_from_idle_source(struct wsm_cursor *cursor,
	enum wlr_input_device_type idle_source);

/**
 * @brief Unhides the specified cursor
 * @param cursor Pointer to the wsm_cursor instance to be unhidden
 */
void cursor_unhide(struct wsm_cursor *cursor);

/**
 * @brief Handles pointer motion for the specified cursor
 * @param cursor Pointer to the wsm_cursor instance
 * @param time_msec Time in milliseconds
 * @param device Pointer to the WLR input device
 * @param dx Change in X position
 * @param dy Change in Y position
 * @param dx_unaccel Change in X position without acceleration
 * @param dy_unaccel Change in Y position without acceleration
 */
void pointer_motion(struct wsm_cursor *cursor, uint32_t time_msec,
	struct wlr_input_device *device, double dx, double dy,
	double dx_unaccel, double dy_unaccel);

/**
 * @brief Dispatches cursor axis events for the specified cursor
 * @param cursor Pointer to the wsm_cursor instance
 * @param event Pointer to the WLR pointer axis event
 */
void dispatch_cursor_axis(struct wsm_cursor *cursor,
	struct wlr_pointer_axis_event *event);

/**
 * @brief Updates the image of the specified cursor
 * @param cursor Pointer to the wsm_cursor instance
 * @param node Pointer to the WSM node associated with the cursor
 */
void cursor_update_image(struct wsm_cursor *cursor, struct wsm_node *node);

/**
 * @brief Warps the cursor to the specified container
 * @param cursor Pointer to the wsm_cursor instance
 * @param container Pointer to the WSM container to warp to
 * @param force Boolean indicating if the warp should be forced
 */
void cursor_warp_to_container(struct wsm_cursor *cursor,
	struct wsm_container *container, bool force);

/**
 * @brief Warps the cursor to the specified workspace
 * @param cursor Pointer to the wsm_cursor instance
 * @param workspace Pointer to the WSM workspace to warp to
 */
void cursor_warp_to_workspace(struct wsm_cursor *cursor,
	struct wsm_workspace *workspace);

/**
 * @brief Constrains the specified cursor to a given constraint
 * @param cursor Pointer to the wsm_cursor instance
 * @param constraint Pointer to the WLR pointer constraint
 */
void wsm_cursor_constrain(struct wsm_cursor *cursor,
	struct wlr_pointer_constraint_v1 *constraint);

/**
 * @brief Warps the cursor to the constraint hint
 * @param cursor Pointer to the wsm_cursor instance
 */
void warp_to_constraint_cursor_hint(struct wsm_cursor *cursor);

/**
 * @brief Retrieves the node at the specified coordinates
 * @param seat Pointer to the WSM seat
 * @param lx X coordinate in layout coordinates
 * @param ly Y coordinate in layout coordinates
 * @param surface Pointer to the WLR surface to be populated
 * @param sx Pointer to the X coordinate to be populated
 * @param sy Pointer to the Y coordinate to be populated
 * @return Pointer to the WSM node at the specified coordinates
 */
struct wsm_node *node_at_coords(
	struct wsm_seat *seat, double lx, double ly,
	struct wlr_surface **surface, double *sx, double *sy);

/**
 * @brief Dispatches button events for the specified cursor
 * @param cursor Pointer to the wsm_cursor instance
 * @param device Pointer to the WLR input device
 * @param time_msec Time in milliseconds
 * @param button Button identifier
 * @param state State of the button (pressed/released)
 */
void dispatch_cursor_button(struct wsm_cursor *cursor,
	struct wlr_input_device *device, uint32_t time_msec, uint32_t button,
	enum wl_pointer_button_state state);

#endif
