/*
MIT License

Copyright (c) 2024 YaoBing Xiao

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

#include "wsm_input.h"
#include "wsm_input_manager.h"
#include "wsm_server.h"
#include "wsm_log.h"
#include "wsm_seat.h"

#include <wlr/types/wlr_input_method_v2.h>
#include <wlr/backend/libinput.h>

static enum libinput_config_status handle_set_send_events(struct libinput_device *device, uint32_t mode) {
	if (libinput_device_config_send_events_get_mode(device) == mode) {
		wsm_log(WSM_ERROR, "handle_set_send_events redundantly!");
		return LIBINPUT_CONFIG_STATUS_INVALID;
	}
	wsm_log(WSM_DEBUG, "send_events_set_mode(%" PRIu32 ")", mode);
	enum libinput_config_status status = libinput_device_config_send_events_set_mode(device, mode);
	return status;
}

static enum libinput_config_status handle_set_tap(struct libinput_device *device,
	enum libinput_config_tap_state tap) {
	if (libinput_device_config_tap_get_finger_count(device) <= 0 ||
		libinput_device_config_tap_get_enabled(device) == tap) {
		wsm_log(WSM_ERROR, "handle_set_tap redundantly!");
		return LIBINPUT_CONFIG_STATUS_INVALID;
	}
	wsm_log(WSM_DEBUG, "tap_set_enabled(%d)", tap);
	enum libinput_config_status status = libinput_device_config_tap_set_enabled(device, tap);
	return status;
}

static enum libinput_config_status handle_set_tap_button_map(
	struct libinput_device *device, enum libinput_config_tap_button_map map) {
	if (libinput_device_config_tap_get_finger_count(device) <= 0 ||
		libinput_device_config_tap_get_button_map(device) == map) {
		wsm_log(WSM_ERROR, "handle_set_tap_button_map redundantly!");
		return LIBINPUT_CONFIG_STATUS_INVALID;
	}
	wsm_log(WSM_DEBUG, "tap_set_button_map(%d)", map);
	enum libinput_config_status status = libinput_device_config_tap_set_button_map(device, map);
	return status;
}

static enum libinput_config_status handle_set_tap_drag(
	struct libinput_device *device, enum libinput_config_drag_state drag) {
	if (libinput_device_config_tap_get_finger_count(device) <= 0 ||
		libinput_device_config_tap_get_drag_enabled(device) == drag) {
		wsm_log(WSM_ERROR, "handle_set_tap_drag redundantly!");
		return LIBINPUT_CONFIG_STATUS_INVALID;
	}
	wsm_log(WSM_DEBUG, "tap_set_drag_enabled(%d)", drag);
	enum libinput_config_status status = libinput_device_config_tap_set_drag_enabled(device, drag);
	return status;
}

static enum libinput_config_status handle_set_tap_drag_lock(
	struct libinput_device *device, enum libinput_config_drag_lock_state lock) {
	if (libinput_device_config_tap_get_finger_count(device) <= 0 ||
		libinput_device_config_tap_get_drag_lock_enabled(device) == lock) {
		wsm_log(WSM_ERROR, "handle_set_tap_drag_lock redundantly!");
		return LIBINPUT_CONFIG_STATUS_INVALID;
	}
	wsm_log(WSM_DEBUG, "tap_set_drag_lock_enabled(%d)", lock);
	enum libinput_config_status status = libinput_device_config_tap_set_drag_lock_enabled(device, lock);
	return status;
}

static enum libinput_config_status handle_set_accel_speed(struct libinput_device *device, double speed) {
	if (!libinput_device_config_accel_is_available(device) ||
		libinput_device_config_accel_get_speed(device) == speed) {
		wsm_log(WSM_ERROR, "handle_set_accel_speed redundantly!");
		return LIBINPUT_CONFIG_STATUS_INVALID;
	}
	wsm_log(WSM_DEBUG, "accel_set_speed(%f)", speed);
	enum libinput_config_status status = libinput_device_config_accel_set_speed(device, speed);
	return status;
}

static enum libinput_config_status handle_set_rotation_angle(struct libinput_device *device, double angle) {
	if (!libinput_device_config_rotation_is_available(device) ||
		libinput_device_config_rotation_get_angle(device) == angle) {
		wsm_log(WSM_ERROR, "handle_set_rotation_angle redundantly!");
		return LIBINPUT_CONFIG_STATUS_INVALID;
	}
	wsm_log(WSM_DEBUG, "rotation_set_angle(%f)", angle);
	enum libinput_config_status status = libinput_device_config_rotation_set_angle(device, angle);
	return status;
}

static enum libinput_config_status handle_set_accel_profile(
	struct libinput_device *device, enum libinput_config_accel_profile profile) {
	if (!libinput_device_config_accel_is_available(device) ||
		libinput_device_config_accel_get_profile(device) == profile) {
		wsm_log(WSM_ERROR, "handle_set_accel_profile redundantly!");
		return LIBINPUT_CONFIG_STATUS_INVALID;
	}
	wsm_log(WSM_DEBUG, "accel_set_profile(%d)", profile);
	enum libinput_config_status status = libinput_device_config_accel_set_profile(device, profile);
	return status;
}

static enum libinput_config_status handle_set_natural_scroll(struct libinput_device *d, bool n) {
	if (!libinput_device_config_scroll_has_natural_scroll(d) ||
		libinput_device_config_scroll_get_natural_scroll_enabled(d) == n) {
		wsm_log(WSM_ERROR, "handle_set_natural_scroll redundantly!");
		return LIBINPUT_CONFIG_STATUS_INVALID;
	}
	wsm_log(WSM_DEBUG, "scroll_set_natural_scroll(%d)", n);
	enum libinput_config_status status = libinput_device_config_scroll_set_natural_scroll_enabled(d, n);
	return status;
}

static enum libinput_config_status handle_set_left_handed(struct libinput_device *device, bool left) {
	if (!libinput_device_config_left_handed_is_available(device) ||
		libinput_device_config_left_handed_get(device) == left) {
		wsm_log(WSM_ERROR, "handle_set_left_handed redundantly!");
		return LIBINPUT_CONFIG_STATUS_INVALID;
	}
	wsm_log(WSM_DEBUG, "left_handed_set(%d)", left);
	enum libinput_config_status status = libinput_device_config_left_handed_set(device, left);
	return status;
}

static enum libinput_config_status handle_set_click_method(
	struct libinput_device *device, enum libinput_config_click_method method) {
	uint32_t click = libinput_device_config_click_get_methods(device);
	if ((click & ~LIBINPUT_CONFIG_CLICK_METHOD_NONE) == 0 ||
		libinput_device_config_click_get_method(device) == method) {
		wsm_log(WSM_ERROR, "handle_set_click_method redundantly!");
		return LIBINPUT_CONFIG_STATUS_INVALID;
	}
	wsm_log(WSM_DEBUG, "click_set_method(%d)", method);
	enum libinput_config_status status = libinput_device_config_click_set_method(device, method);
	return status;
}

static enum libinput_config_status handle_set_middle_emulation(
	struct libinput_device *dev, enum libinput_config_middle_emulation_state mid) {
	if (!libinput_device_config_middle_emulation_is_available(dev) ||
		libinput_device_config_middle_emulation_get_enabled(dev) == mid) {
		wsm_log(WSM_ERROR, "handle_set_middle_emulation redundantly!");
		return LIBINPUT_CONFIG_STATUS_INVALID;
	}
	wsm_log(WSM_DEBUG, "middle_emulation_set_enabled(%d)", mid);
	enum libinput_config_status status = libinput_device_config_middle_emulation_set_enabled(dev, mid);
	return status;
}

static enum libinput_config_status handle_set_scroll_method(
	struct libinput_device *device, enum libinput_config_scroll_method method) {
	uint32_t scroll = libinput_device_config_scroll_get_methods(device);
	if ((scroll & ~LIBINPUT_CONFIG_SCROLL_NO_SCROLL) == 0 ||
		libinput_device_config_scroll_get_method(device) == method) {
		wsm_log(WSM_ERROR, "handle_set_scroll_method redundantly!");
		return LIBINPUT_CONFIG_STATUS_INVALID;
	}
	wsm_log(WSM_DEBUG, "scroll_set_method(%d)", method);
	enum libinput_config_status status = libinput_device_config_scroll_set_method(device, method);
	return status;
}

static enum libinput_config_status handle_set_scroll_button(struct libinput_device *dev, uint32_t button) {
	uint32_t scroll = libinput_device_config_scroll_get_methods(dev);
	if ((scroll & ~LIBINPUT_CONFIG_SCROLL_NO_SCROLL) == 0 ||
		libinput_device_config_scroll_get_button(dev) == button) {
		wsm_log(WSM_ERROR, "handle_set_scroll_button redundantly!");
		return LIBINPUT_CONFIG_STATUS_INVALID;
	}
	wsm_log(WSM_DEBUG, "scroll_set_button(%" PRIu32 ")", button);
	enum libinput_config_status status = libinput_device_config_scroll_set_button(dev, button);
	return status;
}

static enum libinput_config_status handle_set_scroll_button_lock(
	struct libinput_device *dev, enum libinput_config_scroll_button_lock_state lock) {
	uint32_t scroll = libinput_device_config_scroll_get_methods(dev);
	if ((scroll & ~LIBINPUT_CONFIG_SCROLL_NO_SCROLL) == 0 ||
		libinput_device_config_scroll_get_button_lock(dev) == lock) {
		wsm_log(WSM_ERROR, "handle_set_scroll_button_lock redundantly!");
		return LIBINPUT_CONFIG_STATUS_INVALID;
	}
	wsm_log(WSM_DEBUG, "scroll_set_button_lock(%" PRIu32 ")", lock);
	enum libinput_config_status status = libinput_device_config_scroll_set_button_lock(dev, lock);
	return status;
}

static enum libinput_config_status handle_set_dwt(struct libinput_device *device, bool dwt) {
	if (!libinput_device_config_dwt_is_available(device) ||
		libinput_device_config_dwt_get_enabled(device) == dwt) {
		wsm_log(WSM_ERROR, "handle_set_dwt redundantly!");
		return LIBINPUT_CONFIG_STATUS_INVALID;
	}
	wsm_log(WSM_DEBUG, "dwt_set_enabled(%d)", dwt);
	enum libinput_config_status status = libinput_device_config_dwt_set_enabled(device, dwt);
	return status;
}

static enum libinput_config_status handle_set_dwtp(struct libinput_device *device, bool dwtp) {
	if (!libinput_device_config_dwtp_is_available(device) ||
		libinput_device_config_dwtp_get_enabled(device) == dwtp) {
		wsm_log(WSM_ERROR, "handle_set_dwtp redundantly!");
		return LIBINPUT_CONFIG_STATUS_INVALID;
	}
	wsm_log(WSM_DEBUG, "dwtp_set_enabled(%d)", dwtp);
	enum libinput_config_status status = libinput_device_config_dwtp_set_enabled(device, dwtp);
	return status;
}

static enum libinput_config_status handle_set_calibration_matrix(struct libinput_device *dev, float mat[6]) {
	if (!libinput_device_config_calibration_has_matrix(dev)) {
		wsm_log(WSM_ERROR, "handle_set_calibration_matrix redundantly!");
		return LIBINPUT_CONFIG_STATUS_INVALID;
	}
	bool changed = false;
	float current[6];
	libinput_device_config_calibration_get_matrix(dev, current);
	for (int i = 0; i < 6; i++) {
		if (current[i] != mat[i]) {
			changed = true;
			break;
		}
	}
	if (changed) {
		wsm_log(WSM_DEBUG, "calibration_set_matrix(%f, %f, %f, %f, %f, %f)",
			mat[0], mat[1], mat[2], mat[3], mat[4], mat[5]);
		enum libinput_config_status status = libinput_device_config_calibration_set_matrix(dev, mat);
		return status;
	}
	return LIBINPUT_CONFIG_STATUS_INVALID;
}

static const struct wsm_input_device_impl input_device_impl = {
	.set_send_events = handle_set_send_events,
	.set_tap = handle_set_tap,
	.set_tap_button_map = handle_set_tap_button_map,
	.set_tap_drag = handle_set_tap_drag,
	.set_tap_drag_lock = handle_set_tap_drag_lock,
	.set_accel_speed = handle_set_accel_speed,
	.set_rotation_angle = handle_set_rotation_angle,
	.set_accel_profile = handle_set_accel_profile,
	.set_natural_scroll = handle_set_natural_scroll,
	.set_left_handed = handle_set_left_handed,
	.set_click_method = handle_set_click_method,
	.set_middle_emulation = handle_set_middle_emulation,
	.set_scroll_method = handle_set_scroll_method,
	.set_scroll_button = handle_set_scroll_button,
	.set_scroll_button_lock = handle_set_scroll_button_lock,
	.set_dwt = handle_set_dwt,
	.set_dwtp = handle_set_dwtp,
	.set_calibration_matrix = handle_set_calibration_matrix,
};

struct wsm_input_device *wsm_input_device_create() {
	struct wsm_input_device *input_device =
		calloc(1, sizeof(struct wsm_input_device));
	if (!wsm_assert(input_device, "Could not create wsm_input_device: allocation failed!")) {
		return NULL;
	}
	input_device->input_device_impl = &input_device_impl;
	return input_device;
}

struct wsm_input_device *input_wsm_device_from_wlr(struct wlr_input_device *device) {
	struct wsm_input_device *input_device = NULL;
	wl_list_for_each(input_device, &global_server.wsm_input_manager->devices, link) {
		if (input_device->wlr_device == device) {
			return input_device;
		}
	}
	return NULL;
}

void wsm_input_device_destroy(struct wlr_input_device *wlr_device) {
	struct wsm_input_device *input_device = input_wsm_device_from_wlr(wlr_device);

	if (!wsm_assert(input_device, "could not find wsm_input_device")) {
		return;
	}

	wsm_log(WSM_DEBUG, "removing device: '%s'", input_device->identifier);

	struct wsm_seat *seat = NULL;
	wl_list_for_each(seat, &global_server.wsm_input_manager->seats, link) {
		seat_remove_device(seat, input_device);
	}

	wl_list_remove(&input_device->link);
	wl_list_remove(&input_device->device_destroy.link);
	input_device->input_device_impl = NULL;
	free(input_device->identifier);
	free(input_device);
}

void wsm_input_configure_libinput_device_send_events(
	struct wsm_input_device *input_device) {
	if (!wlr_input_device_is_libinput(input_device->wlr_device)) {
		return;
	}
}
