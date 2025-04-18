#include "wsm_seatop_default.h"
#include "wsm_seat.h"
#include "wsm_log.h"
#include "wsm_drag.h"
#include "wsm_list.h"
#include "wsm_view.h"
#include "wsm_server.h"
#include "wsm_scene.h"
#include "wsm_cursor.h"
#include "wsm_container.h"
#include "wsm_tablet.h"
#include "wsm_output.h"
#include "wsm_transaction.h"
#include "wsm_seatop_down.h"
#include "wsm_layer_shell.h"
#include "wsm_workspace.h"
#include "wsm_input_manager.h"
#include "wsm_seatop_move_floating.h"
#include "wsm_seatop_resize_floating.h"
#include "node/wsm_node.h"

#include <linux/input-event-codes.h>

#if HAVE_XWAYLAND
#include <wlr/xwayland.h>
#include <wlr/xwayland/xwayland.h>
#include <wlr/xwayland/server.h>
#include <wlr/xwayland/shell.h>
#endif

#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/util/edges.h>
#include <wlr/types/wlr_tablet_v2.h>
#include <wlr/types/wlr_layer_shell_v1.h>
#include <wlr/types/wlr_pointer_gestures_v1.h>

struct seatop_default_event {
	uint32_t pressed_buttons[WSM_CURSOR_PRESSED_BUTTONS_CAP];
	struct wsm_gesture_tracker gestures;
	struct wsm_node *previous_node;
	size_t pressed_button_count;
};

static bool edge_is_external(struct wsm_container *cont, enum wlr_edges edge) {
	enum wsm_container_layout layout = L_NONE;

	while (cont) {
		if (container_parent_layout(cont) == layout) {
			struct wsm_list *siblings = container_get_siblings(cont);
			if (!siblings) {
				return false;
			}
			int index = wsm_list_find(siblings, cont);
			if (index > 0 && (edge == WLR_EDGE_LEFT || edge == WLR_EDGE_TOP)) {
				return false;
			}
			if (index < siblings->length - 1 &&
				(edge == WLR_EDGE_RIGHT || edge == WLR_EDGE_BOTTOM)) {
				return false;
			}
		}
		cont = cont->pending.parent;
	}
	return true;
}

static enum wlr_edges find_edge(struct wsm_container *cont,
		struct wlr_surface *surface, struct wsm_cursor *cursor) {
	if (!cont->view || (surface && cont->view->surface != surface)) {
		return WLR_EDGE_NONE;
	}
	int max_thickness = get_max_thickness(cont->pending);
	if (cont->pending.border == B_NONE || !max_thickness ||
		cont->pending.border == B_CSD) {
		return WLR_EDGE_NONE;
	}
	if (cont->pending.fullscreen_mode) {
		return WLR_EDGE_NONE;
	}

	enum wlr_edges edge = 0;
	if (cursor->cursor_wlr->x < cont->pending.x + max_thickness) {
		edge |= WLR_EDGE_LEFT;
	}
	if (cursor->cursor_wlr->y < cont->pending.y + max_thickness) {
		edge |= WLR_EDGE_TOP;
	}
	if (cursor->cursor_wlr->x >= cont->pending.x + cont->pending.width - max_thickness) {
		edge |= WLR_EDGE_RIGHT;
	}
	if (cursor->cursor_wlr->y >= cont->pending.y + cont->pending.height - max_thickness) {
		edge |= WLR_EDGE_BOTTOM;
	}

	return edge;
}

/**
 * If the cursor is over a _resizable_ edge, return the edge.
 * Edges that can't be resized are edges of the workspace.
 */
enum wlr_edges find_resize_edge(struct wsm_container *cont,
		struct wlr_surface *surface, struct wsm_cursor *cursor) {
	enum wlr_edges edge = find_edge(cont, surface, cursor);
	if (edge && !container_is_floating(cont) && edge_is_external(cont, edge)) {
		return WLR_EDGE_NONE;
	}
	return edge;
}

