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

struct wsm_seatop_event {
	uint32_t pressed_buttons[WSM_CURSOR_PRESSED_BUTTONS_CAP];
	struct wsm_gesture_tracker gestures;
	struct wlr_scene_node *previous_node;
	size_t pressed_button_count;
};

void seatop_begin_default(struct wsm_seat *seat);
enum wlr_edges find_resize_edge(struct wsm_container *cont,
	struct wlr_surface *surface, struct wsm_cursor *cursor);

#endif
