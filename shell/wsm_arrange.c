#include "wsm_log.h"
#include "wsm_view.h"
#include "wsm_scene.h"
#include "wsm_server.h"
#include "wsm_output.h"
#include "wsm_config.h"
#include "wsm_arrange.h"
#include "wsm_workspace.h"
#include "wsm_common.h"
#include "wsm_titlebar.h"
#include "wsm_layer_shell.h"
#include "wsm_desktop.h"
#include "node/wsm_node_descriptor.h"
#include "node/wsm_text_node.h"
#include "node/wsm_image_node.h"
#include "node/wsm_button_node.h"

#include <wlr/types/wlr_scene.h>

void arrange_root_auto(void) {
	struct wlr_box layout_box;
	wlr_output_layout_get_box(global_server.scene->output_layout, NULL, &layout_box);
	global_server.scene->x = layout_box.x;
	global_server.scene->y = layout_box.y;
	global_server.scene->width = layout_box.width;
	global_server.scene->height = layout_box.height;

	if (global_server.scene->fullscreen_global) {
		struct wsm_window *fs = global_server.scene->fullscreen_global;
		fs->pending.x = global_server.scene->x;
		fs->pending.y = global_server.scene->y;
		fs->pending.width = global_server.scene->width;
		fs->pending.height = global_server.scene->height;
		wsm_arrange_window_auto(fs);
	} else {
		for (int i = 0; i < global_server.scene->outputs->length; ++i) {
			struct wsm_output *output = global_server.scene->outputs->items[i];
			wsm_arrange_output_auto(output);
		}
	}
}

void arrange_root_scene(struct wsm_scene *root) {
	struct wsm_window *fs = root->fullscreen_global;

	wlr_scene_node_set_enabled(&root->layers.shell_background->node, !fs);
	wlr_scene_node_set_enabled(&root->layers.shell_bottom->node, !fs);
	wlr_scene_node_set_enabled(&root->layers.tiling->node, !fs);
	wlr_scene_node_set_enabled(&root->layers.windows->node, !fs);
	wlr_scene_node_set_enabled(&root->layers.shell_top->node, !fs);
	wlr_scene_node_set_enabled(&root->layers.fullscreen->node, !fs);

	for (int i = 0; i < root->scratchpad->length; i++) {
		struct wsm_window *window = root->scratchpad->items[i];
		wlr_scene_node_set_enabled(&window->scene_tree->node, false);
	}

	if (fs) {
		for (int i = 0; i < root->outputs->length; i++) {
			struct wsm_output *output = root->outputs->items[i];
			struct wsm_workspace *ws = output->current.active_workspace;
			if (ws) {
				arrange_workspace_windows(ws);
			}
		}

		wsm_arrange_fullscreen(root->layers.fullscreen_global, fs, NULL,
			root->width, root->height);
	} else {
		for (int i = 0; i < root->outputs->length; i++) {
			struct wsm_output *output = root->outputs->items[i];

			wlr_scene_output_set_position(output->scene_output, output->lx, output->ly);

			wlr_scene_node_reparent(&output->layers.shell_background->node, root->layers.shell_background);
			wlr_scene_node_reparent(&output->layers.shell_bottom->node, root->layers.shell_bottom);
			wlr_scene_node_reparent(&output->layers.tiling->node, root->layers.tiling);
			wlr_scene_node_reparent(&output->layers.shell_top->node, root->layers.shell_top);
			wlr_scene_node_reparent(&output->layers.shell_overlay->node, root->layers.shell_overlay);
			wlr_scene_node_reparent(&output->layers.fullscreen->node, root->layers.fullscreen);
			wlr_scene_node_reparent(&output->layers.session_lock->node, root->layers.session_lock);

			wlr_scene_node_set_position(&output->layers.shell_background->node, output->lx, output->ly);
			wlr_scene_node_set_position(&output->layers.shell_bottom->node, output->lx, output->ly);
			wlr_scene_node_set_position(&output->layers.tiling->node, output->lx, output->ly);
			wlr_scene_node_set_position(&output->layers.fullscreen->node, output->lx, output->ly);
			wlr_scene_node_set_position(&output->layers.shell_top->node, output->lx, output->ly);
			wlr_scene_node_set_position(&output->layers.shell_overlay->node, output->lx, output->ly);
			wlr_scene_node_set_position(&output->layers.session_lock->node, output->lx, output->ly);

			arrange_output_width_size(output, output->width, output->height);
		}
	}

	wsm_arrange_popups(root->layers.popup);
}

