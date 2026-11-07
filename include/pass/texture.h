/**
 * @file        texture.h
 * @brief       Texture rendering with optional rounded clipping.
 * @details     Shares destination-space clipping across texture backends.
 * @author      YaoBing Xiao
 * @date        2026-11-02
 * @version     v1.0
 * @par Copyright(c):
 * @par History:
 *      version: v1.0, YaoBing Xiao, 2026-11-02, initial version\n
 */
#ifndef PASS_TEXTURE_H
#define PASS_TEXTURE_H

#include <wlr/render/pass.h>

/**
 * @brief Draws a texture with destination-space corner radii.
 * @param pass Active render pass.
 * @param options Texture geometry, color, synchronization, and clipping options.
 * @param radii Top-left, top-right, bottom-left, bottom-right radii in target pixels.
 * @return true if the backend supports the draw, false otherwise.
 */
bool wsm_render_pass_add_texture(struct wlr_render_pass *pass,
	const struct wlr_render_texture_options *options, const float radii[static 4]);

/**
 * @brief Computes antialiased superellipse coverage for a destination pixel.
 * @param width Destination width in pixels.
 * @param height Destination height in pixels.
 * @param radii Four corner radii in destination pixels.
 * @param x Pixel center x coordinate relative to the destination.
 * @param y Pixel center y coordinate relative to the destination.
 * @return Coverage in the range [0, 1].
 */
float wsm_render_texture_corner_coverage(int width, int height,
	const float radii[static 4], float x, float y);

#endif // PASS_TEXTURE_H
