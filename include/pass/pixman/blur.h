/**
 * @file        blur.h
 * @brief       Pixman blur render pass backend.
 * @details     Declares the software Dual Kawase blur backend constructor.
 * @author      YaoBing Xiao
 * @date        2026-11-02
 * @version     v1.0
 */

#ifndef WSM_PIXMAN_BLUR_H
#define WSM_PIXMAN_BLUR_H

#include "pass/blur.h"

/**
 * @brief Creates the Pixman blur backend.
 * @details Called by wsm_render_blur_buffer_pass_create, which attaches the
 *          common renderer ownership and destruction listener.
 * @param renderer wsm Pixman renderer used by the common pass constructor.
 * @return New backend pass, or NULL on allocation failure.
 */
struct wsm_render_blur_buffer_pass *wsm_pixman_render_blur_buffer_pass_create(
	struct wlr_renderer *renderer);

#endif // WSM_PIXMAN_BLUR_H
