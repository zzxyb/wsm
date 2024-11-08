#ifndef WSM_SEATOP_DOWN_H
#define WSM_SEATOP_DOWN_H

struct wlr_surface;
struct wlr_touch_down_event;

struct wsm_seat;
struct wsm_container;

/**
 * @brief Begins a down event for the specified seat and container
 * @param seat Pointer to the wsm_seat instance
 * @param con Pointer to the wsm_container associated with the down event
 * @param sx X coordinate of the down event in surface coordinates
 * @param sy Y coordinate of the down event in surface coordinates
 */
void seatop_begin_down(struct wsm_seat *seat, struct wsm_container *con,
	double sx, double sy);

/**
 * @brief Begins a down event on the specified surface for the seat
 * @param seat Pointer to the wsm_seat instance
 * @param surface Pointer to the wlr_surface associated with the down event
 * @param sx X coordinate of the down event in surface coordinates
 * @param sy Y coordinate of the down event in surface coordinates
 */
void seatop_begin_down_on_surface(struct wsm_seat *seat,
	struct wlr_surface *surface, double sx, double sy);

/**
 * @brief Begins a touch down event for the specified seat and surface
 * @param seat Pointer to the wsm_seat instance
 * @param surface Pointer to the wlr_surface associated with the touch event
 * @param event Pointer to the wlr_touch_down_event containing touch details
 * @param sx X coordinate of the touch down event in surface coordinates
 * @param sy Y coordinate of the touch down event in surface coordinates
 * @param lx X coordinate of the touch down event in layout coordinates
 * @param ly Y coordinate of the touch down event in layout coordinates
 */
void seatop_begin_touch_down(struct wsm_seat *seat, struct wlr_surface *surface,
	struct wlr_touch_down_event *event, double sx, double sy, double lx, double ly);

#endif