void wsm_arrange_output_auto(struct wsm_output *output) {
	struct wlr_box output_box;
	wlr_output_layout_get_box(global_server.scene->output_layout,
		output->wlr_output, &output_box);
	output->lx = output_box.x;
	output->ly = output_box.y;
	output->width = output_box.width;
	output->height = output_box.height;

	for (int i = 0; i < output->workspaces->length; ++i) {
		struct wsm_workspace *workspace = output->workspaces->items[i];
		wsm_arrange_workspace_auto(workspace);
	}
}

void arrange_output_width_size(struct wsm_output *output, int width, int height) {
	for (int i = 0; i < output->current.workspaces->length; i++) {
		struct wsm_workspace *workspace = output->current.workspaces->items[i];

		bool activated = output->current.active_workspace == workspace;

		wlr_scene_node_reparent(&workspace->layers.non_fullscreen->node, output->layers.tiling);
		wlr_scene_node_reparent(&workspace->layers.fullscreen->node, output->layers.fullscreen);

		for (int i = 0; i < workspace->current.windows->length; i++) {
			struct wsm_window *window = workspace->current.windows->items[i];
			wlr_scene_node_reparent(&window->scene_tree->node, global_server.scene->layers.windows);
			wlr_scene_node_set_enabled(&window->scene_tree->node,
				activated && (!window->view || window->view->enabled));
		}

		if (activated) {
			struct wsm_window *fs = workspace->current.fullscreen;
			wlr_scene_node_set_enabled(&workspace->layers.non_fullscreen->node, !fs);
			wlr_scene_node_set_enabled(&workspace->layers.fullscreen->node, fs);

			arrange_workspace_windows(workspace);

			wlr_scene_node_set_enabled(&output->layers.shell_background->node, !fs);
			wlr_scene_node_set_enabled(&output->layers.shell_bottom->node, !fs);
			wlr_scene_node_set_enabled(&output->layers.fullscreen->node, fs);

			if (fs) {
				wlr_scene_rect_set_size(output->fullscreen_background, width, height);
				wsm_arrange_fullscreen(workspace->layers.fullscreen, fs, workspace,
					width, height);
			} else {
				wlr_scene_node_set_enabled(&workspace->layers.non_fullscreen->node, true);
			}
		} else {
			wlr_scene_node_set_enabled(&workspace->layers.non_fullscreen->node, false);
			wlr_scene_node_set_enabled(&workspace->layers.fullscreen->node, false);

			disable_workspace(workspace);
		}
	}
}

