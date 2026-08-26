#include "wsm_seatop_move_tiling.h"
#include "wsm_arrange.h"
#include "wsm_config.h"
#include "wsm_container.h"
#include "wsm_cursor.h"
#include "wsm_list.h"
#include "wsm_log.h"
#include "wsm_seat.h"
#include "wsm_seatop_default.h"
#include "wsm_seatop_move_floating.h"
#include "wsm_server.h"
#include "wsm_transaction.h"
#include "wsm_workspace.h"

#include <linux/input-event-codes.h>
#include <stdlib.h>

#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/util/edges.h>

#define DROP_LAYOUT_BORDER 30

struct seatop_move_tiling_event {
	struct wsm_container *con;
	struct wsm_container *target;
	enum wlr_edges target_edge;
	double ref_lx, ref_ly;
	bool threshold_reached;
	struct wlr_scene_rect *indicator_rect;
};

static void update_indicator(struct seatop_move_tiling_event *e,
		struct wlr_box *box) {
	wlr_scene_node_set_position(&e->indicator_rect->node, box->x, box->y);
	wlr_scene_rect_set_size(e->indicator_rect, box->width, box->height);
}

static void handle_end(struct wsm_seat *seat) {
	struct seatop_move_tiling_event *e = seat->seatop_data;
	if (e->indicator_rect) {
		wlr_scene_node_destroy(&e->indicator_rect->node);
		e->indicator_rect = NULL;
	}
}

static void handle_motion_prethreshold(struct wsm_seat *seat) {
	struct seatop_move_tiling_event *e = seat->seatop_data;
	double cx = seat->cursor->cursor_wlr->x;
	double cy = seat->cursor->cursor_wlr->y;
	double dx = cx - e->ref_lx;
	double dy = cy - e->ref_ly;

	struct wlr_output *wlr_output = wlr_output_layout_output_at(
		global_server.scene_state.output_layout, cx, cy);
	double output_scale = wlr_output ? wlr_output->scale : 1;
	double threshold = global_config.tiling_drag_threshold * output_scale;
	threshold *= threshold;

	if (dx * dx + dy * dy > threshold) {
		wlr_scene_node_set_enabled(&e->indicator_rect->node, true);
		e->threshold_reached = true;
		cursor_set_image(seat->cursor, "grab", NULL);
	}
}

static bool container_is_move_target(struct seatop_move_tiling_event *e,
		struct wsm_container *target) {
	if (!target || target == e->con) {
		return false;
	}
	if (container_has_ancestor(target, e->con)) {
		return false;
	}
	if (target->pending.workspace != e->con->pending.workspace) {
		return false;
	}
	return !container_is_floating_or_child(target);
}

static void handle_motion_postthreshold(struct wsm_seat *seat) {
	struct seatop_move_tiling_event *e = seat->seatop_data;
	struct wsm_cursor *cursor = seat->cursor;
	struct wlr_surface *surface = NULL;
	double sx, sy;
	struct wsm_node *node = node_at_coords(seat,
		cursor->cursor_wlr->x, cursor->cursor_wlr->y, &surface, &sx, &sy);

	e->target = NULL;
	e->target_edge = WLR_EDGE_NONE;
	if (!node) {
		return;
	}

	if (node->type == N_WORKSPACE) {
		struct wlr_box box;
		workspace_get_box(node->workspace, &box);
		update_indicator(e, &box);
		return;
	}
	if (node->type != N_CONTAINER) {
		return;
	}

	struct wsm_container *target = node->container;
	if (!container_is_move_target(e, target)) {
		return;
	}

	struct wlr_box box;
	container_get_box(target, &box);

	int left = box.x + DROP_LAYOUT_BORDER;
	int right = box.x + box.width - DROP_LAYOUT_BORDER;
	if (cursor->cursor_wlr->x < left) {
		e->target_edge = WLR_EDGE_LEFT;
		box.width = left - box.x;
	} else if (cursor->cursor_wlr->x > right) {
		e->target_edge = WLR_EDGE_RIGHT;
		box.width = box.x + box.width - right;
		box.x = right;
	} else {
		e->target_edge = WLR_EDGE_NONE;
		int thickness = fmin(box.width, box.height) * 0.3;
		box.x += thickness;
		box.y += thickness;
		box.width -= thickness * 2;
		box.height -= thickness * 2;
	}

	e->target = target;
	update_indicator(e, &box);
}

static void handle_pointer_motion(struct wsm_seat *seat, uint32_t time_msec) {
	struct seatop_move_tiling_event *e = seat->seatop_data;
	if (e->threshold_reached) {
		handle_motion_postthreshold(seat);
	} else {
		handle_motion_prethreshold(seat);
	}
	transaction_commit_dirty();
}

