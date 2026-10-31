#ifndef WSM_PIXMAN_BLUR_H
#define WSM_PIXMAN_BLUR_H
#include "pass/blur.h"
struct wsm_render_blur_buffer_pass *wsm_pixman_render_blur_buffer_pass_create(
	struct wlr_renderer *renderer);
#endif