void wsm_arrange_workspace_auto(struct wsm_workspace *workspace) {
	if (!workspace->output) {
		return;
	}

	struct wsm_output *output = workspace->output;
	struct wlr_box *area = &output->usable_area;
	wsm_log(WSM_DEBUG, "Usable area for ws: %dx%d@%d,%d",
		area->width, area->height, area->x, area->y);

	bool first_arrange = workspace->width == 0 && workspace->height == 0;
	struct wlr_box prev_box;
	workspace_get_box(workspace, &prev_box);

	double prev_x = workspace->x - workspace->current_gaps.left;
	double prev_y = workspace->y - workspace->current_gaps.top;
	workspace->width = area->width;
	workspace->height = area->height;
	workspace->x = output->lx + area->x;
	workspace->y = output->ly + area->y;

	double diff_x = workspace->x - prev_x;
	double diff_y = workspace->y - prev_y;
	if (!first_arrange && (diff_x != 0 || diff_y != 0)) {
		for (int i = 0; i < workspace->windows->length; ++i) {
			struct wsm_window *window = workspace->windows->items[i];
			struct wlr_box workspace_box;
			workspace_get_box(workspace, &workspace_box);
			window_fix_coordinates(window, &prev_box, &workspace_box);
			if (window->scratchpad) {
				struct wlr_box output_box;
				output_get_box(output, &output_box);
				window->transform = output_box;
			}
		}
	}

	workspace_add_gaps(workspace);
	node_set_dirty(&workspace->node);
	wsm_log(WSM_DEBUG, "Arranging workspace '%s' at %f, %f", workspace->name,
		workspace->x, workspace->y);
	if (workspace->fullscreen) {
		struct wsm_window *fs = workspace->fullscreen;
		fs->pending.x = output->lx;
		fs->pending.y = output->ly;
		fs->pending.width = output->width;
		fs->pending.height = output->height;
		wsm_arrange_window_auto(fs);
	} else {
		wsm_arrange_windows(workspace->windows);
	}
}

void wsm_arrange_layer_surface(struct wsm_output *output, const struct wlr_box *full_area,
		struct wlr_box *usable_area, struct wlr_scene_tree *tree) {
	struct wlr_scene_node *node;
	wl_list_for_each(node, &tree->children, link) {
		struct wsm_layer_surface *surface = wsm_scene_descriptor_try_get(node,
			WSM_SCENE_DESC_LAYER_SHELL);
		if (!surface) {
			continue;
		}

		if (!surface->scene->layer_surface->initialized) {
			continue;
		}

		wlr_scene_layer_surface_v1_configure(surface->scene, full_area, usable_area);
	}
}


void wsm_arrange_popups(struct wlr_scene_tree *popups) {
	struct wlr_scene_node *node;
	wl_list_for_each(node, &popups->children, link) {
		struct wsm_popup_desc *popup = wsm_scene_descriptor_try_get(node,
			WSM_SCENE_DESC_POPUP);

		int lx, ly;
		wlr_scene_node_coords(popup->relative, &lx, &ly);
		wlr_scene_node_set_position(node, lx, ly);
	}
}

void wsm_arrange_layers(struct wsm_output *output) {
	struct wlr_box usable_area = { 0 };
	wlr_output_effective_resolution(output->wlr_output,
		&usable_area.width, &usable_area.height);
	const struct wlr_box full_area = usable_area;

	wsm_arrange_layer_surface(output, &full_area, &usable_area, output->layers.shell_background);
	wsm_arrange_layer_surface(output, &full_area, &usable_area, output->layers.shell_bottom);
	wsm_arrange_layer_surface(output, &full_area, &usable_area, output->layers.shell_top);
	wsm_arrange_layer_surface(output, &full_area, &usable_area, output->layers.shell_overlay);

	if (!wlr_box_equal(&usable_area, &output->usable_area)) {
		wsm_log(WSM_DEBUG, "Usable area changed, rearranging output");
		output->usable_area = usable_area;
		wsm_arrange_output_auto(output);
	} else {
		wsm_arrange_popups(global_server.scene->layers.popup);
	}
}

void wsm_arrange_window_auto(struct wsm_window *window) {
	view_autoconfigure(window->view);
	node_set_dirty(&window->node);
}

static void apply_window_list_layout(struct wsm_list *windows, struct wlr_box *parent) {
	if (!windows->length) {
		return;
	}

	for (int i = 0; i < windows->length; ++i) {
		struct wsm_window *window = windows->items[i];
		int parent_offset = window->view ?  0 :
			window_titlebar_height() * windows->length;
		window->pending.x = parent->x;
		window->pending.y = parent->y + parent_offset;
		window->pending.width = parent->width;
		window->pending.height = parent->height - parent_offset;
	}
}

