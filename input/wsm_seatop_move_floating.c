#include "wsm_seatop_move_floating.h"
#include "wsm_container.h"
#include "wsm_seat.h"
#include "wsm_cursor.h"
#include "wsm_list.h"
#include "wsm_arrange.h"
#include "wsm_output.h"
#include "wsm_scene.h"
#include "wsm_server.h"
#include "wsm_workspace.h"
#include "wsm_seatop_default.h"
#include "wsm_transaction.h"
#include "wsm_log.h"
#include "wsm_view.h"

#include <math.h>
#include <stdlib.h>

#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/util/edges.h>

#define TITLEBAR_SNAP_EDGE_THRESHOLD 50

struct seatop_move_floating_event {
	struct wsm_container *container;
	struct wlr_scene_rect *snap_preview;
	struct wsm_output *snap_preview_output;
	struct wlr_box snap_target;
	bool snap_target_valid;
	bool was_in_snap_group;
	double dx, dy; // cursor offset in container
};

static void destroy_snap_preview(struct seatop_move_floating_event *e) {
	if (!e->snap_preview) {
		return;
	}

	wlr_scene_node_destroy(&e->snap_preview->node);
	e->snap_preview = NULL;
	e->snap_preview_output = NULL;
	e->snap_target_valid = false;
}

static struct wsm_container *find_snap_partner(struct wsm_workspace *workspace,
		struct wsm_container *con, const struct wlr_box *target) {
	for (int i = 0; i < workspace->floating->length; ++i) {
		struct wsm_container *floater = workspace->floating->items[i];
		if (floater == con || floater->scratchpad || floater->snap_group_peer ||
				!view_can_split(floater->view)) {
			continue;
		}

		double right = floater->pending.x + floater->pending.width;
		double bottom = floater->pending.y + floater->pending.height;
		bool aligned_horizontally =
			fabs(floater->pending.x - workspace->x) <= 1.0 ||
			fabs(right - (workspace->x + workspace->width)) <= 1.0;
		bool spans_workspace_height =
			fabs(floater->pending.y - workspace->y) <= 1.0 &&
			fabs(bottom - (workspace->y + workspace->height)) <= 1.0;
		bool leaves_room = floater->pending.width >= MIN_SANE_W &&
			floater->pending.width <= workspace->width - MIN_SANE_W;
		bool opposite_side = true;
		if (target) {
			bool target_is_left = target->x <= workspace->x;
			double workspace_center = workspace->x + workspace->width / 2.0;
			double floater_center = floater->pending.x +
				floater->pending.width / 2.0;
			bool floater_is_left = floater_center <= workspace_center;
			opposite_side = target_is_left != floater_is_left;
		}
		if (aligned_horizontally && spans_workspace_height && leaves_room) {
			if (opposite_side) {
				return floater;
			}
		}
	}

	return NULL;
}

static double floating_partner_left_fraction(struct wsm_workspace *workspace,
		struct wsm_container *con) {
	struct wsm_container *partner = find_snap_partner(workspace, con, NULL);
	if (!partner || workspace->width <= 0) {
		return -1.0;
	}

	double workspace_center = workspace->x + workspace->width / 2.0;
	double partner_center = partner->pending.x + partner->pending.width / 2.0;
	double split = partner_center <= workspace_center ?
		partner->pending.x + partner->pending.width : partner->pending.x;
	double fraction = (split - workspace->x) / workspace->width;
	return fraction > 0.0 && fraction < 1.0 ? fraction : -1.0;
}

static double workspace_left_fraction(struct wsm_workspace *workspace) {
	double fallback = 0.5;
	switch (workspace->layout) {
	case L_HORIZ_1_V_2:
		fallback = 1.0 / 3.0;
		break;
	case L_HORIZ_2_V_1:
		fallback = 2.0 / 3.0;
		break;
	case L_HORIZ:
	case L_GRID:
	case L_NONE:
		break;
	}

	if (workspace->tiling->length > 0) {
		struct wsm_container *left = workspace->tiling->items[0];
		if (left->width_fraction > 0) {
			return left->width_fraction;
		}
		if (left->pending.width > 0 && workspace->width > 0) {
			return left->pending.width / workspace->width;
		}
	}

	return fallback;
}