/**
 * Remove a button (and duplicates) from the sorted list of currently pressed
 * buttons.
 */
static void state_erase_button(struct seatop_default_event *e, uint32_t button) {
	size_t j = 0;
	for (size_t i = 0; i < e->pressed_button_count; ++i) {
		if (i > j) {
			e->pressed_buttons[j] = e->pressed_buttons[i];
		}
		if (e->pressed_buttons[i] != button) {
			++j;
		}
	}
	while (e->pressed_button_count > j) {
		--e->pressed_button_count;
		e->pressed_buttons[e->pressed_button_count] = 0;
	}
}

/**
 * Add a button to the sorted list of currently pressed buttons, if there
 * is space.
 */
static void state_add_button(struct seatop_default_event *e, uint32_t button) {
	if (e->pressed_button_count >= WSM_CURSOR_PRESSED_BUTTONS_CAP) {
		return;
	}
	size_t i = 0;
	while (i < e->pressed_button_count && e->pressed_buttons[i] < button) {
		++i;
	}
	size_t j = e->pressed_button_count;
	while (j > i) {
		e->pressed_buttons[j] = e->pressed_buttons[j - 1];
		--j;
	}
	e->pressed_buttons[i] = button;
	e->pressed_button_count++;
}

static void handle_tablet_tool_tip(struct wsm_seat *seat,
		struct wsm_tablet_tool *tool, uint32_t time_msec,
		enum wlr_tablet_tool_tip_state state) {
	if (state == WLR_TABLET_TOOL_TIP_UP) {
		wlr_tablet_v2_tablet_tool_notify_up(tool->tablet_v2_tool);
		return;
	}

	struct wsm_cursor *cursor = seat->cursor;
	struct wlr_surface *surface = NULL;
	double sx, sy;
	struct wsm_node *node = node_at_coords(seat,
		cursor->cursor_wlr->x, cursor->cursor_wlr->y, &surface, &sx, &sy);

	if (!wsm_assert(surface,
				"Expected null-surface tablet input to route through pointer emulation")) {
		return;
	}

	struct wsm_container *cont = node && node->type == N_CONTAINER ?
		node->container : NULL;

	struct wlr_layer_surface_v1 *layer;
#if HAVE_XWAYLAND
	struct wlr_xwayland_surface *xsurface;
#endif
	if ((layer = wlr_layer_surface_v1_try_from_wlr_surface(surface)) &&
		layer->current.keyboard_interactive) {
		seat_set_focus_layer(seat, layer);
		transaction_commit_dirty();
	} else if (cont) {
		bool is_floating_or_child = container_is_floating_or_child(cont);
		bool is_fullscreen_or_child = container_is_fullscreen_or_child(cont);
		struct wlr_keyboard *keyboard = wlr_seat_get_keyboard(seat->seat);
		bool mod_pressed = keyboard &&
			(wlr_keyboard_get_modifiers(keyboard));

		if (is_floating_or_child && !is_fullscreen_or_child && mod_pressed) {
			seat_set_focus_container(seat,
				seat_get_focus_inactive_view(seat, &cont->node));
			seatop_begin_move_floating(seat, container_toplevel_ancestor(cont));
			return;
		}

		seat_set_focus_container(seat, cont);
		seatop_begin_down(seat, node->container, sx, sy);
	}
#if HAVE_XWAYLAND
	// Handle tapping on an xwayland unmanaged view
	else if ((xsurface = wlr_xwayland_surface_try_from_wlr_surface(surface)) &&
			 xsurface->override_redirect &&
			 wlr_xwayland_or_surface_wants_focus(xsurface)) {
		struct wlr_xwayland *xwayland = global_server.xwayland.xwayland_wlr;
		wlr_xwayland_set_seat(xwayland, seat->seat);
		seat_set_focus_surface(seat, xsurface->surface, false);
		transaction_commit_dirty();
	}
#endif

	wlr_tablet_v2_tablet_tool_notify_down(tool->tablet_v2_tool);
	wlr_tablet_tool_v2_start_implicit_grab(tool->tablet_v2_tool);
}

