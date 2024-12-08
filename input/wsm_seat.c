#include "wsm_seat.h"
#include "wsm_server.h"
#include "wsm_scene.h"
#include "wsm_log.h"
#include "wsm_list.h"
#include "wsm_view.h"
#include "wsm_switch.h"
#include "wsm_cursor.h"
#include "wsm_keyboard.h"
#include "wsm_tablet.h"
#include "wsm_output.h"
#include "wsm_drag.h"
#include "wsm_session_lock.h"
#include "wsm_input_manager.h"
#include "wsm_seatop_default.h"
#include "wsm_workspace.h"
#include "wsm_arrange.h"
#include "wsm_workspace.h"
#include "wsm_container.h"
#include "wsm_keyboard.h"
#include "wsm_pointer.h"
#include "node/wsm_node_descriptor.h"

#include <stdlib.h>
#include <assert.h>

#if HAVE_XWAYLAND
#include <wlr/xwayland.h>
#endif
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_touch.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_data_device.h>
#include <wlr/types/wlr_idle_notify_v1.h>
#include <wlr/types/wlr_layer_shell_v1.h>
#include <wlr/types/wlr_primary_selection.h>
#include <wlr/types/wlr_pointer_constraints_v1.h>

static void seat_apply_input_mapping(struct wsm_seat *seat,
	struct wsm_seat_device *device) {

}

static void seat_device_destroy(struct wsm_seat_device *seat_device) {
	if (!seat_device) {
		wsm_log(WSM_ERROR, "wsm_seat_device is NULL!");
		return;
	}

	switch (seat_device->input_device->input_device_wlr->type) {
	case WLR_INPUT_DEVICE_KEYBOARD:
		wsm_keyboard_destroy(seat_device->keyboard);
		seat_device->keyboard = NULL;
		break;
	case WLR_INPUT_DEVICE_POINTER:
		wsm_pointer_destroy(seat_device->pointer);
		seat_device->pointer = NULL;
		break;
	case WLR_INPUT_DEVICE_TOUCH:
		wlr_cursor_detach_input_device(seat_device->seat->cursor->cursor_wlr,
			seat_device->input_device->input_device_wlr);
		break;
	case WLR_INPUT_DEVICE_TABLET:
		wsm_tablet_destroy(seat_device->tablet);
		seat_device->tablet = NULL;
		break;
	case WLR_INPUT_DEVICE_SWITCH:
		wsm_switch_destroy(seat_device->_switch);
		seat_device->_switch = NULL;
		break;
	case WLR_INPUT_DEVICE_TABLET_PAD:
		wsm_tablet_pad_destroy(seat_device->tablet_pad);
		seat_device->tablet_pad = NULL;
		break;
	}

	wl_list_remove(&seat_device->link);
	free(seat_device);
}

static void seat_node_destroy(struct wsm_seat_node *seat_node) {
	wl_list_remove(&seat_node->destroy.link);
	wl_list_remove(&seat_node->link);

	if (wl_list_empty(&seat_node->seat->focus_stack)) {
		seat_node->seat->has_focus = false;
	}

	free(seat_node);
}

static void handle_seat_destroy(struct wl_listener *listener, void *data) {
	struct wsm_seat *seat = wl_container_of(listener, seat, destroy);
	struct wsm_seat_device *seat_device, *next;
	wl_list_for_each_safe(seat_device, next, &seat->devices, link) {
		seat_device_destroy(seat_device);
	}
	struct wsm_seat_node *seat_node, *next_seat_node;
	wl_list_for_each_safe(seat_node, next_seat_node, &seat->focus_stack, link) {
		seat_node_destroy(seat_node);
	}

	wsm_input_method_relay_finish(&seat->im_relay);

	wsm_cursor_destroy(seat->cursor);
	wl_list_remove(&seat->new_node.link);
	wl_list_remove(&seat->request_start_drag.link);
	wl_list_remove(&seat->start_drag.link);
	wl_list_remove(&seat->request_set_selection.link);
	wl_list_remove(&seat->request_set_primary_selection.link);
	wl_list_remove(&seat->link);
	wl_list_remove(&seat->destroy.link);
	wlr_scene_node_destroy(&seat->scene_tree->node);
	wsm_list_destroy(seat->deferred_bindings);
	free(seat);
}

static void seat_send_activate(struct wsm_node *node, struct wsm_seat *seat) {
	if (node_is_view(node)) {
		view_set_activated(node->container->view, true);
	} else {
		struct wsm_list *children = node_get_children(node);
		for (int i = 0; i < children->length; ++i) {
			struct wsm_container *child = children->items[i];
			seat_send_activate(&child->node, seat);
		}
	}
}

static void seat_keyboard_notify_enter(struct wsm_seat *seat, struct wlr_surface *surface) {
	struct wlr_keyboard *keyboard = wlr_seat_get_keyboard(seat->seat);
	if (!keyboard) {
		wlr_seat_keyboard_notify_enter(seat->seat, surface, NULL, 0, NULL);
		return;
	}

	struct wsm_keyboard *wsm_keyboard =
		wsm_keyboard_for_wlr_keyboard(seat, keyboard);
	assert(wsm_keyboard && "Cannot find wsm_keyboard for seat keyboard");

	struct wsm_shortcut_state *state = &wsm_keyboard->state_pressed_sent;
	wlr_seat_keyboard_notify_enter(seat->seat, surface,
		state->pressed_keycodes, state->npressed, &keyboard->modifiers);
}

