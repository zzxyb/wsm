/**
 * @file        round_rect.h
 * @brief       Rounded rectangle render pass API.
 * @details     This file defines the rendering options and dispatch helper for
 *              rounded rectangles on wsm GLES2 and Pixman render passes.
 * @author      YaoBing Xiao
 * @date        2026-10-21
 * @version     v1.0
 */

#ifndef PASS_ROUND_RECT_H
#define PASS_ROUND_RECT_H

#include <pixman.h>
#include <wlr/render/pass.h>
#include <wlr/util/box.h>

/**
 * @brief Options for drawing one rounded rectangle.
 */
struct wsm_render_round_rect_options {
	struct wlr_box box; /**< Destination rectangle in buffer coordinates. */
	struct wlr_render_color color; /**< Premultiplied RGBA fill color. */
	float radii[4]; /**< Top-left, top-right, bottom-left, bottom-right radii in buffer pixels. */
	float corner_smoothness; /**< Superellipse exponent; zero selects the default of 4. */
	const pixman_region32_t *clip; /**< Optional buffer-coordinate clip region, or NULL. */
};

/**
 * @brief Adds a rounded rectangle to an active render pass.
 * @param pass Destination render pass.
 * @param options Rectangle geometry, color, radii, and clipping options.
 * @return true when the pass supports drawing the rectangle, false otherwise.
 */
bool wsm_render_pass_add_round_rect(struct wlr_render_pass *pass,
	const struct wsm_render_round_rect_options *options);

#endif // PASS_ROUND_RECT_H
