#include "../config.h"
#include "wsm_window.h"
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
#include "wsm_transaction.h"
#include "wsm_titlebar.h"
#include "wsm_desktop.h"
#include "wsm_layer_shell.h"
#include "node/wsm_text_node.h"
#include "wsm_titlebar.h"
#include "wsm_xdg_decoration.h"
#include "node/wsm_node_descriptor.h"
#include "node/wsm_image_node.h"
#include "node/wsm_button_node.h"

#include <stdlib.h>
#include <float.h>

#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_xdg_decoration_v1.h>
#include <wlr/types/wlr_foreign_toplevel_management_v1.h>

struct wsm_output *window_find_output(struct wsm_window *window);

static struct border_colors *window_get_current_colors(
	struct wsm_window *window) {
	struct border_colors *colors;

	bool urgent = window->view && view_is_urgent(window->view);
	struct wsm_window *active_window = NULL;

	if (window->current.workspace) {
		active_window = window->current.workspace->current.focused_inactive_window;
	}

	if (urgent) {
		colors = &global_config.border_colors.urgent;
	} else if (window->current.focused ||
			(window->current.workspace && window->current.workspace->current.focused)) {
		colors = &global_config.border_colors.focused;
	} else if (window == active_window) {
		colors = &global_config.border_colors.focused_inactive;
	} else {
		colors = &global_config.border_colors.unfocused;
	}

	return colors;
}