static bool trigger_pointer_button_binding(struct wsm_seat *seat,
		struct wlr_input_device *device, uint32_t button,
		enum wl_pointer_button_state state, uint32_t modifiers,
		bool on_titlebar, bool on_border, bool on_contents, bool on_workspace) {
	if (device && device->type != WLR_INPUT_DEVICE_POINTER) {
		return false;
	}
	
	return false;
}

static void handle_button(struct wsm_seat *seat, uint32_t time_msec,
		struct wlr_input_device *device, uint32_t button,
		enum wl_pointer_button_state state) {
	struct wsm_cursor *cursor = seat->cursor;

	struct wlr_surface *surface = NULL;
	double sx, sy;
	struct wsm_node *node = node_at_coords(seat,
		cursor->cursor_wlr->x, cursor->cursor_wlr->y, &surface, &sx, &sy);

	struct wsm_container *cont = node && node->type == N_CONTAINER ?
		node->container : NULL;
	bool is_floating_or_child = cont && container_is_floating_or_child(cont);
	bool is_fullscreen_or_child = cont && container_is_fullscreen_or_child(cont);
	enum wlr_edges edge = cont ? find_edge(cont, surface, cursor) : WLR_EDGE_NONE;
	enum wlr_edges resize_edge = cont && edge ?
		find_resize_edge(cont, surface, cursor) : WLR_EDGE_NONE;
	bool on_border = edge != WLR_EDGE_NONE;
	bool on_contents = cont && !on_border && surface;
	bool on_workspace = node && node->type == N_WORKSPACE;
	bool on_titlebar = cont && !on_border && !surface;

	struct wlr_keyboard *keyboard = wlr_seat_get_keyboard(seat->seat);
	uint32_t modifiers = keyboard ? wlr_keyboard_get_modifiers(keyboard) : 0;
	if (trigger_pointer_button_binding(seat, device, button, state, modifiers,
			on_titlebar, on_border, on_contents, on_workspace)) {
		return;
	}

	if (node && node->type == N_WORKSPACE) {
		if (state == WL_POINTER_BUTTON_STATE_PRESSED) {
			seat_set_focus(seat, node);
			transaction_commit_dirty();
		}
		seat_pointer_notify_button(seat, time_msec, button, state);
		return;
	}

	struct wlr_layer_surface_v1 *layer = NULL;
	if ((layer = toplevel_layer_surface_from_surface(surface))) {
		if (layer->current.keyboard_interactive) {
			seat_set_focus_layer(seat, layer);
			transaction_commit_dirty();
		}
		if (state == WL_POINTER_BUTTON_STATE_PRESSED) {
			seatop_begin_down_on_surface(seat, surface, sx, sy);
		}
		seat_pointer_notify_button(seat, time_msec, button, state);
		return;
	}

	if (cont && state == WL_POINTER_BUTTON_STATE_PRESSED) {
		node = &cont->node;
		if (on_titlebar) {
			struct wsm_container *focus = seat_get_focused_container(seat);
			if (focus == cont || !container_has_ancestor(focus, cont)) {
				node = seat_get_focus_inactive(seat, &cont->node);
			}
		}

		seat_set_focus(seat, node);
		transaction_commit_dirty();
	}

	bool mod_pressed = modifiers;
	if (cont && is_floating_or_child && !is_fullscreen_or_child &&
		state == WL_POINTER_BUTTON_STATE_PRESSED) {
		uint32_t btn_move = BTN_LEFT;
		if (button == btn_move && (mod_pressed || on_titlebar)) {
			seatop_begin_move_floating(seat, container_toplevel_ancestor(cont));
			return;
		}
	}

	if (cont && is_floating_or_child && !is_fullscreen_or_child &&
		state == WL_POINTER_BUTTON_STATE_PRESSED) {
		if (button == BTN_LEFT && resize_edge != WLR_EDGE_NONE) {
			seat_set_focus_container(seat, cont);
			seatop_begin_resize_floating(seat, cont, resize_edge);
			return;
		}

		uint32_t btn_resize = BTN_LEFT;
		if (mod_pressed && button == btn_resize) {
			struct wsm_container *floater = container_toplevel_ancestor(cont);
			edge = 0;
			edge |= cursor->cursor_wlr->x > floater->pending.x + floater->pending.width / 2 ?
					WLR_EDGE_RIGHT : WLR_EDGE_LEFT;
			edge |= cursor->cursor_wlr->y > floater->pending.y + floater->pending.height / 2 ?
					WLR_EDGE_BOTTOM : WLR_EDGE_TOP;
			seat_set_focus_container(seat, floater);
			seatop_begin_resize_floating(seat, floater, edge);
			return;
		}
	}

	if (surface && cont && state == WL_POINTER_BUTTON_STATE_PRESSED) {
		seatop_begin_down(seat, cont, sx, sy);
		seat_pointer_notify_button(seat, time_msec, button, WL_POINTER_BUTTON_STATE_PRESSED);
		return;
	}
	
	if (cont && state == WL_POINTER_BUTTON_STATE_PRESSED) {
		seat_pointer_notify_button(seat, time_msec, button, state);
		return;
	}

#if HAVE_XWAYLAND
	// Handle clicking on xwayland unmanaged view
	struct wlr_xwayland_surface *xsurface;
	if (surface &&
		(xsurface = wlr_xwayland_surface_try_from_wlr_surface(surface)) &&
		xsurface->override_redirect &&
		wlr_xwayland_or_surface_wants_focus(xsurface)) {
		struct wlr_xwayland *xwayland = global_server.xwayland.xwayland_wlr;
		wlr_xwayland_set_seat(xwayland, seat->seat);
		seat_set_focus_surface(seat, xsurface->surface, false);
		transaction_commit_dirty();
		seat_pointer_notify_button(seat, time_msec, button, state);
	}
#endif
	
	seat_pointer_notify_button(seat, time_msec, button, state);
}

