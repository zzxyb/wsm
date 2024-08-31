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

#include "wsm_container.h"
#include "wsm_view.h"
#include "wsm_log.h"
#include "wsm_output.h"
#include "wsm_server.h"
#include "wsm_scene.h"
#include "wsm_workspace.h"
#include "wsm_input_manager.h"
#include "wsm_seatop_default.h"
#include "wsm_seat.h"
#include "wsm_config.h"
#include "wsm_common.h"
#include "wsm_arrange.h"
#include "wsm_titlebar.h"
#include "wsm_desktop.h"
#include "node/wsm_text_node.h"
#include "wsm_titlebar.h"
#include "wsm_xdg_decoration.h"
#include "node/wsm_node_descriptor.h"
#include "node/wsm_image_node.h"

#include <stdlib.h>
#include <float.h>
#include <limits.h>

#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_xdg_decoration_v1.h>
#include <wlr/types/wlr_foreign_toplevel_management_v1.h>

static bool container_is_focused(struct wsm_container *con, void *data) {
	return con->current.focused;
}

static bool container_has_focused_child(struct wsm_container *con) {
	return container_find_child(con, container_is_focused, NULL);
}

static bool container_is_current_parent_focused(struct wsm_container *con) {
	if (con->current.parent) {
		struct wsm_container *parent = con->current.parent;
		return parent->current.focused || container_is_current_parent_focused(parent);
	} else if (con->current.workspace) {
		struct wsm_workspace *ws = con->current.workspace;
		return ws->current.focused;
	}
	
	return false;
}

static struct border_colors *container_get_current_colors(
		struct wsm_container *con) {
	struct border_colors *colors;
	
	bool urgent = con->view ?
						  view_is_urgent(con->view) : container_has_urgent_child(con);
	struct wsm_container *active_child;
	
	if (con->current.parent) {
		active_child = con->current.parent->current.focused_inactive_child;
	} else if (con->current.workspace) {
		active_child = con->current.workspace->current.focused_inactive_child;
	} else {
		active_child = NULL;
	}
	
	if (urgent) {
		colors = &global_config.border_colors.urgent;
	} else if (con->current.focused || container_is_current_parent_focused(con)) {
		colors = &global_config.border_colors.focused;
	} else if (container_has_focused_child(con)) {
		colors = &global_config.border_colors.focused_tab_title;
	} else if (con == active_child) {
		colors = &global_config.border_colors.focused_inactive;
	} else {
		colors = &global_config.border_colors.unfocused;
	}
	
	return colors;
}

static struct wlr_scene_rect *alloc_rect_node(struct wlr_scene_tree *parent,
											  bool *failed) {
	if (*failed) {
		return NULL;
	}
	
	struct wlr_scene_rect *rect = wlr_scene_rect_create(
			parent, 0, 0, (float[4]){0.f, 0.f, 0.f, 1.f});
	if (!rect) {
		wsm_log(WSM_ERROR, "Could not create wlr_scene_rect: allocation failed!");
		*failed = true;
	}
	
	return rect;
}

static void handle_output_enter(
		struct wl_listener *listener, void *data) {
	struct wsm_container *con = wl_container_of(
			listener, con, output_enter);
	struct wlr_scene_output *output = data;
	
	if (con->view->foreign_toplevel) {
		wlr_foreign_toplevel_handle_v1_output_enter(
				con->view->foreign_toplevel, output->output);
	}
}

static void handle_output_leave(
		struct wl_listener *listener, void *data) {
	struct wsm_container *con = wl_container_of(
			listener, con, output_leave);
	struct wlr_scene_output *output = data;
	
	if (con->view->foreign_toplevel) {
		wlr_foreign_toplevel_handle_v1_output_leave(
				con->view->foreign_toplevel, output->output);
	}
}

static bool handle_point_accepts_input(
		struct wlr_scene_buffer *buffer, double *x, double *y) {
	return false;
}

struct wsm_container *container_create(struct wsm_view *view) {
	struct wsm_container *c = calloc(1, sizeof(struct wsm_container));
	if (!wsm_assert(c, "Could not create wsm_container: allocation failed!")) {
		return NULL;
	}
	
	node_init(&c->node, N_CONTAINER, c);
	
		   // Container tree structure
		   // - scene tree
		   //   - title bar
		   //     - icon
		   //     - background
		   //     - title text
		   //     - button
		   //   - border for decoration
		   //     - border top/bottom/left/right
		   //     - later plan to draw it at the end of rendering
		   //     - buffer used for output enter/leave events for foreign_toplevel
		   //   - sensing_border
		   //     - sensing top/bottom/left/right convenient to perceive edge
	bool failed = false;
	c->scene_tree = alloc_scene_tree(global_server.wsm_scene->staging, &failed);
	
	c->title_bar = wsm_titlebar_create();
	c->title_bar->tree = alloc_scene_tree(c->scene_tree, &failed);
	c->title_bar->background = alloc_rect_node(c->title_bar->tree, &failed);
	
