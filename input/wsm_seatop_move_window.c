#include "wsm_seatop_move_window.h"
#include "wsm_window.h"
#include "wsm_seat.h"
#include "wsm_cursor.h"
#include "wsm_seatop_default.h"
#include "wsm_server.h"
#include "wsm_transaction.h"
#include "wsm_window_snap.h"
#include "wsm_log.h"

#include <linux/input-event-codes.h>

#include <wayland-server-core.h>

#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_touch.h>

static const int move_commit_interval_ms = 8;

struct seatop_move_window_event {
	struct wsm_window *window;
	double dx, dy; // cursor offset in window
	struct wsm_window_snap *snap;
	struct wl_event_source *commit_timer;
};

static void commit_move(struct seatop_move_window_event *e) {
	if (e->commit_timer) {
		wl_event_source_remove(e->commit_timer);
		e->commit_timer = NULL;
	}
	transaction_commit_dirty();
}

static int handle_commit_timer(void *data) {
	struct seatop_move_window_event *e = data;
	wl_event_source_remove(e->commit_timer);
	e->commit_timer = NULL;
	transaction_commit_dirty();
	return 0;
}

static void schedule_move_commit(struct seatop_move_window_event *e) {
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
	struct seatop_move_window_event *e = seat->seatop_data;
	struct wlr_cursor *cursor = seat->cursor->cursor_wlr;

	wsm_window_snap_update(e->snap, cursor->x, cursor->y);

	if (!wsm_window_snap_apply(e->snap, e->window)) {
		window_move_to(e->window,
			e->window->pending.x, e->window->pending.y);
	}
	wsm_window_snap_set_dividers_visible(true);
	commit_move(e);
	seatop_begin_default(seat);
}

static void handle_button(struct wsm_seat *seat, uint32_t time_msec,
		struct wlr_input_device *device, uint32_t button,
		enum wl_pointer_button_state state) {
	if (button == BTN_LEFT && state == WL_POINTER_BUTTON_STATE_RELEASED &&
			seat->cursor->pressed_button_count == 0) {
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

static void handle_touch_up(struct wsm_seat *seat,
		struct wlr_touch_up_event *event) {
	if (seat->cursor->pointer_touch_id == event->touch_id) {
		finalize_move(seat);
	}
}

static void handle_touch_cancel(struct wsm_seat *seat,
		struct wlr_touch_cancel_event *event) {
	if (seat->cursor->pointer_touch_id == event->touch_id) {
		finalize_move(seat);
	}
}

static void update_move(struct wsm_seat *seat) {
	struct seatop_move_window_event *e = seat->seatop_data;
	struct wlr_cursor *cursor = seat->cursor->cursor_wlr;
	double lx = cursor->x - e->dx;
	double ly = cursor->y - e->dy;

	if ((int)lx != (int)e->window->pending.x ||
			(int)ly != (int)e->window->pending.y) {
		window_move_to(e->window, lx, ly);
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
	struct seatop_move_window_event *e = seat->seatop_data;
	if (e->commit_timer) {
		wl_event_source_remove(e->commit_timer);
		e->commit_timer = NULL;
	}
	wsm_window_snap_destroy(e->snap);
	e->snap = NULL;
	wsm_window_snap_set_dividers_visible(true);
}

static void handle_unref(struct wsm_seat *seat, struct wsm_window *window) {
	struct seatop_move_window_event *e = seat->seatop_data;
	if (e->window == window) {
		seatop_begin_default(seat);
	}
}

static const struct wsm_seatop_impl seatop_impl = {
	.button = handle_button,
	.pointer_motion = handle_pointer_motion,
	.tablet_tool_motion = handle_tablet_tool_motion,
	.tablet_tool_tip = handle_tablet_tool_tip,
	.touch_up = handle_touch_up,
	.touch_cancel = handle_touch_cancel,
	.end = handle_end,
	.unref = handle_unref,
};

void seatop_begin_move_window(struct wsm_seat *seat,
		struct wsm_window *window) {
	seatop_end(seat);

	struct wsm_cursor *cursor = seat->cursor;
	struct seatop_move_window_event *e =
		calloc(1, sizeof(struct seatop_move_window_event));
	if (!e) {
		wsm_log(WSM_ERROR, "Could not create seatop_move_window_event: allocation failed!");
		return;
	}
	e->window = window;
	e->dx = cursor->cursor_wlr->x - window->pending.x;
	e->dy = cursor->cursor_wlr->y - window->pending.y;
	e->snap = wsm_window_snap_create();
	wsm_window_snap_forget_window(window);
	wsm_window_snap_set_dividers_visible(false);

	seat->seatop_impl = &seatop_impl;
	seat->seatop_data = e;

	window_raise(window);
	transaction_commit_dirty();

	cursor_set_image(cursor, "grab", NULL);
	wlr_seat_pointer_notify_clear_focus(seat->seat);
}