static bool workspace_has_snap_group(struct wsm_workspace *workspace) {
	for (int i = 0; i < workspace->floating->length; ++i) {
		struct wsm_container *floater = workspace->floating->items[i];
		if (floater->snap_group_peer &&
				floater->snap_group_peer->pending.workspace == workspace) {
			return true;
		}
	}
	return false;
}

static bool edge_has_adjacent_output(struct wsm_output *output,
		struct wlr_box *box, enum wlr_edges edge, double y) {
	double x;
	if (edge == WLR_EDGE_LEFT) {
		x = box->x - 1;
	} else if (edge == WLR_EDGE_RIGHT) {
		x = box->x + box->width + 1;
	} else {
		return false;
	}

	struct wlr_output *adjacent = wlr_output_layout_output_at(
		global_server.scene->output_layout, x, y);
	return adjacent && adjacent != output->wlr_output;
}

static bool get_snap_target(struct wsm_seat *seat, struct wsm_container *con,
		struct wlr_box *target, struct wsm_output **target_output) {
	if (!con || !view_can_split(con->view)) {
		return false;
	}

	struct wlr_cursor *cursor = seat->cursor->cursor_wlr;
	struct wlr_output *wlr_output = wlr_output_layout_output_at(
		global_server.scene->output_layout, cursor->x, cursor->y);
	if (!wlr_output || !wlr_output->data) {
		return false;
	}

	struct wsm_output *output = wlr_output->data;
	struct wsm_workspace *workspace = output_get_active_workspace(output);
	if (!workspace) {
		return false;
	}

	struct wlr_box box;
	workspace_get_box(workspace, &box);
	if (wlr_box_empty(&box)) {
		return false;
	}

	int threshold = TITLEBAR_SNAP_EDGE_THRESHOLD * wlr_output->scale;
	bool left = cursor->x <= box.x + threshold;
	bool right = cursor->x >= box.x + box.width - threshold;
	bool top = cursor->y <= box.y + threshold;
	bool bottom = cursor->y >= box.y + box.height - threshold;
	if (left && edge_has_adjacent_output(output, &box, WLR_EDGE_LEFT, cursor->y)) {
		left = false;
	}
	if (right && edge_has_adjacent_output(output, &box, WLR_EDGE_RIGHT, cursor->y)) {
		right = false;
	}
	if (!left && !right) {
		return false;
	}

	*target = box;
	double left_fraction = workspace_left_fraction(workspace);
	if (workspace->tiling->length == 0) {
		double partner_fraction =
			floating_partner_left_fraction(workspace, con);
		if (partner_fraction > 0.0) {
			left_fraction = partner_fraction;
		}
	}
	if (left_fraction <= 0.0 || left_fraction >= 1.0) {
		left_fraction = 0.5;
	}
	double left_width = box.width * left_fraction;
	target->width = left ? left_width : box.width - left_width;
	if (right) {
		target->x = box.x + left_width;
	}

	// A corner target needs another window in the same column. With fewer
	// than two tiled windows, snapping this window can only produce a
	// full-height horizontal split, so do not preview a quarter-screen target
	// that the resulting layout cannot preserve.
	struct seatop_move_floating_event *move = seat->seatop_data;
	bool can_split_target_column = !move->was_in_snap_group &&
		!workspace_has_snap_group(workspace) &&
		workspace->tiling->length >= 2 &&
		workspace->tiling->length < 4;
	if ((top || bottom) && can_split_target_column) {
		target->height = box.height / 2;
		if (bottom) {
			target->y = box.y + box.height - target->height;
		}
	}

	if (target_output) {
		*target_output = output;
	}

	return true;
}

static void update_snap_preview(struct wsm_seat *seat) {
	struct seatop_move_floating_event *e = seat->seatop_data;
	struct wlr_box target;
	struct wsm_output *output = NULL;

	if (!get_snap_target(seat, e->container, &target, &output)) {
		destroy_snap_preview(e);
		return;
	}

	if (!e->snap_preview || e->snap_preview_output != output) {
		destroy_snap_preview(e);
		e->snap_preview = wlr_scene_rect_create(output->layers.shell_overlay,
			0, 0, (float[4]){0.20f, 0.50f, 0.90f, 0.28f});
		if (!e->snap_preview) {
			wsm_log(WSM_ERROR, "Could not create snap preview rect");
			return;
		}
		e->snap_preview_output = output;
	}

	wlr_scene_node_set_position(&e->snap_preview->node,
		target.x - output->lx, target.y - output->ly);
	wlr_scene_rect_set_size(e->snap_preview, target.width, target.height);
	e->snap_target = target;
	e->snap_target_valid = true;
}

