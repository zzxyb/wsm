/*
MIT License

Copyright (c) 2024 YaoBing Xiao

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

#include "wsm_seatop_resize_floating.h"
#include "wsm_seatop_default.h"
#include "wsm_cursor.h"
#include "wsm_seat.h"
#include "wsm_view.h"
#include "wsm_arrange.h"
#include "wsm_container.h"
#include "wsm_transaction.h"

#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_keyboard.h>
#include <wlr/types/wlr_xcursor_manager.h>

struct seatop_resize_floating_event {
	struct wsm_container *con;
	double ref_lx, ref_ly;
	double ref_width, ref_height;
	double ref_con_lx, ref_con_ly;
	enum wlr_edges edge;
	bool preserve_ratio;
};

static void handle_button(struct wsm_seat *seat, uint32_t time_msec,
		struct wlr_input_device *device, uint32_t button,
		enum wl_pointer_button_state state) {
	struct seatop_resize_floating_event *e = seat->seatop_data;
	struct wsm_container *con = e->con;

	if (seat->wsm_cursor->pressed_button_count == 0) {
		container_set_resizing(con, false);
		wsm_arrange_container_auto(con); // Send configure w/o resizing hint
		transaction_commit_dirty();
		seatop_begin_default(seat);
    }
}

static void handle_pointer_motion(struct wsm_seat *seat, uint32_t time_msec) {
	struct seatop_resize_floating_event *e = seat->seatop_data;
	struct wsm_container *con = e->con;
	enum wlr_edges edge = e->edge;
	struct wsm_cursor *cursor = seat->wsm_cursor;

	double mouse_move_x = cursor->wlr_cursor->x - e->ref_lx;
	double mouse_move_y = cursor->wlr_cursor->y - e->ref_ly;

	if (edge == WLR_EDGE_TOP || edge == WLR_EDGE_BOTTOM) {
		mouse_move_x = 0;
	}
	if (edge == WLR_EDGE_LEFT || edge == WLR_EDGE_RIGHT) {
		mouse_move_y = 0;
	}

	double grow_width = edge & WLR_EDGE_LEFT ? -mouse_move_x : mouse_move_x;
	double grow_height = edge & WLR_EDGE_TOP ? -mouse_move_y : mouse_move_y;

	if (e->preserve_ratio) {
		double x_multiplier = grow_width / e->ref_width;
		double y_multiplier = grow_height / e->ref_height;
		double max_multiplier = fmax(x_multiplier, y_multiplier);
		grow_width = e->ref_width * max_multiplier;
		grow_height = e->ref_height * max_multiplier;
	}

	struct wsm_container_state state = con->current;
	double border_width = 0.0;
	if (con->current.border == B_NORMAL) {
		border_width = get_max_thickness(state) * 2;
	}
	double border_height = 0.0;
	if (con->current.border == B_NORMAL) {
		border_height += container_titlebar_height();
		border_height += get_max_thickness(state);
	}

	double width = e->ref_width + grow_width;
	double height = e->ref_height + grow_height;
	int min_width, max_width, min_height, max_height;
	floating_calculate_constraints(&min_width, &max_width,
		&min_height, &max_height);
	width = fmin(width, max_width - border_width);
	width = fmax(width, min_width + border_width);
	width = fmax(width, 1);
	height = fmin(height, max_height - border_height);
	height = fmax(height, min_height + border_height);
	height = fmax(height, 1);

	if (con->view) {
		double view_min_width, view_max_width, view_min_height, view_max_height;
		view_get_constraints(con->view, &view_min_width, &view_max_width,
			&view_min_height, &view_max_height);
		width = fmin(width, view_max_width - border_width);
		width = fmax(width, view_min_width + border_width);
		width = fmax(width, 1);
		height = fmin(height, view_max_height - border_height);
		height = fmax(height, view_min_height + border_height);
		height = fmax(height, 1);
    }

	grow_width = width - e->ref_width;
	grow_height = height - e->ref_height;

	double grow_x = 0, grow_y = 0;
	if (edge & WLR_EDGE_LEFT) {
		grow_x = -grow_width;
	} else if (edge & WLR_EDGE_RIGHT) {
		grow_x = 0;
	} else {
		grow_x = -grow_width / 2;
	}
	if (edge & WLR_EDGE_TOP) {
		grow_y = -grow_height;
	} else if (edge & WLR_EDGE_BOTTOM) {
		grow_y = 0;
	} else {
		grow_y = -grow_height / 2;
	}

	int relative_grow_width = width - con->pending.width;
	int relative_grow_height = height - con->pending.height;
	int relative_grow_x = (e->ref_con_lx + grow_x) - con->pending.x;
	int relative_grow_y = (e->ref_con_ly + grow_y) - con->pending.y;

	con->pending.x += relative_grow_x;
	con->pending.y += relative_grow_y;
	con->pending.width += relative_grow_width;
	con->pending.height += relative_grow_height;

	con->pending.content_x += relative_grow_x;
	con->pending.content_y += relative_grow_y;
	con->pending.content_width += relative_grow_width;
	con->pending.content_height += relative_grow_height;

	wsm_arrange_container_auto(con);
	transaction_commit_dirty();
}

static void handle_unref(struct wsm_seat *seat, struct wsm_container *con) {
	struct seatop_resize_floating_event *e = seat->seatop_data;
	if (e->con == con) {
		seatop_begin_default(seat);
	}
}

static const struct wsm_seatop_impl seatop_impl = {
	.button = handle_button,
	.pointer_motion = handle_pointer_motion,
	.unref = handle_unref,
};

void seatop_begin_resize_floating(struct wsm_seat *seat,
		struct wsm_container *con, enum wlr_edges edge) {
	seatop_end(seat);

	struct seatop_resize_floating_event *e =
		calloc(1, sizeof(struct seatop_resize_floating_event));
	if (!e) {
		return;
	}
	e->con = con;

	struct wlr_keyboard *keyboard = wlr_seat_get_keyboard(seat->wlr_seat);
	e->preserve_ratio = keyboard &&
		(wlr_keyboard_get_modifiers(keyboard) & WLR_MODIFIER_SHIFT);

	e->edge = edge == WLR_EDGE_NONE ? WLR_EDGE_BOTTOM | WLR_EDGE_RIGHT : edge;
	e->ref_lx = seat->wsm_cursor->wlr_cursor->x;
	e->ref_ly = seat->wsm_cursor->wlr_cursor->y;
	e->ref_con_lx = con->pending.x;
	e->ref_con_ly = con->pending.y;
	e->ref_width = con->pending.width;
	e->ref_height = con->pending.height;

	seat->seatop_impl = &seatop_impl;
	seat->seatop_data = e;

	container_set_resizing(con, true);
	container_raise_floating(con);
	transaction_commit_dirty();

	const char *image = edge == WLR_EDGE_NONE ?
		"se-resize" : wlr_xcursor_get_resize_name(edge);
	cursor_set_image(seat->wsm_cursor, image, NULL);
	wlr_seat_pointer_notify_clear_focus(seat->wlr_seat);
}
