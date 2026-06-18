#ifndef WSM_SEATOP_RESIZE_FLOATING_H
#define WSM_SEATOP_RESIZE_FLOATING_H

#include <stdbool.h>
#include <wlr/util/edges.h>

struct wsm_seat;
struct wsm_container;

/**
 * @brief Begins the process of resizing a floating container
 * @param seat Pointer to the wsm_seat instance associated with the resize operation
 * @param con Pointer to the wsm_container that is to be resized
 * @param edge Enum value representing the edge to resize from (e.g., top, bottom, left, right)
 */
void seatop_begin_resize_floating(struct wsm_seat *seat,
	struct wsm_container *con, enum wlr_edges edge);
void seatop_begin_resize_floating_locked(struct wsm_seat *seat,
	struct wsm_container *con, enum wlr_edges edge,
	bool lock_width, bool lock_height);

bool seatop_resize_floating_is_active(struct wsm_container *con);
bool seatop_resize_floating_update_position(struct wsm_container *con);
bool seatop_resize_floating_get_anchor(struct wsm_container *con,
	enum wlr_edges *edges, double *geo_right, double *geo_bottom);
bool seatop_resize_floating_is_deferred(struct wsm_container *con);
bool seatop_resize_floating_deferred_commit(struct wsm_container *con);

#endif
