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

#include "wsm_transaction.h"
#include "wsm_log.h"
#include "wsm_list.h"
#include "wsm_seat.h"
#include "wsm_view.h"
#include "wsm_cursor.h"
#include "wsm_container.h"
#include "wsm_output.h"
#include "wsm_workspace.h"
#include "node/wsm_node.h"
#include "wsm_titlebar.h"
#include "wsm_server.h"
#include "wsm_scene.h"
#include "wsm_arrange.h"
#include "wsm_input_manager.h"
#include "wsm_idle_inhibit_v1.h"
#include "wsm_workspace_manager.h"
#include "node/wsm_node_descriptor.h"

#include <stdlib.h>

#include <wlr/types/wlr_scene.h>

struct wsm_transaction {
    struct wl_event_source *timer;
    struct wsm_list *instructions;   // struct wsm_transaction_instruction *
    size_t num_waiting;
    size_t num_configures;
    struct timespec commit_time;
};

struct wsm_transaction_instruction {
    struct wsm_transaction *transaction;
    struct wsm_node *node;
    union {
        struct wsm_workspace_manager_state output_state;
        struct wsm_workspace_state workspace_state;
        struct wsm_container_state container_state;
    };
    uint32_t serial;
    bool server_request;
    bool waiting;
};

static struct wsm_transaction *transaction_create(void) {
    struct wsm_transaction *transaction =
        calloc(1, sizeof(struct wsm_transaction));
    if (!wsm_assert(transaction, "Unable to allocate transaction")) {
        return NULL;
    }
    transaction->instructions = create_list();
    return transaction;
}

static void transaction_destroy(struct wsm_transaction *transaction) {
    for (int i = 0; i < transaction->instructions->length; ++i) {
        struct wsm_transaction_instruction *instruction =
            transaction->instructions->items[i];
        struct wsm_node *node = instruction->node;
        node->ntxnrefs--;
        if (node->instruction == instruction) {
            node->instruction = NULL;
        }
        if (node->destroying && node->ntxnrefs == 0) {
            switch (node->type) {
            case N_ROOT:
                wsm_assert(false, "Never reached");
                break;
            case N_OUTPUT:
                wsm_output_destroy(node->wsm_output);
                break;
            case N_WORKSPACE:
                workspace_destroy(node->wsm_workspace);
                break;
            case N_CONTAINER:
                container_destroy(node->wsm_container);
                break;
            }
        }
        free(instruction);
    }
    list_free(transaction->instructions);

    if (transaction->timer) {
        wl_event_source_remove(transaction->timer);
    }
    free(transaction);
}

static void copy_output_state(struct wsm_output *output,
                              struct wsm_transaction_instruction *instruction) {
    struct wsm_workspace_manager_state *state = &instruction->output_state;
    if (state->workspaces) {
        state->workspaces->length = 0;
    } else {
        state->workspaces = create_list();
    }
    list_cat(state->workspaces, output->workspace_manager->current.workspaces);

    state->active_workspace = output_get_active_workspace(output);
}

static void copy_workspace_state(struct wsm_workspace *ws,
                                 struct wsm_transaction_instruction *instruction) {
    struct wsm_workspace_state *state = &instruction->workspace_state;

    state->fullscreen = ws->fullscreen;
    state->x = ws->x;
    state->y = ws->y;
    state->width = ws->width;
    state->height = ws->height;
    state->layout = ws->layout;

    state->output = ws->output;
    if (state->floating) {
        state->floating->length = 0;
    } else {
        state->floating = create_list();
    }
    if (state->tiling) {
        state->tiling->length = 0;
    } else {
        state->tiling = create_list();
    }
    list_cat(state->floating, ws->floating);
    list_cat(state->tiling, ws->tiling);

    struct wsm_seat *seat = input_manager_current_seat();
    state->focused = seat_get_focus(seat) == &ws->node;

    // Set focused_inactive_child to the direct tiling child
    struct wsm_container *focus = seat_get_focus_inactive_tiling(seat, ws);
    if (focus) {
        while (focus->pending.parent) {
            focus = focus->pending.parent;
        }
    }
    state->focused_inactive_child = focus;
}

static void copy_container_state(struct wsm_container *container,
                                 struct wsm_transaction_instruction *instruction) {
    struct wsm_container_state *state = &instruction->container_state;

    if (state->children) {
        list_free(state->children);
    }

    memcpy(state, &container->pending, sizeof(struct wsm_container_state));

    if (!container->view) {
        // We store a copy of the child list to avoid having it mutated after
        // we copy the state.
        state->children = create_list();
        list_cat(state->children, container->pending.children);
    } else {
        state->children = NULL;
    }

    struct wsm_seat *seat = input_manager_current_seat();
    state->focused = seat_get_focus(seat) == &container->node;

    if (!container->view) {
        struct wsm_node *focus =
            seat_get_active_tiling_child(seat, &container->node);
        state->focused_inactive_child = focus ? focus->wsm_container : NULL;
    }
}

