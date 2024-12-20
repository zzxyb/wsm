#include "wsm_seatop_move_floating.h"
#include "wsm_container.h"
#include "wsm_seat.h"
#include "wsm_cursor.h"
#include "wsm_seatop_default.h"
#include "wsm_transaction.h"
#include "wsm_log.h"

#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_seat.h>

struct seatop_move_floating_event {
	struct wsm_container *container;
	double dx, dy; // cursor offset in container
};

static void finalize_move(struct wsm_seat *seat) {
	struct seatop_move_floating_event *e = seat->seatop_data;

	container_floating_move_to(e->container, e->container->pending.x, e->container->pending.y);
	transaction_commit_dirty();
	seatop_begin_default(seat);
}

static void handle_button(struct wsm_seat *seat, uint32_t time_msec,
		struct wlr_input_device *device, uint32_t button,
		enum wl_pointer_button_state state) {
	if (seat->cursor->pressed_button_count == 0) {
		finalize_move(seat);
	}
}

static void handle_tablet_tool_tip(struct wsm_seat *seat,
		struct wsm_tablet_tool *tool, uint32_t time_msec,
		enum wlr_tablet_tool_tip_state state) {
	if (state == WLR_TABLET_TOOL_TIP_UP) {
		finalize_move(seat);
	}
}
static void handle_pointer_motion(struct wsm_seat *seat, uint32_t time_msec) {
	struct seatop_move_floating_event *e = seat->seatop_data;
	struct wlr_cursor *cursor = seat->cursor->cursor_wlr;
	container_floating_move_to(e->container, cursor->x - e->dx, cursor->y - e->dy);
	transaction_commit_dirty();
}

static void handle_unref(struct wsm_seat *seat, struct wsm_container *con) {
	struct seatop_move_floating_event *e = seat->seatop_data;
	if (e->container == con) {
		seatop_begin_default(seat);
	}
}

static const struct wsm_seatop_impl seatop_impl = {
	.button = handle_button,
	.pointer_motion = handle_pointer_motion,
	.tablet_tool_tip = handle_tablet_tool_tip,
	.unref = handle_unref,
};

void seatop_begin_move_floating(struct wsm_seat *seat,
		struct wsm_container *con) {
	seatop_end(seat);

	struct wsm_cursor *cursor = seat->cursor;
	struct seatop_move_floating_event *e =
		calloc(1, sizeof(struct seatop_move_floating_event));
	if (!e) {
		wsm_log(WSM_ERROR, "Could not create seatop_move_floating_event: allocation failed!");
		return;
	}
	e->container = con;
	e->dx = cursor->cursor_wlr->x - con->pending.x;
	e->dy = cursor->cursor_wlr->y - con->pending.y;

	seat->seatop_impl = &seatop_impl;
	seat->seatop_data = e;

	container_raise_floating(con);
	transaction_commit_dirty();

	cursor_set_image(cursor, "grab", NULL);
	wlr_seat_pointer_notify_clear_focus(seat->seat);
}