void wsm_arrange_window_list(struct wsm_list *windows, struct wlr_box *parent) {
	apply_window_list_layout(windows, parent);

	for (int i = 0; i < windows->length; ++i) {
		struct wsm_window *window = windows->items[i];
		wsm_arrange_window_auto(window);
	}
}

void wsm_arrange_windows(struct wsm_list *windows) {
	for (int i = 0; i < windows->length; ++i) {
		struct wsm_window *window = windows->items[i];
		wsm_arrange_window_auto(window);
	}
}

void window_arrange_title_bar_node(struct wsm_window *window) {
	enum alignment title_align = ALIGN_CENTER;
	int marks_buffer_width = 0;
	int width = window->title_width;
	int height = window_titlebar_height();
	int button_gap = 2;
	int button_size = MAX(height - global_config.titlebar_v_padding, 0);
	bool show_min_button = window->title_bar->min_button &&
		window->view && view_can_minimize(window->view);
	bool show_max_button = window->title_bar->max_button &&
		window->view && view_can_maximize(window->view);
	bool show_close_button = window->title_bar->close_button;
	int button_count = (show_min_button ? 1 : 0) + (show_max_button ? 1 : 0) +
		(show_close_button ? 1 : 0);
	int button_area_width = button_count > 0 ?
		button_size * button_count + button_gap * (button_count - 1) +
		global_config.titlebar_h_padding : 0;

	pixman_region32_t text_area;
	pixman_region32_init(&text_area);

	if (window->title_bar->title_text) {
		struct wsm_text_node *node = window->title_bar->title_text;

		int h_padding;
		if (title_align == ALIGN_RIGHT) {
			h_padding = width - global_config.titlebar_h_padding - node->width;
		} else if (title_align == ALIGN_CENTER) {
			h_padding = ((int)width - marks_buffer_width - node->width) >> 1;
		} else {
			h_padding = global_config.titlebar_h_padding;
		}

		h_padding = MAX(h_padding, 0);		
		int alloc_width = MIN((int) node->width,
			width - h_padding - global_config.titlebar_h_padding - button_area_width);
		alloc_width = MAX(alloc_width, 0);

		wsm_text_node_set_max_width(node, alloc_width);
		wlr_scene_node_set_position(node->node_wlr,
			h_padding, ((height - node->height) >> 1) + get_max_thickness(window->pending)
			* window->pending.border_top);
		pixman_region32_union_rect(&text_area, &text_area,
			node->node_wlr->x, node->node_wlr->y, alloc_width, node->height);
	}

	if (width <= 0 || height <= 0) {
		pixman_region32_fini(&text_area);
		return;
	}

	wlr_scene_node_set_position(&window->title_bar->background->node, 0, get_max_thickness(window->pending)
		* window->pending.border_top);
	wlr_scene_rect_set_size(window->title_bar->background, width, height);
	if (!window->title_bar->icon && window->view && window->current.border == B_NORMAL) {
		char *icon_path = window->view->app_icon_path;
		if (icon_path) {
			int size = height - global_config.titlebar_v_padding;
			window->title_bar->icon = wsm_image_node_create(window->title_bar->tree,
				size, size, icon_path, window->alpha);
		}
	}

	if (window->title_bar->icon) {
		int size = height - global_config.titlebar_v_padding;
		wsm_image_node_set_size(window->title_bar->icon, size, size);
		wlr_scene_node_set_position(window->title_bar->icon->node_wlr, ((height - size) >> 1),
			((height - size) >> 1) + get_max_thickness(window->pending)
			* window->pending.border_top);
	}

	if (window->title_bar->close_button) {
		int top = ((height - button_size) >> 1) + get_max_thickness(window->pending)
			* window->pending.border_top;
		int x = width - global_config.titlebar_h_padding - button_size;

		wlr_scene_node_set_enabled(&window->title_bar->min_button->tree->node,
			show_min_button);
		wlr_scene_node_set_enabled(&window->title_bar->max_button->tree->node,
			show_max_button);
		wlr_scene_node_set_enabled(&window->title_bar->close_button->tree->node,
			show_close_button);
		wsm_button_node_set_clickable(window->title_bar->min_button, show_min_button);
		wsm_button_node_set_clickable(window->title_bar->max_button, show_max_button);
		wsm_button_node_set_clickable(window->title_bar->close_button, show_close_button);

		if (show_close_button) {
			wsm_button_node_set_size(window->title_bar->close_button, button_size, button_size);
			wlr_scene_node_set_position(&window->title_bar->close_button->tree->node, x, top);
			x -= button_size + button_gap;
		}

		if (show_max_button) {
			wsm_button_node_set_size(window->title_bar->max_button, button_size, button_size);
			wlr_scene_node_set_position(&window->title_bar->max_button->tree->node, x, top);
			x -= button_size + button_gap;
		}

		if (show_min_button) {
			wsm_button_node_set_size(window->title_bar->min_button, button_size, button_size);
			wlr_scene_node_set_position(&window->title_bar->min_button->tree->node, x, top);
		}

		wlr_scene_node_raise_to_top(&window->title_bar->min_button->tree->node);
		wlr_scene_node_raise_to_top(&window->title_bar->max_button->tree->node);
		wlr_scene_node_raise_to_top(&window->title_bar->close_button->tree->node);
	}

	window_update(window);
}

