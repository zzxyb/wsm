#include "wsm_input_memory.h"

#include "wsm_input.h"
#include "wsm_log.h"
#include "wsm_pointer.h"
#include "wsm_toml.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libinput.h>
#include <wlr/backend/libinput.h>
#include <wlr/types/wlr_input_device.h>

static struct wsm_toml *config;
static char *config_path;
static bool initialized;

static bool ensure_keyboard_group(void) {
	bool changed = false;
	int64_t integer;
	bool boolean;

	if (!wsm_toml_get_int(
			config, "keyboard_group.repeat_info.rate", &integer)) {
		if (!wsm_toml_get_int(config, "keyboard_group.repeat_rate",
				&integer)) {
			integer = 25;
		}
		if (!wsm_toml_set_int(
				config, "keyboard_group.repeat_info.rate", integer)) {
			return false;
		}
		changed = true;
	}
	if (!wsm_toml_get_int(
			config, "keyboard_group.repeat_info.delay", &integer)) {
		if (!wsm_toml_get_int(config, "keyboard_group.repeat_delay",
				&integer)) {
			integer = 600;
		}
		if (!wsm_toml_set_int(
				config, "keyboard_group.repeat_info.delay", integer)) {
			return false;
		}
		changed = true;
	}
	if (!wsm_toml_get_bool(
			config, "keyboard_group.num_lock_on_startup", &boolean)) {
		if (!wsm_toml_get_bool(
				config, "keyboard_group.numlock", &boolean)) {
			boolean = false;
		}
		if (!wsm_toml_set_bool(config,
				"keyboard_group.num_lock_on_startup", boolean)) {
			return false;
		}
		changed = true;
	}

	const char *legacy_keys[] = {
		"keyboard_group.repeat_rate",
		"keyboard_group.repeat_delay",
		"keyboard_group.numlock",
	};
	for (size_t i = 0; i < sizeof(legacy_keys) / sizeof(legacy_keys[0]);
			i++) {
		if (wsm_toml_remove(config, legacy_keys[i])) {
			changed = true;
		}
	}
	return !changed || wsm_toml_save(config, config_path);
}

static bool initialize(void) {
	if (initialized) {
		return config != NULL;
	}
	initialized = true;
	const char *home = getenv("HOME");
	if (home == NULL || *home == '\0') {
		return false;
	}
	const char suffix[] = "/.config/wsm/inputs.toml";
	config_path = malloc(strlen(home) + sizeof(suffix));
	if (config_path == NULL) {
		return false;
	}
	sprintf(config_path, "%s%s", home, suffix);
	char error[256];
	config = wsm_toml_load(config_path, error, sizeof(error));
	if (config == NULL) {
		wsm_log(WSM_ERROR, "Cannot load input configuration %s: %s",
			config_path, error);
		return false;
	}
	if (!ensure_keyboard_group()) {
		wsm_log(WSM_ERROR, "Cannot initialize keyboard configuration %s: %s",
			config_path, strerror(errno));
		wsm_toml_destroy(config);
		config = NULL;
		return false;
	}
	return true;
}

static bool make_key(char *key, size_t size, const char *type, const char *id,
	const char *field) {
	int length = snprintf(key, size, "%s.%s.%s", type, id, field);
	return length >= 0 && (size_t)length < size;
}

static bool get_int(const char *key, int64_t fallback, int64_t *value) {
	if (wsm_toml_get_int(config, key, value)) {
		return true;
	}
	*value = fallback;
	return false;
}

static bool get_bool(const char *key, bool fallback, bool *value) {
	if (wsm_toml_get_bool(config, key, value)) {
		return true;
	}
	*value = fallback;
	return false;
}

int wsm_input_memory_get_repeat_rate(void) {
	if (!initialize()) {
		return 25;
	}
	int64_t value;
	get_int("keyboard_group.repeat_info.rate", 25, &value);
	return value > 0 && value <= 1000 ? value : 25;
}

int wsm_input_memory_get_repeat_delay(void) {
	if (!initialize()) {
		return 600;
	}
	int64_t value;
	get_int("keyboard_group.repeat_info.delay", 600, &value);
	return value >= 0 && value <= 10000 ? value : 600;
}

bool wsm_input_memory_get_numlock(void) {
	if (!initialize()) {
		return false;
	}
	bool value;
	get_bool("keyboard_group.num_lock_on_startup", false, &value);
	return value;
}

static bool copy_double(const char *type, const char *id, const char *field,
	double fallback, double *value) {
	char key[1024];
	if (!make_key(key, sizeof(key), type, id, field)) {
		return false;
	}
	if (!wsm_toml_get_double(config, key, value)) {
		*value = fallback;
		return wsm_toml_set_double(config, key, *value);
	}
	return true;
}

static bool copy_bool(const char *type, const char *id, const char *field,
	bool fallback, bool *value) {
	char key[1024];
	if (!make_key(key, sizeof(key), type, id, field)) {
		return false;
	}
	if (!wsm_toml_get_bool(config, key, value)) {
		*value = fallback;
		return wsm_toml_set_bool(config, key, *value);
	}
	return true;
}

static bool copy_string(const char *type, const char *id, const char *field,
	const char *fallback, char **value) {
	char key[1024];
	if (!make_key(key, sizeof(key), type, id, field)) {
		return false;
	}
	if (!wsm_toml_get_string(config, key, value)) {
		*value = strdup(fallback);
		return *value != NULL &&
			wsm_toml_set_string(config, key, *value);
	}
	return true;
}

