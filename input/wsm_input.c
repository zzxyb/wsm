#include "wsm_input.h"
#include "wsm_common.h"
#include "wsm_config.h"
#include "wsm_input_config.h"
#include "wsm_server.h"
#include "wsm_log.h"
#include "wsm_output.h"
#include "wsm_scene.h"
#include "wsm_seat.h"
#include "wsm_xwayland.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include <wlr/config.h>
#include <wlr/types/wlr_input_method_v2.h>
#include <wlr/backend/libinput.h>
#include <wlr/types/wlr_keyboard_shortcuts_inhibit_v1.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_xcursor_manager.h>
#include <wlr/types/wlr_virtual_keyboard_v1.h>
#include <wlr/types/wlr_virtual_pointer_v1.h>
#include <wlr/types/wlr_pointer_gestures_v1.h>

#if HAVE_XWAYLAND
#include <wlr/xwayland.h>
#endif

#define DEFAULT_SEAT "seat0"

static void handle_device_destroy(struct wl_listener *listener, void *data) {
	struct wlr_input_device *device = data;
	wsm_input_device_destroy(device);
}

static void handle_new_input(struct wl_listener *listener, void *data) {
	struct wsm_input_manager *input_manager =
		wl_container_of(listener, input_manager, new_input);
	struct wlr_input_device *device = data;

	struct wsm_input_device *input_device = wsm_input_device_create();
	device->data = input_device;

	input_device->input_device_wlr = device;
	input_device->identifier = input_device_get_identifier(device);
	wl_list_insert(&input_manager->devices, &input_device->link);

	struct wsm_seat *seat = NULL;
	wl_list_for_each(seat, &input_manager->seats, link) {
		seat_add_device(seat, input_device);
	}

	wsm_log(WSM_DEBUG, "adding device: '%s'",
		input_device->identifier);

	input_device->device_destroy.notify = handle_device_destroy;
	wl_signal_add(&device->events.destroy, &input_device->device_destroy);
}

static void handle_new_virtual_keyboard(struct wl_listener *listener, void *data) {

}

static void handle_new_virtual_pointer(struct wl_listener *listener, void *data) {

}

static void handle_keyboard_shortcuts_inhibit_new_inhibitor(
	struct wl_listener *listener, void *data) {

}

struct wsm_input_manager *wsm_input_manager_create(const struct wsm_server *server) {
	struct wsm_input_manager *input_manager = calloc(1, sizeof(struct wsm_input_manager));
	if (!input_manager) {
		wsm_log(WSM_ERROR, "Could not create wsm_input_manager: allocation failed!");
		return NULL;
	}

	wl_list_init(&input_manager->devices);
	wl_list_init(&input_manager->seats);

	input_manager->pointer_gestures_wlr = wlr_pointer_gestures_v1_create(server->wl_display);

	input_manager->new_input.notify = handle_new_input;
	wl_signal_add(&server->backend->events.new_input, &input_manager->new_input);

	input_manager->virtual_keyboard_manager_wlr =
		wlr_virtual_keyboard_manager_v1_create(server->wl_display);
	input_manager->virtual_keyboard_new.notify = handle_new_virtual_keyboard;
	wl_signal_add(&input_manager->virtual_keyboard_manager_wlr->events.new_virtual_keyboard,
		&input_manager->virtual_keyboard_new);

	input_manager->virtual_pointer_manager_wlr =
		wlr_virtual_pointer_manager_v1_create(server->wl_display);
	input_manager->virtual_pointer_new.notify = handle_new_virtual_pointer;
	wl_signal_add(&input_manager->virtual_pointer_manager_wlr->events.new_virtual_pointer,
		&input_manager->virtual_pointer_new);

	input_manager->keyboard_shortcuts_inhibit_wlr =
		wlr_keyboard_shortcuts_inhibit_v1_create(server->wl_display);
	input_manager->keyboard_shortcuts_inhibit_new_inhibitor.notify =
		handle_keyboard_shortcuts_inhibit_new_inhibitor;
	wl_signal_add(&input_manager->keyboard_shortcuts_inhibit_wlr->events.new_inhibitor,
		&input_manager->keyboard_shortcuts_inhibit_new_inhibitor);

	return input_manager;
}

void wsm_input_manager_detach_backend(struct wsm_input_manager *input_manager) {
	if (!input_manager) {
		return;
	}

	if (!wl_list_empty(&input_manager->new_input.link)) {
		wl_list_remove(&input_manager->new_input.link);
		wl_list_init(&input_manager->new_input.link);
	}
}

