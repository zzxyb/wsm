/**
 * @file        rect.h
 * @brief       Pixman rounded rectangle rendering helper.
 * @details     This file declares the backend helper that rasterizes rounded
 *              rectangles on a Pixman render pass.
 * @author      YaoBing Xiao
 * @date        2026-10-21
 * @version     v1.0
 * @par Copyright(c):
 * @par History:
 *      version: v1.0, YaoBing Xiao, 2026-10-21, initial version\n
 */

#ifndef PASS_PIXMAN_RECT_H
#define PASS_PIXMAN_RECT_H

#include "pass/rect.h"

/**
 * @brief Draws a rounded rectangle on a wsm Pixman render pass.
 * @param pass Active wsm Pixman render pass.
 * @param options Rectangle geometry, color, radii, and clipping options.
 */
void wsm_pixman_render_pass_add_rect(struct wlr_render_pass *pass,
	const struct wsm_render_rect_options *options);

#endif // PASS_PIXMAN_RECT_H
