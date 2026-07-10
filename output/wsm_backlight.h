#ifndef WSM_BACKLIGHT_H
#define WSM_BACKLIGHT_H

#include "wsm_brightness.h"

/**
 * @brief Enumeration of backlight device types
 */
enum wsm_backlight_type {
	WSM_BACKLIGHT_UNKNOWN,
	WSM_BACKLIGHT_RAW,
	WSM_BACKLIGHT_PLATFORM,
	WSM_BACKLIGHT_FIRMWARE,
	WSM_BACKLIGHT_VENDOR,
};

struct wsm_backlight {
	struct wsm_brightness base;
	char *path;
	char *backlight_class;
	enum wsm_backlight_type type;
};

struct wsm_brightness *wsm_backlight_create(struct wsm_output *output);

#endif
