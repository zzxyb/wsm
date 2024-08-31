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

#ifndef WSM_WORKSPACE_H
#define WSM_WORKSPACE_H

#include "wsm_container.h"
#include "node/wsm_node.h"

#include <stdbool.h>

struct wlr_box;
struct wlr_surface;
struct wlr_scene_tree;

struct wsm_list;
struct wsm_output;
struct wsm_xwayland_view;

struct wsm_workspace_state {
	struct wsm_container *fullscreen;
	double x, y;
	int width, height;
	enum wsm_container_layout layout;
	struct wsm_output *output;
	struct wsm_list *floating;
	struct wsm_list *tiling;
	
	struct wsm_container *focused_inactive_child;
	bool focused;
};

struct side_gaps {
	int top;
	int right;
	int bottom;
	int left;
};

/**
 * @brief workspace object.
 *
 * @details multiple wsm_containers are arranged in layers
 * in wsm_workspace to achieve a stacking effect.
 * -------------------------------------------------
 * |                                                |
 * |                wsm_workspace                   |
 * |                                                |
 * |    ***********                                 |
 * |    *         *  wsm_container1 mean layer1     |
 * |    *         *                                 |
 * |    *   ***********                             |
 * |    *   *     *   *                             |
 * |    *   *     *   * wsm_container2 mean layer2  |
 * |    ***********   *                             |
 * |        *         *                             |
 * |        *         *                             |
 * |        ***********                             |
 * |                         container3....max      |
 * --------------------------------------------------
 */
struct wsm_workspace {
	struct wsm_node node;
	
	struct {
		struct wlr_scene_tree *non_fullscreen;
		struct wlr_scene_tree *fullscreen;
	} layers;
	
	struct wsm_container *fullscreen;
	
	char *name;
	char *representation;
	
	double x, y;
	int width, height;
	enum wsm_container_layout layout;
	enum wsm_container_layout prev_split_layout;
	
	struct side_gaps current_gaps;
	int gaps_inner;
	struct side_gaps gaps_outer;
	
	struct wsm_output *output; // NULL if no outputs are connected
	struct wsm_list *floating;
	struct wsm_list *tiling;
	struct wsm_list *output_priority;
	bool urgent;
	
	struct wsm_workspace_state current;
};

struct wsm_workspace *workspace_create(struct wsm_output *output,
									   const char *name);
void workspace_destroy(struct wsm_workspace *workspace);
void workspace_detach(struct wsm_workspace *workspace);
void workspace_get_box(struct wsm_workspace *workspace, struct wlr_box *box);
void workspace_for_each_container(struct wsm_workspace *ws,
								  void (*f)(struct wsm_container *con, void *data), void *data);
bool workspace_is_visible(struct wsm_workspace *ws);
bool workspace_is_empty(struct wsm_workspace *ws);
void root_for_each_container(void (*f)(struct wsm_container *con, void *data),
							 void *data);
void root_for_each_workspace(void (*f)(struct wsm_workspace *ws, void *data),
							 void *data);
void workspace_update_representation(struct wsm_workspace *ws);
void workspace_add_floating(struct wsm_workspace *workspace,
							struct wsm_container *con);
void workspace_add_gaps(struct wsm_workspace *ws);
void workspace_consider_destroy(struct wsm_workspace *ws);
void workspace_begin_destroy(struct wsm_workspace *workspace);
struct wsm_container *workspace_add_tiling(struct wsm_workspace *workspace,
										   struct wsm_container *con);
void workspace_detect_urgent(struct wsm_workspace *workspace);
struct wsm_container *workspace_find_container(struct wsm_workspace *ws,
											   bool (*test)(struct wsm_container *con, void *data), void *data);
struct wsm_output *workspace_output_get_highest_available(
		struct wsm_workspace *ws, struct wsm_output *exclude);
size_t workspace_num_sticky_containers(struct wsm_workspace *ws);
void workspace_output_add_priority(struct wsm_workspace *workspace,
								   struct wsm_output *output);
bool output_match_name_or_id(struct wsm_output *output,
							 const char *name_or_id);
void output_add_workspace(struct wsm_output *output,
						  struct wsm_workspace *workspace);
void output_sort_workspaces(struct wsm_output *output);
void disable_workspace(struct wsm_workspace *ws);

#endif
