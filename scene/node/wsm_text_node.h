#ifndef WSM_TEXT_NODE_H
#define WSM_TEXT_NODE_H

#include <stdbool.h>

struct wlr_scene_node;
struct wlr_scene_tree;

struct wsm_desktop_interface;

struct wsm_text_node {
	float color[4];
	float background[4];

	struct wlr_scene_node *node_wlr;
	int width;
	int max_width;
	int height;
	int baseline;
	bool pango_markup;
};

struct wsm_text_node *wsm_text_node_create(struct wlr_scene_tree *parent,
	const struct wsm_desktop_interface *font,
	char *text, float color[4], bool pango_markup);
void wsm_text_node_set_color(struct wsm_text_node *node, float color[4]);
void wsm_text_node_set_text(struct wsm_text_node *node, char *text);
void wsm_text_node_set_max_width(struct wsm_text_node *node, int max_width);
void wsm_text_node_set_background(struct wsm_text_node *node, float background[4]);

#endif
