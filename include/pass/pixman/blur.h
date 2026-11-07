/**
 * @file        blur.h
 * @brief       Pixman blur render pass backend.
 * @details     Declares software Dual Kawase rendering and capture operations.
 * @author      YaoBing Xiao
 * @date        2026-11-02
 * @version     v1.0
 * @par Copyright(c):
 * @par History:
 *      version: v1.0, YaoBing Xiao, 2026-11-02, initial version\n
 */

#ifndef WSM_PIXMAN_BLUR_H
#define WSM_PIXMAN_BLUR_H

#include "pass/blur.h"

/** Filters and composites a texture on the active Pixman render pass. */
void wsm_pixman_render_pass_add_blur(struct wlr_render_pass *pass,
	const struct wsm_render_blur_buffer_options *options);

/** Creates or updates a snapshot of the active Pixman render target. */
struct wlr_texture *wsm_pixman_render_pass_capture_blur(struct wlr_render_pass *pass,
	struct wlr_texture *previous, const struct wlr_box *box, const pixman_region32_t *damage);

#endif // WSM_PIXMAN_BLUR_H