static void seat_tablet_pads_set_focus(struct wsm_seat *seat,
	struct wlr_surface *surface) {
	struct wsm_seat_device *seat_device;
	wl_list_for_each(seat_device, &seat->devices, link) {
		wsm_tablet_pad_set_focus(seat_device->tablet_pad, surface);
	}
}

static void seat_send_focus(struct wsm_node *node, struct wsm_seat *seat) {
	seat_send_activate(node, seat);

	struct wsm_view *view = node->type == N_CONTAINER ?
		node->container->view : NULL;

	if (view) {
#if HAVE_XWAYLAND
		if (view->type == WSM_VIEW_XWAYLAND) {
			struct wlr_xwayland *xwayland = global_server.xwayland.xwayland_wlr;
			wlr_xwayland_set_seat(xwayland, seat->seat);
		}
#endif

		seat_keyboard_notify_enter(seat, view->surface);
		seat_tablet_pads_set_focus(seat, view->surface);
		wsm_input_method_relay_set_focus(&seat->im_relay, view->surface);

		struct wlr_pointer_constraint_v1 *constraint =
			wlr_pointer_constraints_v1_constraint_for_surface(
				global_server.pointer_constraints, view->surface, seat->seat);
		wsm_cursor_constrain(seat->cursor, constraint);
	}
}

static void handle_seat_node_destroy(struct wl_listener *listener, void *data) {
	struct wsm_seat_node *seat_node =
		wl_container_of(listener, seat_node, destroy);
	struct wsm_seat *seat = seat_node->seat;
	struct wsm_node *node = seat_node->node;
	struct wsm_node *parent = node_get_parent(node);
	struct wsm_node *focus = seat_get_focus(seat);

	if (node->type == N_WORKSPACE) {
		seat_node_destroy(seat_node);
		if (seat->workspace == node->workspace) {
			struct wsm_node *node = seat_get_focus_inactive(seat, &global_server.scene->node);
			seat_set_focus(seat, NULL);
			if (node) {
				seat_set_focus(seat, node);
			} else {
				seat->workspace = NULL;
			}
		}
		return;
	}

	bool needs_new_focus = focus &&
		(focus == node || node_has_ancestor(focus, node));

	seat_node_destroy(seat_node);

	if (!parent && !needs_new_focus) {
		return;
	}

	struct wsm_node *next_focus = NULL;
	while (next_focus == NULL && parent != NULL) {
		struct wsm_container *con =
				seat_get_focus_inactive_view(seat, parent);
		next_focus = con ? &con->node : NULL;
		
		if (next_focus == NULL && parent->type == N_WORKSPACE) {
			next_focus = parent;
			break;
		}
		
		parent = node_get_parent(parent);
	}

	if (!next_focus) {
		struct wsm_workspace *ws = seat_get_last_known_workspace(seat);
		if (!ws) {
			return;
		}
		struct wsm_container *con =
				seat_get_focus_inactive_view(seat, &ws->node);
		next_focus = con ? &(con->node) : &(ws->node);
	}

	if (next_focus->type == N_WORKSPACE &&
		!workspace_is_visible(next_focus->workspace)) {
		// Do not change focus to a non-visible workspace
		return;
	}

	if (needs_new_focus) {
		if (node->type == N_CONTAINER) {
			seat_set_focus(seat, NULL);
		}
		if (seat_get_focus(seat) == next_focus) {
			seat_send_focus(next_focus, seat);
		} else {
			seat_set_focus(seat, next_focus);
		}
	} else {
		focus = seat_get_focus_inactive(seat, &global_server.scene->node);
		seat_set_raw_focus(seat, next_focus);
		if (focus->type == N_CONTAINER && focus->container->pending.workspace) {
			seat_set_raw_focus(seat, &focus->container->pending.workspace->node);
		}
		seat_set_raw_focus(seat, focus);
	}
}

static struct wsm_seat_node *seat_node_from_node(
		struct wsm_seat *seat, struct wsm_node *node) {
	if (node->type == N_ROOT || node->type == N_OUTPUT) {
		return NULL;
	}

	struct wsm_seat_node *seat_node = NULL;
	wl_list_for_each(seat_node, &seat->focus_stack, link) {
		if (seat_node->node == node) {
			return seat_node;
		}
	}

	seat_node = calloc(1, sizeof(struct wsm_seat_node));
	if (!seat_node) {
		wsm_log(WSM_ERROR, "Could not create wsm_seat_node: allocation failed!");
		return NULL;
	}

	seat_node->node = node;
	seat_node->seat = seat;
	wl_list_insert(seat->focus_stack.prev, &seat_node->link);
	wl_signal_add(&node->events.destroy, &seat_node->destroy);
	seat_node->destroy.notify = handle_seat_node_destroy;

	return seat_node;
}

static void handle_new_node(struct wl_listener *listener, void *data) {
	struct wsm_seat *seat = wl_container_of(listener, seat, new_node);
	struct wsm_node *node = data;
	seat_node_from_node(seat, node);
}