static void check_focus_follows_mouse(struct wsm_seat *seat,
		struct seatop_default_event *e, struct wsm_node *hovered_node) {
	struct wsm_node *focus = seat_get_focus(seat);

	if (!hovered_node) {
		struct wlr_output *wlr_output = wlr_output_layout_output_at(
			global_server.scene->output_layout, seat->cursor->cursor_wlr->x,
			seat->cursor->cursor_wlr->y);
		if (wlr_output == NULL) {
			return;
		}

		struct wlr_surface *surface = NULL;
		double sx, sy;
		node_at_coords(seat, seat->cursor->cursor_wlr->x,
			seat->cursor->cursor_wlr->y, &surface, &sx, &sy);

		struct wlr_layer_surface_v1 *layer = NULL;
		if ((layer = toplevel_layer_surface_from_surface(surface)) &&
			layer->current.keyboard_interactive) {
			seat_set_focus_layer(seat, layer);
			transaction_commit_dirty();
			return;
		}

		struct wsm_output *hovered_output = wlr_output->data;
		if (focus && hovered_output != node_get_output(focus)) {
			struct wsm_workspace *ws = output_get_active_workspace(hovered_output);
			seat_set_focus(seat, &ws->node);
			transaction_commit_dirty();
		}
		return;
	}

	if (focus && hovered_node->type == N_WORKSPACE) {
		struct wsm_output *focused_output = node_get_output(focus);
		struct wsm_output *hovered_output = node_get_output(hovered_node);
		if (hovered_output != focused_output) {
			seat_set_focus(seat, seat_get_focus_inactive(seat, hovered_node));
			transaction_commit_dirty();
		}
		return;
	}

	if (node_is_view(hovered_node) &&
		view_is_visible(hovered_node->container->view)) {
		if (hovered_node != e->previous_node) {
			seat_set_focus(seat, hovered_node);
			transaction_commit_dirty();
		}
	}
}

