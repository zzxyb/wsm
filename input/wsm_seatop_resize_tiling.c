#include "wsm_seatop_resize_tiling.h"
#include "wsm_arrange.h"
#include "wsm_container.h"
#include "wsm_cursor.h"
#include "wsm_list.h"
#include "wsm_log.h"
#include "wsm_output.h"
#include "wsm_seat.h"
#include "wsm_seatop_default.h"
#include "wsm_server.h"
#include "wsm_transaction.h"
#include "wsm_workspace.h"
#include "node/wsm_node.h"

#include <math.h>
#include <stdlib.h>

#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_xcursor_manager.h>

#define AXIS_HORIZONTAL (WLR_EDGE_LEFT | WLR_EDGE_RIGHT)
#define AXIS_VERTICAL (WLR_EDGE_TOP | WLR_EDGE_BOTTOM)
#define MIN_TILING_SIDE 100
#define TILING_SPLIT_GRAB_WIDTH 8

struct seatop_resize_tiling_event {
	struct wsm_container *con;
	struct wsm_container *h_con;
	struct wsm_container *h_sib;
	struct wsm_container *v_con;
	struct wsm_container *v_sib;
	enum wlr_edges edge;
	enum wlr_edges edge_x;
	enum wlr_edges edge_y;
	double ref_lx;
	double ref_ly;
	double h_con_orig_width;
	double v_con_orig_height;
};

static const struct wsm_seatop_impl seatop_impl;

static bool is_horizontal(uint32_t axis) {
	return axis & AXIS_HORIZONTAL;
}

static bool is_vertical(uint32_t axis) {
	return axis & AXIS_VERTICAL;
}

static int container_sibling_index(struct wsm_container *con) {
	struct wsm_list *siblings = container_get_siblings(con);
	return siblings ? wsm_list_find(siblings, con) : -1;
}

static bool layout_is_horizontal_resizable(enum wsm_container_layout layout) {
	return layout == L_HORIZ || layout == L_HORIZ_1_V_2 ||
		layout == L_HORIZ_2_V_1 || layout == L_GRID;
}

static bool layout_is_vertical_resizable(enum wsm_container_layout layout) {
	return layout == L_HORIZ_1_V_2 || layout == L_HORIZ_2_V_1 ||
		layout == L_GRID;
}

