#include "wsm_log.h"
#include "wsm_seat.h"
#include "wsm_scene.h"
#include "wsm_view.h"
#include "wsm_server.h"
#include "wsm_output.h"
#include "wsm_workspace.h"
#include "wsm_input.h"
#include "wsm_output_config.h"

#include <stdlib.h>
#include <strings.h>
#include <ctype.h>

#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_output.h>

#define MIN_SANE_W 100
#define MIN_SANE_H 60

static int find_output(const void *id1, const void *id2) {
	return strcmp(id1, id2);
}

static int workspace_output_get_priority(struct wsm_workspace *ws,
		struct wsm_output *output) {
	char identifier[128];
	output_get_identifier(identifier, sizeof(identifier), output);
	int index_id = wsm_list_seq_find(ws->output_priority, find_output, identifier);
	int index_name = wsm_list_seq_find(ws->output_priority, find_output,
		output->wlr_output->name);
	return index_name < 0 || index_id < index_name ? index_id : index_name;
}

struct wsm_workspace *workspace_create(struct wsm_output *output,
		const char *name) {
	wsm_assert(name, "NULL name given to workspace_create");

	wsm_log(WSM_DEBUG, "Adding workspace %s for output %s", name,
		output->wlr_output->name);

	struct wsm_workspace *ws = calloc(1, sizeof(struct wsm_workspace));
	if (!ws) {
		wsm_log(WSM_ERROR, "Could not create wsm_workspace: allocation failed!");
		return NULL;
	}
	node_init(&ws->node, N_WORKSPACE, ws);

	ws->layers.non_fullscreen = wlr_scene_tree_create(global_server.scene->staging);
	ws->layers.fullscreen = wlr_scene_tree_create(global_server.scene->staging);

	bool successed = ws->layers.non_fullscreen && ws->layers.non_fullscreen;
	if (!successed) {
		wlr_scene_node_destroy(&ws->layers.non_fullscreen->node);
		wlr_scene_node_destroy(&ws->layers.fullscreen->node);
		free(ws);
		return NULL;
	}

	ws->name = strdup(name);
	ws->windows = wsm_list_create();
	ws->output_priority = wsm_list_create();

	wsm_output_add_workspace(output, ws);
	wl_signal_emit_mutable(&global_server.scene->events.new_node, &ws->node);

	return ws;
}

void workspace_destroy(struct wsm_workspace *workspace) {
	if (!wsm_assert(workspace->node.destroying,
		"Tried to free workspace which wasn't marked as destroying")) {
		return;
	}
	if (!wsm_assert(workspace->node.ntxnrefs == 0, "Tried to free workspace "
		"which is still referenced by transactions")) {
		return;
	}

	scene_node_disown_children(workspace->layers.non_fullscreen);
	scene_node_disown_children(workspace->layers.fullscreen);
	wlr_scene_node_destroy(&workspace->layers.non_fullscreen->node);
	wlr_scene_node_destroy(&workspace->layers.fullscreen->node);

	free(workspace->name);
	free(workspace->representation);
	wsm_list_free_items_and_destroy(workspace->output_priority);
	wsm_list_destroy(workspace->windows);
	wsm_list_destroy(workspace->current.windows);
	free(workspace);
}

void workspace_detach(struct wsm_workspace *workspace) {
	struct wsm_output *output = workspace->output;
	int index = wsm_list_find(output->workspaces, workspace);
	if (index != -1) {
		wsm_list_delete(output->workspaces, index);
	}
	workspace->output = NULL;

	node_set_dirty(&workspace->node);
	node_set_dirty(&output->node);
}

void workspace_get_box(struct wsm_workspace *workspace, struct wlr_box *box) {
	box->x = workspace->x;
	box->y = workspace->y;
	box->width = workspace->width;
	box->height = workspace->height;
}

void workspace_for_each_window(struct wsm_workspace *ws,
		void (*f)(struct wsm_window *window, void *data), void *data) {
	for (int i = 0; i < ws->windows->length; ++i) {
		struct wsm_window *window = ws->windows->items[i];
		f(window, data);
	}
}

bool workspace_is_visible(struct wsm_workspace *ws) {
	if (ws->node.destroying) {
		return false;
	}
	return output_get_active_workspace(ws->output) == ws;
}