static void handle_pointer_motion(struct wsm_seat *seat, uint32_t time_msec) {
	struct seatop_default_event *e = seat->seatop_data;
	struct wsm_cursor *cursor = seat->cursor;

	struct wlr_surface *surface = NULL;
	double sx, sy;
	struct wsm_node *node = node_at_coords(seat,
		cursor->cursor_wlr->x, cursor->cursor_wlr->y, &surface, &sx, &sy);

	check_focus_follows_mouse(seat, e, node);

	if (surface) {
		if (seat_is_input_allowed(seat, surface)) {
			wlr_seat_pointer_notify_enter(seat->seat, surface, sx, sy);
			wlr_seat_pointer_notify_motion(seat->seat, time_msec, sx, sy);
		}
	} else {
		cursor_update_image(cursor, node);
		wlr_seat_pointer_notify_clear_focus(seat->seat);
	}

	wsm_drag_icons_update_position(seat);

	e->previous_node = node;
}

static void handle_tablet_tool_motion(struct wsm_seat *seat,
		struct wsm_tablet_tool *tool, uint32_t time_msec) {
	struct seatop_default_event *e = seat->seatop_data;
	struct wsm_cursor *cursor = seat->cursor;

	struct wlr_surface *surface = NULL;
	double sx, sy;
	struct wsm_node *node = node_at_coords(seat,
		cursor->cursor_wlr->x, cursor->cursor_wlr->y, &surface, &sx, &sy);

	check_focus_follows_mouse(seat, e, node);

	if (surface) {
		if (seat_is_input_allowed(seat, surface)) {
			wlr_tablet_v2_tablet_tool_notify_proximity_in(tool->tablet_v2_tool,
				tool->tablet->tablet_v2, surface);
			wlr_tablet_v2_tablet_tool_notify_motion(tool->tablet_v2_tool, sx, sy);
		}
	} else {
		cursor_update_image(cursor, node);
		wlr_tablet_v2_tablet_tool_notify_proximity_out(tool->tablet_v2_tool);
	}

	wsm_drag_icons_update_position(seat);
	e->previous_node = node;
}

static void handle_touch_down(struct wsm_seat *seat,
		struct wlr_touch_down_event *event, double lx, double ly) {
	struct wlr_surface *surface = NULL;
	struct wlr_seat *wlr_seat = seat->seat;
	struct wsm_cursor *cursor = seat->cursor;
	double sx, sy;
	node_at_coords(seat, seat->touch_x, seat->touch_y, &surface, &sx, &sy);

	if (surface && wlr_surface_accepts_touch(wlr_seat, surface)) {
		if (seat_is_input_allowed(seat, surface)) {
			cursor->simulating_pointer_from_touch = false;
			seatop_begin_touch_down(seat, surface, event, sx, sy, lx, ly);
		}
	} else if (!cursor->simulating_pointer_from_touch &&
			(!surface || seat_is_input_allowed(seat, surface))) {
		cursor->simulating_pointer_from_touch = true;
		cursor->pointer_touch_id = seat->touch_id;
		double dx, dy;
		dx = seat->touch_x - cursor->cursor_wlr->x;
		dy = seat->touch_y - cursor->cursor_wlr->y;
		pointer_motion(cursor, event->time_msec, &event->touch->base, dx, dy,
			dx, dy);
		dispatch_cursor_button(cursor, &event->touch->base, event->time_msec,
			BTN_LEFT, WL_POINTER_BUTTON_STATE_PRESSED);
	}
}

static uint32_t wl_axis_to_button(struct wlr_pointer_axis_event *event) {
	switch (event->orientation) {
	case WL_POINTER_AXIS_VERTICAL_SCROLL:
		return event->delta < 0 ? WSM_SCROLL_UP : WSM_SCROLL_DOWN;
	case WL_POINTER_AXIS_HORIZONTAL_SCROLL:
		return event->delta < 0 ? WSM_SCROLL_LEFT : WSM_SCROLL_RIGHT;
	default:
		wsm_log(WSM_DEBUG, "Unknown axis orientation");
		return 0;
	}
}

