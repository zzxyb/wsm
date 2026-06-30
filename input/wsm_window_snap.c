#include "wsm_window_snap.h"
#include "wsm_common.h"
#include "wsm_window.h"
#include "wsm_cursor.h"
#include "wsm_log.h"
#include "wsm_output.h"
#include "wsm_scene.h"
#include "wsm_seat.h"
#include "wsm_seatop_default.h"
#include "wsm_server.h"
#include "wsm_transaction.h"
#include "wsm_view.h"

#include <stdlib.h>

#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/util/box.h>

enum snap_divider_orientation {
	SNAP_DIVIDER_VERTICAL,
	SNAP_DIVIDER_HORIZONTAL,
};

struct wsm_window_snap_record {
	struct wl_list link;
	struct wl_listener destroy;
	struct wsm_window *window;
	struct wlr_box box;
};

struct wsm_window_snap_divider {
	struct wl_list link;
	bool seen;
	enum snap_divider_orientation orientation;
	struct wsm_window *first;
	struct wsm_window *second;
	struct wlr_box first_box;
	struct wlr_box second_box;
	struct wlr_box hitbox;
	struct wlr_box rectbox;
	struct wlr_scene_rect *rect;
};

struct wsm_window_snap {
	struct wlr_box box;
	bool has_target;

	struct wlr_scene_tree *preview_tree;
	struct wlr_scene_rect *preview_fill;
	struct wlr_scene_rect *preview_top;
	struct wlr_scene_rect *preview_bottom;
	struct wlr_scene_rect *preview_left;
	struct wlr_scene_rect *preview_right;
};

struct snap_resize_event {
	enum snap_divider_orientation orientation;
	struct wsm_window *first;
	struct wsm_window *second;
	struct wlr_box first_box;
	struct wlr_box second_box;
	double ref_lx, ref_ly;
};

static struct wl_list snap_records;
static struct wl_list snap_dividers;
static bool snap_lists_initialized = false;
static bool snap_dividers_visible = true;

static const struct wsm_seatop_impl snap_resize_seatop_impl;
static const int snap_edge_threshold = 32;
static const int snap_edge_guard = 16;

static void ensure_snap_lists(void) {
	if (snap_lists_initialized) {
		return;
	}
	wl_list_init(&snap_records);
	wl_list_init(&snap_dividers);
	snap_lists_initialized = true;
}

static struct wlr_box output_usable_area_layout(struct wsm_output *output) {
	struct wlr_box box = output->usable_area;
	box.x += output->lx;
	box.y += output->ly;
	return box;
}

static bool point_in_box(struct wlr_box *box, double x, double y) {
	return x >= box->x && x < box->x + box->width &&
		y >= box->y && y < box->y + box->height;
}

static struct wsm_output *output_at(double lx, double ly) {
	for (int i = 0; i < global_server.scene->outputs->length; ++i) {
		struct wsm_output *output = global_server.scene->outputs->items[i];
		struct wlr_box box;
		output_get_box(output, &box);
		if (point_in_box(&box, lx, ly)) {
			return output;
		}
	}
	return wsm_output_nearest_to(lx, ly);
}

static bool output_has_neighbor(struct wsm_output *output,
		enum wlr_direction direction) {
	struct wlr_box reference;
	output_get_box(output, &reference);

	for (int i = 0; i < global_server.scene->outputs->length; ++i) {
		struct wsm_output *other = global_server.scene->outputs->items[i];
		if (other == output) {
			continue;
		}

		struct wlr_box box;
		output_get_box(other, &box);
		if (direction == WLR_DIRECTION_LEFT &&
				box.x + box.width == reference.x &&
				box.y < reference.y + reference.height &&
				box.y + box.height > reference.y) {
			return true;
		}
		if (direction == WLR_DIRECTION_RIGHT &&
				box.x == reference.x + reference.width &&
				box.y < reference.y + reference.height &&
				box.y + box.height > reference.y) {
			return true;
		}
		if (direction == WLR_DIRECTION_UP &&
				box.y + box.height == reference.y &&
				box.x < reference.x + reference.width &&
				box.x + box.width > reference.x) {
			return true;
		}
		if (direction == WLR_DIRECTION_DOWN &&
				box.y == reference.y + reference.height &&
				box.x < reference.x + reference.width &&
				box.x + box.width > reference.x) {
			return true;
		}
	}

	return false;
}

