#ifndef WSM_IMAGE_NODE_H
#define WSM_IMAGE_NODE_H

#include <stdbool.h>

struct wlr_renderer;
struct wlr_texture;
struct wlr_scene_node;
struct wlr_scene_tree;

typedef struct _cairo_surface cairo_surface_t;

struct wsm_image_node {
	struct wlr_scene_node *node;

	int width;
	int height;
	float alpha;
};

struct wsm_image_node *wsm_image_node_create(struct wlr_scene_tree *parent,
	int width, int height, char *path, float alpha);
void wsm_image_node_update_alpha(struct wsm_image_node *node, float alpha);
cairo_surface_t *create_cairo_surface_frome_file(const char *file_path);
struct wlr_texture *create_texture_from_cairo_surface(struct wlr_renderer *renderer, cairo_surface_t *surface);
struct wlr_texture *create_texture_from_file_v1(struct wlr_renderer *renderer, const char *file_path);
void wsm_image_node_load(struct wsm_image_node *node, const char *file_path);
void wsm_image_node_set_size(struct wsm_image_node *node, int width, int height);

#endif
