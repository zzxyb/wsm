/**
 * @file        shadow_cache.h
 * @brief       GLES2 shadow cache generation helper.
 * @details     Declares offscreen rendering of a premultiplied shadow texture
 *              with independently rounded corners.
 * @author      YaoBing Xiao
 * @date        2026-10-22
 * @version     v1.0
 * @par Copyright(c):
 * @par History:
 *      version: v1.0, YaoBing Xiao, 2026-10-22, initial version
 */

#ifndef PASS_GLES_SHADOW_CACHE_H
#define PASS_GLES_SHADOW_CACHE_H

#include "node/wsm_scene_shadow.h"

/**
 * @brief Renders a nine-slice shadow cache with GLES2.
 * @param renderer GLES2 renderer that owns the texture.
 * @param margins Shadow extent on each side in texture pixels.
 * @param corner_extent Fixed corner area containing every radius.
 * @param radii Top-left, top-right, bottom-left, bottom-right radii.
 * @param inner_offset Distance the shadow extends inside its outline.
 * @param color RGBA shadow color; the generated texture is premultiplied.
 * @return New cache, or NULL if texture generation fails.
 */
struct wsm_scene_shadow_cache *wsm_gles2_shadow_cache_create(
	struct wlr_renderer *renderer, const struct wsm_scene_shadow_slices *margins,
	float corner_extent, const float radii[static 4], float inner_offset,
	const float color[static 4]);

#endif // PASS_GLES_SHADOW_CACHE_H
