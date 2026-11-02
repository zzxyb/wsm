/**
 * @file        texture.h
 * @brief       Pixman rounded texture render pass.
 * @author      YaoBing Xiao
 * @date        2026-11-02
 * @version     v1.0
 */
#ifndef PASS_PIXMAN_TEXTURE_H
#define PASS_PIXMAN_TEXTURE_H

#include <wlr/render/pass.h>

/**
 * @brief Draws a texture using the dedicated rounded clipping pass.
 * @param pass Active Pixman render pass.
 * @param options Standard texture drawing options.
 * @param radii Four corner radii in destination pixels.
 */
void wsm_pixman_render_pass_add_texture(struct wlr_render_pass *pass,
	const struct wlr_render_texture_options *options, const float *radii);

#endif // PASS_PIXMAN_TEXTURE_H