bool workspace_is_empty(struct wsm_workspace *ws) {
	for (int i = 0; i < ws->windows->length; ++i) {
		struct wsm_window *window = ws->windows->items[i];
		if (!window_is_sticky(window)) {
			return false;
		}
	}
	return true;
}

void root_for_each_window(void (*f)(struct wsm_window *window, void *data),
	void *data) {
	for (int i = 0; i < global_server.scene->outputs->length; ++i) {
		struct wsm_output *output = global_server.scene->outputs->items[i];
		output_for_each_window(output, f, data);
	}

	for (int i = 0; i < global_server.scene->scratchpad->length; ++i) {
		struct wsm_window *window = global_server.scene->scratchpad->items[i];
		if (window_is_scratchpad_hidden(window)) {
			f(window, data);
		}
	}

	for (int i = 0; i < global_server.scene->fallback_output->
			workspaces->length; ++i) {
		struct wsm_workspace *ws = global_server.scene->fallback_output->
			workspaces->items[i];
		workspace_for_each_window(ws, f, data);
	}
}

void root_for_each_workspace(void (*f)(struct wsm_workspace *ws, void *data), void *data) {
	struct wsm_scene *root = global_server.scene;
	for (int i = 0; i < root->outputs->length; ++i) {
		struct wsm_output *output = root->outputs->items[i];
		output_for_each_workspace(output, f, data);
	}
}

void workspace_update_representation(struct wsm_workspace *ws) {
	size_t len = window_build_representation(ws->windows, NULL);
	free(ws->representation);
	ws->representation = calloc(len + 1, sizeof(char));
	if (!ws->representation) {
		wsm_log(WSM_ERROR, "Could not create title string: allocation failed!");
		return;
	}
	window_build_representation(ws->windows, ws->representation);
}

void workspace_add_window(struct wsm_workspace *workspace, struct wsm_window *window) {
	if (window->pending.workspace) {
		window_detach(window);
	}
	wsm_list_add(workspace->windows, window);
	window->pending.workspace = workspace;
	window_handle_fullscreen_reparent(window);
	workspace_update_representation(workspace);
	node_set_dirty(&workspace->node);
	node_set_dirty(&window->node);
}

void workspace_add_gaps(struct wsm_workspace *ws) {
	ws->current_gaps.top = 0;
	ws->current_gaps.right = 0;
	ws->current_gaps.bottom = 0;
	ws->current_gaps.left = 0;

	ws->current_gaps.top = fmax(0, ws->current_gaps.top + ws->gaps_inner);
	ws->current_gaps.right = fmax(0, ws->current_gaps.right + ws->gaps_inner);
	ws->current_gaps.bottom = fmax(0, ws->current_gaps.bottom + ws->gaps_inner);
	ws->current_gaps.left = fmax(0, ws->current_gaps.left + ws->gaps_inner);

	if (ws->width - ws->current_gaps.left - ws->current_gaps.right < MIN_SANE_W
		&& ws->current_gaps.left + ws->current_gaps.right > 0) {
		int total_gap = fmax(0, ws->width - MIN_SANE_W);
		double left_gap_frac = ((double)ws->current_gaps.left /
			((double)ws->current_gaps.left + (double)ws->current_gaps.right));
		ws->current_gaps.left = left_gap_frac * total_gap;
		ws->current_gaps.right = total_gap - ws->current_gaps.left;
	}
	if (ws->height - ws->current_gaps.top - ws->current_gaps.bottom < MIN_SANE_H
		&& ws->current_gaps.top + ws->current_gaps.bottom > 0) {
		int total_gap = fmax(0, ws->height - MIN_SANE_H);
		double top_gap_frac = ((double) ws->current_gaps.top /
			((double)ws->current_gaps.top + (double)ws->current_gaps.bottom));
		ws->current_gaps.top = top_gap_frac * total_gap;
		ws->current_gaps.bottom = total_gap - ws->current_gaps.top;
	}

	ws->x += ws->current_gaps.left;
	ws->y += ws->current_gaps.top;
	ws->width -= ws->current_gaps.left + ws->current_gaps.right;
	ws->height -= ws->current_gaps.top + ws->current_gaps.bottom;
}

void workspace_consider_destroy(struct wsm_workspace *ws) {
	if (ws->windows->length) {
		return;
	}

	if (ws->output && output_get_active_workspace(ws->output) == ws) {
		return;
	}

	struct wsm_seat *seat;
	wl_list_for_each(seat, &global_server.input_manager->seats, link) {
		struct wsm_node *node = seat_get_focus_inactive(seat, &global_server.scene->node);
		if (node == &ws->node) {
			return;
		}
	}

	workspace_begin_destroy(ws);
}

