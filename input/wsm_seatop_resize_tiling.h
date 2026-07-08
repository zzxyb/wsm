#ifndef WSM_SEATOP_RESIZE_TILING_H
#define WSM_SEATOP_RESIZE_TILING_H

#include <stdbool.h>

#include <wlr/util/edges.h>

struct wsm_container;
struct wsm_node;
struct wsm_seat;

bool seatop_begin_resize_tiling(struct wsm_seat *seat,
	struct wsm_container *con, enum wlr_edges edge);
bool seatop_begin_resize_tiling_at(struct wsm_seat *seat, double lx, double ly);
bool seatop_can_resize_tiling_at_node(struct wsm_node *node);
enum wlr_edges seatop_resize_tiling_at(struct wsm_seat *seat, double lx, double ly);

#endif