	c->sensing.tree = alloc_scene_tree(c->scene_tree, &failed);
	c->content_tree = alloc_scene_tree(c->sensing.tree, &failed);
	
	if (view) {
		c->sensing.top = alloc_rect_node(c->sensing.tree, &failed);
		c->sensing.bottom = alloc_rect_node(c->sensing.tree, &failed);
		c->sensing.left = alloc_rect_node(c->sensing.tree, &failed);
		c->sensing.right = alloc_rect_node(c->sensing.tree, &failed);
		
		c->output_handler = wlr_scene_buffer_create(c->sensing.tree, NULL);
		if (!c->output_handler) {
			wsm_log(WSM_ERROR, "Could not create wlr_scene_buffer for container scene node: allocation failed!");
			failed = true;
		}
		
		if (!failed) {
			c->output_enter.notify = handle_output_enter;
			wl_signal_add(&c->output_handler->events.output_enter,
						  &c->output_enter);
			c->output_leave.notify = handle_output_leave;
			wl_signal_add(&c->output_handler->events.output_leave,
						  &c->output_leave);
			c->output_handler->point_accepts_input = handle_point_accepts_input;
		}
	}
	
	if (!failed && !wsm_scene_descriptor_assign(&c->scene_tree->node,
												WSM_SCENE_DESC_CONTAINER, c)) {
		failed = true;
	}
	
	if (failed) {
		wlr_scene_node_destroy(&c->scene_tree->node);
		free(c);
		return NULL;
	}
	
	if (!view) {
		c->pending.children = create_list();
		c->current.children = create_list();
	}
	
	c->pending.layout = L_NONE;
	c->view = view;
	c->alpha = 1.0f;
	
	wl_signal_emit_mutable(&global_server.wsm_scene->events.new_node, &c->node);
	container_update(c);
	
	return c;
}

void container_destroy(struct wsm_container *con) {
	if (!wsm_assert(con->node.destroying,
					"Tried to free container which wasn't marked as destroying")) {
		return;
	}
	if (!wsm_assert(con->node.ntxnrefs == 0, "Tried to free container "
											 "which is still referenced by transactions")) {
		return;
	}
	free(con->title);
	free(con->formatted_title);
	list_free(con->pending.children);
	list_free(con->current.children);
	
	if (con->view && con->view->container == con) {
		con->view->container = NULL;
		wlr_scene_node_destroy(&con->output_handler->node);
		if (con->view->destroying) {
			view_destroy(con->view);
		}
	}
	
	scene_node_disown_children(con->content_tree);
	if (con->title_bar) {
		wsm_titlebar_destroy(con->title_bar);
		con->title_bar = NULL;
	}
	wlr_scene_node_destroy(&con->scene_tree->node);
	free(con);
}

void container_begin_destroy(struct wsm_container *con) {
	if (con->pending.fullscreen_mode == FULLSCREEN_WORKSPACE && con->pending.workspace) {
		con->pending.workspace->fullscreen = NULL;
	}
	if (con->scratchpad && con->pending.fullscreen_mode == FULLSCREEN_GLOBAL) {
		container_fullscreen_disable(con);
	}
	
	wl_signal_emit_mutable(&con->node.events.destroy, &con->node);
	
	container_end_mouse_operation(con);
	
	con->node.destroying = true;
	node_set_dirty(&con->node);
	
	if (con->scratchpad) {
		root_scratchpad_remove_container(con);
	}
	
	if (con->pending.fullscreen_mode == FULLSCREEN_GLOBAL) {
		container_fullscreen_disable(con);
	}
	
	if (con->pending.parent || con->pending.workspace) {
		container_detach(con);
	}
}

void container_get_box(struct wsm_container *container, struct wlr_box *box) {
	box->x = container->pending.x;
	box->y = container->pending.y;
	box->width = container->pending.width;
	box->height = container->pending.height;
}

struct wsm_container *container_find_child(struct wsm_container *container,
										   bool (*test)(struct wsm_container *view, void *data), void *data) {
	if (!container->pending.children) {
		return NULL;
	}
	for (int i = 0; i < container->pending.children->length; ++i) {
		struct wsm_container *child = container->pending.children->items[i];
		if (test(child, data)) {
			return child;
		}
		struct wsm_container *res = container_find_child(child, test, data);
		if (res) {
			return res;
		}
	}
	return NULL;
}

void container_for_each_child(struct wsm_container *container,
							  void (*f)(struct wsm_container *container, void *data), void *data) {
	if (container->pending.children)  {
		for (int i = 0; i < container->pending.children->length; ++i) {
			struct wsm_container *child = container->pending.children->items[i];
			f(child, data);
			container_for_each_child(child, f, data);
		}
	}
}


bool container_is_fullscreen_or_child(struct wsm_container *container) {
	do {
		if (container->pending.fullscreen_mode) {
			return true;
		}
		container = container->pending.parent;
	} while (container);
	
	return false;
}