static void apply_snap_edge_guard(struct wlr_box *area, double *lx, double *ly) {
	if (*lx <= area->x + snap_edge_threshold) {
		*lx += snap_edge_guard;
	} else if (*lx >= area->x + area->width - snap_edge_threshold) {
		*lx -= snap_edge_guard;
	}

	if (*ly <= area->y + snap_edge_threshold) {
		*ly += snap_edge_guard;
	} else if (*ly >= area->y + area->height - snap_edge_threshold) {
		*ly -= snap_edge_guard;
	}
}

static bool calculate_snap_box(double lx, double ly, struct wlr_box *snap_box) {
	struct wsm_output *output = output_at(lx, ly);
	if (!output) {
		return false;
	}

	struct wlr_box area = output_usable_area_layout(output);
	if (wlr_box_empty(&area)) {
		return false;
	}

	apply_snap_edge_guard(&area, &lx, &ly);

	bool near_left = lx <= area.x + snap_edge_threshold &&
		!output_has_neighbor(output, WLR_DIRECTION_LEFT);
	bool near_right = lx >= area.x + area.width - snap_edge_threshold &&
		!output_has_neighbor(output, WLR_DIRECTION_RIGHT);
	bool near_top = ly <= area.y + snap_edge_threshold &&
		!output_has_neighbor(output, WLR_DIRECTION_UP);
	bool near_bottom = ly >= area.y + area.height - snap_edge_threshold &&
		!output_has_neighbor(output, WLR_DIRECTION_DOWN);

	if (!near_left && !near_right && !near_top && !near_bottom) {
		return false;
	}

	*snap_box = area;
	if (near_left) {
		snap_box->width = area.width / 2;
	} else if (near_right) {
		snap_box->width = area.width / 2;
		snap_box->x = area.x + area.width - snap_box->width;
	}

	if (near_top && (near_left || near_right)) {
		snap_box->height = area.height / 2;
	} else if (near_bottom && (near_left || near_right)) {
		snap_box->height = area.height / 2;
		snap_box->y = area.y + area.height - snap_box->height;
	} else if (!near_left && !near_right) {
		if (near_top) {
			snap_box->height = area.height / 2;
		} else if (near_bottom) {
			snap_box->height = area.height / 2;
			snap_box->y = area.y + area.height - snap_box->height;
		} else {
			return false;
		}
	}

	return snap_box->width > 0 && snap_box->height > 0;
}

static bool point_near_output_edge(double lx, double ly) {
	struct wsm_output *output = output_at(lx, ly);
	if (!output) {
		return false;
	}

	struct wlr_box area = output_usable_area_layout(output);
	if (wlr_box_empty(&area)) {
		return false;
	}

	apply_snap_edge_guard(&area, &lx, &ly);

	return lx <= area.x + snap_edge_threshold ||
		lx >= area.x + area.width - snap_edge_threshold ||
		ly <= area.y + snap_edge_threshold ||
		ly >= area.y + area.height - snap_edge_threshold;
}

