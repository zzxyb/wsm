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

#include "wsm_log.h"
#include "wsm_seat.h"
#include "wsm_scene.h"
#include "wsm_view.h"
#include "wsm_server.h"
#include "wsm_output.h"
#include "wsm_workspace.h"
#include "wsm_input_manager.h"
#include "wsm_workspace_manager.h"
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
    int index_id = list_seq_find(ws->output_priority, find_output, identifier);
    int index_name = list_seq_find(ws->output_priority, find_output,
                                   output->wlr_output->name);
    return index_name < 0 || index_id < index_name ? index_id : index_name;
}

struct wsm_workspace *workspace_create(struct wsm_output *output,
                                       const char *name) {
    wsm_assert(name, "NULL name given to workspace_create");

    wsm_log(WSM_DEBUG, "Adding workspace %s for output %s", name,
             output->wlr_output->name);

    struct wsm_workspace *ws = calloc(1, sizeof(struct wsm_workspace));
    if (!wsm_assert(ws, "Could not create wsm_workspace: allocation failed!")) {
        return NULL;
    }
    node_init(&ws->node, N_WORKSPACE, ws);

    ws->layers.non_fullscreen = wlr_scene_tree_create(global_server.wsm_scene->staging);
    ws->layers.fullscreen = wlr_scene_tree_create(global_server.wsm_scene->staging);

    bool successed = ws->layers.non_fullscreen && ws->layers.non_fullscreen;
    if (!successed) {
        wlr_scene_node_destroy(&ws->layers.non_fullscreen->node);
        wlr_scene_node_destroy(&ws->layers.fullscreen->node);
        free(ws);
        return NULL;
    }

    ws->name = strdup(name);
    ws->prev_split_layout = L_NONE;
    ws->layout = L_NONE;
    ws->floating = create_list();
    ws->tiling = create_list();
    ws->output_priority = create_list();

    wsm_output_add_workspace(output, ws);

    wl_signal_emit_mutable(&global_server.wsm_scene->events.new_node, &ws->node);

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
    list_free_items_and_destroy(workspace->output_priority);
    list_free(workspace->floating);
    list_free(workspace->tiling);
    list_free(workspace->current.floating);
    list_free(workspace->current.tiling);
    free(workspace);
}