bool container_is_scratchpad_hidden(struct wsm_container *con) {
	return con->scratchpad && !con->pending.workspace;
}

bool container_is_scratchpad_hidden_or_child(struct wsm_container *con) {
	con = container_toplevel_ancestor(con);
	return con->scratchpad && !con->pending.workspace;
}

struct wsm_container *container_toplevel_ancestor(
		struct wsm_container *container) {
	while (container->pending.parent) {
		container = container->pending.parent;
	}
	
	return container;
}

bool container_is_floating_or_child(struct wsm_container *container) {
	return container_is_floating(container_toplevel_ancestor(container));
}

bool container_is_floating(struct wsm_container *container) {
	if (!container->pending.parent && container->pending.workspace &&
		list_find(container->pending.workspace->floating, container) != -1) {
		return true;
	}
	if (container->scratchpad) {
		return true;
	}
	return false;
}

size_t container_titlebar_height(void) {
	return global_config.font_height + global_config.titlebar_v_padding * 2;
}

void container_raise_floating(struct wsm_container *con) {
	struct wsm_container *floater = container_toplevel_ancestor(con);
	if (container_is_floating(floater) && floater->pending.workspace) {
		wlr_scene_node_raise_to_top(&floater->scene_tree->node);
		
		list_move_to_end(floater->pending.workspace->floating, floater);
		node_set_dirty(&floater->pending.workspace->node);
	}
}

static void set_fullscreen(struct wsm_container *con, bool enable) {
	if (!con->view) {
		return;
	}
	if (con->view->impl->set_fullscreen) {
		con->view->impl->set_fullscreen(con->view, enable);
		if (con->view->foreign_toplevel) {
			wlr_foreign_toplevel_handle_v1_set_fullscreen(
					con->view->foreign_toplevel, enable);
		}
	}
}

struct wsm_output *container_floating_find_output(struct wsm_container *con) {
	double center_x = con->pending.x + con->pending.width / 2;
	double center_y = con->pending.y + con->pending.height / 2;
	struct wsm_output *closest_output = NULL;
	double closest_distance = DBL_MAX;
	for (int i = 0; i < global_server.wsm_scene->outputs->length; ++i) {
		struct wsm_output *output = global_server.wsm_scene->outputs->items[i];
		struct wlr_box output_box;
		double closest_x, closest_y;
		output_get_box(output, &output_box);
		wlr_box_closest_point(&output_box, center_x, center_y,
							  &closest_x, &closest_y);
		if (center_x == closest_x && center_y == closest_y) {
			// The center of the floating container is on this output
			return output;
		}
		double x_dist = closest_x - center_x;
		double y_dist = closest_y - center_y;
		double distance = x_dist * x_dist + y_dist * y_dist;
		if (distance < closest_distance) {
			closest_output = output;
			closest_distance = distance;
		}
	}
	return closest_output;
}

void container_fullscreen_disable(struct wsm_container *con) {
	if (!wsm_assert(con->pending.fullscreen_mode != FULLSCREEN_NONE,
					"Expected a fullscreen container")) {
		return;
	}
	set_fullscreen(con, false);
	
	if (container_is_floating(con)) {
		con->pending.x = con->saved_x;
		con->pending.y = con->saved_y;
		con->pending.width = con->saved_width;
		con->pending.height = con->saved_height;
	}
	
	if (con->pending.fullscreen_mode == FULLSCREEN_WORKSPACE) {
		if (con->pending.workspace) {
			con->pending.workspace->fullscreen = NULL;
			if (container_is_floating(con)) {
				struct wsm_output *output =
						container_floating_find_output(con);
				if (con->pending.workspace->output != output) {
					container_floating_move_to_center(con);
				}
			}
		}
	} else {
		global_server.wsm_scene->fullscreen_global = NULL;
	}
	
		   // If the container was mapped as fullscreen and set as floating by
		   // criteria, it needs to be reinitialized as floating to get the proper
		   // size and location
	if (container_is_floating(con) && (con->pending.width == 0 || con->pending.height == 0)) {
		container_floating_resize_and_center(con);
	}
	
	con->pending.fullscreen_mode = FULLSCREEN_NONE;
	container_end_mouse_operation(con);
	
	if (con->scratchpad) {
		struct wsm_seat *seat;
		wl_list_for_each(seat, &global_server.wsm_input_manager->seats, link) {
			struct wsm_container *focus = seat_get_focused_container(seat);
			if (focus == con || container_has_ancestor(focus, con)) {
				seat_set_focus(seat,
							   seat_get_focus_inactive(seat, &global_server.wsm_scene->node));
			}
		}
	}
}

