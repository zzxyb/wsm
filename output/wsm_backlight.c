#include "wsm_backlight.h"

#include "wsm_common.h"
#include "wsm_drm.h"
#include "wsm_log.h"
#include "wsm_output.h"
#include "wsm_server.h"

#include <dirent.h>
#include <drm_mode.h>
#include <errno.h>
#include <fcntl.h>
#include <libudev.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

#include <wlr/backend/drm.h>

static struct wsm_backlight *backlight_from_brightness(
		struct wsm_brightness *brightness) {
	return wl_container_of(brightness, (struct wsm_backlight *)0, base);
}

static long backlight_get_node(struct wsm_backlight *backlight, const char *node) {
	char buffer[100];
	char *path = NULL;
	int fd = -1;
	long ret = -1;
	int value;

	str_printf(&path, "%s/%s", backlight->path, node);
	if (!path) {
		return -ENOMEM;
	}

	fd = open(path, O_RDONLY);
	if (fd < 0) {
		wsm_log(WSM_ERROR, "Failed to open %s", path);
		goto out;
	}

	ret = read(fd, buffer, sizeof(buffer) - 1);
	if (ret < 1) {
		ret = -1;
		goto out;
	}
	buffer[ret] = '\0';
	if (buffer[ret - 1] == '\n') {
		buffer[ret - 1] = '\0';
	}

	if (!safe_strtoint(buffer, &value)) {
		ret = -1;
		goto out;
	}
	ret = value;

out:
	if (fd >= 0) {
		close(fd);
	}
	free(path);
	return ret;
}

static long backlight_get_brightness(struct wsm_brightness *brightness) {
	struct wsm_backlight *backlight = backlight_from_brightness(brightness);
	long value = backlight_get_node(backlight, "actual_brightness");
	if (value < 0) {
		value = backlight_get_node(backlight, "brightness");
	}
	return value;
}

static bool backlight_set_brightness(struct wsm_brightness *brightness,
		long value) {
	struct wsm_backlight *backlight = backlight_from_brightness(brightness);
	char *path = NULL;
	char buffer[32];
	int fd = -1;
	bool ok = false;

	str_printf(&path, "%s/%s", backlight->path, "brightness");
	if (!path) {
		return false;
	}

	fd = open(path, O_WRONLY);
	if (fd < 0) {
		wsm_log(WSM_ERROR, "Failed to open %s", path);
		goto out;
	}

	int len = snprintf(buffer, sizeof(buffer), "%ld\n", value);
	if (len < 0 || (size_t)len >= sizeof(buffer)) {
		goto out;
	}
	if (write(fd, buffer, len) != len) {
		wsm_log(WSM_ERROR, "Failed to write brightness to %s", path);
		goto out;
	}

	brightness->brightness = backlight_get_brightness(brightness);
	ok = brightness->brightness >= 0;

out:
	if (fd >= 0) {
		close(fd);
	}
	free(path);
	return ok;
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

static enum wsm_backlight_type parse_backlight_type(const char *type) {
	if (strcmp(type, "raw\n") == 0) {
		return WSM_BACKLIGHT_RAW;
	}
	if (strcmp(type, "platform\n") == 0) {
		return WSM_BACKLIGHT_PLATFORM;
	}
	if (strcmp(type, "firmware\n") == 0) {
		return WSM_BACKLIGHT_FIRMWARE;
	}
	return WSM_BACKLIGHT_UNKNOWN;
}

static enum wsm_backlight_type read_backlight_type(const char *backlight_path) {
	char *path = NULL;
	char buffer[100];
	int fd = -1;
	enum wsm_backlight_type type = WSM_BACKLIGHT_UNKNOWN;

	str_printf(&path, "%s/%s", backlight_path, "type");
	if (!path) {
		return WSM_BACKLIGHT_UNKNOWN;
	}

	fd = open(path, O_RDONLY);
	if (fd < 0) {
		goto out;
	}
	ssize_t len = read(fd, buffer, sizeof(buffer) - 1);
	if (len < 1) {
		goto out;
	}
	buffer[len] = '\0';
	type = parse_backlight_type(buffer);

out:
	if (fd >= 0) {
		close(fd);
	}
	free(path);
	return type;
}

struct wsm_brightness *wsm_backlight_create(struct wsm_output *output) {
	if (!wlr_output_is_drm(output->wlr_output)) {
		return NULL;
	}

	uint32_t connector_type =
		wsm_drm_get_connector_from_output(output, global_server.backend)->connector_type;
	if (connector_type == DRM_MODE_CONNECTOR_Unknown) {
		wsm_log(WSM_ERROR, "Failed to parse connector type");
		return NULL;
	}

	struct udev_device *udev_device = wsm_output_get_device_handle(output);
	if (!udev_device) {
		wsm_log(WSM_ERROR, "Failed to get device handle for output");
		return NULL;
	}

	DIR *backlights = opendir("/sys/class/backlight");
	if (!backlights) {
		return NULL;
	}

	char *chosen_path = NULL;
	char *chosen_class = NULL;
	enum wsm_backlight_type chosen_type = WSM_BACKLIGHT_UNKNOWN;
	struct dirent *entry;
	while ((entry = readdir(backlights))) {
		if (entry->d_name[0] == '.') {
			continue;
		}

		char *backlight_path = NULL;
		str_printf(&backlight_path, "%s/%s", "/sys/class/backlight", entry->d_name);
		if (!backlight_path) {
			continue;
		}

		enum wsm_backlight_type entry_type = read_backlight_type(backlight_path);
		if (entry_type == WSM_BACKLIGHT_UNKNOWN) {
			free(backlight_path);
			continue;
		}

		if (connector_type != DRM_MODE_CONNECTOR_LVDS &&
				connector_type != DRM_MODE_CONNECTOR_eDP &&
				entry_type != WSM_BACKLIGHT_RAW) {
			free(backlight_path);
			continue;
		}

		if (entry_type >= chosen_type) {
			free(chosen_path);
			free(chosen_class);
			chosen_path = backlight_path;
			chosen_class = strdup(entry->d_name);
			chosen_type = entry_type;
		} else {
			free(backlight_path);
		}
	}
	closedir(backlights);

	if (!chosen_path || !chosen_class) {
		free(chosen_path);
		free(chosen_class);
		return NULL;
	}

	struct wsm_backlight *backlight = calloc(1, sizeof(struct wsm_backlight));
	if (!backlight) {
		free(chosen_path);
		free(chosen_class);
		return NULL;
	}

	wsm_brightness_init(&backlight->base, &backlight_impl, output, "backlight");
	backlight->path = chosen_path;
	backlight->backlight_class = chosen_class;
	backlight->type = chosen_type;
	backlight->base.max_brightness =
		backlight_get_node(backlight, "max_brightness");
	backlight->base.brightness = backlight_get_brightness(&backlight->base);
	if (backlight->base.max_brightness < 0 || backlight->base.brightness < 0) {
		wsm_brightness_destroy(&backlight->base);
		return NULL;
	}

	wsm_log(WSM_DEBUG, "Using backlight device %s for %s",
		backlight->backlight_class, output->wlr_output->name);
	return &backlight->base;
}