static void handle_request_start_drag(struct wl_listener *listener, void *data) {
	struct wsm_seat *seat = wl_container_of(listener, seat, request_start_drag);
	struct wlr_seat_request_start_drag_event *event = data;

	if (wlr_seat_validate_pointer_grab_serial(seat->seat,
			event->origin, event->serial)) {
		wlr_seat_start_pointer_drag(seat->seat, event->drag, event->serial);
		return;
	}

	struct wlr_touch_point *point;
	if (wlr_seat_validate_touch_grab_serial(seat->seat,
			event->origin, event->serial, &point)) {
		wlr_seat_start_touch_drag(seat->seat,
			event->drag, event->serial, point);
		return;
	}

	wsm_log(WSM_DEBUG, "Ignoring start_drag request: "
		"could not validate pointer or touch serial %" PRIu32, event->serial);
	wlr_data_source_destroy(event->drag->source);
}

static void handle_start_drag(struct wl_listener *listener, void *data) {
	struct wsm_seat *seat = wl_container_of(listener, seat, start_drag);
	struct wlr_drag *wlr_drag = data;

	struct wsm_drag *drag = wsm_drag_create(seat, wlr_drag);
	if (!drag) {
		return;
	}

	seatop_begin_default(seat);
}

static void handle_request_set_selection(struct wl_listener *listener, void *data) {
	struct wsm_seat *seat =
		wl_container_of(listener, seat, request_set_selection);
	struct wlr_seat_request_set_selection_event *event = data;
	wlr_seat_set_selection(seat->seat, event->source, event->serial);
}

static void handle_request_set_primary_selection(struct wl_listener *listener, void *data) {
	struct wsm_seat *seat =
		wl_container_of(listener, seat, request_set_primary_selection);
	struct wlr_seat_request_set_primary_selection_event *event = data;
	wlr_seat_set_primary_selection(seat->seat, event->source, event->serial);
}

static void collect_focus_iter(struct wsm_node *node, void *data) {
	struct wsm_seat *seat = data;
	struct wsm_seat_node *seat_node = seat_node_from_node(seat, node);
	if (!seat_node) {
		return;
	}
	wl_list_remove(&seat_node->link);
	wl_list_insert(&seat->focus_stack, &seat_node->link);
}

static void collect_focus_workspace_iter(struct wsm_workspace *workspace, void *data) {
	collect_focus_iter(&workspace->node, data);
}

static void collect_focus_container_iter(struct wsm_container *container, void *data) {
	collect_focus_iter(&container->node, data);
}

struct wsm_seat *seat_create(const char *seat_name) {
	struct wsm_seat *seat = calloc(1, sizeof(struct wsm_seat));
	if (!seat) {
		wsm_log(WSM_ERROR, "Could not create wsm_seat: allocation failed!");
		return NULL;
	}

	bool failed = false;
	seat->scene_tree = alloc_scene_tree(global_server.scene->layers.seat, &failed);
	seat->drag_icons = alloc_scene_tree(seat->scene_tree, &failed);

	if (failed) {
		wlr_scene_node_destroy(&seat->scene_tree->node);
		free(seat);
		return NULL;
	}

	seat->seat = wlr_seat_create(global_server.wl_display, seat_name);
	if (!wsm_assert(seat->seat, "could not allocate wlr_seat")) {
		wlr_scene_node_destroy(&seat->scene_tree->node);
		free(seat);
		return NULL;
	}

	seat->seat->data = seat;

	seat->cursor = wsm_cursor_create(&global_server, seat);
	if (!wsm_assert(seat->cursor, "wsm_cursor is NULL!")) {
		wlr_seat_destroy(seat->seat);
		free(seat);
		return NULL;
	}

	seat->destroy.notify = handle_seat_destroy;
	wl_signal_add(&seat->seat->events.destroy, &seat->destroy);
	seat->idle_inhibit_sources = seat->idle_wake_sources =
		IDLE_SOURCE_KEYBOARD |
		IDLE_SOURCE_POINTER |
		IDLE_SOURCE_TOUCH |
		IDLE_SOURCE_TABLET_PAD |
		IDLE_SOURCE_TABLET_TOOL |
		IDLE_SOURCE_SWITCH;

	wl_list_init(&seat->focus_stack);
	wl_list_init(&seat->devices);

	root_for_each_workspace(collect_focus_workspace_iter, seat);
	root_for_each_container(collect_focus_container_iter, seat);

	seat->deferred_bindings = wsm_list_create();

	seat->new_node.notify = handle_new_node;
	wl_signal_add(&global_server.scene->events.new_node, &seat->new_node);

	seat->request_start_drag.notify = handle_request_start_drag;
	wl_signal_add(&seat->seat->events.request_start_drag,
		&seat->request_start_drag);

	seat->start_drag.notify = handle_start_drag;
	wl_signal_add(&seat->seat->events.start_drag, &seat->start_drag);

	seat->request_set_selection.notify = handle_request_set_selection;
	wl_signal_add(&seat->seat->events.request_set_selection,
		&seat->request_set_selection);