void container_floating_move_to(struct wsm_container *con,
								double lx, double ly) {
	if (!wsm_assert(container_is_floating(con),
					"Expected a floating container")) {
		return;
	}
	container_floating_translate(con, lx - con->pending.x, ly - con->pending.y);
	if (container_is_scratchpad_hidden(con)) {
		return;
	}
	struct wsm_workspace *old_workspace = con->pending.workspace;
	struct wsm_output *new_output = container_floating_find_output(con);
	if (!wsm_assert(new_output, "Unable to find any output")) {
		return;
	}
	struct wsm_workspace *new_workspace =
			output_get_active_workspace(new_output);
	if (new_workspace && old_workspace != new_workspace) {
		container_detach(con);
		workspace_add_floating(new_workspace, con);
		wsm_arrange_workspace_auto(old_workspace);
		wsm_arrange_workspace_auto(new_workspace);
		// If the moved container was a visible scratchpad container, then
		// update its transform.
		if (con->scratchpad) {
			struct wlr_box output_box;
			output_get_box(new_output, &output_box);
			con->transform = output_box;
		}
		workspace_detect_urgent(old_workspace);
		workspace_detect_urgent(new_workspace);
	}
}

void container_floating_move_to_center(struct wsm_container *con) {
	if (!wsm_assert(container_is_floating(con),
					"Expected a floating container")) {
		return;
	}
	struct wsm_workspace *ws = con->pending.workspace;
	double new_lx = ws->x + (ws->width - con->pending.width) / 2;
	double new_ly = ws->y + (ws->height - con->pending.height) / 2;
	container_floating_translate(con, new_lx - con->pending.x, new_ly - con->pending.y);
}

void container_floating_translate(struct wsm_container *con,
								  double x_amount, double y_amount) {
	con->pending.x += x_amount;
	con->pending.y += y_amount;
	con->pending.content_x += x_amount;
	con->pending.content_y += y_amount;
	
	if (con->pending.children) {
		for (int i = 0; i < con->pending.children->length; ++i) {
			struct wsm_container *child = con->pending.children->items[i];
			container_floating_translate(child, x_amount, y_amount);
		}
	}
	
	node_set_dirty(&con->node);
}

static void floating_natural_resize(struct wsm_container *con) {
	int min_width = 100, max_width = INT_MAX, min_height = 100, max_height = INT_MAX;
	
		   // config min size
	
	if (!con->view) {
		con->pending.width = fmax(min_width, fmin(con->pending.width, max_width));
		con->pending.height = fmax(min_height, fmin(con->pending.height, max_height));
	} else {
		struct wsm_view *view = con->view;
		con->pending.content_width =
				fmax(min_width, fmin(view->natural_width, max_width));
		con->pending.content_height =
				fmax(min_height, fmin(view->natural_height, max_height));
		container_set_geometry_from_content(con);
	}
}

void container_floating_resize_and_center(struct wsm_container *con) {
	struct wsm_workspace *ws = con->pending.workspace;
	if (!ws) {
		// On scratchpad, just resize
		floating_natural_resize(con);
		return;
	}
	
	struct wlr_box ob;
	wlr_output_layout_get_box(global_server.wsm_scene->output_layout, ws->output->wlr_output, &ob);
	if (wlr_box_empty(&ob)) {
		// On NOOP output. Will be called again when moved to an output
		con->pending.x = 0;
		con->pending.y = 0;
		con->pending.width = 0;
		con->pending.height = 0;
		return;
	}
	
	floating_natural_resize(con);
	if (!con->view) {
		if (con->pending.width > ws->width || con->pending.height > ws->height) {
			con->pending.x = ob.x + (ob.width - con->pending.width) / 2;
			con->pending.y = ob.y + (ob.height - con->pending.height) / 2;
		} else {
			con->pending.x = ws->x + (ws->width - con->pending.width) / 2;
			con->pending.y = ws->y + (ws->height - con->pending.height) / 2;
		}
	} else {
		if (con->pending.content_width > ws->width
			|| con->pending.content_height > ws->height) {
			con->pending.content_x = ob.x + (ob.width - con->pending.content_width) / 2;
			con->pending.content_y = ob.y + (ob.height - con->pending.content_height) / 2;
		} else {
			con->pending.content_x = ws->x + (ws->width - con->pending.content_width) / 2;
			con->pending.content_y = ws->y + (ws->height - con->pending.content_height) / 2;
		}
		
			   // If the view's border is B_NONE then these properties are ignored.
		con->pending.border_top = con->pending.border_bottom = true;
		con->pending.border_left = con->pending.border_right = true;
		
		container_set_geometry_from_content(con);
	}
}

void container_set_geometry_from_content(struct wsm_container *con) {
	if (!wsm_assert(con->view, "Expected a view")) {
		return;
	}
	if (!wsm_assert(container_is_floating(con), "Expected a floating view")) {
		return;
	}
	size_t border_width = 0;
	size_t top = 0;
	
	if (con->pending.border != B_CSD && !con->pending.fullscreen_mode) {
		border_width = get_max_thickness(con->pending) * (con->pending.border != B_NONE);
		top = con->pending.border == B_NORMAL ?
					  container_titlebar_height() : border_width;
	}
	
	con->pending.x = con->pending.content_x - border_width;
	con->pending.y = con->pending.content_y - top - border_width;
	con->pending.width = con->pending.content_width + border_width * 2;
	con->pending.height = top + con->pending.content_height + border_width * 2;
	node_set_dirty(&con->node);
}

