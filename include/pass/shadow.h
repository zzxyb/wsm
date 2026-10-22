/**
 * @file        shadow.h
 * @brief       Shadow render pass helper.
 * @details     Declares nine-slice rendering of cached shadow textures
 *              clipped to the current output damage.
 * @author      YaoBing Xiao
 * @date        2026-10-22
 * @version     v1.0
 * @par Copyright(c):
 * @par History:
 *      version: v1.0, YaoBing Xiao, 2026-10-22, initial version
 */

#ifndef PASS_SHADOW_H
#define PASS_SHADOW_H

#include <pixman.h>

struct wsm_scene_shadow;
struct wsm_render_data;

/**
 * @brief Adds the eight visible shadow slices to a render pass.
 * @param shadow Shadow node and cache to render.
 * @param data Current output rendering state.
 * @param clip Clip region in output buffer coordinates.
 * @param x Shadow node x coordinate relative to the output.
 * @param y Shadow node y coordinate relative to the output.
 */
void wsm_render_pass_add_shadow(struct wsm_scene_shadow *shadow,
	const struct wsm_render_data *data, const pixman_region32_t *clip,
	int x, int y);

#endif // PASS_SHADOW_H