void wsm_arrange_title_bar(struct wsm_window *window,
		int x, int y, int width, int height) {
	window_update(window);

	bool has_title_bar = height > 0;
	wlr_scene_node_set_enabled(&window->title_bar->tree->node, has_title_bar && window->view->enabled);
	if (!has_title_bar) {
		return;
	}

	wlr_scene_node_set_position(&window->title_bar->tree->node, x, y);

	window->title_width = width;
	window_arrange_title_bar_node(window);
}

void wsm_arrange_fullscreen(struct wlr_scene_tree *tree,
		struct wsm_window *fs, struct wsm_workspace *ws,
		int width, int height) {
	struct wlr_scene_node *fs_node;
	if (fs->view) {
		fs_node = &fs->view->scene_tree->node;
		wlr_scene_node_set_enabled(&fs->scene_tree->node, false);
	} else {
		fs_node = &fs->scene_tree->node;
		wsm_arrange_window_with_title_bar(fs, width, height, true, 0);
	}

	wlr_scene_node_reparent(fs_node, tree);
	wlr_scene_node_lower_to_bottom(fs_node);
	wlr_scene_node_set_position(fs_node, 0, 0);
}

void wsm_arrange_window_with_title_bar(struct wsm_window *window,
		int width, int height, bool title_bar, int gaps) {
	wlr_scene_node_set_enabled(&window->scene_tree->node, true);

	if (window->output_handler) {
		wlr_scene_buffer_set_dest_size(window->output_handler, width, height);
	}

	if (window->view) {
		int max_thickness = get_max_thickness(window->current);
		int border_top = window_titlebar_height() + max_thickness * window->current.border_top;
		int border_width = max_thickness;
		int sensing_width = max_thickness;

		if (window->current.border == B_NORMAL) {
			if (title_bar) {
				wsm_arrange_title_bar(window, max_thickness, 0, width - max_thickness * 2, border_top);
			} else {
				border_top = 0;
			}
		} else if (window->current.border == B_NONE) {
			window_update(window);
			border_top = 0;
			border_width = 0;
			sensing_width = 0;
		} else if (window->current.border == B_CSD) {
			border_top = 0;
			border_width = 0;
		} else {
			wsm_assert(false, "unreachable");
		}

		int border_left = window->current.border_left ? border_width : 0;
		int sensing_bottom = window->current.border_bottom ? sensing_width : 0;
		int sensing_left = window->current.border_left ? sensing_width : 0;
		int sensing_right = window->current.border_right ? sensing_width : 0;
		int sensing_top = window->current.border_top ? sensing_width : 0;

		wlr_scene_rect_set_size(window->sensing.top, width, sensing_top);
		wlr_scene_rect_set_size(window->sensing.bottom, width, sensing_bottom);
		wlr_scene_rect_set_size(window->sensing.left,
			sensing_left, height - sensing_bottom - sensing_top);
		wlr_scene_rect_set_size(window->sensing.right,
			sensing_right, height - sensing_bottom - sensing_top);

		wlr_scene_node_set_position(&window->sensing.top->node, 0, 0);
		wlr_scene_node_set_position(&window->sensing.bottom->node,
			0, height - sensing_bottom);
		wlr_scene_node_set_position(&window->sensing.left->node,
			0, sensing_top);
		wlr_scene_node_set_position(&window->sensing.right->node,
			width - sensing_right, sensing_top);

		wlr_scene_node_reparent(&window->view->scene_tree->node, window->content_tree);
		wlr_scene_node_set_position(&window->view->scene_tree->node,
			border_left, border_top);
	}
}

