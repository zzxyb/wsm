#include "wsm_backlight_device.h"
#include "wsm_log.h"
#include "wsm_output.h"
#include "wsm_common.h"
#include "wsm_drm.h"
#include "wsm_server.h"

#include <stdlib.h>
#include <dirent.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <libgen.h>

#include <xf86drm.h>
#include <xf86drmMode.h>
#include <drm_mode.h>
#include <libudev.h>

#include <wlr/backend/drm.h>

static long backlight_get(const struct wsm_backlight_device *device, char *node)
{
	char buffer[100];
	char *path;
	int fd, value;
	long ret;

	str_printf(&path, "%s/%s", device->path, node);
	if (!path) {
		return -ENOMEM;
	}

	fd = open(path, O_RDONLY);
	if (fd < 0) {
		wsm_log(WSM_ERROR, "2 Failed open %s", path);
		ret = -1;
		goto out;
	}

	ret = read(fd, &buffer, sizeof(buffer));
	if (ret < 1) {
		ret = -1;
		goto out;
	}

	if (buffer[ret - 1] == '\n')
		buffer[ret - 1] = '\0';

	if (!safe_strtoint(buffer, &value)) {
		ret = -1;
		goto out;
	}

	ret = value;

out:
	if (fd >= 0)
		close(fd);
	free(path);
	return ret;
}

struct wsm_backlight_device *wsm_backlight_device_create(struct wsm_output *output) {
	if (!wlr_output_is_drm(output->wlr_output)) {
		wsm_log(WSM_ERROR, "Failed init wsm_backlight_device,  only support drm output!");
		return NULL;
	}

	// char *pci_name = NULL;
	char *chosen_path = NULL;
	char *backlight_class;
	struct dirent *entry;
	enum wsm_backlight_device_type type = 0;
	uint32_t connector_type =
		wsm_drm_get_connector_from_output(output, global_server.backend)->connector_type;
	if (connector_type == DRM_MODE_CONNECTOR_Unknown) {
		wsm_log(WSM_ERROR, "Failed to parse connector type");
		return NULL;
	}

	struct udev_device *device = wsm_output_get_device_handle(output);
	if (!device) {
		wsm_log(WSM_ERROR, "Failed to get device handle for output");
		return NULL;
	}

	const char *syspath = udev_device_get_syspath(device);
	if (syspath == NULL) {
		wsm_log(WSM_ERROR, "Failed to get syspath for device");
		return NULL;
	}

	char *path = NULL;
	str_printf(&path, "%s/%s", syspath, "device");
	if (!path) {
		wsm_log(WSM_ERROR, "Failed to create path string");
		return NULL;
	}

	char buffer[100], basename_buffer[100];
	int ret = readlink(path, buffer, sizeof(buffer) - 1);
	free(path);
	if (ret < 0) {
		wsm_log(WSM_ERROR, "Failed to read symlink");
		return NULL;
	}

	strncpy(basename_buffer, buffer, ret);
	buffer[ret] = '\0';
	basename_buffer[ret] = '\0';
	// pci_name = basename(basename_buffer);

	DIR *backlights = opendir("/sys/class/backlight");
	if (!backlights) {
		wsm_log(WSM_ERROR, "Failed open backlight");
		return NULL;
	}

	struct wsm_backlight_device *backlight = calloc(1, sizeof(struct wsm_backlight_device));
	if (!backlight) {
		wsm_log(WSM_ERROR, "Could not create wsm_backlight_device: allocation failed!");
		goto err;
	}

