#ifndef WSM_SEATOP_DOWN_H
#define WSM_SEATOP_DOWN_H

struct wlr_surface;
struct wlr_touch_down_event;

struct wsm_seat;
struct wsm_container;

void seatop_begin_down(struct wsm_seat *seat, struct wsm_container *con,
	double sx, double sy);
void seatop_begin_down_on_surface(struct wsm_seat *seat,
	struct wlr_surface *surface, double sx, double sy);

void seatop_begin_touch_down(struct wsm_seat *seat, struct wlr_surface *surface,
	struct wlr_touch_down_event *event, double sx, double sy, double lx, double ly);

#endif