void arrange_windows_with_titlebar(struct wsm_list *windows,
		struct wsm_window *active, struct wlr_scene_tree *content, int width, int height, int gaps) {
	int title_bar_height = window_titlebar_height();

	struct wsm_window *first = windows->length == 1 ?
		((struct wsm_window *)windows->items[0]) : NULL;
	if (first && first->view &&
		first->current.border != B_NORMAL) {
		title_bar_height = 0;
	}

	int title_height = title_bar_height * windows->length;

	int y = 0;
	for (int i = 0; i < windows->length; i++) {
		struct wsm_window *window = windows->items[i];
		bool activated = window == active;

		wsm_arrange_title_bar(window, 0, y + title_height, width, title_bar_height);
		wlr_scene_node_set_enabled(&window->sensing.tree->node, activated);
		wlr_scene_node_set_position(&window->scene_tree->node, 0, title_height);
		wlr_scene_node_reparent(&window->scene_tree->node, content);

		if (activated) {
			wsm_arrange_window_with_title_bar(window, width, height - title_height,
				false, 0);
		} else {
			disable_window(window);
		}

		y += title_bar_height;
	}
}

void arrange_workspace_tiling(struct wsm_workspace *ws,
		int width, int height) {
}

void arrange_workspace_windows(struct wsm_workspace *ws) {
	for (int i = 0; i < ws->current.windows->length; i++) {
		struct wsm_window *window = ws->current.windows->items[i];
		struct wlr_scene_tree *layer = global_server.scene->layers.windows;

		if (window->current.fullscreen_mode != FULLSCREEN_NONE) {
			continue;
		}

		if (global_server.scene->fullscreen_global) {
			if (window_is_transient_for(window, global_server.scene->fullscreen_global)) {
				layer = global_server.scene->layers.fullscreen_global;
			}
		} else {
			for (int i = 0; i < global_server.scene->outputs->length; i++) {
				struct wsm_output *output = global_server.scene->outputs->items[i];
				struct wsm_workspace *active = output->current.active_workspace;

				if (active && active->fullscreen &&
					window_is_transient_for(window, active->fullscreen)) {
					layer = global_server.scene->layers.fullscreen;
				}
			}
		}

		wlr_scene_node_reparent(&window->scene_tree->node, layer);
		wlr_scene_node_set_position(&window->scene_tree->node,
			window->current.x, window->current.y);
		wlr_scene_node_set_enabled(&window->scene_tree->node,
			!window->view || window->view->enabled);
		wsm_arrange_window_with_title_bar(window, window->current.width, window->current.height,
			true, ws->gaps_inner);
	}
}