static bool layout_supports_child_count(enum wsm_container_layout layout,
		int length) {
	switch (layout) {
	case L_HORIZ:
		return length == 2;
	case L_HORIZ_1_V_2:
	case L_HORIZ_2_V_1:
		return length == 2 || length == 3;
	case L_GRID:
		return length == 4;
	case L_NONE:
		return false;
	}
	return false;
}

static bool workspace_update_fixed_layout(struct wsm_workspace *workspace,
		int old_length, bool insert_left) {
	int length = workspace->tiling->length;
	if (old_length == length && layout_supports_child_count(workspace->layout,
			workspace->tiling->length)) {
		return true;
	}

	switch (length) {
	case 2:
		workspace->layout = L_HORIZ;
		return true;
	case 3:
		workspace->layout = insert_left ? L_HORIZ_2_V_1 : L_HORIZ_1_V_2;
		return true;
	case 4:
		workspace->layout = L_GRID;
		return true;
	default:
		return false;
	}
}

static double layout_child_width_fraction(struct wsm_workspace *workspace,
		int index) {
	switch (workspace->layout) {
	case L_HORIZ:
	case L_GRID:
		return 0.5;
	case L_HORIZ_1_V_2:
		return index == 0 ? 1.0 / 3.0 : 2.0 / 3.0;
	case L_HORIZ_2_V_1:
		return index < workspace->tiling->length - 1 ? 2.0 / 3.0 : 1.0 / 3.0;
	case L_NONE:
		break;
	}
	return workspace->tiling->length > 0 ?
		1.0 / workspace->tiling->length : 1.0;
}

static void workspace_reset_child_width_fractions(
		struct wsm_workspace *workspace, double preserved_left_fraction) {
	for (int i = 0; i < workspace->tiling->length; ++i) {
		struct wsm_container *child = workspace->tiling->items[i];
		if (preserved_left_fraction > 0.0 &&
				preserved_left_fraction < 1.0) {
			bool left_column = false;
			switch (workspace->layout) {
			case L_HORIZ:
			case L_HORIZ_1_V_2:
				left_column = i == 0;
				break;
			case L_HORIZ_2_V_1:
				left_column = i < workspace->tiling->length - 1;
				break;
			case L_GRID:
				left_column = i < 2;
				break;
			case L_NONE:
				break;
			}
			child->width_fraction = left_column ? preserved_left_fraction :
				1.0 - preserved_left_fraction;
		} else {
			child->width_fraction = layout_child_width_fraction(workspace, i);
		}
		child->child_total_width = workspace->width;
	}
}

static bool child_is_in_left_column(struct wsm_workspace *workspace,
		int index) {
	switch (workspace->layout) {
	case L_HORIZ:
	case L_HORIZ_1_V_2:
		return index == 0;
	case L_HORIZ_2_V_1:
		return index < workspace->tiling->length - 1;
	case L_GRID:
		return index < 2;
	case L_NONE:
		return false;
	}
	return false;
}

static void workspace_place_snapped_container(struct wsm_workspace *workspace,
		struct wsm_container *con, bool insert_left,
		const struct wlr_box *target) {
	int column_start = 0;
	int column_count = 1;
	switch (workspace->layout) {
	case L_HORIZ_1_V_2:
		column_start = insert_left ? 0 : 1;
		column_count = insert_left ? 1 : 2;
		break;
	case L_HORIZ_2_V_1:
		column_start = 0;
		column_count = insert_left ? 2 : 1;
		break;
	case L_GRID:
		column_start = insert_left ? 0 : 2;
		column_count = 2;
		break;
	case L_HORIZ:
	case L_NONE:
		return;
	}

	if (column_count != 2) {
		return;
	}

	int old_index = wsm_list_find(workspace->tiling, con);
	if (old_index < 0) {
		return;
	}
	bool insert_bottom = target->y > workspace->y;
	wsm_list_delete(workspace->tiling, old_index);
	wsm_list_insert(workspace->tiling,
		column_start + (insert_bottom ? 1 : 0), con);
}

