#include "wsm_log.h"
#include "wsm_seat.h"
#include "wsm_switch.h"
#include "wsm_input_manager.h"

#include <stdlib.h>

struct wsm_switch *wsm_switch_create(struct wsm_seat *seat, struct wsm_seat_device *device) {
	struct wsm_switch *_switch = calloc(1, sizeof(struct wsm_switch));
	if (!_switch) {
		wsm_log(WSM_ERROR, "Could not create switch_device: allocation failed!");
		return NULL;
	}

	_switch->switch_wlr = wlr_switch_from_input_device(device->input_device->input_device_wlr);
	_switch->seat_device = device;
	_switch->state = WLR_SWITCH_STATE_OFF;
	device->_switch = _switch;

	wl_list_init(&_switch->switch_toggle.link);

	return _switch;
}

static void execute_binding(struct wsm_switch *wsm_switch) {

}

static void handle_switch_toggle(struct wl_listener *listener, void *data) {
	struct wsm_switch *switch_device =
		wl_container_of(listener, switch_device, switch_toggle);
	struct wlr_switch_toggle_event *event = data;
	struct wsm_seat *seat = switch_device->seat_device->seat;
	seat_idle_notify_activity(seat, WLR_INPUT_DEVICE_SWITCH);

	struct wlr_input_device *wlr_device =
		switch_device->seat_device->input_device->input_device_wlr;
	char *device_identifier = input_device_get_identifier(wlr_device);
	wsm_log(WSM_DEBUG, "%s: type %d state %d", device_identifier,
		event->switch_type, event->switch_state);
	free(device_identifier);

	switch_device->type = event->switch_type;
	switch_device->state = event->switch_state;
	execute_binding(switch_device);
}

void wsm_switch_configure(struct wsm_switch *wsm_switch) {
	wl_list_remove(&wsm_switch->switch_toggle.link);

	wsm_switch->switch_toggle.notify = handle_switch_toggle;
	wl_signal_add(&wsm_switch->switch_wlr->events.toggle,
		&wsm_switch->switch_toggle);

	wsm_log(WSM_DEBUG, "Configured switch for device");
}

void wsm_switch_destroy(struct wsm_switch *wsm_switch) {
	if (!wsm_switch) {
		wsm_log(WSM_ERROR, "wsm_switch is NULL!");
		return;
	}
	wl_list_remove(&wsm_switch->switch_toggle.link);
	free(wsm_switch);
}
