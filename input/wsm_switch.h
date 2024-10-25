#ifndef WSM_SWITCH_H
#define WSM_SWITCH_H

#include <wlr/types/wlr_switch.h>

struct wsm_seat;
struct wsm_seat_device;

struct wsm_switch {
	struct wl_listener switch_toggle;

	struct wsm_seat_device *seat_device;
	struct wlr_switch *switch_wlr;
	enum wlr_switch_state state;
	enum wlr_switch_type type;
};

struct wsm_switch *wsm_switch_create(struct wsm_seat *seat,
	struct wsm_seat_device *device);
void wsm_switch_configure(struct wsm_switch *wsm_switch);
void wsm_switch_destroy(struct wsm_switch *wsm_switch);

#endif