static void transaction_add_node(struct wsm_transaction *transaction,
                                 struct wsm_node *node, bool server_request) {
    struct wsm_transaction_instruction *instruction = NULL;

    // Check if we have an instruction for this node already, in which case we
    // update that instead of creating a new one.
    if (node->ntxnrefs > 0) {
        for (int idx = 0; idx < transaction->instructions->length; idx++) {
            struct wsm_transaction_instruction *other =
                transaction->instructions->items[idx];
            if (other->node == node) {
                instruction = other;
                break;
            }
        }
    }

    if (!instruction) {
        instruction = calloc(1, sizeof(struct wsm_transaction_instruction));
        if (!wsm_assert(instruction, "Unable to allocate instruction")) {
            return;
        }
        instruction->transaction = transaction;
        instruction->node = node;
        instruction->server_request = server_request;

        list_add(transaction->instructions, instruction);
        node->ntxnrefs++;
    } else if (server_request) {
        instruction->server_request = true;
    }

    switch (node->type) {
    case N_ROOT:
        break;
    case N_OUTPUT:
        copy_output_state(node->wsm_output, instruction);
        break;
    case N_WORKSPACE:
        copy_workspace_state(node->wsm_workspace, instruction);
        break;
    case N_CONTAINER:
        copy_container_state(node->wsm_container, instruction);
        break;
    }
}

static void apply_output_state(struct wsm_output *output,
                               struct wsm_workspace_manager_state *state) {
    list_free(output->workspace_manager->current.workspaces);
    memcpy(&output->workspace_manager->current, state, sizeof(struct wsm_workspace_manager_state));
}

static void apply_workspace_state(struct wsm_workspace *ws,
                                  struct wsm_workspace_state *state) {
    list_free(ws->current.floating);
    list_free(ws->current.tiling);
    memcpy(&ws->current, state, sizeof(struct wsm_workspace_state));
}

static void apply_container_state(struct wsm_container *container,
                                  struct wsm_container_state *state) {
    struct wsm_view *view = container->view;
    // There are separate children lists for each instruction state, the
    // container's current state and the container's pending state
    // (ie. con->children). The list itself needs to be freed here.
    // Any child containers which are being deleted will be cleaned up in
    // transaction_destroy().
    list_free(container->current.children);

    memcpy(&container->current, state, sizeof(struct wsm_container_state));

    if (view) {
        if (view->saved_surface_tree) {
            if (!container->node.destroying || container->node.ntxnrefs == 1) {
                view_remove_saved_buffer(view);
            }
        }

        // If the view hasn't responded to the configure, center it within
        // the container. This is important for fullscreen views which
        // refuse to resize to the size of the output.
        if (view->surface) {
            view_center_and_clip_surface(view);
        }
    }
}

/**
 * Apply a transaction to the "current" state of the tree.
 */
static void transaction_apply(struct wsm_transaction *transaction) {
    wsm_log(WSM_DEBUG, "Applying transaction %p", transaction);

    // Apply the instruction state to the node's current state
    for (int i = 0; i < transaction->instructions->length; ++i) {
        struct wsm_transaction_instruction *instruction =
            transaction->instructions->items[i];
        struct wsm_node *node = instruction->node;

        switch (node->type) {
        case N_ROOT:
            break;
        case N_OUTPUT:
            apply_output_state(node->wsm_output, &instruction->output_state);
            break;
        case N_WORKSPACE:
            apply_workspace_state(node->wsm_workspace,
                                  &instruction->workspace_state);
            break;
        case N_CONTAINER:
            apply_container_state(node->wsm_container,
                                  &instruction->container_state);
            break;
        }

        node->instruction = NULL;
    }
}

static void transaction_commit_pending(void);

static void transaction_progress(void) {
    if (!global_server.queued_transaction) {
        return;
    }
    if (global_server.queued_transaction->num_waiting > 0) {
        return;
    }
    transaction_apply(global_server.queued_transaction);
    arrange_root_scene(global_server.wsm_scene);
    cursor_rebase_all();
    transaction_destroy(global_server.queued_transaction);
    global_server.queued_transaction = NULL;

    if (!global_server.pending_transaction) {
        wsm_idle_inhibit_v1_check_active();
        return;
    }

    transaction_commit_pending();
}

static int handle_timeout(void *data) {
    struct wsm_transaction *transaction = data;
    wsm_log(WSM_DEBUG, "Transaction %p timed out (%zi waiting)",
             transaction, transaction->num_waiting);
    transaction->num_waiting = 0;
    transaction_progress();
    return 0;
}

