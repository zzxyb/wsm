#ifndef WSM_TRANSACTION_H
#define WSM_TRANSACTION_H

#include <stdint.h>
#include <stdbool.h>

#include <wlr/util/edges.h>

/**
 * @brief Transactions enable atomic layout updates in the WSM.
 *
 * A transaction contains a list of containers and their new state.
 * A state might include a new size, new border settings, or new parent/child
 * relationships.
 *
 * Committing a transaction notifies all affected clients of their new sizes.
 * The system waits for all views to respond with their new surface sizes. Once
 * all are ready, or when a timeout occurs, the updates are applied simultaneously.
 *
 * To adjust the layout, we modify the pending state in containers, mark them as
 * dirty, and call transaction_commit_dirty(). This creates and commits a
 * transaction from the dirty containers.
 */

struct wlr_scene_tree;

struct wsm_transaction_instruction;
struct wsm_view;

/**
 * @brief Finds all dirty containers, creates and commits a transaction containing them,
 * and unmarks them as dirty.
 */
void transaction_commit_dirty(void);

/**
 * @brief Same as transaction_commit_dirty, but signals that a client-initiated change
 * has already taken effect.
 */
void transaction_commit_dirty_client(void);

/**
 * @brief Updates the currently queued resize instruction for a view.
 *
 * This is used when a client commits a size different from the configure
 * request before the transaction is applied. The instruction is updated in
 * place so newer pointer-motion pending state is not disturbed.
 *
 * @param view Pointer to the wsm_view instance to update.
 * @return true if an existing instruction was updated, false otherwise.
 */
bool transaction_update_view_resize_state(struct wsm_view *view,
	enum wlr_edges edges, double geo_right, double geo_bottom,
	int geo_x, int geo_y, int geo_width, int geo_height);

bool transaction_update_view_resize_state_by_serial(struct wsm_view *view,
	uint32_t serial, enum wlr_edges edges, double geo_right,
	double geo_bottom, int geo_x, int geo_y, int geo_width,
	int geo_height);

/**
 * @brief Marks the current instruction for a view as ready.
 *
 * This should only be used after the caller has matched the commit to the
 * pending configure by other means.
 *
 * @param view Pointer to the wsm_view instance that is ready.
 * @return true if an existing instruction was marked ready, false otherwise.
 */
bool transaction_notify_view_ready(struct wsm_view *view);

/**
 * @brief Notifies the transaction system that a view is ready for the new layout.
 *
 * When all views in the transaction are ready, the layout will be applied.
 *
 * @param view Pointer to the wsm_view instance that is ready.
 * @param serial Serial number associated with the view.
 * @return true if the notification was successful, false otherwise.
 */
bool transaction_notify_view_ready_by_serial(struct wsm_view *view,
	uint32_t serial);

/**
 * @brief Notifies the transaction system that a view is ready for the new layout,
 * identifying the instruction by geometry rather than by serial.
 *
 * This is used by XWayland views, as they do not have serials.
 *
 * @param view Pointer to the wsm_view instance that is ready.
 * @param x X coordinate of the view.
 * @param y Y coordinate of the view.
 * @param width Width of the view.
 * @param height Height of the view.
 * @return true if the notification was successful, false otherwise.
 */
bool transaction_notify_view_ready_by_geometry(struct wsm_view *view,
	double x, double y, int width, int height);

#endif
