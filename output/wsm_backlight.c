#include "wsm_backlight.h"

#include "wsm_common.h"
#include "wsm_drm.h"
#include "wsm_log.h"
#include "wsm_output.h"
#include "wsm_server.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <drm_mode.h>
#include <xf86drmMode.h>
#include <wlr/backend/drm.h>

static struct wsm_backlight *backlight_from_brightness(
		struct wsm_brightness *brightness) {
	return wl_container_of(brightness, (struct wsm_backlight *)0, base);
}

static bool read_long_file(const char *path, long *value) {
	int fd = open(path, O_RDONLY | O_CLOEXEC);
	if (fd < 0) {
		return false;
	}
	char buffer[64];
	ssize_t length = read(fd, buffer, sizeof(buffer) - 1);
	close(fd);
	if (length <= 0) {
		return false;
	}
	buffer[length] = '\0';
	errno = 0;
	char *end = NULL;
	long parsed = strtol(buffer, &end, 10);
	if (errno || end == buffer) {
		return false;
	}
	*value = parsed;
	return true;
}

static bool read_backlight_value(const char *directory, const char *name,
		long *value) {
	char *path = NULL;
	str_printf(&path, "%s/%s", directory, name);
	if (!path) {
		return false;
	}
	bool ok = read_long_file(path, value);
	free(path);
	return ok;
}

static enum wsm_backlight_type read_backlight_type(const char *directory) {
	char *path = NULL;
	str_printf(&path, "%s/type", directory);
	if (!path) {
		return WSM_BACKLIGHT_UNKNOWN;
	}
	int fd = open(path, O_RDONLY | O_CLOEXEC);
	free(path);
	if (fd < 0) {
		return WSM_BACKLIGHT_UNKNOWN;
	}
	char buffer[32];
	ssize_t length = read(fd, buffer, sizeof(buffer) - 1);
	close(fd);
	if (length <= 0) {
		return WSM_BACKLIGHT_UNKNOWN;
	}
	buffer[length] = '\0';
	if (strncmp(buffer, "firmware", 8) == 0) {
		return WSM_BACKLIGHT_FIRMWARE;
	}
	if (strncmp(buffer, "platform", 8) == 0) {
		return WSM_BACKLIGHT_PLATFORM;
	}
	if (strncmp(buffer, "raw", 3) == 0) {
		return WSM_BACKLIGHT_RAW;
	}
	return WSM_BACKLIGHT_UNKNOWN;
}

static bool backlight_set_brightness(struct wsm_brightness *brightness,
		long value) {
	struct wsm_backlight *backlight = backlight_from_brightness(brightness);
	char *path = NULL;
	str_printf(&path, "%s/brightness", backlight->path);
	if (!path) {
		return false;
	}
	int fd = open(path, O_WRONLY | O_CLOEXEC);
	if (fd < 0) {
		wsm_log(WSM_ERROR, "Could not open backlight %s: %s",
			path, strerror(errno));
		free(path);
		return false;
	}
	char buffer[32];
	int length = snprintf(buffer, sizeof(buffer), "%ld\n", value);
	ssize_t written = length > 0 ? write(fd, buffer, length) : -1;
	int saved_errno = errno;
	close(fd);
	free(path);
	if (written != length) {
		wsm_log(WSM_ERROR, "Could not set backlight brightness: %s",
			strerror(saved_errno));
		return false;
	}
	brightness->brightness = value;
	return true;
}

static bool backlight_is_writable(const char *directory) {
	char *path = NULL;
	str_printf(&path, "%s/brightness", directory);
	if (!path) {
		return false;
	}
	int fd = open(path, O_WRONLY | O_CLOEXEC);
	free(path);
	if (fd < 0) {
		return false;
	}
	close(fd);
	return true;
}

static void backlight_destroy(struct wsm_brightness *brightness) {
	struct wsm_backlight *backlight = backlight_from_brightness(brightness);
	free(backlight->path);
	free(backlight->backlight_class);
	free(backlight);
}

static const struct wsm_brightness_impl backlight_impl = {
	.destroy = backlight_destroy,
	.set_brightness = backlight_set_brightness,
};

static bool output_uses_internal_panel(struct wsm_output *output) {
	if (!output || !output->wlr_output ||
			!wlr_output_is_drm(output->wlr_output)) {
		return false;
	}
	drmModeConnector *connector = wsm_drm_get_connector_from_output(
		output, global_server.backend);
	if (!connector) {
		return false;
	}
	bool internal = connector->connector_type == DRM_MODE_CONNECTOR_eDP ||
		connector->connector_type == DRM_MODE_CONNECTOR_LVDS;
	drmModeFreeConnector(connector);
	return internal;
}

struct wsm_brightness *wsm_backlight_create(struct wsm_output *output) {
	if (!output_uses_internal_panel(output)) {
		return NULL;
	}

	DIR *directory = opendir("/sys/class/backlight");
	if (!directory) {
		return NULL;
	}
	char *best_path = NULL;
	char *best_class = NULL;
	enum wsm_backlight_type best_type = WSM_BACKLIGHT_UNKNOWN;
	struct dirent *entry;
	while ((entry = readdir(directory))) {
		if (entry->d_name[0] == '.') {
			continue;
		}
		char *path = NULL;
		str_printf(&path, "/sys/class/backlight/%s", entry->d_name);
		if (!path) {
			continue;
		}
		enum wsm_backlight_type type = read_backlight_type(path);
		long current, maximum;
		if (type == WSM_BACKLIGHT_UNKNOWN || type < best_type ||
				!backlight_is_writable(path) ||
				!read_backlight_value(path, "brightness", &current) ||
				!read_backlight_value(path, "max_brightness", &maximum) ||
				maximum <= 0) {
			free(path);
			continue;
		}
		free(best_path);
		free(best_class);
		best_path = path;
		best_class = strdup(entry->d_name);
		best_type = type;
	}
	closedir(directory);
	if (!best_path || !best_class) {
		free(best_path);
		free(best_class);
		return NULL;
	}

	struct wsm_backlight *backlight = calloc(1, sizeof(*backlight));
	if (!backlight) {
		free(best_path);
		free(best_class);
		return NULL;
	}
	wsm_brightness_init(&backlight->base, &backlight_impl, output,
		WSM_BRIGHTNESS_METHOD_BACKLIGHT);
	backlight->path = best_path;
	backlight->backlight_class = best_class;
	backlight->type = best_type;
	if (!read_backlight_value(best_path, "brightness",
			&backlight->base.brightness) ||
			!read_backlight_value(best_path, "max_brightness",
			&backlight->base.max_brightness)) {
		wsm_brightness_destroy(&backlight->base);
		return NULL;
	}
	backlight->base.min_brightness = 0;
	wsm_log(WSM_INFO, "Using backlight %s for output %s",
		best_class, output->wlr_output->name);
	return &backlight->base;
}
