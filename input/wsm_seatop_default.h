#ifndef WSM_SEATOP_DEFAULT_H
#define WSM_SEATOP_DEFAULT_H

#include "gesture/wsm_gesture.h"

#include <stdint.h>

#include <xkbcommon/xkbcommon.h>

#include <wlr/util/edges.h>

#define WSM_CURSOR_PRESSED_BUTTONS_CAP 32

struct wlr_scene_node;

struct wsm_list;
struct wsm_seat;
struct wsm_container;
struct wlr_surface;
struct wsm_cursor;

/**
 * @brief Structure representing events related to seat operations
 */
struct wsm_seatop_event {
	uint32_t pressed_buttons[WSM_CURSOR_PRESSED_BUTTONS_CAP]; /**< Array of currently pressed buttons */
	struct wsm_gesture_tracker gestures_tracker; /**< Tracker for gesture events */
	struct wlr_scene_node *previous_node; /**< Pointer to the previously active scene node */
	size_t pressed_button_count; /**< Count of currently pressed buttons */
};

/**
 * @brief Begins the default seat operation for the specified seat
 * @param seat Pointer to the wsm_seat instance to begin the operation for
 */
void seatop_begin_default(struct wsm_seat *seat);

/**
 * @brief Finds the edge for resizing a container
 * @param cont Pointer to the wsm_container to resize
 * @param surface Pointer to the wlr_surface associated with the container
 * @param cursor Pointer to the wsm_cursor for the current cursor position
 * @return Enum value representing the edge for resizing
 */
enum wlr_edges find_resize_edge(struct wsm_container *cont,
	struct wlr_surface *surface, struct wsm_cursor *cursor);

#endif
