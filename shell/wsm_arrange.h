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

#ifndef WSM_ARRANGE_H
#define WSM_ARRANGE_H

#include "wsm_container.h"

struct wsm_list;
struct wsm_scene;
struct wsm_output;
struct wsm_workspace;

void arrange_root_auto(void);
void arrange_root_scene(struct wsm_scene *root);
void wsm_arrange_output_auto(struct wsm_output *output);
void arrange_output_width_size(struct wsm_output *output, int width, int height);
void wsm_arrange_workspace_auto(struct wsm_workspace *workspace);
void arrange_workspace_tiling(struct wsm_workspace *ws,
	int width, int height);
void arrange_workspace_floating(struct wsm_workspace *ws);
void wsm_arrange_layer_surface(struct wsm_output *output, const struct wlr_box *full_area,
	struct wlr_box *usable_area, struct wlr_scene_tree *tree);
void wsm_arrange_popups(struct wlr_scene_tree *popups);
void wsm_arrange_layers(struct wsm_output *output);
void wsm_arrange_container_auto(struct wsm_container *container);
void wsm_arrange_container_with_title_bar(struct wsm_container *con,
	int width, int height, bool title_bar, int gaps);
void wsm_arrange_children(struct wsm_list *children,
	enum wsm_container_layout layout, struct wlr_box *parent);
void arrange_children_with_titlebar(enum wsm_container_layout layout, struct wsm_list *children,
	struct wsm_container *active, struct wlr_scene_tree *content,
	int width, int height, int gaps);
void wsm_arrange_floating(struct wsm_list *floating);
void container_arrange_title_bar_node(struct wsm_container *con);
void wsm_arrange_title_bar(struct wsm_container *con,
	int x, int y, int width, int height);
void wsm_arrange_fullscreen(struct wlr_scene_tree *tree,
	struct wsm_container *fs, struct wsm_workspace *ws,
	int width, int height);


#endif