static void set_content_geometry_from_box(struct wsm_window *window) {
	int border_width = 0;
	int title_height = 0;

	if (window->pending.border != B_CSD && !window->pending.fullscreen_mode) {
		border_width = get_max_thickness(window->pending) *
			(window->pending.border != B_NONE);
		title_height = window->pending.border == B_NORMAL ?
			(int)window_titlebar_height() : border_width;
	}

	int border_top = window->pending.border_top ? border_width : 0;
	int border_bottom = window->pending.border_bottom ? border_width : 0;
	int border_left = window->pending.border_left ? border_width : 0;
	int border_right = window->pending.border_right ? border_width : 0;
	int top = title_height + border_top;

	window->pending.content_x = window->pending.x + border_left;
	window->pending.content_y = window->pending.y + top;
	window->pending.content_width =
		MAX(window->pending.width - border_left - border_right, 0);
	window->pending.content_height =
		MAX(window->pending.height - top - border_bottom, 0);
}

static void set_window_box(struct wsm_window *window,
		struct wlr_box *box) {
	window->pending.x = box->x;
	window->pending.y = box->y;
	window->pending.width = box->width;
	window->pending.height = box->height;
	set_content_geometry_from_box(window);
	node_set_dirty(&window->node);
}

static struct wsm_window_snap_record *find_record(
		struct wsm_window *window) {
	ensure_snap_lists();
	struct wsm_window_snap_record *record;
	wl_list_for_each(record, &snap_records, link) {
		if (record->window == window) {
			return record;
		}
	}
	return NULL;
}

static bool same_vertical_span(struct wlr_box *a, struct wlr_box *b) {
	return a->y == b->y && a->height == b->height;
}

static bool same_horizontal_span(struct wlr_box *a, struct wlr_box *b) {
	return a->x == b->x && a->width == b->width;
}

static void create_divider(enum snap_divider_orientation orientation,
		struct wsm_window_snap_record *first,
		struct wsm_window_snap_record *second, int edge) {
	struct wsm_window_snap_divider *divider =
		calloc(1, sizeof(struct wsm_window_snap_divider));
	if (!divider) {
		wsm_log(WSM_ERROR, "Could not create snap divider: allocation failed!");
		return;
	}

	const int hit_thickness = 6;
	const int visual_thickness = 6;
	const int visual_length = 80;
	struct wlr_box visual_box = {0};
	divider->seen = true;
	divider->orientation = orientation;
	divider->first = first->window;
	divider->second = second->window;
	divider->first_box = first->box;
	divider->second_box = second->box;
	if (orientation == SNAP_DIVIDER_VERTICAL) {
		int length = MIN(visual_length, first->box.height);
		divider->hitbox.x = edge - hit_thickness / 2;
		divider->hitbox.y = first->box.y;
		divider->hitbox.width = hit_thickness;
		divider->hitbox.height = first->box.height;
		visual_box.x = edge - visual_thickness / 2;
		visual_box.y = first->box.y + (first->box.height - length) / 2;
		visual_box.width = visual_thickness;
		visual_box.height = length;
	} else {
		int length = MIN(visual_length, first->box.width);
		divider->hitbox.x = first->box.x;
		divider->hitbox.y = edge - hit_thickness / 2;
		divider->hitbox.width = first->box.width;
		divider->hitbox.height = hit_thickness;
		visual_box.x = first->box.x + (first->box.width - length) / 2;
		visual_box.y = edge - visual_thickness / 2;
		visual_box.width = length;
		visual_box.height = visual_thickness;
	}
	divider->rectbox = visual_box;

	float color[] = {0.24f, 0.52f, 1.0f, 0.35f};
	divider->rect = wlr_scene_rect_create(first->window->scene_tree,
		visual_box.width, visual_box.height, color);
	if (!divider->rect) {
		free(divider);
		wsm_log(WSM_ERROR, "Could not create snap divider rect");
		return;
	}
	wlr_scene_node_set_position(&divider->rect->node,
		visual_box.x - first->box.x, visual_box.y - first->box.y);
	wlr_scene_node_set_enabled(&divider->rect->node, false);
	wl_list_insert(&snap_dividers, &divider->link);
}

