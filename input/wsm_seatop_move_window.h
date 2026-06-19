#ifndef WSM_SEATOP_MOVE_WINDOW_H
#define WSM_SEATOP_MOVE_WINDOW_H

struct wsm_seat;
struct wsm_window;

/**
 * @brief Begins the process of moving a window
 * @param seat Pointer to the wsm_seat instance associated with the move operation
 * @param window Pointer to the wsm_window that is to be moved
 */
void seatop_begin_move_window(struct wsm_seat *seat,
	struct wsm_window *window);

#endif
