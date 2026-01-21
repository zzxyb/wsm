#ifndef WSM_BUTTON_NODE_H
#define WSM_BUTTON_NODE_H

#include "wsm_image_node.h"

#include <stdbool.h>
#include <stdint.h>

#include <wayland-server-core.h>
#include <wayland-server-protocol.h>

struct wsm_button_node {
	struct wlr_scene_tree *tree;
	struct wsm_image_node *image;
	struct wlr_scene_rect *hover_rect;
	struct wl_listener destroy;

	struct {
		struct wl_signal clicked;
	} events;

	float alpha, hover_alpha, color[3];
	int width, height;
	bool clickable, focused;
};

struct wsm_button_node *wsm_button_node_create(struct wlr_scene_tree *parent,
	int width, int height, char *path, const float color[static 3]);

void wsm_button_node_set_clickable(struct wsm_button_node *button, bool enable);

void wsm_button_node_set_alpha(struct wsm_button_node *button, float alpha);

void wsm_button_node_set_size(struct wsm_button_node *button, int width, int height);

void wsm_button_node_set_color(struct wsm_button_node *button, const float color[static 3]);

void wsm_button_node_load_icon(struct wsm_button_node *button, const char *path);

void wsm_button_node_set_hovered(struct wsm_button_node *button, bool hovered);

void wsm_button_node_notify_button(struct wsm_button_node *node,
	uint32_t button, enum wl_pointer_button_state state);

void wsm_button_node_notify_touch_up(struct wsm_button_node *button);

#endif // WSM_BUTTON_NODE_H