	seat->request_set_primary_selection.notify =
		handle_request_set_primary_selection;
	wl_signal_add(&seat->seat->events.request_set_primary_selection,
		&seat->request_set_primary_selection);

	wl_list_init(&seat->keyboard_groups);
	wl_list_init(&seat->keyboard_shortcuts_inhibitors);

	wsm_input_method_relay_init(seat, &seat->im_relay);

	bool first = wl_list_empty(&global_server.input_manager->seats);
	wl_list_insert(&global_server.input_manager->seats, &seat->link);

	if (!first) {
		struct wsm_seat *current_seat = input_manager_current_seat();
		struct wsm_node *current_focus =
			seat_get_focus_inactive(current_seat, &global_server.scene->node);
		seat_set_focus(seat, current_focus);
	}

	seatop_begin_default(seat);

	return seat;
}

static struct wsm_seat_device *seat_get_device(struct wsm_seat *seat,
		struct wsm_input_device *input_device) {
	struct wsm_seat_device *seat_device = NULL;
	wl_list_for_each(seat_device, &seat->devices, link) {
		if (seat_device->input_device == input_device) {
			return seat_device;
		}
	}

	struct wsm_keyboard_group *group = NULL;
	wl_list_for_each(group, &seat->keyboard_groups, link) {
		if (group->seat_device->input_device == input_device) {
			return group->seat_device;
		}
	}

	return NULL;
}

static void seat_update_capabilities(struct wsm_seat *seat) {
	uint32_t caps = 0;
	uint32_t previous_caps = seat->seat->capabilities;
	struct wsm_seat_device *seat_device;
	wl_list_for_each(seat_device, &seat->devices, link) {
		switch (seat_device->input_device->input_device_wlr->type) {
		case WLR_INPUT_DEVICE_KEYBOARD:
			caps |= WL_SEAT_CAPABILITY_KEYBOARD;
			break;
		case WLR_INPUT_DEVICE_POINTER:
			caps |= WL_SEAT_CAPABILITY_POINTER;
			break;
		case WLR_INPUT_DEVICE_TOUCH:
			caps |= WL_SEAT_CAPABILITY_TOUCH;
			break;
		case WLR_INPUT_DEVICE_TABLET:
			caps |= WL_SEAT_CAPABILITY_POINTER;
			break;
		case WLR_INPUT_DEVICE_SWITCH:
		case WLR_INPUT_DEVICE_TABLET_PAD:
			break;
		}
	}

	if ((caps & WL_SEAT_CAPABILITY_POINTER) == 0) {
		cursor_set_image(seat->cursor, NULL, NULL);
		wlr_seat_set_capabilities(seat->seat, caps);
	} else {
		wlr_seat_set_capabilities(seat->seat, caps);
		if ((previous_caps & WL_SEAT_CAPABILITY_POINTER) == 0) {
			cursor_set_image(seat->cursor, "default", NULL);
		}
	}
}

void seat_remove_device(struct wsm_seat *seat,
		struct wsm_input_device *input_device) {
	struct wsm_seat_device *seat_device = seat_get_device(seat, input_device);

	if (!seat_device) {
		wsm_log(WSM_ERROR, "wsm_seat_device is NULL in seat_remove_device!");
		return;
	}

	wsm_log(WSM_DEBUG, "removing device %s from seat %s",
		input_device->identifier, seat->seat->name);

	seat_device_destroy(seat_device);
	seat_update_capabilities(seat);
}

void seat_idle_notify_activity(struct wsm_seat *seat,
		enum wlr_input_device_type source) {
	if ((source & seat->idle_inhibit_sources) == 0) {
		return;
	}
	wlr_idle_notifier_v1_notify_activity(global_server.idle_notifier_v1, seat->seat);
}

void seatop_tablet_tool_tip(struct wsm_seat *seat,
		struct wsm_tablet_tool *tool, uint32_t time_msec,
		enum wlr_tablet_tool_tip_state state) {
	if (seat->seatop_impl->tablet_tool_tip) {
		seat->seatop_impl->tablet_tool_tip(seat, tool, time_msec, state);
	}
}

void seatop_hold_begin(struct wsm_seat *seat,
		struct wlr_pointer_hold_begin_event *event) {
	if (seat->seatop_impl->hold_begin) {
		seat->seatop_impl->hold_begin(seat, event);
	}
}

void seatop_hold_end(struct wsm_seat *seat,
		struct wlr_pointer_hold_end_event *event) {
	if (seat->seatop_impl->hold_end) {
		seat->seatop_impl->hold_end(seat, event);
	}
}

void seatop_pinch_begin(struct wsm_seat *seat,
		struct wlr_pointer_pinch_begin_event *event) {
	if (seat->seatop_impl->pinch_begin) {
		seat->seatop_impl->pinch_begin(seat, event);
	}
}

void seatop_pinch_update(struct wsm_seat *seat,
		struct wlr_pointer_pinch_update_event *event) {
	if (seat->seatop_impl->pinch_update) {
		seat->seatop_impl->pinch_update(seat, event);
	}
}

void seatop_pinch_end(struct wsm_seat *seat,
		struct wlr_pointer_pinch_end_event *event) {
	if (seat->seatop_impl->pinch_end) {
		seat->seatop_impl->pinch_end(seat, event);
	}
}