	while ((entry = readdir(backlights))) {
		char *backlight_path;
		// char *parent;
		enum wsm_backlight_device_type entry_type = WSM_BACKLIGHT_UNKNOW;
		int fd;

		if (entry->d_name[0] == '.')
			continue;

		str_printf(&backlight_class, "%s", entry->d_name);
		if (!backlight_path)
			goto err;

		str_printf(&backlight_path, "%s/%s", "/sys/class/backlight",
			   entry->d_name);
		str_printf(&path, "%s/%s", backlight_path, "type");
		if (!path) {
			free(backlight_path);
			goto err;
		}

		fd = open(path, O_RDONLY);

		if (fd < 0)
			goto out;

		ret = read (fd, &buffer, sizeof(buffer));
		close (fd);

		if (ret < 1)
			goto out;

		buffer[ret] = '\0';

		if (!strncmp(buffer, "raw\n", sizeof(buffer)))
			entry_type = WSM_BACKLIGHT_RAM;
		else if (!strncmp(buffer, "platform\n", sizeof(buffer)))
			entry_type = WSM_BACKLIGHT_PLATFORM;
		else if (!strncmp(buffer, "firmware\n", sizeof(buffer)))
			entry_type = WSM_BACKLIGHT_FIRMWARE;
		else
			goto out;

		if (connector_type != DRM_MODE_CONNECTOR_LVDS &&
			connector_type != DRM_MODE_CONNECTOR_eDP) {
			if (entry_type != WSM_BACKLIGHT_RAM)
				goto out;
		}

		free (path);

		str_printf(&path, "%s/%s", backlight_path, "device");
		if (!path)
			goto err;

		ret = readlink(path, buffer, sizeof(buffer) - 1);

		if (ret < 0)
			goto out;

		buffer[ret] = '\0';

		// parent = basename(buffer);

		// if (entry_type == WSM_BACKLIGHT_RAM ||
		// 	entry_type == WSM_BACKLIGHT_FIRMWARE) {
		// 	if (!(pci_name && !strcmp(pci_name, parent)))
		// 		goto out;
		// }

		if (entry_type < type)
			goto out;

		type = entry_type;

		if (chosen_path)
			free(chosen_path);
		chosen_path = strdup(backlight_path);

	out:
		free(backlight_path);
		free(path);
	}

	if (!chosen_path)
		goto err;

	wsm_log(WSM_ERROR, "backlight node %s", backlight_class);
	backlight->output = output;
	backlight->path = chosen_path;
	backlight->backlight_class = backlight_class;
	backlight->type = type;
	backlight->max_brightness = wsm_backlight_device_get_max_brightness(backlight);
	if (backlight->max_brightness < 0)
		goto err;

	backlight->brightness = wsm_backlight_device_get_actual_brightness(backlight);
	if (backlight->brightness < 0)
		goto err;

	closedir(backlights);

	wsm_backlight_device_set_brightness(backlight, 20);
	wsm_log(WSM_ERROR, "finish backlight %s", output->wlr_output->name);
	return backlight;

err:
	closedir(backlights);
	free(chosen_path);
	free(backlight_class);
	free (backlight);
	return NULL;
}

void wsm_backlight_device_destroy(struct wsm_backlight_device *device) {
	assert(device);

	if (device->path)
		free(device->path);

	if (device->backlight_class)
		free(device->backlight_class);

	free(device);
}

long wsm_backlight_device_get_max_brightness(const struct wsm_backlight_device *device) {
	return backlight_get(device, "max_brightness");
}

long wsm_backlight_device_get_brightness(const struct wsm_backlight_device *device) {
	return backlight_get(device, "brightness");
}

long wsm_backlight_device_get_actual_brightness(const struct wsm_backlight_device *device) {
	return backlight_get(device, "actual_brightness");
}

bool wsm_backlight_device_set_brightness(struct wsm_backlight_device *device, long brightness) {
	wsm_log(WSM_ERROR, "xyb----%s", libseat_seat_name(device->session->seat_handle));
	return true;
// 	char *path;
// 	char *buffer = NULL;
// 	int fd;
// 	long ret;

// 	str_printf(&path, "%s/%s", device->path, "brightness");
// 	if (!path) {
// 		return -ENOMEM;
// 	}

// 	fd = open(path, O_RDWR);
// 	if (fd < 0) {
// 		wsm_log(WSM_ERROR, "1 Failed open %s", path);
// 		ret = -1;
// 		goto out;
// 	}

// 	ret = read(fd, &buffer, sizeof(buffer));
// 	if (ret < 1) {
// 		ret = -1;
// 		goto out;
// 	}

// 	str_printf(&buffer, "%ld", brightness);
// 	if (!buffer) {
// 		ret = -1;
// 		goto out;
// 	}

// 	ret = write(fd, buffer, strlen(buffer));
// 	if (ret < 0) {
// 		ret = -1;
// 		goto out;
// 	}

// 	ret = wsm_backlight_device_get_brightness(device);
// 	device->brightness = ret;
// out:
// 	free(buffer);
// 	free(path);
// 	if (fd >= 0)
// 		close(fd);
// 	return ret;
}
