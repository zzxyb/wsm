/*
MIT License

Copyright (c) 2024 YaoBing Xiao

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

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