void seatop_swipe_begin(struct wsm_seat *seat,
		struct wlr_pointer_swipe_begin_event *event) {
	if (seat->seatop_impl->swipe_begin) {
		seat->seatop_impl->swipe_begin(seat, event);
	}
}

void seatop_swipe_update(struct wsm_seat *seat,
		struct wlr_pointer_swipe_update_event *event) {
	if (seat->seatop_impl->swipe_update) {
		seat->seatop_impl->swipe_update(seat, event);
	}
}

void seatop_swipe_end(struct wsm_seat *seat,
		struct wlr_pointer_swipe_end_event *event) {
	if (seat->seatop_impl->swipe_end) {
		seat->seatop_impl->swipe_end(seat, event);
	}
}

void seatop_pointer_motion(struct wsm_seat *seat, uint32_t time_msec) {
	if (seat->seatop_impl->pointer_motion) {
		seat->seatop_impl->pointer_motion(seat, time_msec);
	}
}

void seatop_pointer_axis(struct wsm_seat *seat,
		struct wlr_pointer_axis_event *event) {
	if (seat->seatop_impl->pointer_axis) {
		seat->seatop_impl->pointer_axis(seat, event);
	}
}

void seatop_button(struct wsm_seat *seat, uint32_t time_msec,
		struct wlr_input_device *device, uint32_t button,
	enum wl_pointer_button_state state) {
	if (seat->seatop_impl->button) {
		seat->seatop_impl->button(seat, time_msec, device, button, state);
	}
}

void seatop_touch_motion(struct wsm_seat *seat, struct wlr_touch_motion_event *event,
		double lx, double ly) {
	if (seat->seatop_impl->touch_motion) {
		seat->seatop_impl->touch_motion(seat, event, lx, ly);
	}
}

void seatop_touch_up(struct wsm_seat *seat, struct wlr_touch_up_event *event) {
	if (seat->seatop_impl->touch_up) {
		seat->seatop_impl->touch_up(seat, event);
	}
}

void seatop_touch_down(struct wsm_seat *seat, struct wlr_touch_down_event *event,
		double lx, double ly) {
	if (seat->seatop_impl->touch_down) {
		seat->seatop_impl->touch_down(seat, event, lx, ly);
	}
}

void seatop_touch_cancel(struct wsm_seat *seat, struct wlr_touch_cancel_event *event) {
	if (seat->seatop_impl->touch_cancel) {
		seat->seatop_impl->touch_cancel(seat, event);
	}
}

void seatop_rebase(struct wsm_seat *seat, uint32_t time_msec) {
	if (seat->seatop_impl->rebase) {
		seat->seatop_impl->rebase(seat, time_msec);
	}
}

void seatop_end(struct wsm_seat *seat) {
	if (seat->seatop_impl && seat->seatop_impl->end) {
		seat->seatop_impl->end(seat);
	}
	free(seat->seatop_data);
	seat->seatop_data = NULL;
	seat->seatop_impl = NULL;
}

void seatop_tablet_tool_motion(struct wsm_seat *seat,
		struct wsm_tablet_tool *tool, uint32_t time_msec) {
	if (seat->seatop_impl->tablet_tool_motion) {
		seat->seatop_impl->tablet_tool_motion(seat, tool, time_msec);
	} else {
		seatop_pointer_motion(seat, time_msec);
	}
}

bool seatop_allows_set_cursor(struct wsm_seat *seat) {
	return seat->seatop_impl->allow_set_cursor;
}

void seat_add_device(struct wsm_seat *seat,
		struct wsm_input_device *input_device) {
	if (seat_get_device(seat, input_device)) {
		wsm_log(WSM_ERROR, "device %s already exists in seat %s",
			input_device->identifier, seat->seat->name);
		return;
	}

	struct wsm_seat_device *seat_device =
		calloc(1, sizeof(struct wsm_seat_device));
	if (!seat_device) {
		wsm_log(WSM_ERROR, "Could not create wsm_seat_device: allocation failed!");
		return;
	}

	wsm_log(WSM_DEBUG, "adding device %s to seat %s",
		input_device->identifier, seat->seat->name);

	seat_device->seat = seat;
	seat_device->input_device = input_device;
	wl_list_insert(&seat->devices, &seat_device->link);

	seat_configure_device(seat, input_device);
	seat_update_capabilities(seat);
}

static void seat_configure_pointer(struct wsm_seat *seat,
		struct wsm_seat_device *seat_device) {
	if (!seat_device->pointer) {
		wsm_pointer_create(seat, seat_device);
	}

	wlr_cursor_attach_input_device(seat->cursor->cursor_wlr,
		seat_device->input_device->input_device_wlr);
}

static void seat_configure_keyboard(struct wsm_seat *seat,
		struct wsm_seat_device *seat_device) {
	if (!seat_device->keyboard) {
		wsm_keyboard_create(seat, seat_device);
	}
	wsm_keyboard_configure(seat_device->keyboard);

	struct wlr_keyboard *wlr_keyboard =
		wlr_keyboard_from_input_device(seat_device->input_device->input_device_wlr);
	struct wlr_keyboard *current_keyboard = seat->seat->keyboard_state.keyboard;
	if (wlr_keyboard != current_keyboard) {
		return;
	}

	struct wlr_surface *surface = seat->seat->keyboard_state.focused_surface;
	if (surface) {
		wlr_seat_keyboard_notify_clear_focus(seat->seat);
		seat_keyboard_notify_enter(seat, surface);
	}
}

