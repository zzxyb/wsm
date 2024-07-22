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

#ifndef WSM_TRANSACTION_H
#define WSM_TRANSACTION_H

#include <stdint.h>
#include <stdbool.h>

/**
 * Transactions enable us to perform atomic layout updates.
 *
 * A transaction contains a list of containers and their new state.
 * A state might contain a new size, or new border settings, or new parent/child
 * relationships.
 *
 * Committing a transaction makes wsm notify of all the affected clients with
 * their new sizes. We then wait for all the views to respond with their new
 * surface sizes. When all are ready, or when a timeout has passed, we apply the
 * updates all at the same time.
 *
 * When we want to make adjustments to the layout, we change the pending state
 * in containers, mark them as dirty and call transaction_commit_dirty(). This
 * create and commits a transaction from the dirty containers.
 */

struct wlr_scene_tree;

struct wsm_transaction_instruction;
struct wsm_view;

/**
 * Find all dirty containers, create and commit a transaction containing them,
 * and unmark them as dirty.
 */
void transaction_commit_dirty(void);

/*
 * Same as transaction_commit_dirty, but signalling that this is a
 * client-initiated change has already taken effect.
 */
void transaction_commit_dirty_client(void);

/**
 * Notify the transaction system that a view is ready for the new layout.
 *
 * When all views in the transaction are ready, the layout will be applied.
 *
 * A success boolean is returned denoting that this part of the transaction is
 * ready.
 */
bool transaction_notify_view_ready_by_serial(struct wsm_view *view,
                                             uint32_t serial);

/**
 * Notify the transaction system that a view is ready for the new layout, but
 * identifying the instruction by geometry rather than by serial.
 *
 * This is used by xwayland views, as they don't have serials.
 *
 * A success boolean is returned denoting that this part of the transaction is
 * ready.
 */
bool transaction_notify_view_ready_by_geometry(struct wsm_view *view,
                                               double x, double y, int width, int height);

#endif
