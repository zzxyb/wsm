#ifndef WSM_POINTER_H
#define WSM_POINTER_H

struct wlr_pointer;

struct wsm_seat;
struct wsm_seat_device;

enum wsm_pointer_type {
	WSM_POINTER_TYPE_UNKNOWN,
	WSM_POINTER_TYPE_MOUSE,
	WSM_POINTER_TYPE_TOUCHPAD,
	WSM_POINTER_TYPE_TRACKBALL,
	WSM_POINTER_TYPE_JOYSTICK,
	WSM_POINTER_TYPE_POINTING_STICK,
};

struct wsm_pointer {
	struct wlr_pointer *pointer_wlr;
	struct wsm_seat_device *seat_device_wsm;

	enum wsm_pointer_type pointer_type;
};

struct wsm_pointer *wsm_pointer_create(struct wsm_seat *seat,
	struct wsm_seat_device *device);
void wsm_pointer_destroy(struct wsm_pointer *pointer);

#endif