static void seat_configure_switch(struct wsm_seat *seat,
		struct wsm_seat_device *seat_device) {
	if (!seat_device->_switch) {
		wsm_switch_create(seat, seat_device);
	}
	wsm_switch_configure(seat_device->_switch);
}

static void seat_configure_touch(struct wsm_seat *seat,
		struct wsm_seat_device *wsm_device) {
	wlr_cursor_attach_input_device(seat->cursor->cursor_wlr,
		wsm_device->input_device->input_device_wlr);
}

static void seat_configure_tablet_tool(struct wsm_seat *seat,
		struct wsm_seat_device *wsm_device) {
	if (!wsm_device->tablet) {
		wsm_tablet_create(seat, wsm_device);
	}
	wsm_configure_tablet(wsm_device->tablet);
	wlr_cursor_attach_input_device(seat->cursor->cursor_wlr,
		wsm_device->input_device->input_device_wlr);
}

static void seat_configure_tablet_pad(struct wsm_seat *seat,
		struct wsm_seat_device *wsm_device) {
	if (!wsm_device->tablet_pad) {
		wsm_tablet_pad_create(seat, wsm_device);
	}
	wsm_configure_tablet_pad(wsm_device->tablet_pad);
}

void seat_configure_device(struct wsm_seat *seat, struct wsm_input_device *device) {
	struct wsm_seat_device *seat_device = seat_get_device(seat, device);
	if (!seat_device) {
		wsm_log(WSM_ERROR, "seat_get_device is NULL!");
		return;
	}

	switch (device->input_device_wlr->type) {
	case WLR_INPUT_DEVICE_POINTER:
		seat_configure_pointer(seat, seat_device);
		break;
	case WLR_INPUT_DEVICE_KEYBOARD:
		seat_configure_keyboard(seat, seat_device);
		break;
	case WLR_INPUT_DEVICE_SWITCH:
		seat_configure_switch(seat, seat_device);
		break;
	case WLR_INPUT_DEVICE_TOUCH:
		seat_configure_touch(seat, seat_device);
		break;
	case WLR_INPUT_DEVICE_TABLET:
		seat_configure_tablet_tool(seat, seat_device);
		break;
	case WLR_INPUT_DEVICE_TABLET_PAD:
		seat_configure_tablet_pad(seat, seat_device);
		break;
	}
}

static void send_unfocus(struct wsm_container *con, void *data) {
	if (con->view) {
		view_set_activated(con->view, false);
	}
}

static void seat_send_unfocus(struct wsm_node *node, struct wsm_seat *seat) {
	wsm_cursor_constrain(seat->cursor, NULL);
	wlr_seat_keyboard_notify_clear_focus(seat->seat);
	if (node->type == N_WORKSPACE) {
		workspace_for_each_container(node->workspace, send_unfocus, seat);
	} else {
		send_unfocus(node->container, seat);
		container_for_each_child(node->container, send_unfocus, seat);
	}
}

void seat_set_focus_surface(struct wsm_seat *seat,
		struct wlr_surface *surface, bool unfocus) {
	if (seat->has_focus && unfocus) {
		struct wsm_node *focus = seat_get_focus(seat);
		seat_send_unfocus(focus, seat);
		seat->has_focus = false;
	}

	if (surface) {
		seat_keyboard_notify_enter(seat, surface);
	} else {
		wlr_seat_keyboard_notify_clear_focus(seat->seat);
	}

	wsm_input_method_relay_set_focus(&seat->im_relay, surface);
	seat_tablet_pads_set_focus(seat, surface);
}

void seat_set_focus_layer(struct wsm_seat *seat,
		struct wlr_layer_surface_v1 *layer) {
	if (!layer && seat->focused_layer_wlr) {
		return;
	} else if (!layer) {
		return;
	}
	assert(layer->surface->mapped);
	if (layer->current.layer >= ZWLR_LAYER_SHELL_V1_LAYER_TOP &&
			layer->current.keyboard_interactive
			== ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_EXCLUSIVE) {
		seat->has_exclusive_layer = true;
	}
	if (seat->focused_layer_wlr == layer) {
		return;
	}
	seat_set_focus_surface(seat, layer->surface, true);
	seat->focused_layer_wlr = layer;
}

void seat_pointer_notify_button(struct wsm_seat *seat, uint32_t time_msec,
		uint32_t button, enum wl_pointer_button_state state) {
	seat->last_button_serial = wlr_seat_pointer_notify_button(seat->seat,
		time_msec, button, state);
}

void seat_set_focus_workspace(struct wsm_seat *seat, struct wsm_workspace *ws) {
	seat_set_focus(seat, ws ? &ws->node : NULL);
}