static void update_divider(struct wsm_window_snap_divider *divider,
		enum snap_divider_orientation orientation,
		struct wsm_window_snap_record *first,
		struct wsm_window_snap_record *second, int edge) {
	const int hit_thickness = 6;
	const int visual_thickness = 6;
	const int visual_length = 80;
	struct wlr_box visual_box = {0};

	divider->seen = true;
	divider->orientation = orientation;
	divider->first = first->window;
	divider->second = second->window;
	divider->first_box = first->box;
	divider->second_box = second->box;
	if (orientation == SNAP_DIVIDER_VERTICAL) {
		int length = MIN(visual_length, first->box.height);
		divider->hitbox.x = edge - hit_thickness / 2;
		divider->hitbox.y = first->box.y;
		divider->hitbox.width = hit_thickness;
		divider->hitbox.height = first->box.height;
		visual_box.x = edge - visual_thickness / 2;
		visual_box.y = first->box.y + (first->box.height - length) / 2;
		visual_box.width = visual_thickness;
		visual_box.height = length;
	} else {
		int length = MIN(visual_length, first->box.width);
		divider->hitbox.x = first->box.x;
		divider->hitbox.y = edge - hit_thickness / 2;
		divider->hitbox.width = first->box.width;
		divider->hitbox.height = hit_thickness;
		visual_box.x = first->box.x + (first->box.width - length) / 2;
		visual_box.y = edge - visual_thickness / 2;
		visual_box.width = length;
		visual_box.height = visual_thickness;
	}

	divider->rectbox = visual_box;
	wlr_scene_rect_set_size(divider->rect, visual_box.width, visual_box.height);
	wlr_scene_node_set_position(&divider->rect->node,
		visual_box.x - first->box.x, visual_box.y - first->box.y);
}

static void upsert_divider(enum snap_divider_orientation orientation,
		struct wsm_window_snap_record *first,
		struct wsm_window_snap_record *second, int edge) {
	struct wsm_window_snap_divider *divider;
	wl_list_for_each(divider, &snap_dividers, link) {
		if (divider->orientation == orientation &&
				divider->first == first->window &&
				divider->second == second->window) {
			update_divider(divider, orientation, first, second, edge);
			return;
		}
	}

	create_divider(orientation, first, second, edge);
}

static void rebuild_dividers(void) {
	ensure_snap_lists();

	struct wsm_window_snap_divider *divider, *tmp;
	wl_list_for_each(divider, &snap_dividers, link) {
		divider->seen = false;
	}

	struct wsm_window_snap_record *a, *b;
	wl_list_for_each(a, &snap_records, link) {
		wl_list_for_each(b, &snap_records, link) {
			if (a == b) {
				break;
			}

			if (same_vertical_span(&a->box, &b->box)) {
				if (a->box.x + a->box.width == b->box.x) {
					upsert_divider(SNAP_DIVIDER_VERTICAL,
						a, b, b->box.x);
				} else if (b->box.x + b->box.width == a->box.x) {
					upsert_divider(SNAP_DIVIDER_VERTICAL,
						b, a, a->box.x);
				}
			}

			if (same_horizontal_span(&a->box, &b->box)) {
				if (a->box.y + a->box.height == b->box.y) {
					upsert_divider(SNAP_DIVIDER_HORIZONTAL,
						a, b, b->box.y);
				} else if (b->box.y + b->box.height == a->box.y) {
					upsert_divider(SNAP_DIVIDER_HORIZONTAL,
						b, a, a->box.y);
				}
			}
		}
	}

	wl_list_for_each_safe(divider, tmp, &snap_dividers, link) {
		if (divider->seen) {
			continue;
		}
		wl_list_remove(&divider->link);
		if (divider->rect) {
			wlr_scene_node_destroy(&divider->rect->node);
		}
		free(divider);
	}
}

static void remove_record(struct wsm_window_snap_record *record) {
	wl_list_remove(&record->destroy.link);
	wl_list_remove(&record->link);
	free(record);
}

