#ifndef WSM_BRIGHTNESS_H
#define WSM_BRIGHTNESS_H

#include <stdbool.h>

struct wsm_brightness;
struct wsm_output;

struct wsm_brightness_impl {
	void (*destroy)(struct wsm_brightness *brightness);
	bool (*set_brightness)(struct wsm_brightness *brightness, long value);
};

struct wsm_brightness {
	const struct wsm_brightness_impl *impl;
	struct wsm_output *output;
	long brightness;
	long max_brightness;
	const char *kind;
};

void wsm_brightness_init(struct wsm_brightness *brightness,
	const struct wsm_brightness_impl *impl, struct wsm_output *output,
	const char *kind);
void wsm_brightness_destroy(struct wsm_brightness *brightness);
bool wsm_brightness_set(struct wsm_brightness *brightness, long value);

#endif