static bool should_configure(struct wsm_node *node,
                             struct wsm_transaction_instruction *instruction) {
    if (!node_is_view(node)) {
        return false;
    }
    if (node->destroying) {
        return false;
    }
    if (!instruction->server_request) {
        return false;
    }
    struct wsm_container_state *cstate = &node->wsm_container->current;
    struct wsm_container_state *istate = &instruction->container_state;
#if HAVE_XWAYLAND
    // Xwayland views are position-aware and need to be reconfigured
    // when their position changes.
    if (node->wsm_container->view->type == WSM_VIEW_XWAYLAND) {
        // wsm logical coordinates are doubles, but they get truncated to
        // integers when sent to Xwayland through `xcb_configure_window`.
        // X11 apps will not respond to duplicate configure requests (from their
        // truncated point of view) and cause transactions to time out.
        if ((int)cstate->content_x != (int)istate->content_x ||
            (int)cstate->content_y != (int)istate->content_y) {
            return true;
        }
    }
#endif
    if (cstate->content_width == istate->content_width &&
        cstate->content_height == istate->content_height) {
        return false;
    }
    return true;
}

static void transaction_commit(struct wsm_transaction *transaction) {
    wsm_log(WSM_DEBUG, "Transaction %p committing with %i instructions",
             transaction, transaction->instructions->length);
    transaction->num_waiting = 0;
    for (int i = 0; i < transaction->instructions->length; ++i) {
        struct wsm_transaction_instruction *instruction =
            transaction->instructions->items[i];
        struct wsm_node *node = instruction->node;
        bool hidden = node_is_view(node) && !node->destroying &&
                      !view_is_visible(node->wsm_container->view);
        if (should_configure(node, instruction)) {
            instruction->serial = view_configure(node->wsm_container->view,
                                                 instruction->container_state.content_x,
                                                 instruction->container_state.content_y,
                                                 instruction->container_state.content_width,
                                                 instruction->container_state.content_height);
            if (!hidden) {
                instruction->waiting = true;
                ++transaction->num_waiting;
            }

            view_send_frame_done(node->wsm_container->view);
        }
        if (!hidden && node_is_view(node) &&
            !node->wsm_container->view->saved_surface_tree) {
            view_save_buffer(node->wsm_container->view);
        }
        node->instruction = instruction;
    }
    transaction->num_configures = transaction->num_waiting;

    if (transaction->num_waiting) {
        // Set up a timer which the views must respond within
        transaction->timer = wl_event_loop_add_timer(global_server.wl_event_loop,
                                                     handle_timeout, transaction);
        if (transaction->timer) {
            wl_event_source_timer_update(transaction->timer,
                                         global_server.txn_timeout_ms);
        } else {
            wsm_log_errno(WSM_ERROR, "Unable to create transaction timer "
                                       "(some imperfect frames might be rendered)");
            transaction->num_waiting = 0;
        }
    }
}

static void transaction_commit_pending(void) {
    if (global_server.queued_transaction) {
        return;
    }
    struct wsm_transaction *transaction = global_server.pending_transaction;
    global_server.pending_transaction = NULL;
    global_server.queued_transaction = transaction;
    transaction_commit(transaction);
    transaction_progress();
}

static void set_instruction_ready(
    struct wsm_transaction_instruction *instruction) {
    struct wsm_transaction *transaction = instruction->transaction;

    // If the transaction has timed out then its num_waiting will be 0 already.
    if (instruction->waiting && transaction->num_waiting > 0 &&
        --transaction->num_waiting == 0) {
        wsm_log(WSM_DEBUG, "Transaction %p is ready", transaction);
        wl_event_source_timer_update(transaction->timer, 0);
    }

    instruction->node->instruction = NULL;
    transaction_progress();
}

bool transaction_notify_view_ready_by_serial(struct wsm_view *view,
                                             uint32_t serial) {
    struct wsm_transaction_instruction *instruction =
        view->container->node.instruction;
    if (instruction != NULL && instruction->serial == serial) {
        set_instruction_ready(instruction);
        return true;
    }
    return false;
}

bool transaction_notify_view_ready_by_geometry(struct wsm_view *view,
                                               double x, double y, int width, int height) {
    struct wsm_transaction_instruction *instruction =
        view->container->node.instruction;
    if (instruction != NULL &&
        (int)instruction->container_state.content_x == (int)x &&
        (int)instruction->container_state.content_y == (int)y &&
        instruction->container_state.content_width == width &&
        instruction->container_state.content_height == height) {
        set_instruction_ready(instruction);
        return true;
    }
    return false;
}

static void _transaction_commit_dirty(bool server_request) {
    if (!global_server.dirty_nodes->length) {
        return;
    }

    if (!global_server.pending_transaction) {
        global_server.pending_transaction = transaction_create();
        if (!global_server.pending_transaction) {
            return;
        }
    }

    for (int i = 0; i < global_server.dirty_nodes->length; ++i) {
        struct wsm_node *node = global_server.dirty_nodes->items[i];
        transaction_add_node(global_server.pending_transaction, node, server_request);
        node->dirty = false;
    }
    global_server.dirty_nodes->length = 0;

    transaction_commit_pending();
}

void transaction_commit_dirty(void) {
    _transaction_commit_dirty(true);
}

void transaction_commit_dirty_client(void) {
    _transaction_commit_dirty(false);
}
