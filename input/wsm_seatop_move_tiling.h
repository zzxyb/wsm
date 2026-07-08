#ifndef WSM_SEATOP_MOVE_TILING_H
#define WSM_SEATOP_MOVE_TILING_H

struct wsm_container;
struct wsm_seat;

void seatop_begin_move_tiling(struct wsm_seat *seat,
	struct wsm_container *con);
void seatop_begin_move_tiling_threshold(struct wsm_seat *seat,
	struct wsm_container *con);
void seatop_begin_move_tiling_to_floating(struct wsm_seat *seat,
	struct wsm_container *con);

#endif