void wsm_input_manager_destroy(struct wsm_input_manager *input_manager) {
	if (!input_manager) {
		return;
	}

	wsm_input_manager_detach_backend(input_manager);

	struct wsm_seat *seat, *tmp_seat;
	wl_list_for_each_safe(seat, tmp_seat, &input_manager->seats, link) {
		wsm_seat_destroy(seat);
	}

	struct wsm_input_device *device, *tmp_device;
	wl_list_for_each_safe(device, tmp_device, &input_manager->devices, link) {
		wl_list_remove(&device->device_destroy.link);
		wl_list_remove(&device->link);
		free(device->identifier);
		free(device);
	}

	wl_list_remove(&input_manager->virtual_keyboard_new.link);
	wl_list_remove(&input_manager->virtual_pointer_new.link);
	wl_list_remove(&input_manager->keyboard_shortcuts_inhibit_new_inhibitor.link);

	free(input_manager);
}

struct wsm_seat *input_manager_get_default_seat(void) {
	return input_manager_get_seat(DEFAULT_SEAT, true);
}

struct wsm_seat *input_manager_current_seat(void) {
	return input_manager_get_default_seat();
}

struct wsm_seat *input_manager_get_seat(const char *seat_name, bool create) {
	if (!global_server.input_manager) {
		return NULL;
	}

	struct wsm_seat *seat = NULL;
	wl_list_for_each(seat, &global_server.input_manager->seats, link) {
		if (strcmp(seat->seat->name, seat_name) == 0) {
			return seat;
		}
	}

	return create ? wsm_seat_create(seat_name) : NULL;
}

struct wsm_seat *input_manager_seat_from_wlr_seat(struct wlr_seat *wlr_seat) {
	struct wsm_seat *seat = NULL;

	wl_list_for_each(seat, &global_server.input_manager->seats, link) {
		if (seat->seat == wlr_seat) {
			return seat;
		}
	}

	return NULL;
}

char *input_device_get_identifier(struct wlr_input_device *device) {
	int vendor = 0, product = 0;
#if WLR_HAS_LIBINPUT_BACKEND
	if (wlr_input_device_is_libinput(device)) {
		struct libinput_device *libinput_dev = wlr_libinput_get_device_handle(device);
		vendor = libinput_device_get_id_vendor(libinput_dev);
		product = libinput_device_get_id_product(libinput_dev);
	}
#endif
	char *name = strdup(device->name ? device->name : "");
	strip_whitespace(name);

	char *p = name;
	for (; *p; ++p) {
		if (*p == ' ' || !isprint(*p)) {
			*p = '_';
		}
	}

	char *identifier = format_str("%d:%d:%s", vendor, product, name);
	free(name);
	return identifier;
}

void input_manager_configure_xcursor(void) {
	if (!global_server.xcursor_manager) {
		global_server.xcursor_manager =
			wlr_xcursor_manager_create(NULL, 24);
		if (!global_server.xcursor_manager) {
			wsm_log(WSM_ERROR, "Could not create xcursor manager");
			return;
		}
	}

	if (global_server.scene && global_server.scene->outputs &&
			global_server.scene->outputs->length > 0) {
		for (int i = 0; i < global_server.scene->outputs->length; ++i) {
			struct wsm_output *output =
				global_server.scene->outputs->items[i];
			if (!wlr_xcursor_manager_load(global_server.xcursor_manager,
					output->wlr_output->scale)) {
				wsm_log(WSM_ERROR, "Could not load xcursor theme '%s' at scale %f",
					global_server.xcursor_manager->name ?
					global_server.xcursor_manager->name : "(default)",
					output->wlr_output->scale);
			}
		}
	} else if (!wlr_xcursor_manager_load(global_server.xcursor_manager, 1.0f)) {
		wsm_log(WSM_ERROR, "Could not load xcursor theme '%s'",
			global_server.xcursor_manager->name ?
			global_server.xcursor_manager->name : "(default)");
	}

#if HAVE_XWAYLAND
	if (global_server.xwayland.xwayland_wlr) {
		struct wlr_xcursor *xcursor =
			wlr_xcursor_manager_get_xcursor(global_server.xcursor_manager,
				"left_ptr", 1.0f);
		if (!xcursor) {
			xcursor = wlr_xcursor_manager_get_xcursor(
				global_server.xcursor_manager, "default", 1.0f);
		}
		if (xcursor && xcursor->image_count > 0) {
			struct wlr_xcursor_image *image = xcursor->images[0];
			struct wlr_buffer *buffer = wlr_xcursor_image_get_buffer(image);
			wlr_xwayland_set_cursor(global_server.xwayland.xwayland_wlr,
				buffer, image->hotspot_x, image->hotspot_y);
		} else {
			wsm_log(WSM_ERROR, "Could not load default XWayland cursor");
		}
	}
#endif

	struct wsm_seat *seat = NULL;
	wl_list_for_each(seat, &global_server.input_manager->seats, link) {
		seat_configure_xcursor(seat);
	}
}