static void workspace_apply_snap_target(struct wsm_workspace *workspace,
		struct wsm_container *con, const struct wlr_box *target) {
	double split_x = target->x <= workspace->x ?
		target->x + target->width : target->x;
	double left_width = split_x - workspace->x;
	if (workspace->width <= 0 || left_width <= 0 ||
			left_width >= workspace->width) {
		left_width = workspace->width / 2.0;
	}

	double left_fraction = left_width / workspace->width;
	workspace_reset_child_width_fractions(workspace, left_fraction);
	wsm_arrange_workspace_auto(workspace);

	// Keep the committed geometry tied to the preview boundary as well as the
	// stored fractions. This prevents an intermediate retile from showing 1:1.
	for (int i = 0; i < workspace->tiling->length; ++i) {
		struct wsm_container *child = workspace->tiling->items[i];
		bool left_column = child_is_in_left_column(workspace, i);
		child->pending.x = left_column ? workspace->x : split_x;
		child->pending.width = left_column ? left_width :
			workspace->width - left_width;
		child->child_total_width = workspace->width;
		child->width_fraction = left_column ? left_fraction :
			1.0 - left_fraction;
		wsm_arrange_container_auto(child);
	}

	if (target->height >= workspace->height) {
		return;
	}

	int con_index = wsm_list_find(workspace->tiling, con);
	if (con_index < 0) {
		return;
	}
	bool left_column = child_is_in_left_column(workspace, con_index);
	struct wsm_container *other = NULL;
	for (int i = 0; i < workspace->tiling->length; ++i) {
		if (i != con_index &&
				child_is_in_left_column(workspace, i) == left_column) {
			other = workspace->tiling->items[i];
			break;
		}
	}
	if (!other) {
		return;
	}

	double top_height = target->y <= workspace->y ? target->height :
		workspace->height - target->height;
	double top_fraction = top_height / workspace->height;
	bool con_is_top = target->y <= workspace->y;
	con->pending.y = target->y;
	con->pending.height = target->height;
	con->height_fraction = con_is_top ? top_fraction : 1.0 - top_fraction;
	con->child_total_height = workspace->height;
	other->pending.y = con_is_top ? workspace->y + target->height :
		workspace->y;
	other->pending.height = workspace->height - target->height;
	other->height_fraction = con_is_top ? 1.0 - top_fraction : top_fraction;
	other->child_total_height = workspace->height;
	wsm_arrange_container_auto(con);
	wsm_arrange_container_auto(other);
}

