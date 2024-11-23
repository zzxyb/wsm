#include "wsm_drag.h"
#include "wsm_seat.h"
#include "wsm_log.h"
#include "wsm_cursor.h"
#include "node/wsm_node_descriptor.h"

#include <stdlib.h>

#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_data_device.h>

static void drag_handle_destroy(struct wl_listener *listener, void *data) {
	struct wsm_drag *drag = wl_container_of(listener, drag, destroy);
	wsm_drag_destroy(drag);
}

struct wsm_drag *wsm_drag_create(struct wsm_seat *seat, struct wlr_drag *wlr_drag) {
	struct wsm_drag *drag = calloc(1, sizeof(struct wsm_drag));
	if (!drag) {
		wsm_log(WSM_ERROR, "Could not create wsm_drag: allocation failed!");
		return NULL;
	}

	drag->seat = seat;
	drag->drag = wlr_drag;
	wlr_drag->data = drag;

	drag->destroy.notify = drag_handle_destroy;
	wl_signal_add(&wlr_drag->events.destroy, &drag->destroy);

	struct wlr_drag_icon *wlr_drag_icon = wlr_drag->icon;
	if (wlr_drag_icon != NULL) {
		struct wlr_scene_tree *tree = wlr_scene_drag_icon_create(seat->drag_icons, wlr_drag_icon);
		if (!tree) {
			wsm_log(WSM_ERROR, "Failed to allocate a drag icon scene tree");
			free(drag);
			return NULL;
		}

		if (!wsm_scene_descriptor_assign(&tree->node, WSM_SCENE_DESC_DRAG_ICON,
				wlr_drag_icon)) {
			wsm_log(WSM_ERROR, "Failed to allocate a drag icon scene descriptor");
			wlr_scene_node_destroy(&tree->node);
			free(drag);
			return NULL;
		}

		wsm_drag_icon_node_update_position(seat, &tree->node);
	}

	return drag;
}

void wsm_drag_destroy(struct wsm_drag *drag) {
	struct wsm_seat *seat = drag->seat;
	if (seat->focused_layer_wlr) {
		struct wlr_layer_surface_v1 *layer = seat->focused_layer_wlr;
		seat_set_focus_layer(seat, NULL);
		seat_set_focus_layer(seat, layer);
	} else {
		struct wlr_surface *surface = seat->seat->keyboard_state.focused_surface;
		seat_set_focus_surface(seat, NULL, false);
		seat_set_focus_surface(seat, surface, false);
	}

	drag->drag->data = NULL;
	wl_list_remove(&drag->destroy.link);
	free(drag);
}

void wsm_drag_icons_update_position(struct wsm_seat *seat) {
	struct wlr_scene_node *node;
	wl_list_for_each(node, &seat->drag_icons->children, link) {
		wsm_drag_icon_node_update_position(seat, node);
	}
}

void wsm_drag_icon_node_update_position(struct wsm_seat *seat, struct wlr_scene_node *node) {
	struct wlr_drag_icon *wlr_icon = wsm_scene_descriptor_try_get(node, WSM_SCENE_DESC_DRAG_ICON);
	struct wlr_cursor *cursor = seat->cursor->cursor_wlr;

	switch (wlr_icon->drag->grab_type) {
	case WLR_DRAG_GRAB_KEYBOARD:
		return;
	case WLR_DRAG_GRAB_KEYBOARD_POINTER:
		wlr_scene_node_set_position(node, cursor->x, cursor->y);
		break;
	case WLR_DRAG_GRAB_KEYBOARD_TOUCH:;
		struct wlr_touch_point *point =
			wlr_seat_touch_get_point(seat->seat, wlr_icon->drag->touch_id);
		if (point == NULL) {
			return;
		}
		wlr_scene_node_set_position(node, seat->touch_x, seat->touch_y);
	}
}