static void handle_record_destroy(struct wl_listener *listener, void *data) {
	struct wsm_window_snap_record *record =
		wl_container_of(listener, record, destroy);
	remove_record(record);
	rebuild_dividers();
}

static void record_window(struct wsm_window *window,
		struct wlr_box *box) {
	ensure_snap_lists();
	struct wsm_window_snap_record *record = find_record(window);
	if (!record) {
		record = calloc(1, sizeof(struct wsm_window_snap_record));
		if (!record) {
			wsm_log(WSM_ERROR, "Could not create snap record: allocation failed!");
			return;
		}
		record->window = window;
		record->destroy.notify = handle_record_destroy;
		wl_signal_add(&window->node.events.destroy, &record->destroy);
		wl_list_insert(&snap_records, &record->link);
	}
	record->box = *box;
	rebuild_dividers();
}

static void record_window_without_rebuild(struct wsm_window *window,
		struct wlr_box *box) {
	ensure_snap_lists();
	struct wsm_window_snap_record *record = find_record(window);
	if (!record) {
		record = calloc(1, sizeof(struct wsm_window_snap_record));
		if (!record) {
			wsm_log(WSM_ERROR, "Could not create snap record: allocation failed!");
			return;
		}
		record->window = window;
		record->destroy.notify = handle_record_destroy;
		wl_signal_add(&window->node.events.destroy, &record->destroy);
		wl_list_insert(&snap_records, &record->link);
	}
	record->box = *box;
}

static bool create_preview(struct wsm_window_snap *snap) {
	if (snap->preview_tree) {
		return true;
	}

	snap->preview_tree =
		wlr_scene_tree_create(global_server.scene->layers.seat);
	if (!snap->preview_tree) {
		wsm_log(WSM_ERROR, "Could not create snap preview tree");
		return false;
	}

	float fill[] = {0.24f, 0.52f, 1.0f, 0.18f};
	float border[] = {0.24f, 0.52f, 1.0f, 0.9f};
	snap->preview_fill = wlr_scene_rect_create(snap->preview_tree, 1, 1, fill);
	snap->preview_top = wlr_scene_rect_create(snap->preview_tree, 1, 1, border);
	snap->preview_bottom = wlr_scene_rect_create(snap->preview_tree, 1, 1, border);
	snap->preview_left = wlr_scene_rect_create(snap->preview_tree, 1, 1, border);
	snap->preview_right = wlr_scene_rect_create(snap->preview_tree, 1, 1, border);
	if (!snap->preview_fill || !snap->preview_top || !snap->preview_bottom ||
			!snap->preview_left || !snap->preview_right) {
		wlr_scene_node_destroy(&snap->preview_tree->node);
		snap->preview_tree = NULL;
		snap->preview_fill = NULL;
		snap->preview_top = NULL;
		snap->preview_bottom = NULL;
		snap->preview_left = NULL;
		snap->preview_right = NULL;
		wsm_log(WSM_ERROR, "Could not create snap preview rectangles");
		return false;
	}

	return true;
}

static void destroy_preview(struct wsm_window_snap *snap) {
	if (snap->preview_tree) {
		wlr_scene_node_destroy(&snap->preview_tree->node);
		snap->preview_tree = NULL;
		snap->preview_fill = NULL;
		snap->preview_top = NULL;
		snap->preview_bottom = NULL;
		snap->preview_left = NULL;
		snap->preview_right = NULL;
	}
}