static void handle_pointer_axis(struct wsm_seat *seat,
		struct wlr_pointer_axis_event *event) {
	struct wsm_input_device *input_device =
			event->pointer ? event->pointer->base.data : NULL;
	struct wsm_cursor *cursor = seat->cursor;
	struct seatop_default_event *e = seat->seatop_data;

	struct wlr_surface *surface = NULL;
	double sx, sy;
	struct wsm_node *node = node_at_coords(seat,
		cursor->cursor_wlr->x, cursor->cursor_wlr->y, &surface, &sx, &sy);
	struct wsm_container *cont = node && node->type == N_CONTAINER ?
		node->container : NULL;
	enum wlr_edges edge = cont ? find_edge(cont, surface, cursor) : WLR_EDGE_NONE;
	bool on_border = edge != WLR_EDGE_NONE;
	bool on_titlebar = cont && !on_border && !surface;
	bool on_titlebar_border = cont && on_border &&
		cursor->cursor_wlr->y < cont->pending.content_y;

		float scroll_factor =1.0f;
	bool handled = false;
	struct wlr_input_device *device =
		input_device ? input_device->input_device_wlr : NULL;
	char *dev_id = device ? input_device_get_identifier(device) : strdup("*");
	uint32_t button = wl_axis_to_button(event);
	state_add_button(e, button);

	if (!handled && (on_titlebar || on_titlebar_border)) {
		struct wsm_node *new_focus;
		struct wsm_node *tabcontainer = node_get_parent(node);
		struct wsm_node *active =
			seat_get_active_tiling_child(seat, tabcontainer);
		struct wsm_list *siblings = container_get_siblings(cont);
		int desired = wsm_list_find(siblings, active->container) +
			roundf(scroll_factor * event->delta_discrete / WLR_POINTER_AXIS_DISCRETE_STEP);
		if (desired < 0) {
			desired = 0;
		} else if (desired >= siblings->length) {
			desired = siblings->length - 1;
		}

		struct wsm_container *new_sibling_con = siblings->items[desired];
		struct wsm_node *new_sibling = &new_sibling_con->node;
		new_focus = seat_get_focus_inactive(seat, new_sibling);

		seat_set_focus(seat, new_focus);
		transaction_commit_dirty();
		handled = true;
	}

	state_erase_button(e, button);
	free(dev_id);

	if (!handled) {
		wlr_seat_pointer_notify_axis(cursor->seat_wsm->seat, event->time_msec,
			event->orientation, scroll_factor * event->delta,
			roundf(scroll_factor * event->delta_discrete), event->source,
			event->relative_direction);
	}
}

static void handle_hold_begin(struct wsm_seat *seat,
		struct wlr_pointer_hold_begin_event *event) {
	struct wsm_cursor *cursor = seat->cursor;
	wlr_pointer_gestures_v1_send_hold_begin(
		global_server.input_manager->pointer_gestures_wlr, cursor->seat_wsm->seat,
		event->time_msec, event->fingers);
}

static void handle_hold_end(struct wsm_seat *seat,
		struct wlr_pointer_hold_end_event *event) {
	struct wsm_cursor *cursor = seat->cursor;
	wlr_pointer_gestures_v1_send_hold_end(
		global_server.input_manager->pointer_gestures_wlr, cursor->seat_wsm->seat,
		event->time_msec, event->cancelled);
}

static void handle_pinch_begin(struct wsm_seat *seat,
		struct wlr_pointer_pinch_begin_event *event) {
	struct wsm_cursor *cursor = seat->cursor;
	wlr_pointer_gestures_v1_send_pinch_begin(
		global_server.input_manager->pointer_gestures_wlr, cursor->seat_wsm->seat,
		event->time_msec, event->fingers);
}