void workspace_begin_destroy(struct wsm_workspace *workspace) {
	wsm_log(WSM_DEBUG, "Destroying workspace '%s'", workspace->name);
	wl_signal_emit_mutable(&workspace->node.events.destroy, &workspace->node);

	if (workspace->output) {
		workspace_detach(workspace);
	}
	workspace->node.destroying = true;
	node_set_dirty(&workspace->node);
}

static bool find_urgent_iterator(struct wsm_window *window, void *data) {
	return window->view && view_is_urgent(window->view);
}

void workspace_detect_urgent(struct wsm_workspace *workspace) {
	bool new_urgent = (bool)workspace_find_window(workspace,
		find_urgent_iterator, NULL);

	if (workspace->urgent != new_urgent) {
		workspace->urgent = new_urgent;
	}
}

struct wsm_window *workspace_find_window(struct wsm_workspace *ws,
		bool (*test)(struct wsm_window *window, void *data), void *data) {
	for (int i = 0; i < ws->windows->length; ++i) {
		struct wsm_window *child = ws->windows->items[i];
		if (test(child, data)) {
			return child;
		}
	}
	return NULL;
}

bool output_match_name_or_id(struct wsm_output *output, const char *name_or_id) {
	if (strcmp(name_or_id, "*") == 0) {
		return true;
	}

	char identifier[128];
	output_get_identifier(identifier, sizeof(identifier), output);
	return strcasecmp(identifier, name_or_id) == 0
		   || strcasecmp(output->wlr_output->name, name_or_id) == 0;
}

struct wsm_output *workspace_output_get_highest_available(
		struct wsm_workspace *ws, struct wsm_output *exclude) {
	for (int i = 0; i < ws->output_priority->length; i++) {
		const char *name = ws->output_priority->items[i];
		if (exclude && output_match_name_or_id(exclude, name)) {
			continue;
		}

		struct wsm_output *output = output_by_name_or_id(name);
		if (output) {
			return output;
		}
	}

	return NULL;
}

static void count_sticky_containers(struct wsm_window *window, void *data) {
	if (window_is_sticky(window)) {
		size_t *count = data;
		*count += 1;
	}
}

size_t workspace_num_sticky_containers(struct wsm_workspace *ws) {
	size_t count = 0;
	workspace_for_each_window(ws, count_sticky_containers, &count);
	return count;
}

void workspace_output_add_priority(struct wsm_workspace *workspace,
		struct wsm_output *output) {
	if (workspace_output_get_priority(workspace, output) < 0) {
		char identifier[128];
		output_get_identifier(identifier, sizeof(identifier), output);
		wsm_list_add(workspace->output_priority, strdup(identifier));
	}
}

void output_add_workspace(struct wsm_output *output,
		struct wsm_workspace *workspace) {
	if (workspace->output) {
		workspace_detach(workspace);
	}
	wsm_list_add(output->workspaces, workspace);
	workspace->output = output;
	node_set_dirty(&output->node);
	node_set_dirty(&workspace->node);
}

static int sort_workspace_cmp_qsort(const void *_a, const void *_b) {
	struct wsm_workspace *a = *(void **)_a;
	struct wsm_workspace *b = *(void **)_b;

	if (isdigit(a->name[0]) && isdigit(b->name[0])) {
		int a_num = strtol(a->name, NULL, 10);
		int b_num = strtol(b->name, NULL, 10);
		return (a_num < b_num) ? -1 : (a_num > b_num);
	} else if (isdigit(a->name[0])) {
		return -1;
	} else if (isdigit(b->name[0])) {
		return 1;
	}
	return 0;
}

void output_sort_workspaces(struct wsm_output *output) {
	wsm_list_stable_sort(output->workspaces, sort_workspace_cmp_qsort);
}

void disable_workspace(struct wsm_workspace *ws) {
	for (int i = 0; i < ws->current.windows->length; i++) {
		struct wsm_window *window = ws->current.windows->items[i];
		wlr_scene_node_reparent(&window->scene_tree->node, global_server.scene->layers.windows);
		disable_window(window);
		wlr_scene_node_set_enabled(&window->scene_tree->node, false);
	}
}