void container_end_mouse_operation(struct wsm_container *container) {
	struct wsm_seat *seat;
	wl_list_for_each(seat, &global_server.wsm_input_manager->seats, link) {
		seatop_unref(seat, container);
	}
}

bool container_has_ancestor(struct wsm_container *descendant,
							struct wsm_container *ancestor) {
	while (descendant) {
		descendant = descendant->pending.parent;
		if (descendant == ancestor) {
			return true;
		}
	}
	return false;
}

static void set_workspace(struct wsm_container *container, void *data) {
	container->pending.workspace = container->pending.parent->pending.workspace;
}

void container_detach(struct wsm_container *child) {
	if (child->pending.fullscreen_mode == FULLSCREEN_WORKSPACE) {
		child->pending.workspace->fullscreen = NULL;
	}
	if (child->pending.fullscreen_mode == FULLSCREEN_GLOBAL) {
		global_server.wsm_scene->fullscreen_global = NULL;
	}
	
	struct wsm_container *old_parent = child->pending.parent;
	struct wsm_workspace *old_workspace = child->pending.workspace;
	struct wsm_list *siblings = container_get_siblings(child);
	if (siblings) {
		int index = list_find(siblings, child);
		if (index != -1) {
			list_del(siblings, index);
		}
	}
	child->pending.parent = NULL;
	child->pending.workspace = NULL;
	container_for_each_child(child, set_workspace, NULL);
	
	if (old_parent) {
		container_update_representation(old_parent);
		node_set_dirty(&old_parent->node);
	} else if (old_workspace) {
		workspace_update_representation(old_workspace);
		node_set_dirty(&old_workspace->node);
	}
	node_set_dirty(&child->node);
}

struct wsm_list *container_get_siblings(struct wsm_container *container) {
	if (container->pending.parent) {
		return container->pending.parent->pending.children;
	}
	if (!container->pending.workspace) {
		return NULL;
	}
	if (list_find(container->pending.workspace->tiling, container) != -1) {
		return container->pending.workspace->tiling;
	}
	return container->pending.workspace->floating;
}

void container_update_representation(struct wsm_container *con) {
	if (!con->view) {
		size_t len = container_build_representation(con->pending.layout,
													con->pending.children, NULL);
		free(con->formatted_title);
		con->formatted_title = calloc(len + 1, sizeof(char));
		if (!wsm_assert(con->formatted_title,
						"Unable to allocate title string")) {
			return;
		}
		container_build_representation(con->pending.layout, con->pending.children,
									   con->formatted_title);
		
		if (con->title_bar->title_text) {
			wsm_text_node_set_text(con->title_bar->title_text, con->formatted_title);
			container_arrange_title_bar_node(con);
		} else {
			container_update_title_bar(con);
		}
	}
	if (con->pending.parent) {
		container_update_representation(con->pending.parent);
	} else if (con->pending.workspace) {
		workspace_update_representation(con->pending.workspace);
	}
}

size_t container_build_representation(enum wsm_container_layout layout,
									  struct wsm_list *children, char *buffer) {
	size_t len = 2;
	switch (layout) {
	case L_HORIZ:
		lenient_strcat(buffer, "V[");
		break;
	case L_HORIZ_1_V_2:
		lenient_strcat(buffer, "H[");
		break;
	case L_HORIZ_2_V_1:
		lenient_strcat(buffer, "T[");
		break;
	case L_GRID:
		lenient_strcat(buffer, "S[");
		break;
	case L_NONE:
		lenient_strcat(buffer, "D[");
		break;
	}
	for (int i = 0; i < children->length; ++i) {
		if (i != 0) {
			++len;
			lenient_strcat(buffer, " ");
		}
		struct wsm_container *child = children->items[i];
		const char *identifier = NULL;
		if (child->view) {
			identifier = view_get_class(child->view);
			if (!identifier) {
				identifier = view_get_app_id(child->view);
			}
		} else {
			identifier = child->formatted_title;
		}
		if (identifier) {
			len += strlen(identifier);
			lenient_strcat(buffer, identifier);
		} else {
			len += 6;
			lenient_strcat(buffer, "(null)");
		}
	}
	++len;
	lenient_strcat(buffer, "]");
	return len;
}

void container_update_title_bar(struct wsm_container *con) {
	if (!con->formatted_title) {
		return;
	}
	
	struct border_colors *colors = container_get_current_colors(con);
	
	if (con->title_bar->title_text) {
		wlr_scene_node_destroy(con->title_bar->title_text->node);
		con->title_bar->title_text = NULL;
	}
	
	con->title_bar->title_text = wsm_text_node_create(con->title_bar->tree,
													  global_server.desktop_interface, con->formatted_title, colors->text, true);
	
	container_arrange_title_bar_node(con);
}