static void handle_pinch_update(struct wsm_seat *seat,
		struct wlr_pointer_pinch_update_event *event) {
	struct wsm_cursor *cursor = seat->cursor;
	wlr_pointer_gestures_v1_send_pinch_update(
		global_server.input_manager->pointer_gestures_wlr,
		cursor->seat_wsm->seat,
		event->time_msec, event->dx, event->dy,
		event->scale, event->rotation);
}

static void handle_pinch_end(struct wsm_seat *seat,
		struct wlr_pointer_pinch_end_event *event) {
	struct wsm_cursor *cursor = seat->cursor;
	wlr_pointer_gestures_v1_send_pinch_end(
		global_server.input_manager->pointer_gestures_wlr, cursor->seat_wsm->seat,
		event->time_msec, event->cancelled);
}

static void handle_swipe_begin(struct wsm_seat *seat,
		struct wlr_pointer_swipe_begin_event *event) {
	struct wsm_cursor *cursor = seat->cursor;
	wlr_pointer_gestures_v1_send_swipe_begin(
		global_server.input_manager->pointer_gestures_wlr, cursor->seat_wsm->seat,
		event->time_msec, event->fingers);
}

static void handle_swipe_update(struct wsm_seat *seat,
		struct wlr_pointer_swipe_update_event *event) {

	struct wsm_cursor *cursor = seat->cursor;
	wlr_pointer_gestures_v1_send_swipe_update(
		global_server.input_manager->pointer_gestures_wlr, cursor->seat_wsm->seat,
		event->time_msec, event->dx, event->dy);
}

static void handle_swipe_end(struct wsm_seat *seat,
		struct wlr_pointer_swipe_end_event *event) {
	struct wsm_cursor *cursor = seat->cursor;
	wlr_pointer_gestures_v1_send_swipe_end(global_server.input_manager->pointer_gestures_wlr,
		cursor->seat_wsm->seat, event->time_msec, event->cancelled);
}

static void handle_rebase(struct wsm_seat *seat, uint32_t time_msec) {
	struct seatop_default_event *e = seat->seatop_data;
	struct wsm_cursor *cursor = seat->cursor;
	struct wlr_surface *surface = NULL;
	double sx = 0.0, sy = 0.0;
	e->previous_node = node_at_coords(seat,
		cursor->cursor_wlr->x, cursor->cursor_wlr->y, &surface, &sx, &sy);

	if (surface) {
		if (seat_is_input_allowed(seat, surface)) {
			wlr_seat_pointer_notify_enter(seat->seat, surface, sx, sy);
			wlr_seat_pointer_notify_motion(seat->seat, time_msec, sx, sy);
		}
	} else {
		cursor_update_image(cursor, e->previous_node);
		wlr_seat_pointer_notify_clear_focus(seat->seat);
	}
}

static const struct wsm_seatop_impl seatop_impl = {
	.button = handle_button,
	.pointer_motion = handle_pointer_motion,
	.pointer_axis = handle_pointer_axis,
	.tablet_tool_tip = handle_tablet_tool_tip,
	.tablet_tool_motion = handle_tablet_tool_motion,
	.hold_begin = handle_hold_begin,
	.hold_end = handle_hold_end,
	.pinch_begin = handle_pinch_begin,
	.pinch_update = handle_pinch_update,
	.pinch_end = handle_pinch_end,
	.swipe_begin = handle_swipe_begin,
	.swipe_update = handle_swipe_update,
	.swipe_end = handle_swipe_end,
	.touch_down = handle_touch_down,
	.rebase = handle_rebase,
	.allow_set_cursor = true,
};

void seatop_begin_default(struct wsm_seat *seat) {
	seatop_end(seat);

	struct seatop_default_event *e =
		calloc(1, sizeof(struct seatop_default_event));
	if (!e) {
		wsm_log(WSM_ERROR, "Could not create seatop_default_event: allocation failed!");
		return;
	}

	seat->seatop_impl = &seatop_impl;
	seat->seatop_data = e;
	seatop_rebase(seat, 0);
}
