/**
 * @file        round_border.h
 * @brief       Rounded rectangle border render pass API.
 * @details     Defines a border drawn inside a rectangle with independent
 *              corner radii on wsm GLES2 and Pixman render passes.
 * @author      YaoBing Xiao
 * @date        2026-10-21
 * @version     v1.0
 * @par Copyright(c):
 * @par History:
 *      version: v1.0, YaoBing Xiao, 2026-10-21, initial version
 */

#ifndef PASS_ROUND_BORDER_H
#define PASS_ROUND_BORDER_H

#include <pixman.h>
#include <wlr/render/pass.h>
#include <wlr/util/box.h>

/**
 * @brief Options for drawing one rounded rectangle border.
 */
struct wsm_render_round_border_options {
	struct wlr_box box; /**< Outer rectangle in buffer coordinates. */
	struct wlr_render_color color; /**< Premultiplied RGBA border color. */
	float radii[4]; /**< Top-left, top-right, bottom-left, bottom-right radii. */
	float border_width; /**< Border width in buffer pixels, drawn inside the box. */
	float corner_smoothness; /**< Superellipse exponent; zero selects 4. */
	const pixman_region32_t *clip; /**< Optional buffer-coordinate clip region. */
};

/**
 * @brief Adds a rounded border to an active render pass.
 * @param pass Destination render pass.
 * @param options Border geometry, color, radii, and clipping options.
 * @return true when the pass supports rounded borders, false otherwise.
 */
bool wsm_render_pass_add_round_border(struct wlr_render_pass *pass,
	const struct wsm_render_round_border_options *options);

#endif // PASS_ROUND_BORDER_H