void container_handle_fullscreen_reparent(struct wsm_container *con) {
	if (con->pending.fullscreen_mode != FULLSCREEN_WORKSPACE || !con->pending.workspace ||
		con->pending.workspace->fullscreen == con) {
		return;
	}
	if (con->pending.workspace->fullscreen) {
		container_fullscreen_disable(con->pending.workspace->fullscreen);
	}
	con->pending.workspace->fullscreen = con;
	
	wsm_arrange_workspace_auto(con->pending.workspace);
}

void floating_fix_coordinates(struct wsm_container *con,
							  struct wlr_box *old, struct wlr_box *new) {
	if (!old->width || !old->height) {
		// Fall back to centering on the workspace.
		container_floating_move_to_center(con);
	} else {
		int rel_x = con->pending.x - old->x + (con->pending.width / 2);
		int rel_y = con->pending.y - old->y + (con->pending.height / 2);
		
		con->pending.x = new->x + (double)(rel_x * new->width) / old->width - (con->pending.width / 2);
		con->pending.y = new->y + (double)(rel_y * new->height) / old->height - (con->pending.height / 2);
		
		wsm_log(WSM_DEBUG, "Transformed container %p to coords (%f, %f)", con, con->pending.x, con->pending.y);
	}
}

enum wsm_container_layout container_parent_layout(struct wsm_container *con) {
	if (con->pending.parent) {
		return con->pending.parent->pending.layout;
	}
	if (con->pending.workspace) {
		return con->pending.workspace->layout;
	}
	return L_NONE;
}

static void container_fullscreen_workspace(struct wsm_container *con) {
	if (!wsm_assert(con->pending.fullscreen_mode == FULLSCREEN_NONE,
					"Expected a non-fullscreen container")) {
		return;
	}
	set_fullscreen(con, true);
	con->pending.fullscreen_mode = FULLSCREEN_WORKSPACE;
	
	con->saved_x = con->pending.x;
	con->saved_y = con->pending.y;
	con->saved_width = con->pending.width;
	con->saved_height = con->pending.height;
	
	if (con->pending.workspace) {
		con->pending.workspace->fullscreen = con;
		struct wsm_seat *seat;
		struct wsm_workspace *focus_ws;
		wl_list_for_each(seat, &global_server.wsm_input_manager->seats, link) {
			focus_ws = seat_get_focused_workspace(seat);
			if (focus_ws == con->pending.workspace) {
				seat_set_focus_container(seat, con);
			} else {
				struct wsm_node *focus =
						seat_get_focus_inactive(seat, &global_server.wsm_scene->node);
				seat_set_raw_focus(seat, &con->node);
				seat_set_raw_focus(seat, focus);
			}
		}
	}
	
	container_end_mouse_operation(con);
}

static void container_fullscreen_global(struct wsm_container *con) {
	if (!wsm_assert(con->pending.fullscreen_mode == FULLSCREEN_NONE,
					"Expected a non-fullscreen container")) {
		return;
	}
	set_fullscreen(con, true);
	
	global_server.wsm_scene->fullscreen_global = con;
	con->saved_x = con->pending.x;
	con->saved_y = con->pending.y;
	con->saved_width = con->pending.width;
	con->saved_height = con->pending.height;
	
	struct wsm_seat *seat;
	wl_list_for_each(seat, &global_server.wsm_input_manager->seats, link) {
		struct wsm_container *focus = seat_get_focused_container(seat);
		if (focus && focus != con) {
			seat_set_focus_container(seat, con);
		}
	}
	
	con->pending.fullscreen_mode = FULLSCREEN_GLOBAL;
	container_end_mouse_operation(con);
}

void container_set_fullscreen(struct wsm_container *con,
							  enum wsm_fullscreen_mode mode) {
	if (con->pending.fullscreen_mode == mode) {
		return;
	}
	
	switch (mode) {
	case FULLSCREEN_NONE:
		container_fullscreen_disable(con);
		break;
	case FULLSCREEN_WORKSPACE:
		if (global_server.wsm_scene->fullscreen_global) {
			container_fullscreen_disable(global_server.wsm_scene->fullscreen_global);
		}
		if (con->pending.workspace && con->pending.workspace->fullscreen) {
			container_fullscreen_disable(con->pending.workspace->fullscreen);
		}
		container_fullscreen_workspace(con);
		break;
	case FULLSCREEN_GLOBAL:
		if (global_server.wsm_scene->fullscreen_global) {
			container_fullscreen_disable(global_server.wsm_scene->fullscreen_global);
		}
		if (con->pending.fullscreen_mode == FULLSCREEN_WORKSPACE) {
			container_fullscreen_disable(con);
		}
		container_fullscreen_global(con);
		break;
	}
}

