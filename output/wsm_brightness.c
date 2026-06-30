#include "wsm_brightness.h"

#include <assert.h>

void wsm_brightness_init(struct wsm_brightness *brightness,
		const struct wsm_brightness_impl *impl, struct wsm_output *output,
		const char *kind) {
	assert(brightness);
	assert(impl);

	brightness->impl = impl;
	brightness->output = output;
	brightness->brightness = -1;
	brightness->max_brightness = -1;
	brightness->kind = kind;
}

void wsm_brightness_destroy(struct wsm_brightness *brightness) {
	if (!brightness) {
		return;
	}
	assert(brightness->impl && brightness->impl->destroy);
	brightness->impl->destroy(brightness);
}

bool wsm_brightness_set(struct wsm_brightness *brightness, long value) {
	if (!brightness || !brightness->impl->set_brightness) {
		return false;
	}
	if (value < 0 || value > brightness->max_brightness) {
		return false;
	}
	return brightness->impl->set_brightness(brightness, value);
}