static void update_preview(struct wsm_window_snap *snap) {
	if (!snap->has_target) {
		destroy_preview(snap);
		return;
	}
	if (!create_preview(snap)) {
		return;
	}

	const int border = 3;
	struct wlr_box box = snap->box;
	wlr_scene_node_set_position(&snap->preview_tree->node, box.x, box.y);
	wlr_scene_rect_set_size(snap->preview_fill, box.width, box.height);

	wlr_scene_rect_set_size(snap->preview_top, box.width, border);
	wlr_scene_rect_set_size(snap->preview_bottom, box.width, border);
	wlr_scene_rect_set_size(snap->preview_left, border, box.height);
	wlr_scene_rect_set_size(snap->preview_right, border, box.height);

	wlr_scene_node_set_position(&snap->preview_top->node, 0, 0);
	wlr_scene_node_set_position(&snap->preview_bottom->node,
		0, MAX(box.height - border, 0));
	wlr_scene_node_set_position(&snap->preview_left->node, 0, 0);
	wlr_scene_node_set_position(&snap->preview_right->node,
		MAX(box.width - border, 0), 0);
}

struct wsm_window_snap *wsm_window_snap_create(void) {
	return calloc(1, sizeof(struct wsm_window_snap));
}

void wsm_window_snap_destroy(struct wsm_window_snap *snap) {
	if (!snap) {
		return;
	}
	destroy_preview(snap);
	free(snap);
}

bool wsm_window_snap_update(struct wsm_window_snap *snap, double lx, double ly) {
	if (!snap) {
		return false;
	}

	if (!point_near_output_edge(lx, ly)) {
		if (snap->has_target) {
			snap->has_target = false;
			snap->box = (struct wlr_box){0};
			update_preview(snap);
		}
		return false;
	}

	struct wlr_box box = {0};
	bool has_target = calculate_snap_box(lx, ly, &box);
	if (snap->has_target == has_target &&
			(!has_target || wlr_box_equal(&snap->box, &box))) {
		return snap->has_target;
	}
	snap->has_target = has_target;
	snap->box = box;
	update_preview(snap);
	return snap->has_target;
}

bool wsm_window_snap_apply(struct wsm_window_snap *snap,
		struct wsm_window *window) {
	if (!snap || !snap->has_target) {
		return false;
	}

	if (window->maximized) {
		window->maximized = false;
		view_maximize(window->view, false);
	}

	window->pending.x = snap->box.x;
	window->pending.y = snap->box.y;
	window->pending.width = snap->box.width;
	window->pending.height = snap->box.height;
	set_content_geometry_from_box(window);
	window_move_to(window,
		window->pending.x, window->pending.y);
	node_set_dirty(&window->node);
	record_window(window, &snap->box);
	return true;
}

void wsm_window_snap_forget_window(struct wsm_window *window) {
	struct wsm_window_snap_record *record = find_record(window);
	if (!record) {
		return;
	}
	remove_record(record);
	rebuild_dividers();
}

void wsm_window_snap_set_dividers_visible(bool visible) {
	ensure_snap_lists();
	if (snap_dividers_visible == visible) {
		return;
	}
	snap_dividers_visible = visible;

	struct wsm_window_snap_divider *divider;
	wl_list_for_each(divider, &snap_dividers, link) {
		if (divider->rect) {
			wlr_scene_node_set_enabled(&divider->rect->node, false);
		}
	}
}

struct wsm_window_snap_divider *wsm_window_snap_divider_at(double lx, double ly) {
	ensure_snap_lists();
	if (!snap_dividers_visible || wl_list_empty(&snap_dividers)) {
		struct wsm_window_snap_divider *divider;
		wl_list_for_each(divider, &snap_dividers, link) {
			if (divider->rect) {
				wlr_scene_node_set_enabled(&divider->rect->node, false);
			}
		}
		return NULL;
	}
	struct wsm_window_snap_divider *hovered = NULL;
	struct wsm_window_snap_divider *active = NULL;
	struct wsm_window_snap_divider *divider;
	wl_list_for_each(divider, &snap_dividers, link) {
		if (point_in_box(&divider->hitbox, lx, ly)) {
			hovered = divider;
			if (point_in_box(&divider->rectbox, lx, ly)) {
				active = divider;
			}
			break;
		}
	}
	wl_list_for_each(divider, &snap_dividers, link) {
		if (divider->rect) {
			wlr_scene_node_set_enabled(&divider->rect->node, hovered == divider);
		}
	}
	return active;
}

