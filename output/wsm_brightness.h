#ifndef WSM_BRIGHTNESS_H
#define WSM_BRIGHTNESS_H

#include <stdbool.h>

struct wsm_brightness;
struct wsm_output;

enum wsm_brightness_method {
	WSM_BRIGHTNESS_METHOD_NONE,
	WSM_BRIGHTNESS_METHOD_BACKLIGHT,
	WSM_BRIGHTNESS_METHOD_DDCUTIL,
};

struct wsm_brightness_impl {
	void (*destroy)(struct wsm_brightness *brightness);
	bool (*set_brightness)(struct wsm_brightness *brightness, long value);
};

struct wsm_brightness {
	const struct wsm_brightness_impl *impl;
	struct wsm_output *output;
	long brightness;
	long min_brightness;
	long max_brightness;
	enum wsm_brightness_method method;
	char *state_key;
};

void wsm_brightness_init(struct wsm_brightness *brightness,
	const struct wsm_brightness_impl *impl, struct wsm_output *output,
	enum wsm_brightness_method method);
void wsm_brightness_destroy(struct wsm_brightness *brightness);
bool wsm_brightness_set(struct wsm_brightness *brightness, long value);
bool wsm_brightness_restore(struct wsm_brightness *brightness);
const char *wsm_brightness_method_name(enum wsm_brightness_method method);

#endif