void container_reap_empty(struct wsm_container *con) {
	if (con->view) {
		return;
	}
	struct wsm_workspace *ws = con->pending.workspace;
	while (con) {
		if (con->pending.children->length) {
			return;
		}
		struct wsm_container *parent = con->pending.parent;
		container_begin_destroy(con);
		con = parent;
	}
	if (ws) {
		workspace_consider_destroy(ws);
	}
}

void root_scratchpad_remove_container(struct wsm_container *con) {
	if (!wsm_assert(con->scratchpad, "Container is not in scratchpad")) {
		return;
	}
	con->scratchpad = false;
	int index = list_find(global_server.wsm_scene->scratchpad, con);
	if (index != -1) {
		list_del(global_server.wsm_scene->scratchpad, index);
	}
}

void container_add_sibling(struct wsm_container *parent,
						   struct wsm_container *child, bool after) {
	if (child->pending.workspace) {
		container_detach(child);
	}
	struct wsm_list *siblings = container_get_siblings(parent);
	int index = list_find(siblings, parent);
	list_insert(siblings, index + after, child);
	child->pending.parent = parent->pending.parent;
	child->pending.workspace = parent->pending.workspace;
	container_for_each_child(child, set_workspace, NULL);
	container_handle_fullscreen_reparent(child);
	container_update_representation(child);
}

