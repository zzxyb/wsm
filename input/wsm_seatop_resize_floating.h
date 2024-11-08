#ifndef WSM_SEATOP_RESIZE_FLOATING_H
#define WSM_SEATOP_RESIZE_FLOATING_H

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

#endif
