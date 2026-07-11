#include "wsm_output_memory.h"

#include "wsm_log.h"
#include "wsm_output.h"
#include "wsm_output_config.h"
#include "wsm_scene.h"
#include "wsm_server.h"
#include "wsm_toml.h"

#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <drm_fourcc.h>

#include <wlr/types/wlr_output.h>
#include <wlr/backend/wayland.h>
#include <wlr/backend/x11.h>

static struct wsm_toml *memory;
static char *memory_path;
static bool initialized;

static bool output_is_nested(struct wlr_output *output) {
	return wlr_output_is_wl(output) || wlr_output_is_x11(output);
}

static bool initialize(void) {
	if (initialized) {
		return memory != NULL;
	}
	initialized = true;

	const char *home = getenv("HOME");
	if (!home || !*home) {
		wsm_log(WSM_ERROR,
			"Cannot load output memory: HOME is not set");
		return false;
	}
	const char suffix[] = "/.config/wsm/outputs.toml";
	memory_path = malloc(strlen(home) + sizeof(suffix));
	if (!memory_path) {
		return false;
	}
	sprintf(memory_path, "%s%s", home, suffix);

	char error[256];
	memory = wsm_toml_load(memory_path, error, sizeof(error));
	if (!memory) {
		wsm_log(WSM_ERROR, "Cannot load output memory %s: %s",
			memory_path, error);
		return false;
	}
	if (!wsm_toml_has(memory, "primary_output") &&
		!wsm_toml_set_string(memory, "primary_output", "")) {
		wsm_toml_destroy(memory);
		memory = NULL;
		return false;
	}
	return true;
}

/* Keep model and serial readable while escaping characters forbidden in bare
 * TOML keys. Underscore itself is escaped, so the _00 separator is unique. */
static char *output_prefix(const struct wlr_output *output) {
	const char *model = output->model ? output->model : "";
	const char *serial = output->serial ? output->serial : "";
	size_t model_len = strlen(model);
	size_t serial_len = strlen(serial);
	char *prefix = malloc(sizeof("output.") + (model_len + serial_len) * 3 +
		sizeof("_00"));
	if (!prefix) {
		return NULL;
	}
	char *p = stpcpy(prefix, "output.");
	const char *parts[] = {model, serial};
	for (size_t part = 0; part < 2; ++part) {
		if (part == 1) {
			p = stpcpy(p, "_00");
		}
		for (const unsigned char *value =
				(const unsigned char *)parts[part];
			*value; ++value) {
			if ((*value >= 'a' && *value <= 'z') ||
				(*value >= 'A' && *value <= 'Z') ||
				(*value >= '0' && *value <= '9') ||
				*value == '-') {
				*p++ = *value;
			} else {
				sprintf(p, "_%02X", *value);
				p += 3;
			}
		}
	}
	*p = '\0';
	return prefix;
}

char *wsm_output_memory_get_output_id(struct wsm_output *output) {
	char *prefix = output_prefix(output->wlr_output);
	if (prefix == NULL) {
		return NULL;
	}
	char *output_id = strdup(prefix + strlen("output."));
	free(prefix);
	return output_id;
}

char *wsm_output_memory_get_primary_output(void) {
	if (!initialize()) {
		return NULL;
	}
	char *output_id = NULL;
	if (!wsm_toml_get_string(memory, "primary_output", &output_id)) {
		return NULL;
	}
	return output_id;
}

bool wsm_output_memory_set_primary_output(struct wsm_output *output) {
	if (!initialize()) {
		return false;
	}
	if (output != NULL && output_is_nested(output->wlr_output)) {
		return false;
	}
	char *prefix = NULL;
	const char *output_id = "";
	if (output != NULL) {
		prefix = output_prefix(output->wlr_output);
		if (prefix == NULL) {
			return false;
		}
		output_id = prefix + strlen("output.");
	}
	bool ok = wsm_toml_set_string(memory, "primary_output", output_id) &&
		wsm_toml_save(memory, memory_path);
	free(prefix);
	return ok;
}

static bool make_key(
	char *key, size_t size, const char *prefix, const char *field) {
	return snprintf(key, size, "%s.%s", prefix, field) < (int)size;
}