static const char *get_type(struct wsm_input_device *device) {
	switch (device->input_device_wlr->type) {
	case WLR_INPUT_DEVICE_TOUCH:
		return "touchscreen";
	case WLR_INPUT_DEVICE_POINTER: {
		switch (wsm_pointer_get_type(device->input_device_wlr)) {
		case WSM_POINTER_TYPE_MOUSE:
			return "mouse";
		case WSM_POINTER_TYPE_TOUCHPAD:
			return "touchpad";
		default:
			return NULL;
		}
	}
	default:
		return NULL;
	}
}

static void remove_legacy_defaults(void) {
	static const char *keys[] = {
		"mouse.defaults.scroll_factor",
		"mouse.defaults.hand_mode",
		"mouse.defaults.accel_speed",
		"mouse.defaults.acceleration_profile",
		"mouse.defaults.natural_scroll",
		"mouse.defaults.send_events_mode",
		"touchpad.defaults.scroll_factor",
		"touchpad.defaults.hand_mode",
		"touchpad.defaults.accel_speed",
		"touchpad.defaults.acceleration_profile",
		"touchpad.defaults.natural_scroll",
		"touchpad.defaults.disable_while_typing",
		"touchpad.defaults.tap_to_click",
		"touchpad.defaults.send_events_mode",
		"touchscreen.defaults.map_output",
	};
	for (size_t i = 0; i < sizeof(keys) / sizeof(keys[0]); ++i) {
		wsm_toml_remove(config, keys[i]);
	}
}

static uint32_t parse_send_events(const char *value) {
	if (strcmp(value, "disabled") == 0) {
		return LIBINPUT_CONFIG_SEND_EVENTS_DISABLED;
	}
	if (strcmp(value, "disabled_on_external_mouse") == 0) {
		return LIBINPUT_CONFIG_SEND_EVENTS_DISABLED_ON_EXTERNAL_MOUSE;
	}
	return LIBINPUT_CONFIG_SEND_EVENTS_ENABLED;
}

bool wsm_input_memory_configure(struct wsm_input_device *device) {
	if (!initialize() ||
		!wlr_input_device_is_libinput(device->input_device_wlr)) {
		return false;
	}
	const char *type = get_type(device);
	if (type == NULL) {
		return true;
	}
	remove_legacy_defaults();
	bool ok = true, value;
	double number;
	char *string = NULL;
	if (strcmp(type, "touchscreen") == 0) {
		ok = copy_string(
			type, device->identifier, "map_output", "", &string);
		if (ok && *string != '\0') {
			device->mapped_output_id = strdup(string);
		}
		free(string);
		goto save;
	}

	struct libinput_device *libinput =
		wlr_libinput_get_device_handle(device->input_device_wlr);
	ok = copy_double(
		     type, device->identifier, "scroll_factor", 1.0, &number) &&
		ok;
	device->scroll_factor = number > 0 ? number : 1.0;
	ok = copy_double(
		     type, device->identifier, "accel_speed", 0.0, &number) &&
		ok;
	if (number >= -1.0 && number <= 1.0 &&
		libinput_device_config_accel_is_available(libinput)) {
		libinput_device_config_accel_set_speed(libinput, number);
	}
	ok = copy_string(
		     type, device->identifier, "hand_mode", "right", &string) &&
		ok;
	if (libinput_device_config_left_handed_is_available(libinput)) {
		libinput_device_config_left_handed_set(
			libinput, strcmp(string, "left") == 0);
	}
	free(string);
	ok = copy_string(type, device->identifier, "acceleration_profile",
		     "adaptive", &string) &&
		ok;
	if (libinput_device_config_accel_is_available(libinput)) {
		enum libinput_config_accel_profile profile =
			strcmp(string, "flat") == 0
			? LIBINPUT_CONFIG_ACCEL_PROFILE_FLAT
			: LIBINPUT_CONFIG_ACCEL_PROFILE_ADAPTIVE;
		libinput_device_config_accel_set_profile(libinput, profile);
	}
	free(string);
	ok = copy_bool(type, device->identifier, "natural_scroll",
		     strcmp(type, "touchpad") == 0, &value) &&
		ok;
	if (libinput_device_config_scroll_has_natural_scroll(libinput)) {
		libinput_device_config_scroll_set_natural_scroll_enabled(
			libinput, value);
	}
	ok = copy_string(type, device->identifier, "send_events_mode",
		     "enabled", &string) &&
		ok;
	libinput_device_config_send_events_set_mode(
		libinput, parse_send_events(string));
	free(string);
	if (strcmp(type, "touchpad") == 0) {
		ok = copy_bool(type, device->identifier, "disable_while_typing",
			     true, &value) &&
			ok;
		if (libinput_device_config_dwt_is_available(libinput)) {
			libinput_device_config_dwt_set_enabled(libinput, value);
		}
		ok = copy_bool(type, device->identifier, "tap_to_click", true,
			     &value) &&
			ok;
		if (libinput_device_config_tap_get_finger_count(libinput) > 0) {
			libinput_device_config_tap_set_enabled(libinput, value);
		}
	}

save:
	if (!ok || !wsm_toml_save(config, config_path)) {
		wsm_log(WSM_ERROR, "Cannot save input configuration %s: %s",
			config_path, strerror(errno));
		return false;
	}
	return true;
}

void wsm_input_memory_finish(void) {
	wsm_toml_destroy(config);
	config = NULL;
	free(config_path);
	config_path = NULL;
	initialized = false;
}