void input_manager_set_focus(struct wsm_node *node) {
	struct wsm_seat *seat;
	wl_list_for_each(seat, &global_server.input_manager->seats, link) {
		seat_set_focus(seat, node);
		seat_consider_warp_to_focus(seat);
	}
}

void input_manager_configure_all_input_mappings(void) {
	struct wsm_input_device *input_device;
	wl_list_for_each(input_device, &global_server.input_manager->devices, link) {
		struct wsm_seat *seat;
		wl_list_for_each(seat, &global_server.input_manager->seats, link) {
			seat_configure_device_mapping(seat, input_device);
		}

#if WLR_HAS_LIBINPUT_BACKEND
		wsm_input_configure_libinput_device_send_events(input_device);
#endif
	}
}

struct input_config *input_device_get_config(struct wsm_input_device *device) {
	struct input_config *wildcard_config = NULL;
	struct input_config *input_config = NULL;
	for (int i = 0; i < global_config.input_configs->length; ++i) {
		input_config = global_config.input_configs->items[i];
		if (strcmp(input_config->identifier, device->identifier) == 0) {
			return input_config;
		} else if (strcmp(input_config->identifier, "*") == 0) {
			wildcard_config = input_config;
		}
	}

	const char *device_type = input_device_get_type(device);
	for (int i = 0; i < global_config.input_type_configs->length; ++i) {
		input_config = global_config.input_type_configs->items[i];
		if (strcmp(input_config->identifier + 5, device_type) == 0) {
			return input_config;
		}
	}

	return wildcard_config;
}

static bool device_is_touchpad(struct wsm_input_device *device) {
#if WLR_HAS_LIBINPUT_BACKEND
	if (device->input_device_wlr->type != WLR_INPUT_DEVICE_POINTER ||
		!wlr_input_device_is_libinput(device->input_device_wlr)) {
		return false;
	}

	struct libinput_device *libinput_device =
		wlr_libinput_get_device_handle(device->input_device_wlr);

	return libinput_device_config_tap_get_finger_count(libinput_device) > 0;
#else
	return false;
#endif
}

const char *input_device_get_type(struct wsm_input_device *device) {
	switch (device->input_device_wlr->type) {
	case WLR_INPUT_DEVICE_POINTER:
		if (device_is_touchpad(device)) {
			return "touchpad";
		} else {
			return "pointer";
		}
	case WLR_INPUT_DEVICE_KEYBOARD:
		return "keyboard";
	case WLR_INPUT_DEVICE_TOUCH:
		return "touch";
	case WLR_INPUT_DEVICE_TABLET:
		return "tablet_tool";
	case WLR_INPUT_DEVICE_TABLET_PAD:
		return "tablet_pad";
	case WLR_INPUT_DEVICE_SWITCH:
		return "switch";
	}
	return "unknown";
}

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
	if (!input_device) {
		wsm_log(WSM_ERROR, "Could not create wsm_input_device: allocation failed!");
		return NULL;
	}
	input_device->input_device_impl_wsm = &input_device_impl;
	return input_device;
}

struct wsm_input_device *input_wsm_device_from_wlr(struct wlr_input_device *device) {
	struct wsm_input_device *input_device = NULL;
	wl_list_for_each(input_device, &global_server.input_manager->devices, link) {
		if (input_device->input_device_wlr == device) {
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
	wl_list_for_each(seat, &global_server.input_manager->seats, link) {
		seat_remove_device(seat, input_device);
	}

	wl_list_remove(&input_device->link);
	wl_list_remove(&input_device->device_destroy.link);
	input_device->input_device_impl_wsm = NULL;
	free(input_device->identifier);
	free(input_device);
}

void wsm_input_configure_libinput_device_send_events(
	struct wsm_input_device *input_device) {
	if (!wlr_input_device_is_libinput(input_device->input_device_wlr)) {
		return;
	}
}
