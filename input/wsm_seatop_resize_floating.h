#ifndef WSM_SEATOP_RESIZE_FLOATING_H
#define WSM_SEATOP_RESIZE_FLOATING_H

#include <wlr/util/edges.h>

struct wsm_seat;
struct wsm_container;

void seatop_begin_resize_floating(struct wsm_seat *seat,
	struct wsm_container *con, enum wlr_edges edge);

#endif
