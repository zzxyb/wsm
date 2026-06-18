#include "wsm_seatop_move_floating.h"
#include "wsm_container.h"
#include "wsm_seat.h"
#include "wsm_cursor.h"
#include "wsm_seatop_default.h"
#include "wsm_server.h"
#include "wsm_transaction.h"
#include "wsm_window_snap.h"
#include "wsm_log.h"

#include <wayland-server-core.h>

#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_seat.h>

static const int move_commit_interval_ms = 8;

struct seatop_move_floating_event {
	struct wsm_container *container;
	double dx, dy; // cursor offset in container
	struct wsm_window_snap *snap;
	struct wl_event_source *commit_timer;
};

static void commit_move(struct seatop_move_floating_event *e) {
	if (e->commit_timer) {
		wl_event_source_remove(e->commit_timer);
		e->commit_timer = NULL;
	}
	transaction_commit_dirty();
}

static int handle_commit_timer(void *data) {
	struct seatop_move_floating_event *e = data;
	wl_event_source_remove(e->commit_timer);
	e->commit_timer = NULL;
	transaction_commit_dirty();
	return 0;
}

static void schedule_move_commit(struct seatop_move_floating_event *e) {
	if (e->commit_timer) {
		return;
	}
	e->commit_timer = wl_event_loop_add_timer(global_server.wl_event_loop,
		handle_commit_timer, e);
	if (!e->commit_timer) {
		transaction_commit_dirty();
		return;
	}
	wl_event_source_timer_update(e->commit_timer, move_commit_interval_ms);
}

static void finalize_move(struct wsm_seat *seat) {
	struct seatop_move_floating_event *e = seat->seatop_data;

	if (!wsm_window_snap_apply(e->snap, e->container)) {
		container_floating_move_to(e->container,
			e->container->pending.x, e->container->pending.y);
	}
	wsm_window_snap_set_dividers_visible(true);
	commit_move(e);
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

static void update_move(struct wsm_seat *seat) {
	struct seatop_move_floating_event *e = seat->seatop_data;
	struct wlr_cursor *cursor = seat->cursor->cursor_wlr;
	double lx = cursor->x - e->dx;
	double ly = cursor->y - e->dy;

	if ((int)lx != (int)e->container->pending.x ||
			(int)ly != (int)e->container->pending.y) {
		container_floating_move_to(e->container, lx, ly);
		schedule_move_commit(e);
	}
	wsm_window_snap_update(e->snap, cursor->x, cursor->y);
}

static void handle_pointer_motion(struct wsm_seat *seat, uint32_t time_msec) {
	update_move(seat);
}

static void handle_tablet_tool_motion(struct wsm_seat *seat,
		struct wsm_tablet_tool *tool, uint32_t time_msec) {
	update_move(seat);
}

static void handle_end(struct wsm_seat *seat) {
	struct seatop_move_floating_event *e = seat->seatop_data;
	if (e->commit_timer) {
		wl_event_source_remove(e->commit_timer);
		e->commit_timer = NULL;
	}
	wsm_window_snap_destroy(e->snap);
	e->snap = NULL;
	wsm_window_snap_set_dividers_visible(true);
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
	.tablet_tool_motion = handle_tablet_tool_motion,
	.tablet_tool_tip = handle_tablet_tool_tip,
	.end = handle_end,
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
	e->snap = wsm_window_snap_create();
	wsm_window_snap_forget_container(con);
	wsm_window_snap_set_dividers_visible(false);

	seat->seatop_impl = &seatop_impl;
	seat->seatop_data = e;

	container_raise_floating(con);
	transaction_commit_dirty();

	cursor_set_image(cursor, "grab", NULL);
	wlr_seat_pointer_notify_clear_focus(seat->seat);
}