static void normalize_width_fractions(struct wsm_list *siblings) {
	if (!siblings || siblings->length == 0) {
		return;
	}

	for (int i = 0; i < siblings->length; ++i) {
		struct wsm_container *child = siblings->items[i];
		child->width_fraction = 1.0 / siblings->length;
	}
}

static void finalize_move(struct wsm_seat *seat) {
	struct seatop_move_tiling_event *e = seat->seatop_data;
	if (!e->target) {
		seatop_begin_default(seat);
		return;
	}

	struct wsm_container *con = e->con;
	struct wsm_workspace *old_ws = con->pending.workspace;
	struct wsm_container *old_parent = con->pending.parent;
	bool after = e->target_edge != WLR_EDGE_LEFT;

	container_detach(con);
	container_add_sibling(e->target, con, after);
	if (old_parent) {
		container_reap_empty(old_parent);
	}

	struct wsm_list *siblings = container_get_siblings(con);
	normalize_width_fractions(siblings);
	if (old_ws) {
		wsm_arrange_workspace_auto(old_ws);
	}

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

static void handle_unref(struct wsm_seat *seat, struct wsm_container *con) {
	struct seatop_move_tiling_event *e = seat->seatop_data;
	if (e->con == con || e->target == con) {
		seatop_begin_default(seat);
	}
}

static const struct wsm_seatop_impl seatop_impl = {
	.button = handle_button,
	.pointer_motion = handle_pointer_motion,
	.tablet_tool_tip = handle_tablet_tool_tip,
	.unref = handle_unref,
	.end = handle_end,
};

void seatop_begin_move_tiling_threshold(struct wsm_seat *seat,
		struct wsm_container *con) {
	if (con->maximized) {
		return;
	}

	seatop_end(seat);

	struct seatop_move_tiling_event *e =
		calloc(1, sizeof(struct seatop_move_tiling_event));
	if (!e) {
		wsm_log(WSM_ERROR, "Could not create seatop_move_tiling_event: allocation failed!");
		return;
	}

	float color[4] = {0.20f, 0.50f, 0.90f, 0.35f};
	e->indicator_rect = wlr_scene_rect_create(seat->scene_tree, 0, 0, color);
	if (!e->indicator_rect) {
		free(e);
		return;
	}
	wlr_scene_node_set_enabled(&e->indicator_rect->node, false);

	e->con = con;
	e->ref_lx = seat->cursor->cursor_wlr->x;
	e->ref_ly = seat->cursor->cursor_wlr->y;

	seat->seatop_impl = &seatop_impl;
	seat->seatop_data = e;

	transaction_commit_dirty();
	wlr_seat_pointer_notify_clear_focus(seat->seat);
}

void seatop_begin_move_tiling(struct wsm_seat *seat,
		struct wsm_container *con) {
	seatop_begin_move_tiling_threshold(seat, con);
	struct seatop_move_tiling_event *e = seat->seatop_data;
	if (e) {
		e->threshold_reached = true;
		wlr_scene_node_set_enabled(&e->indicator_rect->node, true);
		cursor_set_image(seat->cursor, "grab", NULL);
	}
}

void seatop_begin_move_tiling_to_floating(struct wsm_seat *seat,
		struct wsm_container *con) {
	if (con->maximized) {
		return;
	}

	struct wlr_box box;
	struct wlr_box other_box;
	struct wsm_container *other = NULL;
	struct wsm_workspace *workspace = con->pending.workspace;
	struct wsm_list *siblings = container_get_siblings(con);
	bool split_pair = siblings && siblings->length == 2;

	if (split_pair) {
		for (int i = 0; i < siblings->length; ++i) {
			struct wsm_container *sibling = siblings->items[i];
			if (sibling != con) {
				other = sibling;
				container_get_box(other, &other_box);
				break;
			}
		}
	}

	container_get_box(con, &box);
	container_set_floating(con, true);
	container_floating_set_geometry_from_box(con, box);

	if (other) {
		container_set_floating(other, true);
		container_floating_set_geometry_from_box(other, other_box);
	}

	if (workspace) {
		switch (workspace->tiling->length) {
		case 0:
		case 1:
			workspace->layout = L_NONE;
			break;
		case 2:
			if (workspace->layout != L_HORIZ_1_V_2 &&
					workspace->layout != L_HORIZ_2_V_1) {
				workspace->layout = L_HORIZ;
			}
			break;
		case 3:
			if (workspace->layout != L_HORIZ_2_V_1) {
				workspace->layout = L_HORIZ_1_V_2;
			}
			break;
		case 4:
			workspace->layout = L_GRID;
			break;
		}
		wsm_arrange_workspace_auto(workspace);
	}

	seatop_begin_move_floating(seat, con);
}
