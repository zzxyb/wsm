/**
 * @file        round_rect.h
 * @brief       GLES2 rounded rectangle rendering helper.
 * @details     This file declares the backend helper that draws rounded
 *              rectangles using the GLES2 signed-distance shader.
 * @author      YaoBing Xiao
 * @date        2026-10-21
 * @version     v1.0
 */

#ifndef PASS_GLES_ROUND_RECT_H
#define PASS_GLES_ROUND_RECT_H

#include "pass/round_rect.h"

/**
 * @brief Draws a rounded rectangle on a wsm GLES2 render pass.
 * @param pass Active wsm GLES2 render pass.
 * @param options Rectangle geometry, color, radii, and clipping options.
 */
void wsm_gles2_render_pass_add_round_rect(struct wlr_render_pass *pass,
	const struct wsm_render_round_rect_options *options);

#endif // PASS_GLES_ROUND_RECT_H
