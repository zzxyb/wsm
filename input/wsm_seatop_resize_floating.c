#include "wsm_seatop_resize_floating.h"
#include "wsm_seatop_default.h"
#include "wsm_cursor.h"
#include "wsm_seat.h"
#include "wsm_view.h"
#include "wsm_arrange.h"
#include "wsm_server.h"
#include "wsm_container.h"
#include "wsm_input_manager.h"
#include "wsm_transaction.h"
#include "wsm_log.h"

#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_keyboard.h>
#include <wlr/types/wlr_xcursor_manager.h>

struct seatop_resize_floating_event {
	struct wsm_container *container;
	double ref_lx, ref_ly;
	double ref_width, ref_height;
	double ref_con_lx, ref_con_ly;
	double ref_content_lx, ref_content_ly;
	double ref_content_width, ref_content_height;
	double ref_geo_x, ref_geo_y;
	double ref_geo_width, ref_geo_height;
	enum wlr_edges edge;
	bool preserve_ratio;
	bool deferred;
};

static const struct wsm_seatop_impl seatop_impl;

static struct seatop_resize_floating_event *resize_event_for_container(
		struct wsm_seat *seat, struct wsm_container *con) {
	if (seat->seatop_impl != &seatop_impl || seat->seatop_data == NULL) {
		return NULL;
	}

	struct seatop_resize_floating_event *e = seat->seatop_data;
	if (e->container != con) {
		return NULL;
	}
	return e;
}

static bool update_resize_position(struct wsm_seat *seat,
		struct wsm_container *con) {
	struct seatop_resize_floating_event *e =
		resize_event_for_container(seat, con);
	if (e == NULL) {
		return false;
	}

	double content_x = con->pending.content_x;
	double content_y = con->pending.content_y;
	if (e->edge & WLR_EDGE_LEFT) {
		content_x = e->ref_content_lx + e->ref_geo_x +
			e->ref_geo_width - con->view->geometry.x -
			con->pending.content_width;
	}
	if (e->edge & WLR_EDGE_TOP) {
		content_y = e->ref_content_ly + e->ref_geo_y +
			e->ref_geo_height - con->view->geometry.y -
			con->pending.content_height;
	}

	if (content_x == con->pending.content_x &&
			content_y == con->pending.content_y) {
		return false;
	}

	con->pending.content_x = content_x;
	con->pending.content_y = content_y;
	container_set_geometry_from_content(con);
	return true;
}

bool seatop_resize_floating_is_active(struct wsm_container *con) {
	struct wsm_seat *seat;
	wl_list_for_each(seat, &global_server.input_manager->seats, link) {
		if (resize_event_for_container(seat, con) != NULL) {
			return true;
		}
	}
	return false;
}

bool seatop_resize_floating_update_position(struct wsm_container *con) {
	bool updated = false;
	struct wsm_seat *seat;
	wl_list_for_each(seat, &global_server.input_manager->seats, link) {
		updated |= update_resize_position(seat, con);
	}
	return updated;
}

bool seatop_resize_floating_get_anchor(struct wsm_container *con,
		enum wlr_edges *edges, double *geo_right,
		double *geo_bottom) {
	struct wsm_seat *seat;
	wl_list_for_each(seat, &global_server.input_manager->seats, link) {
		struct seatop_resize_floating_event *e =
			resize_event_for_container(seat, con);
		if (e == NULL) {
			continue;
		}

		*edges = e->edge;
		*geo_right = e->ref_content_lx + e->ref_geo_x +
			e->ref_geo_width;
		*geo_bottom = e->ref_content_ly + e->ref_geo_y +
			e->ref_geo_height;
		return true;
	}
	return false;
}

bool seatop_resize_floating_is_deferred(struct wsm_container *con) {
	struct wsm_seat *seat;
	wl_list_for_each(seat, &global_server.input_manager->seats, link) {
		struct seatop_resize_floating_event *e =
			resize_event_for_container(seat, con);
		if (e != NULL) {
			return e->deferred;
		}
	}
	return false;
}

bool seatop_resize_floating_deferred_commit(struct wsm_container *con) {
	struct wsm_seat *seat;
	wl_list_for_each(seat, &global_server.input_manager->seats, link) {
		struct seatop_resize_floating_event *e =
			resize_event_for_container(seat, con);
		if (e == NULL || !e->deferred) {
			continue;
		}

		con->pending.content_width = con->view->geometry.width;
		con->pending.content_height = con->view->geometry.height;
		update_resize_position(seat, con);
		container_set_geometry_from_content(con);
		return true;
	}
	return false;
}

static void handle_button(struct wsm_seat *seat, uint32_t time_msec,
		struct wlr_input_device *device, uint32_t button,
		enum wl_pointer_button_state state) {
	struct seatop_resize_floating_event *e = seat->seatop_data;
	struct wsm_container *con = e->container;

	if (seat->cursor->pressed_button_count == 0) {
		container_set_resizing(con, false);
		if (con->view && con->view->using_csd) {
			seatop_begin_default(seat);
			return;
		}
		wsm_arrange_container_auto(con); // Send configure w/o resizing hint
		transaction_commit_dirty();
		seatop_begin_default(seat);
    }
}

static void handle_pointer_motion(struct wsm_seat *seat, uint32_t time_msec) {
	struct seatop_resize_floating_event *e = seat->seatop_data;
	struct wsm_container *con = e->container;
	enum wlr_edges edge = e->edge;
	struct wsm_cursor *cursor = seat->cursor;

	double mouse_move_x = cursor->cursor_wlr->x - e->ref_lx;
	double mouse_move_y = cursor->cursor_wlr->y - e->ref_ly;

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

	if (con->view && con->view->using_csd) {
		e->deferred = true;
		view_configure(con->view, con->pending.content_x,
			con->pending.content_y, width, height);
		return;
	}

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
	if (e->container == con) {
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
		wsm_log(WSM_ERROR, "Could not create seatop_resize_floating_event: allocation failed!");
		return;
	}
	e->container = con;

	struct wlr_keyboard *keyboard = wlr_seat_get_keyboard(seat->seat);
	e->preserve_ratio = keyboard &&
		(wlr_keyboard_get_modifiers(keyboard) & WLR_MODIFIER_SHIFT);

	e->edge = edge == WLR_EDGE_NONE ? WLR_EDGE_BOTTOM | WLR_EDGE_RIGHT : edge;
	e->ref_lx = seat->cursor->cursor_wlr->x;
	e->ref_ly = seat->cursor->cursor_wlr->y;
	e->ref_con_lx = con->pending.x;
	e->ref_con_ly = con->pending.y;
	e->ref_width = con->pending.width;
	e->ref_height = con->pending.height;
	e->ref_content_lx = con->pending.content_x;
	e->ref_content_ly = con->pending.content_y;
	e->ref_content_width = con->pending.content_width;
	e->ref_content_height = con->pending.content_height;
	if (con->view) {
		e->ref_geo_x = con->view->geometry.x;
		e->ref_geo_y = con->view->geometry.y;
		e->ref_geo_width = con->view->geometry.width;
		e->ref_geo_height = con->view->geometry.height;
	}

	seat->seatop_impl = &seatop_impl;
	seat->seatop_data = e;

	container_set_resizing(con, true);
	container_raise_floating(con);
	transaction_commit_dirty();

	const char *image = edge == WLR_EDGE_NONE ?
		"se-resize" : wlr_xcursor_get_resize_name(edge);
	cursor_set_image(seat->cursor, image, NULL);
	wlr_seat_pointer_notify_clear_focus(seat->seat);
}