struct wsm_window_snap_divider *wsm_window_snap_divider_for_window_edge(
		struct wsm_window *window, int edge, double lx, double ly) {
	ensure_snap_lists();
	if (wl_list_empty(&snap_dividers)) {
		return NULL;
	}
	struct wsm_window_snap_divider *divider;
	wl_list_for_each(divider, &snap_dividers, link) {
		if (!point_in_box(&divider->hitbox, lx, ly)) {
			continue;
		}
		if (divider->orientation == SNAP_DIVIDER_VERTICAL &&
				((edge & WLR_EDGE_RIGHT && divider->first == window) ||
				(edge & WLR_EDGE_LEFT && divider->second == window))) {
			return divider;
		}
		if (divider->orientation == SNAP_DIVIDER_HORIZONTAL &&
				((edge & WLR_EDGE_BOTTOM && divider->first == window) ||
				(edge & WLR_EDGE_TOP && divider->second == window))) {
			return divider;
		}
	}
	return NULL;
}

bool wsm_window_snap_constrain_inner_edge(struct wsm_window *window,
		enum wlr_edges *edge, bool *lock_width, bool *lock_height) {
	ensure_snap_lists();
	if (wl_list_empty(&snap_dividers)) {
		return false;
	}

	struct wsm_window_snap_divider *divider;
	wl_list_for_each(divider, &snap_dividers, link) {
		if (divider->orientation == SNAP_DIVIDER_VERTICAL &&
				((*edge & WLR_EDGE_RIGHT && divider->first == window) ||
				(*edge & WLR_EDGE_LEFT && divider->second == window))) {
			*edge &= WLR_EDGE_LEFT | WLR_EDGE_RIGHT;
			*lock_height = true;
			return true;
		}
		if (divider->orientation == SNAP_DIVIDER_HORIZONTAL &&
				((*edge & WLR_EDGE_BOTTOM && divider->first == window) ||
				(*edge & WLR_EDGE_TOP && divider->second == window))) {
			*edge &= WLR_EDGE_TOP | WLR_EDGE_BOTTOM;
			*lock_width = true;
			return true;
		}
	}
	return false;
}

const char *wsm_window_snap_divider_cursor(
		struct wsm_window_snap_divider *divider) {
	if (!divider) {
		return "default";
	}
	return divider->orientation == SNAP_DIVIDER_VERTICAL ?
		"col-resize" : "row-resize";
}

static void show_snap_divider(enum snap_divider_orientation orientation,
		struct wsm_window *first, struct wsm_window *second) {
	ensure_snap_lists();
	struct wsm_window_snap_divider *divider;
	wl_list_for_each(divider, &snap_dividers, link) {
		if (divider->rect) {
			bool active = divider->orientation == orientation &&
				divider->first == first && divider->second == second;
			wlr_scene_node_set_enabled(&divider->rect->node, active);
		}
	}
}

static void update_active_snap_divider(struct snap_resize_event *e,
		struct wlr_box *first_box, struct wlr_box *second_box) {
	struct wsm_window_snap_divider *divider;
	wl_list_for_each(divider, &snap_dividers, link) {
		if (divider->orientation != e->orientation ||
				divider->first != e->first ||
				divider->second != e->second) {
			continue;
		}

		struct wsm_window_snap_record first = {
			.window = e->first,
			.box = *first_box,
		};
		struct wsm_window_snap_record second = {
			.window = e->second,
			.box = *second_box,
		};
		int edge = e->orientation == SNAP_DIVIDER_VERTICAL ?
			second_box->x : second_box->y;
		update_divider(divider, e->orientation, &first, &second, edge);
		return;
	}

	rebuild_dividers();
}

