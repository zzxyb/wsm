#ifndef WSM_SEATOP_RESIZE_WINDOW_H
#define WSM_SEATOP_RESIZE_WINDOW_H

#include <stdbool.h>
#include <wlr/util/edges.h>

struct wsm_seat;
struct wsm_window;

/**
 * @brief Begins the process of resizing a window
 * @param seat Pointer to the wsm_seat instance associated with the resize operation
 * @param window Pointer to the wsm_window that is to be resized
 * @param edge Enum value representing the edge to resize from (e.g., top, bottom, left, right)
 */
void seatop_begin_resize_window(struct wsm_seat *seat,
	struct wsm_window *window, enum wlr_edges edge);
void seatop_begin_resize_window_locked(struct wsm_seat *seat,
	struct wsm_window *window, enum wlr_edges edge,
	bool lock_width, bool lock_height);

bool seatop_resize_window_is_active(struct wsm_window *window);
bool seatop_resize_window_update_position(struct wsm_window *window);
bool seatop_resize_window_get_anchor(struct wsm_window *window,
	enum wlr_edges *edges, double *geo_right, double *geo_bottom);
bool seatop_resize_window_is_deferred(struct wsm_window *window);
bool seatop_resize_window_deferred_commit(struct wsm_window *window);

#endif