void container_set_floating(struct wsm_container *container, bool enable) {
	if (container_is_floating(container) == enable) {
		return;
	}
	
	struct wsm_seat *seat = input_manager_current_seat();
	struct wsm_workspace *workspace = container->pending.workspace;
	struct wsm_container *focus = seat_get_focused_container(seat);
	bool set_focus = focus == container;
	
	if (enable) {
		struct wsm_container *old_parent = container->pending.parent;
		container_detach(container);
		workspace_add_floating(workspace, container);
		if (container->view) {
			view_set_tiled(container->view, false);
			if (container->view->using_csd) {
				container->saved_border = container->pending.border;
				container->pending.border = B_CSD;
				if (container->view->xdg_decoration) {
					struct wsm_xdg_decoration *deco = container->view->xdg_decoration;
					wlr_xdg_toplevel_decoration_v1_set_mode(deco->wlr_xdg_decoration,
															WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE);
				}
			}
		}
		container_floating_set_default_size(container);
		container_floating_resize_and_center(container);
		if (old_parent) {
			if (set_focus) {
				seat_set_raw_focus(seat, &old_parent->node);
				seat_set_raw_focus(seat, &container->node);
			}
			container_reap_empty(old_parent);
		}
	} else {
		// Returning to tiled
		if (container->scratchpad) {
			root_scratchpad_remove_container(container);
		}
		container_detach(container);
		struct wsm_container *reference =
				seat_get_focus_inactive_tiling(seat, workspace);
		if (reference) {
			if (reference->view) {
				container_add_sibling(reference, container, 1);
			} else {
				container_add_child(reference, container);
			}
			container->pending.width = reference->pending.width;
			container->pending.height = reference->pending.height;
		} else {
			struct wsm_container *other =
					workspace_add_tiling(workspace, container);
			other->pending.width = workspace->width;
			other->pending.height = workspace->height;
		}
		if (container->view) {
			view_set_tiled(container->view, true);
			if (container->view->using_csd) {
				container->pending.border = container->saved_border;
				if (container->view->xdg_decoration) {
					struct wsm_xdg_decoration *deco = container->view->xdg_decoration;
					wlr_xdg_toplevel_decoration_v1_set_mode(deco->wlr_xdg_decoration,
															WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
				}
			}
		}
		container->width_fraction = 0;
		container->height_fraction = 0;
	}
	
	container_end_mouse_operation(container);
}

void container_floating_set_default_size(struct wsm_container *con) {
	if (!wsm_assert(con->pending.workspace, "Expected a container on a workspace")) {
		return;
	}
	
	int min_width = 75, max_width = INT_MAX, min_height = 50, max_height = INT_MAX;
	struct wlr_box box;
	workspace_get_box(con->pending.workspace, &box);
	
	double width = fmax(min_width, fmin(box.width * 0.5, max_width));
	double height = fmax(min_height, fmin(box.height * 0.75, max_height));
	if (!con->view) {
		con->pending.width = width;
		con->pending.height = height;
	} else {
		con->pending.content_width = width;
		con->pending.content_height = height;
		container_set_geometry_from_content(con);
	}
}

void container_add_child(struct wsm_container *parent,
						 struct wsm_container *child) {
	if (child->pending.workspace) {
		container_detach(child);
	}
	list_add(parent->pending.children, child);
	child->pending.parent = parent;
	child->pending.workspace = parent->pending.workspace;
	container_for_each_child(child, set_workspace, NULL);
	container_handle_fullscreen_reparent(child);
	container_update_representation(parent);
	node_set_dirty(&child->node);
	node_set_dirty(&parent->node);
}

bool container_is_sticky(struct wsm_container *con) {
	return con->is_sticky && container_is_floating(con);
}

bool container_is_sticky_or_child(struct wsm_container *con) {
	return container_is_sticky(container_toplevel_ancestor(con));
}

bool container_is_transient_for(struct wsm_container *child,
								struct wsm_container *ancestor) {
	return child->view && ancestor->view &&
		   view_is_transient_for(child->view, ancestor->view);
}

static void scene_rect_set_color(struct wlr_scene_rect *rect,
								 const float color[4], float opacity) {
	const float premultiplied[] = {
			color[0] * color[3] * opacity,
			color[1] * color[3] * opacity,
			color[2] * color[3] * opacity,
			color[3] * opacity,
	};
	
	wlr_scene_rect_set_color(rect, premultiplied);
}

void container_update(struct wsm_container *con) {
	struct border_colors *colors = container_get_current_colors(con);
	float alpha = con->alpha;
	scene_rect_set_color(con->title_bar->background, colors->background, alpha);
	
	if (con->view) {
		wlr_scene_rect_set_color(con->sensing.top, global_config.sensing_border_color);
		wlr_scene_rect_set_color(con->sensing.bottom, global_config.sensing_border_color);
		wlr_scene_rect_set_color(con->sensing.left, global_config.sensing_border_color);
		wlr_scene_rect_set_color(con->sensing.right, global_config.sensing_border_color);
	}
	
	if (con->title_bar->title_text) {
		wsm_text_node_set_color(con->title_bar->title_text, colors->text);
		wsm_text_node_set_background(con->title_bar->title_text, global_config.text_background_color);
	}
}

void container_update_itself_and_parents(struct wsm_container *con) {
	container_update(con);
	
	if (con->current.parent) {
		container_update_itself_and_parents(con->current.parent);
	}
}

static bool find_urgent_iterator(struct wsm_container *con, void *data) {
	return con->view && view_is_urgent(con->view);
}

bool container_has_urgent_child(struct wsm_container *container) {
	return container_find_child(container, find_urgent_iterator, NULL);
}

void container_set_resizing(struct wsm_container *con, bool resizing) {
	if (!con) {
		return;
	}
	
	if (con->view) {
		if (con->view->impl->set_resizing) {
			con->view->impl->set_resizing(con->view, resizing);
		}
	} else {
		for (int i = 0; i < con->pending.children->length; ++i ) {
			struct wsm_container *child = con->pending.children->items[i];
			container_set_resizing(child, resizing);
		}
	}
}

void floating_calculate_constraints(int *min_width, int *max_width,
									int *min_height, int *max_height) {
	if (global_config.floating_minimum_width == -1) { // no minimum
		*min_width = 0;
	} else if (global_config.floating_minimum_width == 0) { // automatic
		*min_width = 75;
	} else {
		*min_width = global_config.floating_minimum_width;
	}
	
	if (global_config.floating_minimum_height == -1) { // no minimum
		*min_height = 0;
	} else if (global_config.floating_minimum_height == 0) { // automatic
		*min_height = 50;
	} else {
		*min_height = global_config.floating_minimum_height;
	}
	
	struct wlr_box box;
	wlr_output_layout_get_box(global_server.wsm_scene->output_layout, NULL, &box);
	
	if (global_config.floating_maximum_width == -1) { // no maximum
		*max_width = INT_MAX;
	} else if (global_config.floating_maximum_width == 0) { // automatic
		*max_width = box.width;
	} else {
		*max_width = global_config.floating_maximum_width;
	}
	
	if (global_config.floating_maximum_height == -1) { // no maximum
		*max_height = INT_MAX;
	} else if (global_config.floating_maximum_height == 0) { // automatic
		*max_height = box.height;
	} else {
		*max_height = global_config.floating_maximum_height;
	}
}

struct wsm_container *container_obstructing_fullscreen_container(struct wsm_container *container) {
	struct wsm_workspace *workspace = container->pending.workspace;
	
	if (workspace && workspace->fullscreen && !container_is_fullscreen_or_child(container)) {
		if (container_is_transient_for(container, workspace->fullscreen)) {
			return NULL;
		}
		return workspace->fullscreen;
	}
	
	struct wsm_container *fullscreen_global = global_server.wsm_scene->fullscreen_global;
	if (fullscreen_global && container != fullscreen_global && !container_has_ancestor(container, fullscreen_global)) {
		if (container_is_transient_for(container, fullscreen_global)) {
			return NULL;
		}
		return fullscreen_global;
	}
	
	return NULL;
}

void disable_container(struct wsm_container *con) {
	if (con->view) {
		wlr_scene_node_reparent(&con->view->scene_tree->node, con->content_tree);
	} else {
		for (int i = 0; i < con->current.children->length; i++) {
			struct wsm_container *child = con->current.children->items[i];
			
			wlr_scene_node_reparent(&child->scene_tree->node, con->content_tree);
			
			disable_container(child);
		}
	}
}

int get_max_thickness(struct wsm_container_state state) {
	return state.sensing_thickness > state.border_thickness
				   ? state.sensing_thickness : state.border_thickness;
}