static void set_workspace(struct wsm_seat *seat, struct wsm_workspace *new_ws) {
	if (seat->workspace == new_ws) {
		return;
	}

	if (seat->workspace) {
		free(seat->prev_workspace_name);
		seat->prev_workspace_name = strdup(seat->workspace->name);
		if (!seat->prev_workspace_name) {
			wsm_log(WSM_ERROR, "Unable to allocate previous workspace name");
		}
	}

	seat->workspace = new_ws;
}

static void seat_set_workspace_focus(struct wsm_seat *seat, struct wsm_node *node) {
	struct wsm_node *last_focus = seat_get_focus(seat);
	if (last_focus == node) {
		return;
	}

	struct wsm_workspace *last_workspace = seat_get_focused_workspace(seat);

	if (node == NULL) {
		if (node_is_view(last_focus)) {
			view_close_popups(last_focus->container->view);
		}
		seat_send_unfocus(last_focus, seat);
		wsm_input_method_relay_set_focus(&seat->im_relay, NULL);
		seat->has_focus = false;
		return;
	}

	struct wsm_workspace *new_workspace = node->type == N_WORKSPACE ?
		node->workspace : node->container->pending.workspace;
	struct wsm_container *container = node->type == N_CONTAINER ?
		node->container : NULL;

	if (container && container_obstructing_fullscreen_container(container)) {
		return;
	}

	if (global_server.scene->fullscreen_global && !container && new_workspace) {
		return;
	}

	struct wsm_output *new_output =
		new_workspace ? new_workspace->output : NULL;

	if (last_workspace != new_workspace && new_output) {
		node_set_dirty(&new_output->node);
	}

	struct wsm_workspace *new_output_last_ws =
		new_output ? output_get_active_workspace(new_output) : NULL;

	if (last_focus) {
		seat_send_unfocus(last_focus, seat);
		node_set_dirty(last_focus);
		struct wsm_node *parent = node_get_parent(last_focus);
		if (parent) {
			node_set_dirty(parent);
		}
	}

	if (container) {
		struct wsm_container *parent = container->pending.parent;
		while (parent) {
			seat_set_raw_focus(seat, &parent->node);
			parent = parent->pending.parent;
		}
	}
	if (new_workspace) {
		seat_set_raw_focus(seat, &new_workspace->node);
	}
	if (container) {
		seat_set_raw_focus(seat, &container->node);
		seat_send_focus(&container->node, seat);
	}

	set_workspace(seat, new_workspace);
	if (new_workspace && new_output_last_ws
		&& new_workspace != new_output_last_ws) {
		for (int i = 0; i < new_output_last_ws->floating->length; ++i) {
			struct wsm_container *floater =
					new_output_last_ws->floating->items[i];
			if (container_is_sticky(floater)) {
				container_detach(floater);
				workspace_add_floating(new_workspace, floater);
				--i;
			}
		}
	}

	if (last_focus && node_is_view(last_focus)) {
		view_close_popups(last_focus->container->view);
	}

	if (container && container->view && view_is_urgent(container->view) &&
		!container->view->urgent_timer) {
		struct wsm_view *view = container->view;
		if (last_workspace && last_workspace != new_workspace) {
			//
		} else {
			view_set_urgent(view, false);
		}
	}

	if (new_output_last_ws) {
		workspace_consider_destroy(new_output_last_ws);
	}
	if (last_workspace && last_workspace != new_output_last_ws) {
		workspace_consider_destroy(last_workspace);
	}

	seat->has_focus = true;

	if (new_workspace) {
		wsm_arrange_workspace_auto(new_workspace);
	}
}

void seat_set_focus(struct wsm_seat *seat, struct wsm_node *node) {
	if (seat->has_exclusive_layer) {
		struct wlr_layer_surface_v1 *layer = seat->focused_layer_wlr;
		seat_set_focus_layer(seat, NULL);
		seat_set_workspace_focus(seat, node);
		seat_set_focus_layer(seat, layer);
	} else if (seat->focused_layer_wlr) {
		seat_set_focus_layer(seat, NULL);
		seat_set_workspace_focus(seat, node);
	} else {
		seat_set_workspace_focus(seat, node);
	}
}

struct wsm_node *seat_get_focus(struct wsm_seat *seat) {
	if (!seat->has_focus) {
		return NULL;
	}
	wsm_assert(!wl_list_empty(&seat->focus_stack),
		"focus_stack is empty, but has_focus is true");
	struct wsm_seat_node *current =
		wl_container_of(seat->focus_stack.next, current, link);
	return current->node;
}

struct wsm_workspace *seat_get_focused_workspace(struct wsm_seat *seat) {
	struct wsm_node *focus = seat_get_focus_inactive(seat, &global_server.scene->node);
	if (!focus) {
		return NULL;
	}
	if (focus->type == N_CONTAINER) {
		return focus->container->pending.workspace;
	}
	if (focus->type == N_WORKSPACE) {
		return focus->workspace;
	}
	return NULL;
}

struct wsm_node *seat_get_focus_inactive(struct wsm_seat *seat, struct wsm_node *node) {
	if (node_is_view(node)) {
		return node;
	}
	struct wsm_seat_node *current;
	wl_list_for_each(current, &seat->focus_stack, link) {
		if (node_has_ancestor(current->node, node)) {
			return current->node;
		}
	}
	if (node->type == N_WORKSPACE) {
		return node;
	}
	return NULL;
}

