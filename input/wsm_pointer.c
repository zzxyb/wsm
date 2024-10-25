#include "wsm_pointer.h"
#include "wsm_seat.h"
#include "wsm_log.h"
#include "wsm_server.h"

#include <stdlib.h>

#include <libudev.h>

#include <wlr/types/wlr_pointer.h>
#include <wlr/backend/libinput.h>

struct wsm_pointer *wsm_pointer_create(struct wsm_seat *seat,
	struct wsm_seat_device *device) {
	struct wsm_pointer *pointer =
		calloc(1, sizeof(struct wsm_pointer));
	if (!pointer) {
		wsm_log(WSM_ERROR, "Could not create wsm_pointer: allocation failed!");
		return NULL;
	}

	struct wlr_input_device *wlr_device = device->input_device->input_device_wlr;
	pointer->pointer_wlr = wlr_pointer_from_input_device(wlr_device);
	pointer->seat_device_wsm = device;
	device->pointer = pointer;
	
	if (wlr_input_device_is_libinput(wlr_device)) {
		struct udev_device *udev_device = 
			libinput_device_get_udev_device(wlr_libinput_get_device_handle(wlr_device));
		if (udev_device_get_property_value(udev_device, "ID_INPUT_MOUSE")) {
			pointer->pointer_type = WSM_POINTER_TYPE_MOUSE;
		} else if (udev_device_get_property_value(udev_device, "ID_INPUT_TOUCHPAD")) {
			pointer->pointer_type = WSM_POINTER_TYPE_TOUCHPAD;
		} else if (udev_device_get_property_value(udev_device, "ID_INPUT_TRACKBALL")) {
			pointer->pointer_type = WSM_POINTER_TYPE_TRACKBALL;
		} else if (udev_device_get_property_value(udev_device, "ID_INPUT_POINTINGSTICK")) {
			pointer->pointer_type = WSM_POINTER_TYPE_POINTING_STICK;
		} else if (udev_device_get_property_value(udev_device, "ID_INPUT_JOYSTICK")) {
			pointer->pointer_type = WSM_POINTER_TYPE_JOYSTICK;
		} else {
			pointer->pointer_type = WSM_POINTER_TYPE_UNKNOWN;
		}
	}

	return pointer;
}

void wsm_pointer_destroy(struct wsm_pointer *pointer) {
	if (!pointer) {
		return;
	}

	free(pointer);
}