bool wsm_output_memory_load(
	struct wsm_output *output, struct output_config *config) {
	if (output_is_nested(output->wlr_output)) {
		return false;
	}
	if (!initialize()) {
		return false;
	}
	char *prefix = output_prefix(output->wlr_output);
	if (!prefix) {
		return false;
	}
	char key[512];
	int64_t integer;
	double number;
	bool boolean;
	bool has_x = false;
	bool has_y = false;
	bool has_width = false;
	bool has_height = false;
	bool has_refresh = false;

#define LOAD_INT(field, target, found) \
	if (make_key(key, sizeof(key), prefix, field) && \
		wsm_toml_get_int(memory, key, &integer)) { \
		target = integer; \
		found = true; \
	}
	LOAD_INT("x", config->x, has_x)
	LOAD_INT("y", config->y, has_y)
	LOAD_INT("width", config->width, has_width)
	LOAD_INT("height", config->height, has_height)
#undef LOAD_INT
	if (make_key(key, sizeof(key), prefix, "transform") &&
		wsm_toml_get_int(memory, key, &integer)) {
		config->transform = integer;
	}
	if (make_key(key, sizeof(key), prefix, "refresh_rate") &&
		wsm_toml_get_double(memory, key, &number)) {
		config->refresh_rate = number;
		has_refresh = true;
	}
	if (make_key(key, sizeof(key), prefix, "scale") &&
		wsm_toml_get_double(memory, key, &number)) {
		config->scale = number;
	}
	if (make_key(key, sizeof(key), prefix, "vrr") &&
		wsm_toml_get_bool(memory, key, &boolean)) {
		config->adaptive_sync = boolean;
	}
	if (make_key(key, sizeof(key), prefix, "render_bit_depth") &&
		wsm_toml_get_int(memory, key, &integer)) {
		if (integer == 8) {
			config->render_bit_depth = RENDER_BIT_DEPTH_8;
		} else if (integer == 10) {
			config->render_bit_depth = RENDER_BIT_DEPTH_10;
		}
	}
	free(prefix);
	return has_x && has_y && has_width && has_height && has_refresh;
}

static bool store_output(struct wsm_output *output) {
	struct wlr_output *wlr_output = output->wlr_output;
	if (output_is_nested(wlr_output)) {
		return true;
	}
	char *prefix = output_prefix(wlr_output);
	if (!prefix) {
		return false;
	}
	struct wlr_box box;
	wlr_output_layout_get_box(
		global_server.scene->output_layout, wlr_output, &box);
	double refresh_rate = wlr_output->current_mode
		? wlr_output->current_mode->refresh / 1000.0
		: 0.0;
	int width = wlr_output->current_mode ? wlr_output->current_mode->width
					     : wlr_output->width;
	int height = wlr_output->current_mode ? wlr_output->current_mode->height
					      : wlr_output->height;
	char key[512];
	bool ok = true;
#define STORE_INT(field, value) \
	ok = make_key(key, sizeof(key), prefix, field) && \
		wsm_toml_set_int(memory, key, value) && ok
	STORE_INT("x", box.x);
	STORE_INT("y", box.y);
	STORE_INT("width", width);
	STORE_INT("height", height);
	STORE_INT("transform", wlr_output->transform);
	STORE_INT("render_bit_depth",
		wlr_output->render_format == DRM_FORMAT_XRGB2101010 ||
				wlr_output->render_format ==
					DRM_FORMAT_XBGR2101010
			? 10
			: 8);
#undef STORE_INT
#define STORE_VALUE(field, function, value) \
	ok = make_key(key, sizeof(key), prefix, field) && \
		function(memory, key, value) && ok
	STORE_VALUE("refresh_rate", wsm_toml_set_double, refresh_rate);
	STORE_VALUE("scale", wsm_toml_set_double, wlr_output->scale);
	STORE_VALUE("vrr", wsm_toml_set_bool,
		wlr_output->adaptive_sync_status ==
			WLR_OUTPUT_ADAPTIVE_SYNC_ENABLED);
#undef STORE_VALUE
	free(prefix);
	return ok;
}

void wsm_output_memory_store_all(void) {
	if (!initialize()) {
		return;
	}
	bool ok = true;
	struct wsm_output *output;
	wl_list_for_each(output, &global_server.scene->all_outputs, link) {
		if (output != global_server.scene->fallback_output) {
			ok = store_output(output) && ok;
		}
	}
	if (!ok || !wsm_toml_save(memory, memory_path)) {
		wsm_log(WSM_ERROR, "Cannot save output memory %s: %s",
			memory_path, strerror(errno));
	}
}

void wsm_output_memory_finish(void) {
	wsm_toml_destroy(memory);
	memory = NULL;
	free(memory_path);
	memory_path = NULL;
	initialized = false;
}