static struct border_colors *window_get_titlebar_colors(
		struct wsm_window *window) {
	struct border_colors *colors = window_get_current_colors(window);
	static struct border_colors themed_colors;
	themed_colors = *colors;

	if (global_server.desktop_interface &&
			global_server.desktop_interface->color_scheme == Dark) {
		themed_colors.background[0] = 0.16f;
		themed_colors.background[1] = 0.17f;
		themed_colors.background[2] = 0.18f;
		themed_colors.background[3] = colors->background[3];
		themed_colors.text[0] = 0.96f;
		themed_colors.text[1] = 0.96f;
		themed_colors.text[2] = 0.96f;
		themed_colors.text[3] = colors->text[3];
	} else {
		themed_colors.background[0] = 0.94f;
		themed_colors.background[1] = 0.94f;
		themed_colors.background[2] = 0.95f;
		themed_colors.background[3] = colors->background[3];
		themed_colors.text[0] = 0.10f;
		themed_colors.text[1] = 0.11f;
		themed_colors.text[2] = 0.12f;
		themed_colors.text[3] = colors->text[3];
	}

	return &themed_colors;
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

static char *titlebar_icon_path(const char *names[]) {
	struct wsm_desktop_interface *desktop = global_server.desktop_interface;
	for (size_t i = 0; names[i] != NULL; ++i) {
		char *path = find_icon_file_frome_theme(desktop, names[i]);
		if (path && path[0] != '\0') {
			return path;
		}
		free(path);
	}
	return NULL;
}

static bool titlebar_load_button_icon(struct wsm_button_node *button,
		const char *names[]) {
	char *path = titlebar_icon_path(names);
	if (!path) {
		wsm_log(WSM_ERROR, "Could not find titlebar button icon");
		return false;
	}

	wsm_button_node_load_icon(button, path);
	free(path);
	return true;
}

static void titlebar_update_button_icons(struct wsm_window *window) {
	if (!window->title_bar || !window->title_bar->max_button) {
		return;
	}
	if (window->title_bar->button_icons_loaded &&
			window->title_bar->button_icons_maximized == window->maximized) {
		return;
	}

	static const char *minimize_icons[] = {
		"window-minimize-symbolic",
		"window-minimize",
		"window-minimize-pip",
		NULL,
	};
	static const char *maximize_icons[] = {
		"window-maximize-symbolic",
		"window-maximize",
		"preferences-system-windows-effect-maximize",
		NULL,
	};
	static const char *restore_icons[] = {
		"window-restore-symbolic",
		"window-restore",
		"view-restore",
		"window-restore-pip",
		NULL,
	};
	static const char *close_icons[] = {
		"window-close",
		"window-close-symbolic",
		"view-close",
		NULL,
	};

	bool loaded = true;
	if (!window->title_bar->button_icons_loaded) {
		loaded &= titlebar_load_button_icon(window->title_bar->min_button, minimize_icons);
		loaded &= titlebar_load_button_icon(window->title_bar->close_button, close_icons);
	}
	loaded &= titlebar_load_button_icon(window->title_bar->max_button,
		window->maximized ? restore_icons : maximize_icons);

	if (loaded) {
		window->title_bar->button_icons_loaded = true;
		window->title_bar->button_icons_maximized = window->maximized;
	}
}

static void handle_min_button_clicked(struct wl_listener *listener, void *data) {
	struct wsm_titlebar *titlebar =
		wl_container_of(listener, titlebar, min_button_clicked);
	if (titlebar->window && titlebar->window->view &&
			view_can_minimize(titlebar->window->view)) {
		window_minimize(titlebar->window);
	}
}

static void titlebar_toggle_maximized(struct wsm_titlebar *titlebar) {
	if (titlebar->window && titlebar->window->view &&
			view_can_maximize(titlebar->window->view)) {
		window_set_maximized(titlebar->window,
			!titlebar->window->maximized);
		transaction_commit_dirty();
	}
}

static void handle_max_button_clicked(struct wl_listener *listener, void *data) {
	struct wsm_titlebar *titlebar =
		wl_container_of(listener, titlebar, max_button_clicked);
	titlebar_toggle_maximized(titlebar);
}

static void handle_titlebar_double_clicked(struct wl_listener *listener,
		void *data) {
	struct wsm_titlebar *titlebar =
		wl_container_of(listener, titlebar, double_clicked);
	titlebar_toggle_maximized(titlebar);
}

static void handle_close_button_clicked(struct wl_listener *listener, void *data) {
	struct wsm_titlebar *titlebar =
		wl_container_of(listener, titlebar, close_button_clicked);
	if (titlebar->window && titlebar->window->view) {
		view_close(titlebar->window->view);
	}
}

static void create_titlebar_buttons(struct wsm_window *window, bool *failed) {
	if (*failed || !window->view) {
		return;
	}

	float hover_color[3] = {0.f, 0.f, 0.f};
	static const char *minimize_icons[] = {
		"window-minimize-symbolic",
		"window-minimize",
		"window-minimize-pip",
		NULL,
	};
	static const char *maximize_icons[] = {
		"window-maximize-symbolic",
		"window-maximize",
		"preferences-system-windows-effect-maximize",
		NULL,
	};
	static const char *close_icons[] = {
		"window-close",
		"window-close-symbolic",
		"view-close",
		NULL,
	};
	char *min_path = titlebar_icon_path(minimize_icons);
	char *max_path = titlebar_icon_path(maximize_icons);
	char *close_path = titlebar_icon_path(close_icons);
	if (!min_path || !max_path || !close_path) {
		wsm_log(WSM_ERROR, "Could not find titlebar button system icons");
		*failed = true;
		goto cleanup;
	}

	struct wsm_titlebar *titlebar = window->title_bar;
	titlebar->min_button = wsm_button_node_create(titlebar->tree, 0, 0,
		min_path, hover_color);
	titlebar->max_button = wsm_button_node_create(titlebar->tree, 0, 0,
		max_path, hover_color);
	titlebar->close_button = wsm_button_node_create(titlebar->tree, 0, 0,
		close_path, hover_color);

	if (!titlebar->min_button || !titlebar->max_button || !titlebar->close_button) {
		*failed = true;
		goto cleanup;
	}
	titlebar->button_icons_loaded = !window->maximized;
	titlebar->button_icons_maximized = false;

	titlebar->min_button_clicked.notify = handle_min_button_clicked;
	titlebar->max_button_clicked.notify = handle_max_button_clicked;
	titlebar->close_button_clicked.notify = handle_close_button_clicked;
	titlebar->double_clicked.notify = handle_titlebar_double_clicked;
	wl_signal_add(&titlebar->min_button->events.clicked,
		&titlebar->min_button_clicked);
	wl_signal_add(&titlebar->max_button->events.clicked,
		&titlebar->max_button_clicked);
	wl_signal_add(&titlebar->close_button->events.clicked,
		&titlebar->close_button_clicked);
	wl_signal_add(&titlebar->events.double_click,
		&titlebar->double_clicked);

cleanup:
	free(min_path);
	free(max_path);
	free(close_path);
}

static void handle_output_enter(struct wl_listener *listener, void *data) {
	struct wsm_window *window = wl_container_of(
		listener, window, output_enter);
	struct wlr_scene_output *output = data;

	if (window->view->foreign_toplevel) {
		wlr_foreign_toplevel_handle_v1_output_enter(
			window->view->foreign_toplevel, output->output);
	}
}

static void handle_output_leave(struct wl_listener *listener, void *data) {
	struct wsm_window *window = wl_container_of(
		listener, window, output_leave);
	struct wlr_scene_output *output = data;

	if (window->view->foreign_toplevel) {
		wlr_foreign_toplevel_handle_v1_output_leave(
			window->view->foreign_toplevel, output->output);
	}
}

static bool handle_point_accepts_input(
	struct wlr_scene_buffer *buffer, double *x, double *y) {
	return false;
}

struct wsm_window *window_create(struct wsm_view *view) {
	struct wsm_window *c = calloc(1, sizeof(struct wsm_window));
	if (!c) {
		wsm_log(WSM_ERROR, "Could not create wsm_window: allocation failed!");
		return NULL;
	}

	node_init(&c->node, N_WINDOW, c);
	c->view = view;

	bool failed = false;
	c->scene_tree = alloc_scene_tree(global_server.scene->staging, &failed);
	c->title_bar = wsm_titlebar_create();
	if (!c->title_bar) {
		failed = true;
	} else {
		c->title_bar->window = c;
		c->title_bar->tree = alloc_scene_tree(c->scene_tree, &failed);
		c->title_bar->background = alloc_rect_node(c->title_bar->tree, &failed);
		create_titlebar_buttons(c, &failed);
	}
	c->sensing.tree = alloc_scene_tree(c->scene_tree, &failed);
	c->content_tree = alloc_scene_tree(c->sensing.tree, &failed);

	if (view) {
		c->sensing.top = alloc_rect_node(c->sensing.tree, &failed);
		c->sensing.bottom = alloc_rect_node(c->sensing.tree, &failed);
		c->sensing.left = alloc_rect_node(c->sensing.tree, &failed);
		c->sensing.right = alloc_rect_node(c->sensing.tree, &failed);

		c->output_handler = wlr_scene_buffer_create(c->sensing.tree, NULL);
		if (!c->output_handler) {
			wsm_log(WSM_ERROR, "Could not create wlr_scene_buffer for window scene node: allocation failed!");
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
		WSM_SCENE_DESC_WINDOW, c)) {
		failed = true;
	}

	if (failed) {
		wlr_scene_node_destroy(&c->scene_tree->node);
		free(c);
		return NULL;
	}

	c->alpha = 1.0f;

	wl_signal_emit_mutable(&global_server.scene->events.new_node, &c->node);
	window_update(c);

	return c;
}

void window_destroy(struct wsm_window *window) {
	if (!wsm_assert(window->node.destroying,
		"Tried to free window which wasn't marked as destroying")) {
		return;
	}
	if (!wsm_assert(window->node.ntxnrefs == 0, "Tried to free window "
		"which is still referenced by transactions")) {
		return;
	}
	free(window->title);
	free(window->formatted_title);

	if (window->view && window->view->window == window) {
		window->view->window = NULL;
		wlr_scene_node_destroy(&window->output_handler->node);
		if (window->view->destroying) {
			view_destroy(window->view);
		}
	}

	scene_node_disown_children(window->content_tree);
	if (window->title_bar) {
		wsm_titlebar_destroy(window->title_bar);
		window->title_bar = NULL;
	}
	wlr_scene_node_destroy(&window->scene_tree->node);
	free(window);
}

void window_begin_destroy(struct wsm_window *window) {
	if (window->pending.fullscreen_mode == FULLSCREEN_WORKSPACE && window->pending.workspace) {
		window->pending.workspace->fullscreen = NULL;
	}
	if (window->scratchpad && window->pending.fullscreen_mode == FULLSCREEN_GLOBAL) {
		window_fullscreen_disable(window);
	}

	wl_signal_emit_mutable(&window->node.events.destroy, &window->node);

	window_end_mouse_operation(window);
	window->node.destroying = true;
	node_set_dirty(&window->node);
	if (window->scratchpad) {
		root_scratchpad_remove_window(window);
	}

	if (window->pending.fullscreen_mode == FULLSCREEN_GLOBAL) {
		window_fullscreen_disable(window);
	}

	if (window->pending.workspace) {
		window_detach(window);
	}
}

void window_get_box(struct wsm_window *window, struct wlr_box *box) {
	box->x = window->pending.x;
	box->y = window->pending.y;
	box->width = window->pending.width;
	box->height = window->pending.height;
}

bool window_is_fullscreen(struct wsm_window *window) {
	return window->pending.fullscreen_mode != FULLSCREEN_NONE;
}

bool window_is_scratchpad_hidden(struct wsm_window *window) {
	return window->scratchpad && !window->pending.workspace;
}

bool window_is_managed(struct wsm_window *window) {
	return window->pending.workspace || window->scratchpad;
}

size_t window_titlebar_height(void) {
	return global_config.font_height + global_config.titlebar_v_padding * 2;
}

void window_raise(struct wsm_window *window) {
	if (window_is_managed(window) && window->pending.workspace) {
		wlr_scene_node_raise_to_top(&window->scene_tree->node);
		wsm_list_move_to_end(window->pending.workspace->windows, window);
		node_set_dirty(&window->pending.workspace->node);
	}
}

static void window_set_content_geometry_from_box(struct wsm_window *window) {
	int border_width = 0;
	int title_height = 0;

	if (window->pending.border != B_CSD && !window->pending.fullscreen_mode) {
		border_width = get_max_thickness(window->pending) *
			(window->pending.border != B_NONE);
		title_height = window->pending.border == B_NORMAL ?
			(int)window_titlebar_height() : border_width;
	}

	int border_top = window->pending.border_top ? border_width : 0;
	int border_bottom = window->pending.border_bottom ? border_width : 0;
	int border_left = window->pending.border_left ? border_width : 0;
	int border_right = window->pending.border_right ? border_width : 0;
	int top = title_height + border_top;

	window->pending.content_x = window->pending.x + border_left;
	window->pending.content_y = window->pending.y + top;
	window->pending.content_width = MAX(window->pending.width - border_left - border_right, 0);
	window->pending.content_height = MAX(window->pending.height - top - border_bottom, 0);
}

static void shrink_area_for_panel(struct wlr_box *area, struct wlr_box panel,
		uint32_t anchor) {
	struct wlr_box intersection;
	if (!wlr_box_intersection(&intersection, area, &panel)) {
		return;
	}

	bool horizontal = intersection.width >= area->width;
	bool vertical = intersection.height >= area->height;
	if (horizontal && (anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP)) {
		int delta = panel.y + panel.height - area->y;
		if (delta > 0 && delta < area->height) {
			area->y += delta;
			area->height -= delta;
		}
	} else if (horizontal && (anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM)) {
		int delta = area->y + area->height - panel.y;
		if (delta > 0 && delta < area->height) {
			area->height -= delta;
		}
	} else if (vertical && (anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT)) {
		int delta = panel.x + panel.width - area->x;
		if (delta > 0 && delta < area->width) {
			area->x += delta;
			area->width -= delta;
		}
	} else if (vertical && (anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT)) {
		int delta = area->x + area->width - panel.x;
		if (delta > 0 && delta < area->width) {
			area->width -= delta;
		}
	}
}

static void shrink_area_for_panel_layer(struct wlr_scene_tree *tree,
		struct wlr_box *area) {
	struct wlr_scene_node *node;
	wl_list_for_each(node, &tree->children, link) {
		struct wsm_layer_surface *surface = wsm_scene_descriptor_try_get(node,
			WSM_SCENE_DESC_LAYER_SHELL);
		if (!surface || !surface->panel || !surface->mapped) {
			continue;
		}

		struct wlr_layer_surface_v1 *layer_surface = surface->layer_surface_wlr;
		if (!layer_surface->surface || !layer_surface->surface->mapped) {
			continue;
		}

		int lx = 0, ly = 0;
		if (!wlr_scene_node_coords(&surface->scene->tree->node, &lx, &ly)) {
			continue;
		}

		struct wlr_box panel = {
			.x = lx,
			.y = ly,
			.width = layer_surface->current.actual_width,
			.height = layer_surface->current.actual_height,
		};
		shrink_area_for_panel(area, panel, layer_surface->current.anchor);
	}
}

static struct wlr_box window_maximize_area(struct wsm_window *window) {
	struct wsm_output *output = window_find_output(window);
	struct wlr_box area = {0};
	if (!output) {
		return area;
	}

	output_get_box(output, &area);
	shrink_area_for_panel_layer(output->layers.shell_background, &area);
	shrink_area_for_panel_layer(output->layers.shell_bottom, &area);
	shrink_area_for_panel_layer(output->layers.shell_top, &area);
	shrink_area_for_panel_layer(output->layers.shell_overlay, &area);
	return area;
}

void window_set_maximized(struct wsm_window *window, bool maximized) {
	if (!window->view || !window_is_managed(window) || !view_can_maximize(window->view)) {
		return;
	}

	if (window->maximized == maximized) {
		return;
	}

	if (maximized) {
		struct wlr_box area = window_maximize_area(window);
		if (wlr_box_empty(&area)) {
			return;
		}

		window->saved_maximized_x = window->pending.x;
		window->saved_maximized_y = window->pending.y;
		window->saved_maximized_width = window->pending.width;
		window->saved_maximized_height = window->pending.height;
		window->saved_maximized_content_x = window->pending.content_x;
		window->saved_maximized_content_y = window->pending.content_y;
		window->saved_maximized_content_width = window->pending.content_width;
		window->saved_maximized_content_height = window->pending.content_height;

		window->pending.x = area.x;
		window->pending.y = area.y;
		window->pending.width = area.width;
		window->pending.height = area.height;
		window_set_content_geometry_from_box(window);
	} else {
		window->pending.x = window->saved_maximized_x;
		window->pending.y = window->saved_maximized_y;
		window->pending.width = window->saved_maximized_width;
		window->pending.height = window->saved_maximized_height;
		window->pending.content_x = window->saved_maximized_content_x;
		window->pending.content_y = window->saved_maximized_content_y;
		window->pending.content_width = window->saved_maximized_content_width;
		window->pending.content_height = window->saved_maximized_content_height;
	}

	window->maximized = maximized;
	view_maximize(window->view, maximized);
	window_raise(window);
	node_set_dirty(&window->node);
	if (window->pending.workspace) {
		node_set_dirty(&window->pending.workspace->node);
	}
	window_end_mouse_operation(window);
}

void window_minimize(struct wsm_window *window) {
	if (!window->view || !view_can_minimize(window->view)) {
		return;
	}

	if (window->maximized) {
		window_set_maximized(window, false);
	}
	view_minimize(window->view, true);
	transaction_commit_dirty();
}

static void set_fullscreen(struct wsm_window *window, bool enable) {
	if (!window->view) {
		return;
	}
	if (window->view->impl->set_fullscreen) {
		window->view->impl->set_fullscreen(window->view, enable);
		if (window->view->foreign_toplevel) {
			wlr_foreign_toplevel_handle_v1_set_fullscreen(
				window->view->foreign_toplevel, enable);
		}
	}
}

struct wsm_output *window_find_output(struct wsm_window *window) {
	double center_x = window->pending.x + window->pending.width / 2;
	double center_y = window->pending.y + window->pending.height / 2;
	struct wsm_output *closest_output = NULL;
	double closest_distance = DBL_MAX;
	for (int i = 0; i < global_server.scene->outputs->length; ++i) {
		struct wsm_output *output = global_server.scene->outputs->items[i];
		struct wlr_box output_box;
		double closest_x, closest_y;
		output_get_box(output, &output_box);
		wlr_box_closest_point(&output_box, center_x, center_y,
			&closest_x, &closest_y);
		if (center_x == closest_x && center_y == closest_y) {
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

void window_fullscreen_disable(struct wsm_window *window) {
	if (!wsm_assert(window->pending.fullscreen_mode != FULLSCREEN_NONE,
		"Expected a fullscreen window")) {
		return;
	}
	set_fullscreen(window, false);

	if (window_is_managed(window)) {
		window->pending.x = window->saved_x;
		window->pending.y = window->saved_y;
		window->pending.width = window->saved_width;
		window->pending.height = window->saved_height;
	}

	if (window->pending.fullscreen_mode == FULLSCREEN_WORKSPACE) {
		if (window->pending.workspace) {
			window->pending.workspace->fullscreen = NULL;
			if (window_is_managed(window)) {
				struct wsm_output *output =
						window_find_output(window);
				if (window->pending.workspace->output != output) {
					window_move_to_center(window);
				}
			}
		}
	} else {
		global_server.scene->fullscreen_global = NULL;
	}

	if (window_is_managed(window) && (window->pending.width == 0 || window->pending.height == 0)) {
		window_resize_and_center(window);
	}

	window->pending.fullscreen_mode = FULLSCREEN_NONE;
	window_end_mouse_operation(window);

	if (window->scratchpad) {
		struct wsm_seat *seat;
		wl_list_for_each(seat, &global_server.input_manager->seats, link) {
			struct wsm_window *focus = seat_get_focused_window(seat);
			if (focus == window) {
				seat_set_focus(seat,
					seat_get_focus_inactive(seat, &global_server.scene->node));
			}
		}
	}
}

void window_move_to(struct wsm_window *window, double lx, double ly) {
	if (!wsm_assert(window_is_managed(window),
			"Expected a managed window")) {
		return;
	}
	window_translate(window, lx - window->pending.x, ly - window->pending.y);
	if (window_is_scratchpad_hidden(window)) {
		return;
	}
	struct wsm_workspace *old_workspace = window->pending.workspace;
	struct wsm_output *new_output = window_find_output(window);
	if (!wsm_assert(new_output, "Unable to find any output")) {
		return;
	}
	struct wsm_workspace *new_workspace =
			output_get_active_workspace(new_output);
	if (new_workspace && old_workspace != new_workspace) {
		window_detach(window);
		workspace_add_window(new_workspace, window);
		wsm_arrange_workspace_auto(old_workspace);
		wsm_arrange_workspace_auto(new_workspace);
		if (window->scratchpad) {
			struct wlr_box output_box;
			output_get_box(new_output, &output_box);
			window->transform = output_box;
		}
		workspace_detect_urgent(old_workspace);
		workspace_detect_urgent(new_workspace);
	}
}

void window_move_to_center(struct wsm_window *window) {
	if (!wsm_assert(window_is_managed(window),
				"Expected a managed window")) {
		return;
	}
	struct wsm_workspace *ws = window->pending.workspace;
	double new_lx = ws->x + (ws->width - window->pending.width) / 2;
	double new_ly = ws->y + (ws->height - window->pending.height) / 2;
	window_translate(window, new_lx - window->pending.x, new_ly - window->pending.y);
}

void window_translate(struct wsm_window *window,
		double x_amount, double y_amount) {
	window->pending.x += x_amount;
	window->pending.y += y_amount;
	window->pending.content_x += x_amount;
	window->pending.content_y += y_amount;

	node_set_dirty(&window->node);
}

static void window_natural_resize(struct wsm_window *window) {
	int min_width = 100, max_width = INT_MAX, min_height = 100, max_height = INT_MAX;

	if (!window->view) {
		window->pending.width = fmax(min_width, fmin(window->pending.width, max_width));
		window->pending.height = fmax(min_height, fmin(window->pending.height, max_height));
	} else {
		struct wsm_view *view = window->view;
		window->pending.content_width =
			fmax(min_width, fmin(view->natural_width, max_width));
		window->pending.content_height =
			fmax(min_height, fmin(view->natural_height, max_height));
		window_set_geometry_from_content(window);
	}
}

void window_resize_and_center(struct wsm_window *window) {
	struct wsm_workspace *ws = window->pending.workspace;
	if (!ws) {
		window_natural_resize(window);
		return;
	}

	struct wlr_box ob;
	wlr_output_layout_get_box(global_server.scene->output_layout, ws->output->wlr_output, &ob);
	if (wlr_box_empty(&ob)) {
		// On NOOP output. Will be called again when moved to an output
		window->pending.x = 0;
		window->pending.y = 0;
		window->pending.width = 0;
		window->pending.height = 0;
		return;
	}

	window_natural_resize(window);
	if (!window->view) {
		if (window->pending.width > ws->width || window->pending.height > ws->height) {
			window->pending.x = ob.x + (ob.width - window->pending.width) / 2;
			window->pending.y = ob.y + (ob.height - window->pending.height) / 2;
		} else {
			window->pending.x = ws->x + (ws->width - window->pending.width) / 2;
			window->pending.y = ws->y + (ws->height - window->pending.height) / 2;
		}
	} else {
		if (window->pending.content_width > ws->width
			|| window->pending.content_height > ws->height) {
			window->pending.content_x = ob.x + (ob.width - window->pending.content_width) / 2;
			window->pending.content_y = ob.y + (ob.height - window->pending.content_height) / 2;
		} else {
			window->pending.content_x = ws->x + (ws->width - window->pending.content_width) / 2;
			window->pending.content_y = ws->y + (ws->height - window->pending.content_height) / 2;
		}

		window->pending.border_top = window->pending.border_bottom = true;
		window->pending.border_left = window->pending.border_right = true;
		window_set_geometry_from_content(window);
	}
}

void window_set_geometry_from_content(struct wsm_window *window) {
	if (!wsm_assert(window->view, "Expected a view")) {
		return;
	}
	if (!wsm_assert(window_is_managed(window), "Expected a managed window")) {
		return;
	}
	size_t border_width = 0;
	size_t top = 0;

	if (window->pending.border != B_CSD && !window->pending.fullscreen_mode) {
		border_width = get_max_thickness(window->pending) * (window->pending.border != B_NONE);
		top = window->pending.border == B_NORMAL ?
			window_titlebar_height() : border_width;
	}

	window->pending.x = window->pending.content_x - border_width;
	window->pending.y = window->pending.content_y - top - border_width;
	window->pending.width = window->pending.content_width + border_width * 2;
	window->pending.height = top + window->pending.content_height + border_width * 2;
	node_set_dirty(&window->node);
}

void window_end_mouse_operation(struct wsm_window *window) {
	struct wsm_seat *seat;
	wl_list_for_each(seat, &global_server.input_manager->seats, link) {
		seatop_unref(seat, window);
	}
}

void window_detach(struct wsm_window *window) {
	if (window->pending.fullscreen_mode == FULLSCREEN_WORKSPACE) {
		window->pending.workspace->fullscreen = NULL;
	}
	if (window->pending.fullscreen_mode == FULLSCREEN_GLOBAL) {
		global_server.scene->fullscreen_global = NULL;
	}

	struct wsm_workspace *old_workspace = window->pending.workspace;
	if (old_workspace) {
		int index = wsm_list_find(old_workspace->windows, window);
		if (index != -1) {
			wsm_list_delete(old_workspace->windows, index);
		}
	}
	window->pending.workspace = NULL;

	if (old_workspace) {
		workspace_update_representation(old_workspace);
		node_set_dirty(&old_workspace->node);
	}
	node_set_dirty(&window->node);
}

size_t window_build_representation(struct wsm_list *windows, char *buffer) {
	size_t len = 2;
	lenient_strcat(buffer, "D[");
	for (int i = 0; i < windows->length; ++i) {
		if (i != 0) {
			++len;
			lenient_strcat(buffer, " ");
		}
		struct wsm_window *window = windows->items[i];
		const char *identifier = NULL;
		if (window->view) {
			identifier = view_get_class(window->view);
			if (!identifier) {
				identifier = view_get_app_id(window->view);
			}
		} else {
			identifier = window->formatted_title;
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

void window_update_title_bar(struct wsm_window *window) {
	if (!window->formatted_title) {
		return;
	}

	struct border_colors *colors = window_get_titlebar_colors(window);

	if (window->title_bar->title_text) {
		wlr_scene_node_destroy(window->title_bar->title_text->node_wlr);
		window->title_bar->title_text = NULL;
	}

	window->title_bar->title_text = wsm_text_node_create(window->title_bar->tree,
		global_server.desktop_interface, window->formatted_title, colors->text, false);
	
	window_arrange_title_bar_node(window);
}

void window_handle_fullscreen_reparent(struct wsm_window *window) {
	if (window->pending.fullscreen_mode != FULLSCREEN_WORKSPACE || !window->pending.workspace ||
		window->pending.workspace->fullscreen == window) {
		return;
	}
	if (window->pending.workspace->fullscreen) {
		window_fullscreen_disable(window->pending.workspace->fullscreen);
	}
	window->pending.workspace->fullscreen = window;

	wsm_arrange_workspace_auto(window->pending.workspace);
}

void window_fix_coordinates(struct wsm_window *window,
		struct wlr_box *old, struct wlr_box *new) {
	if (!old->width || !old->height) {
		window_move_to_center(window);
	} else {
		int rel_x = window->pending.x - old->x + (window->pending.width / 2);
		int rel_y = window->pending.y - old->y + (window->pending.height / 2);

		window->pending.x = new->x + (double)(rel_x * new->width) / old->width - (window->pending.width / 2);
		window->pending.y = new->y + (double)(rel_y * new->height) / old->height - (window->pending.height / 2);

		wsm_log(WSM_DEBUG, "Transformed window %p to coords (%f, %f)", window, window->pending.x, window->pending.y);
	}
}

static void window_fullscreen_workspace(struct wsm_window *window) {
	if (!wsm_assert(window->pending.fullscreen_mode == FULLSCREEN_NONE,
			"Expected a non-fullscreen window")) {
		return;
	}
	set_fullscreen(window, true);
	window->pending.fullscreen_mode = FULLSCREEN_WORKSPACE;

	window->saved_x = window->pending.x;
	window->saved_y = window->pending.y;
	window->saved_width = window->pending.width;
	window->saved_height = window->pending.height;

	if (window->pending.workspace) {
		window->pending.workspace->fullscreen = window;
		struct wsm_seat *seat;
		struct wsm_workspace *focus_ws;
		wl_list_for_each(seat, &global_server.input_manager->seats, link) {
			focus_ws = seat_get_focused_workspace(seat);
			if (focus_ws == window->pending.workspace) {
				seat_set_focus_window(seat, window);
			} else {
				struct wsm_node *focus =
					seat_get_focus_inactive(seat, &global_server.scene->node);
				seat_set_raw_focus(seat, &window->node);
				seat_set_raw_focus(seat, focus);
			}
		}
	}
	
	window_end_mouse_operation(window);
}

static void window_fullscreen_global(struct wsm_window *window) {
	if (!wsm_assert(window->pending.fullscreen_mode == FULLSCREEN_NONE,
		"Expected a non-fullscreen window")) {
		return;
	}
	set_fullscreen(window, true);

	global_server.scene->fullscreen_global = window;
	window->saved_x = window->pending.x;
	window->saved_y = window->pending.y;
	window->saved_width = window->pending.width;
	window->saved_height = window->pending.height;

	struct wsm_seat *seat;
	wl_list_for_each(seat, &global_server.input_manager->seats, link) {
		struct wsm_window *focus = seat_get_focused_window(seat);
		if (focus && focus != window) {
			seat_set_focus_window(seat, window);
		}
	}

	window->pending.fullscreen_mode = FULLSCREEN_GLOBAL;
	window_end_mouse_operation(window);
}

void window_set_fullscreen(struct wsm_window *window, enum wsm_fullscreen_mode mode) {
	if (window->pending.fullscreen_mode == mode) {
		return;
	}

	switch (mode) {
	case FULLSCREEN_NONE:
		window_fullscreen_disable(window);
		break;
	case FULLSCREEN_WORKSPACE:
		if (global_server.scene->fullscreen_global) {
			window_fullscreen_disable(global_server.scene->fullscreen_global);
		}
		if (window->pending.workspace && window->pending.workspace->fullscreen) {
			window_fullscreen_disable(window->pending.workspace->fullscreen);
		}
		window_fullscreen_workspace(window);
		break;
	case FULLSCREEN_GLOBAL:
		if (global_server.scene->fullscreen_global) {
			window_fullscreen_disable(global_server.scene->fullscreen_global);
		}
		if (window->pending.fullscreen_mode == FULLSCREEN_WORKSPACE) {
			window_fullscreen_disable(window);
		}
		window_fullscreen_global(window);
		break;
	}
}

void root_scratchpad_remove_window(struct wsm_window *window) {
	if (!wsm_assert(window->scratchpad, "Window is not in scratchpad")) {
		return;
	}
	window->scratchpad = false;
	int index = wsm_list_find(global_server.scene->scratchpad, window);
	if (index != -1) {
		wsm_list_delete(global_server.scene->scratchpad, index);
	}
}

void window_set_default_size(struct wsm_window *window) {
	if (!wsm_assert(window->pending.workspace, "Expected a window on a workspace")) {
		return;
	}

	int min_width = 75, max_width = INT_MAX, min_height = 50, max_height = INT_MAX;
	struct wlr_box box;
	workspace_get_box(window->pending.workspace, &box);

	double width = fmax(min_width, fmin(box.width * 0.5, max_width));
	double height = fmax(min_height, fmin(box.height * 0.75, max_height));
	if (!window->view) {
		window->pending.width = width;
		window->pending.height = height;
	} else {
		window->pending.content_width = width;
		window->pending.content_height = height;
		window_set_geometry_from_content(window);
	}
}

bool window_is_sticky(struct wsm_window *window) {
	return window->is_sticky && window_is_managed(window);
}

bool window_is_transient_for(struct wsm_window *child,
								struct wsm_window *ancestor) {
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

void window_update(struct wsm_window *window) {
	struct border_colors *colors = window_get_titlebar_colors(window);
	float alpha = window->alpha;
	scene_rect_set_color(window->title_bar->background, colors->background, alpha);

	if (window->view) {
		wlr_scene_rect_set_color(window->sensing.top, global_config.sensing_border_color);
		wlr_scene_rect_set_color(window->sensing.bottom, global_config.sensing_border_color);
		wlr_scene_rect_set_color(window->sensing.left, global_config.sensing_border_color);
		wlr_scene_rect_set_color(window->sensing.right, global_config.sensing_border_color);
	}

	if (window->title_bar->title_text) {
		wsm_text_node_set_color(window->title_bar->title_text, colors->text);
		wsm_text_node_set_background(window->title_bar->title_text, global_config.text_background_color);
	}

	if (window->title_bar->close_button) {
		bool can_minimize = window->view && view_can_minimize(window->view);
		bool can_maximize = window->view && view_can_maximize(window->view);
		float hover_color[3] = {
			colors->indicator[0],
			colors->indicator[1],
			colors->indicator[2],
		};
		wlr_scene_node_set_enabled(&window->title_bar->min_button->tree->node,
			can_minimize);
		wlr_scene_node_set_enabled(&window->title_bar->max_button->tree->node,
			can_maximize);
		wsm_button_node_set_clickable(window->title_bar->min_button, can_minimize);
		wsm_button_node_set_clickable(window->title_bar->max_button, can_maximize);
		wsm_button_node_set_alpha(window->title_bar->min_button, alpha);
		wsm_button_node_set_alpha(window->title_bar->max_button, alpha);
		wsm_button_node_set_alpha(window->title_bar->close_button, alpha);
		titlebar_update_button_icons(window);
		wsm_button_node_set_color(window->title_bar->min_button, hover_color);
		wsm_button_node_set_color(window->title_bar->max_button, hover_color);
		wsm_button_node_set_color(window->title_bar->close_button, hover_color);
	}
}

void window_update_itself_and_parents(struct wsm_window *window) {
	window_update(window);
}

void window_set_resizing(struct wsm_window *window, bool resizing) {
	if (!window) {
		return;
	}

	if (window->view && window->view->impl->set_resizing) {
		window->view->impl->set_resizing(window->view, resizing);
	}
}

void window_calculate_constraints(int *min_width, int *max_width,
		int *min_height, int *max_height) {
	if (global_config.window_minimum_width == -1) { // no minimum
		*min_width = 0;
	} else if (global_config.window_minimum_width == 0) { // automatic
		*min_width = 75;
	} else {
		*min_width = global_config.window_minimum_width;
	}

	if (global_config.window_minimum_height == -1) { // no minimum
		*min_height = 0;
	} else if (global_config.window_minimum_height == 0) { // automatic
		*min_height = 50;
	} else {
		*min_height = global_config.window_minimum_height;
	}

	struct wlr_box box;
	wlr_output_layout_get_box(global_server.scene->output_layout, NULL, &box);

	if (global_config.window_maximum_width == -1) { // no maximum
		*max_width = INT_MAX;
	} else if (global_config.window_maximum_width == 0) { // automatic
		*max_width = box.width;
	} else {
		*max_width = global_config.window_maximum_width;
	}

	if (global_config.window_maximum_height == -1) { // no maximum
		*max_height = INT_MAX;
	} else if (global_config.window_maximum_height == 0) { // automatic
		*max_height = box.height;
	} else {
		*max_height = global_config.window_maximum_height;
	}
}

struct wsm_window *window_obstructing_fullscreen_window(struct wsm_window *window) {
	struct wsm_workspace *workspace = window->pending.workspace;

	if (workspace && workspace->fullscreen && !window_is_fullscreen(window)) {
		if (window_is_transient_for(window, workspace->fullscreen)) {
			return NULL;
		}
		return workspace->fullscreen;
	}

	struct wsm_window *fullscreen_global = global_server.scene->fullscreen_global;
	if (fullscreen_global && window != fullscreen_global) {
		if (window_is_transient_for(window, fullscreen_global)) {
			return NULL;
		}
		return fullscreen_global;
	}

	return NULL;
}

void disable_window(struct wsm_window *window) {
	if (window->view) {
		wlr_scene_node_reparent(&window->view->scene_tree->node, window->content_tree);
	}
}

int get_max_thickness(struct wsm_window_state state) {
	return state.sensing_thickness > state.border_thickness
		? state.sensing_thickness : state.border_thickness;
}
