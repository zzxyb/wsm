#ifndef WSM_WINDOW_SNAP_H
#define WSM_WINDOW_SNAP_H

#include <stdbool.h>
#include <wlr/util/edges.h>

struct wsm_window;
struct wsm_seat;
struct wsm_window_snap;
struct wsm_window_snap_divider;

/*
 * Supported snap layouts:
 *
 * Single snapped window:
 * ------------------------
 * |                      |
 * |       snapped        |
 * |       window         |
 * |                      |
 * ------------------------
 *
 * Two windows split left/right:
 * --------------------------------------------
 * |                  snap                    |
 * | ****************** |  ****************** |
 * | *                * |  *                * |
 * | *   window 1     * |  *   window 2     * |
 * | *                * |  *                * |
 * | ****************** |  ****************** |
 * --------------------------------------------
 *
 * Two windows split top/bottom:
 * ------------------------
 * |       snap           |
 * | ******************   |
 * | *                *   |
 * | *   window 1     *   |
 * | *                *   |
 * | ******************   |
 * | ------split-------   |
 * | ******************   |
 * | *                *   |
 * | *   window 2     *   |
 * | *                *   |
 * | ******************   |
 * ------------------------
 *
 * Three windows, primary on the left:
 * --------------------------------------------
 * |                  snap                    |
 * | ****************** |  ****************** |
 * | *                * |  *                * |
 * | *                * |  *   window 2     * |
 * | *                * |  *                * |
 * | *                * |  ****************** |
 * | *                * |                     |
 * | *   window 1     * |  ---------split-----|
 * | *                * |                     |
 * | *                * |  ****************** |
 * | *                * |  *                * |
 * | *                * |  *   window 3     * |
 * | *                * |  *                * |
 * | ****************** |  ****************** |
 * --------------------------------------------
 *
 * Three windows, primary on the right:
 * --------------------------------------------
 * |                  snap                    |
 * | ****************** |  ****************** |
 * | *                * |  *                * |
 * | *   window 2     * |  *                * |
 * | *                * |  *                * |
 * | ****************** |  *                * |
 * |                    |  *                * |
 * | ------split--------|  *   window 1     * |
 * |                    |  *                * |
 * | ****************** |  *                * |
 * | *                * |  *                * |
 * | *   window 3     * |  *                * |
 * | *                * |  *                * |
 * | ****************** |  ****************** |
 * --------------------------------------------
 *
 * Four windows in a grid:
 * --------------------------------------------
 * |                  snap                    |
 * | ****************** |  ****************** |
 * | *                * |  *                * |
 * | *   window 1     * |  *   window 3     * |
 * | *                * |  *                * |
 * | ****************** |  ****************** |
 * |                                          |
 * | ------split--------   --------split----- |
 * |                  split                   |
 * | ****************** |  ****************** |
 * | *                * |  *                * |
 * | *   window 2     * |  *   window 4     * |
 * | *                * |  *                * |
 * | ****************** |  ****************** |
 * --------------------------------------------
 */

struct wsm_window_snap *wsm_window_snap_create(void);
void wsm_window_snap_destroy(struct wsm_window_snap *snap);

bool wsm_window_snap_update(struct wsm_window_snap *snap, double lx, double ly);
bool wsm_window_snap_apply(struct wsm_window_snap *snap,
	struct wsm_window *window);
void wsm_window_snap_forget_window(struct wsm_window *window);
void wsm_window_snap_set_dividers_visible(bool visible);

struct wsm_window_snap_divider *wsm_window_snap_divider_at(double lx, double ly);
struct wsm_window_snap_divider *wsm_window_snap_divider_for_window_edge(
	struct wsm_window *window, int edge, double lx, double ly);
bool wsm_window_snap_constrain_inner_edge(struct wsm_window *window,
	enum wlr_edges *edge, bool *lock_width, bool *lock_height);
const char *wsm_window_snap_divider_cursor(
	struct wsm_window_snap_divider *divider);
void wsm_window_snap_begin_resize_divider(struct wsm_seat *seat,
	struct wsm_window_snap_divider *divider);

#endif
