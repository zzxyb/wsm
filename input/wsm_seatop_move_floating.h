#ifndef WSM_SEATOP_MOVE_FLOATING_H
#define WSM_SEATOP_MOVE_FLOATING_H

struct wsm_seat;
struct wsm_container;

void seatop_begin_move_floating(struct wsm_seat *seat,
	struct wsm_container *con);

#endif