void workspace_detach(struct wsm_workspace *workspace) {
    struct wsm_output *output = workspace->output;
    int index = list_find(output->workspace_manager->current.workspaces, workspace);
    if (index != -1) {
        list_del(output->workspace_manager->current.workspaces, index);
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

void workspace_for_each_container(struct wsm_workspace *ws,
                                  void (*f)(struct wsm_container *con, void *data), void *data) {
    for (int i = 0; i < ws->floating->length; ++i) {
        struct wsm_container *container = ws->floating->items[i];
        f(container, data);
        container_for_each_child(container, f, data);
    }
}

bool workspace_is_visible(struct wsm_workspace *ws) {
    if (ws->node.destroying) {
        return false;
    }
    return output_get_active_workspace(ws->output) == ws;
}

bool workspace_is_empty(struct wsm_workspace *ws) {
    if (ws->tiling->length) {
        return false;
    }
    // Sticky views are not considered to be part of this workspace
    for (int i = 0; i < ws->floating->length; ++i) {
        struct wsm_container *floater = ws->floating->items[i];
        if (!container_is_sticky(floater)) {
            return false;
        }
    }
    return true;
}

void root_for_each_container(void (*f)(struct wsm_container *con, void *data),
                             void *data) {
    for (int i = 0; i < global_server.wsm_scene->outputs->length; ++i) {
        struct wsm_output *output = global_server.wsm_scene->outputs->items[i];
        output_for_each_container(output, f, data);
    }

    // Scratchpad
    for (int i = 0; i < global_server.wsm_scene->scratchpad->length; ++i) {
        struct wsm_container *container = global_server.wsm_scene->scratchpad->items[i];
        if (container_is_scratchpad_hidden(container)) {
            f(container, data);
            container_for_each_child(container, f, data);
        }
    }

    // Saved workspaces
    for (int i = 0; i < global_server.wsm_scene->fallback_output->
                        workspace_manager->current.workspaces->length; ++i) {
        struct wsm_workspace *ws = global_server.wsm_scene->fallback_output->
                                   workspace_manager->current.workspaces->items[i];
        workspace_for_each_container(ws, f, data);
    }
}

void workspace_update_representation(struct wsm_workspace *ws) {
    size_t len = container_build_representation(ws->layout, ws->tiling, NULL);
    free(ws->representation);
    ws->representation = calloc(len + 1, sizeof(char));
    if (!wsm_assert(ws->representation, "Unable to allocate title string")) {
        return;
    }
    container_build_representation(ws->layout, ws->tiling, ws->representation);
}

static void set_workspace(struct wsm_container *container, void *data) {
    container->pending.workspace = container->pending.parent->pending.workspace;
}

void workspace_add_floating(struct wsm_workspace *workspace,
                            struct wsm_container *con) {
    if (con->pending.workspace) {
        container_detach(con);
    }
    list_add(workspace->floating, con);
    con->pending.workspace = workspace;
    container_for_each_child(con, set_workspace, NULL);
    container_handle_fullscreen_reparent(con);
    node_set_dirty(&workspace->node);
    node_set_dirty(&con->node);
}

void workspace_add_gaps(struct wsm_workspace *ws) {
    ws->current_gaps.top = 0;
    ws->current_gaps.right = 0;
    ws->current_gaps.bottom = 0;
    ws->current_gaps.left = 0;

    // Add inner gaps and make sure we don't turn out negative
    ws->current_gaps.top = fmax(0, ws->current_gaps.top + ws->gaps_inner);
    ws->current_gaps.right = fmax(0, ws->current_gaps.right + ws->gaps_inner);
    ws->current_gaps.bottom = fmax(0, ws->current_gaps.bottom + ws->gaps_inner);
    ws->current_gaps.left = fmax(0, ws->current_gaps.left + ws->gaps_inner);

    // Now that we have the total gaps calculated we may need to clamp them in
    // case they've made the available area too small
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
    if (ws->tiling->length || ws->floating->length) {
        return;
    }

    if (ws->output && output_get_active_workspace(ws->output) == ws) {
        return;
    }

    struct wsm_seat *seat;
    wl_list_for_each(seat, &global_server.wsm_input_manager->seats, link) {
        struct wsm_node *node = seat_get_focus_inactive(seat, &global_server.wsm_scene->node);
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

struct wsm_container *workspace_add_tiling(struct wsm_workspace *workspace,
                                           struct wsm_container *con) {
    if (con->pending.workspace) {
        struct wsm_container *old_parent = con->pending.parent;
        container_detach(con);
        if (old_parent) {
            container_reap_empty(old_parent);
        }
    }

    list_add(workspace->tiling, con);
    con->pending.workspace = workspace;
    container_for_each_child(con, set_workspace, NULL);
    container_handle_fullscreen_reparent(con);
    workspace_update_representation(workspace);
    node_set_dirty(&workspace->node);
    node_set_dirty(&con->node);
    return con;
}

static bool find_urgent_iterator(struct wsm_container *con, void *data) {
    return con->view && view_is_urgent(con->view);
}

void workspace_detect_urgent(struct wsm_workspace *workspace) {
    bool new_urgent = (bool)workspace_find_container(workspace,
                                                      find_urgent_iterator, NULL);

    if (workspace->urgent != new_urgent) {
        workspace->urgent = new_urgent;
    }
}

struct wsm_container *workspace_find_container(struct wsm_workspace *ws,
                                               bool (*test)(struct wsm_container *con, void *data), void *data) {
    struct wsm_container *result = NULL;
    // Tiling
    for (int i = 0; i < ws->tiling->length; ++i) {
        struct wsm_container *child = ws->tiling->items[i];
        if (test(child, data)) {
            return child;
        }
        if ((result = container_find_child(child, test, data))) {
            return result;
        }
    }
    // Floating
    for (int i = 0; i < ws->floating->length; ++i) {
        struct wsm_container *child = ws->floating->items[i];
        if (test(child, data)) {
            return child;
        }
        if ((result = container_find_child(child, test, data))) {
            return result;
        }
    }
    return NULL;
}

bool output_match_name_or_id(struct wsm_output *output,
                             const char *name_or_id) {
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

static void count_sticky_containers(struct wsm_container *con, void *data) {
    if (container_is_sticky(con)) {
        size_t *count = data;
        *count += 1;
    }
}

size_t workspace_num_sticky_containers(struct wsm_workspace *ws) {
    size_t count = 0;
    workspace_for_each_container(ws, count_sticky_containers, &count);
    return count;
}

void workspace_output_add_priority(struct wsm_workspace *workspace,
                                   struct wsm_output *output) {
    if (workspace_output_get_priority(workspace, output) < 0) {
        char identifier[128];
        output_get_identifier(identifier, sizeof(identifier), output);
        list_add(workspace->output_priority, strdup(identifier));
    }
}

void output_add_workspace(struct wsm_output *output,
                          struct wsm_workspace *workspace) {
    if (workspace->output) {
        workspace_detach(workspace);
    }
    list_add(output->workspace_manager->current.workspaces, workspace);
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
    list_stable_sort(output->workspace_manager->current.workspaces, sort_workspace_cmp_qsort);
}

void disable_workspace(struct wsm_workspace *ws) {
    for (int i = 0; i < ws->current.tiling->length; i++) {
        struct wsm_container *child = ws->current.tiling->items[i];

        wlr_scene_node_reparent(&child->scene_tree->node, ws->layers.non_fullscreen);
        disable_container(child);
    }

    for (int i = 0; i < ws->current.floating->length; i++) {
        struct wsm_container *floater = ws->current.floating->items[i];
        wlr_scene_node_reparent(&floater->scene_tree->node, global_server.wsm_scene->layers.floating);
        disable_container(floater);
        wlr_scene_node_set_enabled(&floater->scene_tree->node, false);
    }
}
