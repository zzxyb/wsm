#include "wsm_backlight.h"
#include "wsm_log.h"
#include "wsm_output.h"

#include <stdlib.h>

#include <wlr/backend/drm.h>

struct wsm_backlight *wsm_backlight_create(struct wsm_output *output) {
	if (!wlr_output_is_drm(output->wlr_output)) {
		wsm_log(WSM_ERROR, "Failed init wsm_backlight， only support drm output!");
		return NULL;
	}

	struct wsm_backlight *backlight = calloc(1, sizeof(struct wsm_backlight));
	if (!wsm_assert(output, "Could not create wsm_backlight: allocation failed!")) {
		return NULL;
	}

	return backlight;
}

// void wsm_backlight_destroy(struct wsm_backlight *backlight) {

// }

// int wsm_backlight_get_max_brightness(struct wsm_backlight *backlight) {

// }

// int wsm_backlight_get_brightness(struct wsm_backlight *backlight) {

// }

// int wsm_backlight_get_actual_brightness(struct wsm_backlight *backlight) {

// }

// bool wsm_backlight_set_brightness(struct wsm_backlight *backlight, long brightness) {

// }