struct wsm_container *seat_get_focus_inactive_view(struct wsm_seat *seat,
	struct wsm_node *ancestor) {
	if (node_is_view(ancestor)) {
		return ancestor->container;
	}
	struct wsm_seat_node *current;
	wl_list_for_each(current, &seat->focus_stack, link) {
		struct wsm_node *node = current->node;
		if (node_is_view(node) && node_has_ancestor(node, ancestor)) {
			return node->container;
		}
	}
	return NULL;
}

struct wsm_workspace *seat_get_last_known_workspace(struct wsm_seat *seat) {
	struct wsm_seat_node *current;
	wl_list_for_each(current, &seat->focus_stack, link) {
		struct wsm_node *node = current->node;
		if (node->type == N_CONTAINER &&
			node->container->pending.workspace) {
			return node->container->pending.workspace;
		} else if (node->type == N_WORKSPACE) {
			return node->workspace;
		}
	}
	return NULL;
}

struct wsm_node *seat_get_active_tiling_child(struct wsm_seat *seat,
	struct wsm_node *parent) {
	if (node_is_view(parent)) {
		return parent;
	}
	struct wsm_seat_node *current;
	wl_list_for_each(current, &seat->focus_stack, link) {
		struct wsm_node *node = current->node;
		if (node_get_parent(node) != parent) {
			continue;
		}
		if (parent->type == N_WORKSPACE) {
			struct wsm_workspace *ws = parent->workspace;
			if (wsm_list_find(ws->tiling, node->container) == -1) {
				continue;
			}
		}
		return node;
	}
	return NULL;
}

void seat_set_raw_focus(struct wsm_seat *seat, struct wsm_node *node) {
	struct wsm_seat_node *seat_node = seat_node_from_node(seat, node);
	wl_list_remove(&seat_node->link);
	wl_list_insert(&seat->focus_stack, &seat_node->link);
	node_set_dirty(node);

	struct wsm_node *parent = node_get_parent(node);
	if (parent) {
		node_set_dirty(parent);
	}
}

void seat_configure_xcursor(struct wsm_seat *seat) {
	
}

struct wsm_container *seat_get_focused_container(struct wsm_seat *seat) {
	struct wsm_node *focus = seat_get_focus(seat);
	if (focus && focus->type == N_CONTAINER) {
		return focus->container;
	}
	return NULL;
}

void seat_set_focus_container(struct wsm_seat *seat,
		struct wsm_container *con) {
	seat_set_focus(seat, con ? &con->node : NULL);
}

void seatop_unref(struct wsm_seat *seat, struct wsm_container *con) {
	if (seat->seatop_impl->unref) {
		seat->seatop_impl->unref(seat, con);
	}
}

void seat_consider_warp_to_focus(struct wsm_seat *seat) {
	struct wsm_node *focus = seat_get_focus(seat);
	if (focus->type == N_CONTAINER) {
		cursor_warp_to_container(seat->cursor, focus->container, false);
	} else {
		cursor_warp_to_workspace(seat->cursor, focus->workspace);
	}
}

struct wsm_container *seat_get_focus_inactive_tiling(struct wsm_seat *seat,
		struct wsm_workspace *workspace) {
	if (!workspace->tiling->length) {
		return NULL;
	}
	struct wsm_seat_node *current;
	wl_list_for_each(current, &seat->focus_stack, link) {
		struct wsm_node *node = current->node;
		if (node->type == N_CONTAINER &&
			!container_is_floating_or_child(node->container) &&
			node->container->pending.workspace == workspace) {
			return node->container;
		}
	}
	return NULL;
}

bool seat_is_input_allowed(struct wsm_seat *seat,
		struct wlr_surface *surface) {
	if (global_server.session_lock.lock) {
		return wsm_session_lock_has_surface(global_server.session_lock.lock, surface);
	}
	return true;
}

void seat_unfocus_unless_client(struct wsm_seat *seat, struct wl_client *client) {
	if (seat->focused_layer_wlr) {
		if (wl_resource_get_client(seat->focused_layer_wlr->resource) != client) {
			seat_set_focus_layer(seat, NULL);
		}
	}
	if (seat->has_focus) {
		struct wsm_node *focus = seat_get_focus(seat);
		if (node_is_view(focus) && wl_resource_get_client(
					focus->container->view->surface->resource) != client) {
			seat_set_focus(seat, NULL);
		}
	}
	if (seat->seat->pointer_state.focused_client) {
		if (seat->seat->pointer_state.focused_client->client != client) {
			wlr_seat_pointer_notify_clear_focus(seat->seat);
		}
	}
	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);
	struct wlr_touch_point *point;
	wl_list_for_each(point, &seat->seat->touch_state.touch_points, link) {
		if (point->client->client != client) {
			wlr_seat_touch_point_clear_focus(seat->seat,
				now.tv_nsec / 1000, point->touch_id);
		}
	}
}

void seat_configure_device_mapping(struct wsm_seat *seat,
		struct wsm_input_device *input_device) {
	struct wsm_seat_device *seat_device = seat_get_device(seat, input_device);
	if (!seat_device) {
		return;
	}

	seat_apply_input_mapping(seat, seat_device);
}
