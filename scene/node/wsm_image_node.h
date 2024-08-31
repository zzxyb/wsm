/*
MIT License

Copyright (c) 2024 YaoBing Xiao

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

#ifndef WSM_IMAGE_NODE_H
#define WSM_IMAGE_NODE_H

#include <stdbool.h>

struct wlr_renderer;
struct wlr_texture;
struct wlr_scene_node;
struct wlr_scene_tree;

typedef struct _cairo_surface cairo_surface_t;

struct wsm_image_node {
	int width;
	int height;
	float alpha;
	
	struct wlr_scene_node *node;
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
