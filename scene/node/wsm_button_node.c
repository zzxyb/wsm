#include "wsm_button_node.h"
#include "wsm_image_node.h"
#include "wsm_log.h"
#include "wsm_node_descriptor.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include <linux/input-event-codes.h>

#include <time.h>
#include <unistd.h>
#include <wayland-util.h>
#include <wlr/types/wlr_scene.h>

static struct wsm_button_node *hovered_button = NULL;

static void button_destroy(struct wsm_button_node *button) {
	if (hovered_button == button) {
		hovered_button = NULL;
	}

	free(button);
}

static void handle_destroy(struct wl_listener *listener, void *data) {
	struct wsm_button_node *button = wl_container_of(listener, button, destroy);
	wl_list_remove(&button->destroy.link);
	button_destroy(button);
}

static bool button_point_accepts_input(struct wlr_scene_buffer *buffer,
		double *sx, double *sy) {
	struct wsm_button_node *button = buffer->node.data;
	return *sx >= 0 && *sx < button->width && *sy >= 0 && *sy < button->height;
}

struct wsm_button_node *wsm_button_node_create(struct wlr_scene_tree *parent,
		int width, int height, char *path, const float color[static 3]) {
	struct wsm_button_node *button = calloc(1, sizeof(*button));
	if (button == NULL) {
		wsm_log(WSM_ERROR, "Could not create wsm_button_node: allocation failed!");
		return NULL;
	}

	button->tree = wlr_scene_tree_create(parent);
	if (button->tree == NULL) {
		wsm_log(WSM_ERROR, "Could not create wlr_scene_tree: allocation failed!");
		goto error;
	}

	button->image = wsm_image_node_create(button->tree,
			width,
			height,
			path,
			1.0);
	if (button->image == NULL) {
		goto error;
	}

	button->alpha = 1.0;
	button->hover_alpha = 0.18;
	memcpy(button->color, color, sizeof(button->color));
	const float rgba[4] = {
		color[0] * button->hover_alpha,
		color[1] * button->hover_alpha,
		color[2] * button->hover_alpha,
		button->hover_alpha
	};
	button->hover_rect = wlr_scene_rect_create(button->tree,
			width,
			height,
			rgba);
	if (button->hover_rect == NULL) {
		wsm_log(WSM_ERROR, "Could not create wlr_scene_rect: allocation failed!");
		goto error;
	}

	button->width = width;
	button->height = height;
	button->clickable = true;
	button->focused = false;

	wlr_scene_node_set_enabled(&button->hover_rect->node, button->focused);
	wl_signal_init(&button->events.clicked);

	struct wlr_scene_buffer *buffer_node = wlr_scene_buffer_from_node(button->image->node_wlr);
	buffer_node->point_accepts_input = button_point_accepts_input;
	buffer_node->node.data = button;

	if (!wsm_scene_descriptor_assign(&button->tree->node,
			WSM_SCENE_DESC_BUTTON, button)) {
		goto error;
	}

	button->destroy.notify = handle_destroy;
	wl_signal_add(&button->tree->node.events.destroy, &button->destroy);

	return button;

error:
	if (button->hover_rect) {
		wlr_scene_node_destroy(&button->hover_rect->node);
	}

	if (button->image) {
		wlr_scene_node_destroy(button->image->node_wlr);
	}

	if (button->tree) {
		wlr_scene_node_destroy(&button->tree->node);
	}

	free(button);
	return NULL;
}

void wsm_button_node_set_clickable(struct wsm_button_node *button, bool enable) {
	if (button->clickable == enable) {
		return;
	}

	button->clickable = enable;
	struct wlr_scene_buffer *buffer_node = wlr_scene_buffer_from_node(button->image->node_wlr);
	if (!enable) {
		buffer_node->point_accepts_input = NULL;
	} else {
		buffer_node->point_accepts_input = button_point_accepts_input;
	}
}

void wsm_button_node_set_enabled(struct wsm_button_node *button, bool enabled) {
	if (button->tree->node.enabled == enabled) {
		return;
	}

	wlr_scene_node_set_enabled(&button->tree->node, enabled);
}

void wsm_button_node_set_alpha(struct wsm_button_node *button, float alpha) {
	if (button->alpha == alpha) {
		return;
	}

	button->alpha = alpha;
	const float rgba[4] = {
		button->color[0] * button->hover_alpha * button->alpha,
		button->color[1] * button->hover_alpha * button->alpha,
		button->color[2] * button->hover_alpha * button->alpha,
		button->hover_alpha * button->alpha
	};

	wsm_image_node_update_alpha(button->image, button->alpha);
	wlr_scene_rect_set_color(button->hover_rect, rgba);
}

void wsm_button_node_set_size(struct wsm_button_node *button,
		int width, int height) {
	if (button->width == width && button->height == height) {
		return;
	}

	button->width = width;
	button->height = height;
	wsm_image_node_set_size(button->image, width, height);
	wlr_scene_rect_set_size(button->hover_rect, width, height);
}

void wsm_button_node_set_position(struct wsm_button_node *button, int x, int y) {
	if (button->tree->node.x == x && button->tree->node.y == y) {
		return;
	}

	wlr_scene_node_set_position(&button->tree->node, x, y);
}

void wsm_button_node_set_color(struct wsm_button_node *button,
		const float color[static 3]) {
	if (memcmp(button->color, color, sizeof(button->color)) == 0) {
		return;
	}

	memcpy(button->color, color, sizeof(button->color));
	const float rgba[4] = {
		color[0] * button->hover_alpha * button->alpha,
		color[1] * button->hover_alpha * button->alpha,
		color[2] * button->hover_alpha * button->alpha,
		button->hover_alpha * button->alpha
	};

	wlr_scene_rect_set_color(button->hover_rect, rgba);
}

void wsm_button_node_load_icon(struct wsm_button_node *button, const char *path) {
	wsm_image_node_load(button->image, path);
}

void wsm_button_node_set_hovered(struct wsm_button_node *button, bool hovered) {
	if (hovered_button && hovered_button != button) {
		hovered_button->focused = false;
		wlr_scene_node_set_enabled(&hovered_button->hover_rect->node, false);
		hovered_button = NULL;
	}

	if (!button) {
		return;
	}

	if (button->focused == hovered) {
		return;
	}

	button->focused = hovered;
	wlr_scene_node_set_enabled(&button->hover_rect->node, hovered);
	hovered_button = hovered ? button : NULL;
}

void wsm_button_node_notify_button(struct wsm_button_node *node,
		uint32_t button, enum wl_pointer_button_state state) {
	if (node->clickable &&
			node->focused &&
			button == BTN_LEFT &&
			state != WL_POINTER_BUTTON_STATE_RELEASED) {
		wl_signal_emit(&node->events.clicked, NULL);
	}
}

void wsm_button_node_notify_touch_up(struct wsm_button_node *button) {
	if (button->clickable && button->focused) {
		wl_signal_emit(&button->events.clicked, NULL);
	}
}
