#ifndef WSM_SEATOP_MOVE_FLOATING_H
#define WSM_SEATOP_MOVE_FLOATING_H

struct wsm_seat;
struct wsm_container;

/**
 * @brief Begins the process of moving a floating container
 * @param seat Pointer to the wsm_seat instance associated with the move operation
 * @param con Pointer to the wsm_container that is to be moved
 */
void seatop_begin_move_floating(struct wsm_seat *seat,
	struct wsm_container *con);

#endif
