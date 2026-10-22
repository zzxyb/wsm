/**
 * @file        round_border.h
 * @brief       Pixman rounded border rendering helper.
 * @details     Declares the backend helper that rasterizes rounded rectangle
 *              borders on a Pixman render pass.
 * @author      YaoBing Xiao
 * @date        2026-10-21
 * @version     v1.0
 * @par Copyright(c):
 * @par History:
 *      version: v1.0, YaoBing Xiao, 2026-10-21, initial version
 */

#ifndef PASS_PIXMAN_ROUND_BORDER_H
#define PASS_PIXMAN_ROUND_BORDER_H

#include "pass/round_border.h"

/**
 * @brief Draws a rounded border on a wsm Pixman render pass.
 * @param pass Active wsm Pixman render pass.
 * @param options Border geometry, color, radii, width, and clipping options.
 */
void wsm_pixman_render_pass_add_round_border(struct wlr_render_pass *pass,
	const struct wsm_render_round_border_options *options);

#endif // PASS_PIXMAN_ROUND_BORDER_H