static void update_snap_resize(struct wsm_seat *seat) {
	struct snap_resize_event *e = seat->seatop_data;
	struct wlr_cursor *cursor = seat->cursor->cursor_wlr;
	struct wlr_box first = e->first_box;
	struct wlr_box second = e->second_box;

	const int min_size = 100;
	if (e->orientation == SNAP_DIVIDER_VERTICAL) {
		int left = e->first_box.x;
		int right = e->second_box.x + e->second_box.width;
		int edge = e->first_box.x + e->first_box.width +
			(cursor->x - e->ref_lx);
		edge = MAX(edge, left + min_size);
		edge = MIN(edge, right - min_size);
		first.width = edge - first.x;
		second.x = edge;
		second.width = right - edge;
	} else {
		int top = e->first_box.y;
		int bottom = e->second_box.y + e->second_box.height;
		int edge = e->first_box.y + e->first_box.height +
			(cursor->y - e->ref_ly);
		edge = MAX(edge, top + min_size);
		edge = MIN(edge, bottom - min_size);
		first.height = edge - first.y;
		second.y = edge;
		second.height = bottom - edge;
	}

	struct wlr_box current_first = {
		.x = e->first->pending.x,
		.y = e->first->pending.y,
		.width = e->first->pending.width,
		.height = e->first->pending.height,
	};
	struct wlr_box current_second = {
		.x = e->second->pending.x,
		.y = e->second->pending.y,
		.width = e->second->pending.width,
		.height = e->second->pending.height,
	};
	if (wlr_box_equal(&first, &current_first) &&
			wlr_box_equal(&second, &current_second)) {
		return;
	}

	set_window_box(e->first, &first);
	set_window_box(e->second, &second);
	record_window_without_rebuild(e->first, &first);
	record_window_without_rebuild(e->second, &second);
	update_active_snap_divider(e, &first, &second);
	show_snap_divider(e->orientation, e->first, e->second);
	transaction_commit_dirty();
}

static void handle_snap_resize_button(struct wsm_seat *seat, uint32_t time_msec,
		struct wlr_input_device *device, uint32_t button,
		enum wl_pointer_button_state state) {
	if (seat->cursor->pressed_button_count == 0) {
		rebuild_dividers();
		seatop_begin_default(seat);
	}
}

static void handle_snap_resize_pointer_motion(struct wsm_seat *seat,
		uint32_t time_msec) {
	update_snap_resize(seat);
}

static void handle_snap_resize_unref(struct wsm_seat *seat,
		struct wsm_window *window) {
	struct snap_resize_event *e = seat->seatop_data;
	if (e->first == window || e->second == window) {
		seatop_begin_default(seat);
	}
}

static const struct wsm_seatop_impl snap_resize_seatop_impl = {
	.button = handle_snap_resize_button,
	.pointer_motion = handle_snap_resize_pointer_motion,
	.unref = handle_snap_resize_unref,
};

void wsm_window_snap_begin_resize_divider(struct wsm_seat *seat,
		struct wsm_window_snap_divider *divider) {
	if (!divider) {
		return;
	}
	seatop_end(seat);

	struct snap_resize_event *e = calloc(1, sizeof(struct snap_resize_event));
	if (!e) {
		wsm_log(WSM_ERROR, "Could not create snap resize event: allocation failed!");
		return;
	}
	e->orientation = divider->orientation;
	e->first = divider->first;
	e->second = divider->second;
	e->first_box = divider->first_box;
	e->second_box = divider->second_box;
	e->ref_lx = seat->cursor->cursor_wlr->x;
	e->ref_ly = seat->cursor->cursor_wlr->y;
	show_snap_divider(e->orientation, e->first, e->second);

	seat->seatop_impl = &snap_resize_seatop_impl;
	seat->seatop_data = e;
	cursor_set_image(seat->cursor,
		wsm_window_snap_divider_cursor(divider), NULL);
	wlr_seat_pointer_notify_clear_focus(seat->seat);
}