static bool snap_container(struct wsm_seat *seat, struct wsm_container *con) {
	struct seatop_move_floating_event *e = seat->seatop_data;
	if (!con || !view_can_split(con->view)) {
		return false;
	}

	struct wlr_box target;
	struct wsm_output *output = NULL;
	if (e->snap_target_valid && e->snap_preview_output) {
		target = e->snap_target;
		output = e->snap_preview_output;
	} else if (!get_snap_target(seat, con, &target, &output)) {
		return false;
	}

	struct wsm_workspace *workspace = output_get_active_workspace(output);
	if (!workspace) {
		return false;
	}

	// A full-height side snap on a workspace which already has a complete
	// tiled pair starts (or completes) another independent floating snap
	// group instead of extending the workspace's fixed layout.
	if (target.height >= workspace->height && workspace->tiling->length >= 2) {
		struct wsm_container *partner =
			find_snap_partner(workspace, con, &target);
		container_floating_set_geometry_from_box(con, target);
		if (partner) {
			if (partner->snap_group_peer) {
				partner->snap_group_peer->snap_group_peer = NULL;
			}
			if (con->snap_group_peer) {
				con->snap_group_peer->snap_group_peer = NULL;
			}
			partner->snap_group_peer = con;
			con->snap_group_peer = partner;
			container_raise_floating(con);
		}
		return true;
	}

	if (workspace->tiling->length == 0) {
		struct wsm_container *partner = find_snap_partner(workspace, con,
			&target);
		if (partner) {
			int old_length = workspace->tiling->length;
			bool insert_left = target.x <= workspace->x;
			// Retiling calls container_end_mouse_operation(). Keep it from
			// ending and freeing this seat operation while the pair is only
			// partially rebuilt.
			e->container = NULL;
			container_set_floating(partner, false);
			container_set_floating(con, false);
			e->container = con;

			int index = wsm_list_find(workspace->tiling, con);
			if (index >= 0) {
				wsm_list_delete(workspace->tiling, index);
				wsm_list_insert(workspace->tiling,
					insert_left ? 0 : workspace->tiling->length, con);
			}

			workspace_update_fixed_layout(workspace, old_length, insert_left);
			workspace_place_snapped_container(workspace, con, insert_left,
				&target);
			workspace_apply_snap_target(workspace, con, &target);
			return true;
		}

		container_floating_set_geometry_from_box(con, target);
		return true;
	}

	if (workspace->tiling->length >= 4) {
		return false;
	}

	int old_length = workspace->tiling->length;
	bool insert_left = target.x <= workspace->x;
	e->container = NULL;
	container_set_floating(con, false);
	e->container = con;

	int index = wsm_list_find(workspace->tiling, con);
	if (index >= 0) {
		wsm_list_delete(workspace->tiling, index);
		wsm_list_insert(workspace->tiling,
			insert_left ? 0 : workspace->tiling->length, con);
	}

	workspace_update_fixed_layout(workspace, old_length, insert_left);
	workspace_place_snapped_container(workspace, con, insert_left, &target);
	workspace_apply_snap_target(workspace, con, &target);
	return true;
}

static void finalize_move(struct wsm_seat *seat) {
	struct seatop_move_floating_event *e = seat->seatop_data;

	if (!snap_container(seat, e->container)) {
		container_floating_move_to(e->container, e->container->pending.x,
			e->container->pending.y);
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
static void handle_pointer_motion(struct wsm_seat *seat, uint32_t time_msec) {
	struct seatop_move_floating_event *e = seat->seatop_data;
	struct wlr_cursor *cursor = seat->cursor->cursor_wlr;
	container_floating_move_to(e->container, cursor->x - e->dx, cursor->y - e->dy);
	update_snap_preview(seat);
	transaction_commit_dirty();
}

static void handle_unref(struct wsm_seat *seat, struct wsm_container *con) {
	struct seatop_move_floating_event *e = seat->seatop_data;
	if (e->container == con) {
		seatop_begin_default(seat);
	}
}

static void handle_end(struct wsm_seat *seat) {
	struct seatop_move_floating_event *e = seat->seatop_data;
	destroy_snap_preview(e);
}

static const struct wsm_seatop_impl seatop_impl = {
	.button = handle_button,
	.pointer_motion = handle_pointer_motion,
	.tablet_tool_tip = handle_tablet_tool_tip,
	.unref = handle_unref,
	.end = handle_end,
};

void seatop_begin_move_floating(struct wsm_seat *seat,
		struct wsm_container *con) {
	if (con->maximized) {
		return;
	}

	seatop_end(seat);
	bool was_in_snap_group = con->snap_group_peer != NULL;
	if (con->snap_group_peer) {
		con->snap_group_peer->snap_group_peer = NULL;
		con->snap_group_peer = NULL;
	}

	struct wsm_cursor *cursor = seat->cursor;
	struct seatop_move_floating_event *e =
		calloc(1, sizeof(struct seatop_move_floating_event));
	if (!e) {
		wsm_log(WSM_ERROR, "Could not create seatop_move_floating_event: allocation failed!");
		return;
	}
	e->container = con;
	e->was_in_snap_group = was_in_snap_group;
	e->dx = cursor->cursor_wlr->x - con->pending.x;
	e->dy = cursor->cursor_wlr->y - con->pending.y;

	seat->seatop_impl = &seatop_impl;
	seat->seatop_data = e;

	container_raise_floating(con);
	update_snap_preview(seat);
	transaction_commit_dirty();

	cursor_set_image(cursor, "grab", NULL);
	wlr_seat_pointer_notify_clear_focus(seat->seat);
}