static bool layout_has_fixed_children(enum wsm_container_layout layout,
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

static struct wsm_container *container_find_resize_parent(
		struct wsm_container *con, uint32_t axis) {
	bool allow_first = axis != WLR_EDGE_TOP && axis != WLR_EDGE_LEFT;
	bool allow_last = axis != WLR_EDGE_RIGHT && axis != WLR_EDGE_BOTTOM;

	while (con) {
		struct wsm_list *siblings = container_get_siblings(con);
		int index = container_sibling_index(con);
		enum wsm_container_layout layout = container_parent_layout(con);
		bool resizable_layout =
			(is_horizontal(axis) && layout_is_horizontal_resizable(layout)) ||
			(is_vertical(axis) && layout_is_vertical_resizable(layout));
		if (resizable_layout && siblings && siblings->length > 1 &&
				(allow_first || index > 0) &&
				(allow_last || index < siblings->length - 1)) {
			return con;
		}
		con = con->pending.parent;
	}

	return NULL;
}

static struct wsm_container *container_get_resize_sibling(
		struct wsm_container *con, uint32_t edge) {
	if (!con) {
		return NULL;
	}

	struct wsm_list *siblings = container_get_siblings(con);
	int index = container_sibling_index(con);
	int offset = edge & (WLR_EDGE_TOP | WLR_EDGE_LEFT) ? -1 : 1;
	if (!siblings || siblings->length == 1 ||
			index + offset < 0 || index + offset >= siblings->length) {
		return NULL;
	}
	return siblings->items[index + offset];
}

static double column_fraction(struct wsm_container *con, double fallback) {
	return con->width_fraction > 0 ? con->width_fraction : fallback;
}

static bool resize_fixed_columns(struct wsm_list *siblings,
		enum wsm_container_layout layout, struct wsm_container *con,
		uint32_t edge, int amount) {
	if (!siblings || amount == 0) {
		return false;
	}

	struct wsm_container *left = NULL;
	struct wsm_container *right = NULL;
	double left_fraction = 0.5;
	double total_width = con->child_total_width;

	if (layout == L_HORIZ_1_V_2 && siblings->length == 2) {
		left = siblings->items[0];
		right = siblings->items[1];
		left_fraction = column_fraction(left, 1.0 / 3.0);
	} else if (layout == L_HORIZ_1_V_2 && siblings->length == 3) {
		left = siblings->items[0];
		right = siblings->items[1];
		left_fraction = column_fraction(left, 1.0 / 3.0);
	} else if (layout == L_HORIZ_2_V_1 && siblings->length == 2) {
		left = siblings->items[0];
		right = siblings->items[1];
		left_fraction = column_fraction(left, 2.0 / 3.0);
	} else if (layout == L_HORIZ_2_V_1 && siblings->length == 3) {
		left = siblings->items[0];
		right = siblings->items[2];
		left_fraction = column_fraction(left, 2.0 / 3.0);
	} else if (layout == L_GRID && siblings->length == 4) {
		left = siblings->items[0];
		right = siblings->items[2];
		left_fraction = column_fraction(left, 0.5);
	} else {
		return false;
	}

	if (!left || !right || total_width <= 0) {
		return false;
	}

	bool resizing_left = con == left ||
		(layout == L_HORIZ_2_V_1 && siblings->length == 3 &&
			con == siblings->items[1]) ||
		(layout == L_GRID && con == siblings->items[1]);
	bool resizing_right = con == right ||
		(layout == L_HORIZ_1_V_2 && siblings->length == 3 &&
			con == siblings->items[2]) ||
		(layout == L_GRID && con == siblings->items[3]);
	if (!resizing_left && !resizing_right) {
		return false;
	}

	double amount_fraction = (double)amount / total_width;
	if (edge == WLR_EDGE_RIGHT && resizing_left) {
		left_fraction += amount_fraction;
	} else if (edge == WLR_EDGE_LEFT && resizing_right) {
		left_fraction -= amount_fraction;
	} else {
		return false;
	}

	double min_fraction = (double)MIN_TILING_SIDE / total_width;
	if (left_fraction < min_fraction || 1.0 - left_fraction < min_fraction) {
		return false;
	}

	if (layout == L_HORIZ_1_V_2) {
		((struct wsm_container *)siblings->items[0])->width_fraction = left_fraction;
		((struct wsm_container *)siblings->items[1])->width_fraction = 1.0 - left_fraction;
		if (siblings->length == 3) {
			((struct wsm_container *)siblings->items[2])->width_fraction =
				1.0 - left_fraction;
		}
	} else if (layout == L_HORIZ_2_V_1) {
		((struct wsm_container *)siblings->items[0])->width_fraction = left_fraction;
		((struct wsm_container *)siblings->items[1])->width_fraction =
			siblings->length == 2 ? 1.0 - left_fraction : left_fraction;
		if (siblings->length == 3) {
			((struct wsm_container *)siblings->items[2])->width_fraction =
				1.0 - left_fraction;
		}
	} else if (layout == L_GRID) {
		((struct wsm_container *)siblings->items[0])->width_fraction = left_fraction;
		((struct wsm_container *)siblings->items[1])->width_fraction = left_fraction;
		((struct wsm_container *)siblings->items[2])->width_fraction = 1.0 - left_fraction;
		((struct wsm_container *)siblings->items[3])->width_fraction = 1.0 - left_fraction;
	}

	return true;
}

static bool resize_fixed_rows(struct wsm_list *siblings,
		enum wsm_container_layout layout, struct wsm_container *con,
		uint32_t edge, int amount) {
	if (!siblings || amount == 0) {
		return false;
	}

	int top_index = -1;
	int bottom_index = -1;
	if (layout == L_HORIZ_1_V_2 && siblings->length == 3) {
		if (con == siblings->items[1] || con == siblings->items[2]) {
			top_index = 1;
			bottom_index = 2;
		}
	} else if (layout == L_HORIZ_2_V_1 && siblings->length == 3) {
		if (con == siblings->items[0] || con == siblings->items[1]) {
			top_index = 0;
			bottom_index = 1;
		}
	} else if (layout == L_GRID && siblings->length == 4) {
		if (con == siblings->items[0] || con == siblings->items[1]) {
			top_index = 0;
			bottom_index = 1;
		} else if (con == siblings->items[2] || con == siblings->items[3]) {
			top_index = 2;
			bottom_index = 3;
		}
	}
	if (top_index < 0 || bottom_index < 0) {
		return false;
	}

	struct wsm_container *top = siblings->items[top_index];
	struct wsm_container *bottom = siblings->items[bottom_index];
	double total_height = con->child_total_height;
	if (total_height <= 0) {
		return false;
	}

	double top_fraction = top->height_fraction > 0 ?
		top->height_fraction : 0.5;
	double amount_fraction = (double)amount / total_height;
	if (edge == WLR_EDGE_BOTTOM && con == top) {
		top_fraction += amount_fraction;
	} else if (edge == WLR_EDGE_TOP && con == bottom) {
		top_fraction -= amount_fraction;
	} else {
		return false;
	}

	double min_fraction = (double)MIN_TILING_SIDE / total_height;
	if (top_fraction < min_fraction || 1.0 - top_fraction < min_fraction) {
		return false;
	}

	top->height_fraction = top_fraction;
	bottom->height_fraction = 1.0 - top_fraction;
	top->child_total_height = total_height;
	bottom->child_total_height = total_height;
	return true;
}

static void container_resize_tiled(struct wsm_container *con,
		uint32_t axis, int amount) {
	con = container_find_resize_parent(con, axis);
	if (!con || container_is_scratchpad_hidden_or_child(con)) {
		return;
	}

	struct wsm_list *siblings = container_get_siblings(con);
	int index = container_sibling_index(con);
	enum wsm_container_layout layout = container_parent_layout(con);

	if (resize_fixed_columns(siblings, layout, con, axis, amount)) {
		if (con->pending.parent) {
			wsm_arrange_container_auto(con->pending.parent);
			node_set_dirty(&con->pending.parent->node);
		} else if (con->pending.workspace) {
			wsm_arrange_workspace_auto(con->pending.workspace);
			node_set_dirty(&con->pending.workspace->node);
		}
		return;
	}

	if (resize_fixed_rows(siblings, layout, con, axis, amount)) {
		if (con->pending.parent) {
			wsm_arrange_container_auto(con->pending.parent);
			node_set_dirty(&con->pending.parent->node);
		} else if (con->pending.workspace) {
			wsm_arrange_workspace_auto(con->pending.workspace);
			node_set_dirty(&con->pending.workspace->node);
		}
		return;
	}

	if (layout != L_HORIZ) {
		return;
	}

	struct wsm_container *resize[2] = {0};
	int resize_len = 0;

	if (axis == WLR_EDGE_LEFT) {
		if (index <= 0) {
			return;
		}
		resize[resize_len++] = siblings->items[index - 1];
		resize[resize_len++] = con;
	} else if (axis == WLR_EDGE_RIGHT) {
		if (index < 0 || index >= siblings->length - 1) {
			return;
		}
		resize[resize_len++] = con;
		resize[resize_len++] = siblings->items[index + 1];
	} else {
		return;
	}

	int sibling_amount = ceil((double)amount / (double)(resize_len - 1));
	for (int i = 0; i < resize_len; i++) {
		struct wsm_container *sibling = resize[i];
		double change = sibling == con ? amount : -sibling_amount;
		if (sibling->pending.width + change < MIN_SANE_W) {
			return;
		}
	}
	if (con->child_total_width <= 0) {
		return;
	}

	for (int i = 0; i < siblings->length; ++i) {
		struct wsm_container *sibling = siblings->items[i];
		sibling->width_fraction =
			sibling->pending.width / con->child_total_width;
	}

	double amount_fraction = (double)amount / con->child_total_width;
	double sibling_amount_fraction =
		amount_fraction / (double)(resize_len - 1);
	for (int i = 0; i < resize_len; i++) {
		struct wsm_container *sibling = resize[i];
		sibling->width_fraction +=
			sibling == con ? amount_fraction : -sibling_amount_fraction;
	}

	if (con->pending.parent) {
		wsm_arrange_container_auto(con->pending.parent);
		node_set_dirty(&con->pending.parent->node);
	} else if (con->pending.workspace) {
		wsm_arrange_workspace_auto(con->pending.workspace);
		node_set_dirty(&con->pending.workspace->node);
	}
}

static bool find_resize_pair(struct wsm_container *con,
		enum wlr_edges edge, struct wsm_container **first,
		struct wsm_container **second) {
	struct wsm_container *resize_parent = container_find_resize_parent(con, edge);
	struct wsm_list *siblings = container_get_siblings(resize_parent);
	enum wsm_container_layout layout = resize_parent ?
		container_parent_layout(resize_parent) : L_NONE;
	if (!resize_parent || !siblings) {
		return false;
	}

	if (edge == WLR_EDGE_RIGHT) {
		struct wsm_container *sibling =
			container_get_resize_sibling(resize_parent, edge);
		if (!sibling) {
			return false;
		}
		*first = resize_parent;
		*second = sibling;
		return true;
	}
	if (edge == WLR_EDGE_LEFT) {
		struct wsm_container *sibling =
			container_get_resize_sibling(resize_parent, edge);
		if (!sibling) {
			return false;
		}
		*first = sibling;
		*second = resize_parent;
		return true;
	}
	if (edge == WLR_EDGE_BOTTOM || edge == WLR_EDGE_TOP) {
		int top_index = -1;
		int bottom_index = -1;
		if (layout == L_HORIZ_1_V_2 && siblings->length == 3 &&
				(resize_parent == siblings->items[1] ||
					resize_parent == siblings->items[2])) {
			top_index = 1;
			bottom_index = 2;
		} else if (layout == L_HORIZ_2_V_1 && siblings->length == 3 &&
				(resize_parent == siblings->items[0] ||
					resize_parent == siblings->items[1])) {
			top_index = 0;
			bottom_index = 1;
		} else if (layout == L_GRID && siblings->length == 4) {
			if (resize_parent == siblings->items[0] ||
					resize_parent == siblings->items[1]) {
				top_index = 0;
				bottom_index = 1;
			} else if (resize_parent == siblings->items[2] ||
					resize_parent == siblings->items[3]) {
				top_index = 2;
				bottom_index = 3;
			}
		}
		if (top_index < 0 || bottom_index < 0) {
			return false;
		}
		*first = siblings->items[top_index];
		*second = siblings->items[bottom_index];
		return true;
	}
	return false;
}

static bool begin_resize_tiling(struct wsm_seat *seat,
		struct wsm_container *con, enum wlr_edges edge, double ref_lx,
		double ref_ly) {
	seatop_end(seat);

	struct seatop_resize_tiling_event *e =
		calloc(1, sizeof(struct seatop_resize_tiling_event));
	if (!e) {
		wsm_log(WSM_ERROR, "Could not create seatop_resize_tiling_event: allocation failed!");
		return false;
	}

	e->con = con;
	e->edge = edge;
	e->ref_lx = ref_lx;
	e->ref_ly = ref_ly;
	if (edge & AXIS_HORIZONTAL) {
		e->edge_x = edge & AXIS_HORIZONTAL;
		e->h_con = container_find_resize_parent(e->con, e->edge_x);
		e->h_sib = container_get_resize_sibling(e->h_con, e->edge_x);
		if (e->h_con && e->h_sib) {
			container_set_resizing(e->h_con, true);
			container_set_resizing(e->h_sib, true);
			e->h_con_orig_width = e->h_con->pending.width;
		}
	}
	if (edge & AXIS_VERTICAL) {
		e->edge_y = edge & AXIS_VERTICAL;
		e->v_con = container_find_resize_parent(e->con, e->edge_y);
		struct wsm_container *first = NULL;
		struct wsm_container *second = NULL;
		if (find_resize_pair(e->con, e->edge_y, &first, &second)) {
			e->v_sib = e->edge_y == WLR_EDGE_BOTTOM ? second : first;
		}
		if (e->v_con && e->v_sib) {
			container_set_resizing(e->v_con, true);
			container_set_resizing(e->v_sib, true);
			e->v_con_orig_height = e->v_con->pending.height;
		}
	}

	if ((!e->h_con || !e->h_sib) && (!e->v_con || !e->v_sib)) {
		free(e);
		return false;
	}

	seat->seatop_impl = &seatop_impl;
	seat->seatop_data = e;

	transaction_commit_dirty();
	cursor_set_image(seat->cursor, e->edge_y ? "ns-resize" : "ew-resize", NULL);
	wlr_seat_pointer_notify_clear_focus(seat->seat);
	return true;
}

static bool list_find_fixed_horizontal_resize_pair_at(struct wsm_list *children,
		enum wsm_container_layout layout, double lx, double ly,
		struct wsm_container **left, struct wsm_container **right) {
	if (!children) {
		return false;
	}

	if (layout == L_HORIZ_1_V_2 &&
			(children->length == 2 || children->length == 3)) {
		struct wsm_container *left_col = children->items[0];
		struct wsm_container *right_col = children->items[1];
		double split = left_col->pending.x + left_col->pending.width;
		if (fabs(lx - split) <= TILING_SPLIT_GRAB_WIDTH &&
				ly >= left_col->pending.y &&
				ly <= left_col->pending.y + left_col->pending.height) {
			*left = left_col;
			*right = right_col;
			return true;
		}
	} else if (layout == L_HORIZ_2_V_1 &&
			(children->length == 2 || children->length == 3)) {
		struct wsm_container *left_col = children->items[0];
		struct wsm_container *right_col =
			children->items[children->length == 2 ? 1 : 2];
		double split = left_col->pending.x + left_col->pending.width;
		if (fabs(lx - split) <= TILING_SPLIT_GRAB_WIDTH &&
				ly >= left_col->pending.y &&
				ly <= left_col->pending.y + left_col->pending.height) {
			*left = left_col;
			*right = right_col;
			return true;
		}
	} else if (layout == L_GRID && children->length == 4) {
		struct wsm_container *left_col = children->items[0];
		struct wsm_container *right_col = children->items[2];
		double split = left_col->pending.x + left_col->pending.width;
		if (fabs(lx - split) <= TILING_SPLIT_GRAB_WIDTH &&
				ly >= left_col->pending.y &&
				ly <= ((struct wsm_container *)children->items[1])->pending.y +
					((struct wsm_container *)children->items[1])->pending.height) {
			*left = left_col;
			*right = right_col;
			return true;
		}
	}

	for (int i = 0; i + 1 < children->length; ++i) {
		struct wsm_container *a = children->items[i];
		struct wsm_container *b = children->items[i + 1];
		double split = a->pending.x + a->pending.width;
		double top = fmax(a->pending.y, b->pending.y);
		double bottom = fmin(a->pending.y + a->pending.height,
			b->pending.y + b->pending.height);

		if (fabs(lx - split) <= TILING_SPLIT_GRAB_WIDTH &&
				ly >= top && ly <= bottom) {
			*left = a;
			*right = b;
			return true;
		}
	}

	for (int i = 0; i < children->length; ++i) {
		struct wsm_container *child = children->items[i];
		if (!child->view && layout_has_fixed_children(child->pending.layout,
					child->pending.children->length) &&
				list_find_fixed_horizontal_resize_pair_at(child->pending.children,
					child->pending.layout, lx, ly, left, right)) {
			return true;
		}
	}

	return false;
}

static bool list_find_fixed_vertical_resize_pair_at(struct wsm_list *children,
		enum wsm_container_layout layout, double lx, double ly,
		struct wsm_container **top, struct wsm_container **bottom) {
	if (!children) {
		return false;
	}

	int pairs[2][2] = {{-1, -1}, {-1, -1}};
	int pair_count = 0;
	if (layout == L_HORIZ_1_V_2 && children->length == 3) {
		pairs[pair_count][0] = 1;
		pairs[pair_count++][1] = 2;
	} else if (layout == L_HORIZ_2_V_1 && children->length == 3) {
		pairs[pair_count][0] = 0;
		pairs[pair_count++][1] = 1;
	} else if (layout == L_GRID && children->length == 4) {
		pairs[pair_count][0] = 0;
		pairs[pair_count++][1] = 1;
		pairs[pair_count][0] = 2;
		pairs[pair_count++][1] = 3;
	}

	for (int i = 0; i < pair_count; ++i) {
		struct wsm_container *a = children->items[pairs[i][0]];
		struct wsm_container *b = children->items[pairs[i][1]];
		double split = a->pending.y + a->pending.height;
		double left = fmax(a->pending.x, b->pending.x);
		double right = fmin(a->pending.x + a->pending.width,
			b->pending.x + b->pending.width);
		if (fabs(ly - split) <= TILING_SPLIT_GRAB_WIDTH &&
				lx >= left && lx <= right) {
			*top = a;
			*bottom = b;
			return true;
		}
	}

	for (int i = 0; i < children->length; ++i) {
		struct wsm_container *child = children->items[i];
		if (!child->view && layout_has_fixed_children(child->pending.layout,
					child->pending.children->length) &&
				list_find_fixed_vertical_resize_pair_at(child->pending.children,
					child->pending.layout, lx, ly, top, bottom)) {
			return true;
		}
	}

	return false;
}

static bool find_resize_pair_at(double lx, double ly,
		struct wsm_container **first, struct wsm_container **second,
		enum wlr_edges *edge) {
	struct wlr_output *wlr_output = wlr_output_layout_output_at(
		global_server.scene_state.output_layout, lx, ly);
	if (!wlr_output || !wlr_output->data) {
		return false;
	}

	struct wsm_output *output = wlr_output->data;
	struct wsm_workspace *workspace = output_get_active_workspace(output);
	if (!workspace) {
		return false;
	}

	if (list_find_fixed_horizontal_resize_pair_at(workspace->tiling,
			workspace->layout, lx, ly, first, second)) {
		*edge = WLR_EDGE_RIGHT;
		return true;
	}
	if (list_find_fixed_vertical_resize_pair_at(workspace->tiling,
			workspace->layout, lx, ly, first, second)) {
		*edge = WLR_EDGE_BOTTOM;
		return true;
	}
	return false;
}

bool seatop_can_resize_tiling_at_node(struct wsm_node *node) {
	if (!node) {
		return false;
	}
	if (node->type == N_WORKSPACE) {
		return true;
	}
	if (node->type != N_CONTAINER) {
		return false;
	}

	struct wsm_container *con = node->container;
	return con && !container_is_floating_or_child(con) &&
		!container_is_fullscreen_or_child(con);
}

enum wlr_edges seatop_resize_tiling_at(struct wsm_seat *seat, double lx, double ly) {
	struct wsm_container *first = NULL;
	struct wsm_container *second = NULL;
	enum wlr_edges edge = WLR_EDGE_NONE;
	if (find_resize_pair_at(lx, ly, &first, &second, &edge)) {
		return edge;
	}
	return WLR_EDGE_NONE;
}

static void finish_resize(struct wsm_seat *seat) {
	struct seatop_resize_tiling_event *e = seat->seatop_data;
	if (e->h_con) {
		container_set_resizing(e->h_con, false);
		container_set_resizing(e->h_sib, false);
		if (e->h_con->pending.parent) {
			wsm_arrange_container_auto(e->h_con->pending.parent);
		} else if (e->h_con->pending.workspace) {
			wsm_arrange_workspace_auto(e->h_con->pending.workspace);
		}
	}
	if (e->v_con) {
		container_set_resizing(e->v_con, false);
		container_set_resizing(e->v_sib, false);
		if (e->v_con->pending.parent) {
			wsm_arrange_container_auto(e->v_con->pending.parent);
		} else if (e->v_con->pending.workspace) {
			wsm_arrange_workspace_auto(e->v_con->pending.workspace);
		}
	}
	transaction_commit_dirty();
	seatop_begin_default(seat);
}

static void handle_button(struct wsm_seat *seat, uint32_t time_msec,
		struct wlr_input_device *device, uint32_t button,
		enum wl_pointer_button_state state) {
	if (seat->cursor->pressed_button_count == 0) {
		finish_resize(seat);
	}
}

static void handle_tablet_tool_tip(struct wsm_seat *seat,
		struct wsm_tablet_tool *tool, uint32_t time_msec,
		enum wlr_tablet_tool_tip_state state) {
	if (state == WLR_TABLET_TOOL_TIP_UP) {
		finish_resize(seat);
	}
}

static void handle_pointer_motion(struct wsm_seat *seat, uint32_t time_msec) {
	struct seatop_resize_tiling_event *e = seat->seatop_data;
	int amount_x = 0;
	int amount_y = 0;
	int moved_x = seat->cursor->cursor_wlr->x - e->ref_lx;
	int moved_y = seat->cursor->cursor_wlr->y - e->ref_ly;

	if (e->h_con) {
		if (e->edge & WLR_EDGE_LEFT) {
			amount_x = (e->h_con_orig_width - moved_x) -
				e->h_con->pending.width;
		} else if (e->edge & WLR_EDGE_RIGHT) {
			amount_x = (e->h_con_orig_width + moved_x) -
				e->h_con->pending.width;
		}
	}
	if (e->v_con) {
		if (e->edge & WLR_EDGE_TOP) {
			amount_y = (e->v_con_orig_height - moved_y) -
				e->v_con->pending.height;
		} else if (e->edge & WLR_EDGE_BOTTOM) {
			amount_y = (e->v_con_orig_height + moved_y) -
				e->v_con->pending.height;
		}
	}

	if (amount_x != 0) {
		container_resize_tiled(e->h_con, e->edge_x, amount_x);
	}
	if (amount_y != 0) {
		container_resize_tiled(e->v_con, e->edge_y, amount_y);
	}
	transaction_commit_dirty();
}

static void handle_unref(struct wsm_seat *seat, struct wsm_container *con) {
	struct seatop_resize_tiling_event *e = seat->seatop_data;
	if (e->con == con || e->h_sib == con || e->v_sib == con) {
		seatop_begin_default(seat);
	}
}

static const struct wsm_seatop_impl seatop_impl = {
	.button = handle_button,
	.pointer_motion = handle_pointer_motion,
	.tablet_tool_tip = handle_tablet_tool_tip,
	.unref = handle_unref,
};

bool seatop_begin_resize_tiling(struct wsm_seat *seat,
		struct wsm_container *con, enum wlr_edges edge) {
	struct wsm_container *left = NULL;
	struct wsm_container *right = NULL;
	if (!find_resize_pair(con, edge, &left, &right)) {
		return false;
	}
	return begin_resize_tiling(seat, edge == WLR_EDGE_RIGHT ? left : right,
		edge, seat->cursor->cursor_wlr->x, seat->cursor->cursor_wlr->y);
}

bool seatop_begin_resize_tiling_at(struct wsm_seat *seat, double lx, double ly) {
	struct wsm_container *first = NULL;
	struct wsm_container *second = NULL;
	enum wlr_edges edge = WLR_EDGE_NONE;
	if (!find_resize_pair_at(lx, ly, &first, &second, &edge)) {
		return false;
	}
	return begin_resize_tiling(seat, first, edge, lx, ly);
}
