/* SPDX-License-Identifier: MIT */

/**
 * @file        texture.h
 * @brief       Vulkan texture rendering with rounded clipping.
 * @author      YaoBing Xiao
 * @date        2026-11-20
 * @version     v1.0
 * @par Copyright(c):
 *              See LICENSE for wsm copyright and license.
 * @par History:
 *      version: v1.0, YaoBing Xiao, 2026-11-20, initial version\n
 */

#ifndef PASS_VULKAN_TEXTURE_H
#define PASS_VULKAN_TEXTURE_H

#include "pass/texture.h"

/**
 * @brief Records a texture draw with destination-space rounded clipping.
 * @param target Active wsm Vulkan render pass.
 * @param options Texture geometry, blending, clipping, and optional synchronization.
 * @param radii Top-left, top-right, bottom-left, and bottom-right radii in target pixels.
 */
void wsm_vk_render_pass_add_texture(struct wlr_render_pass *target,
	const struct wlr_render_texture_options *options, const float radii[static 4]);

#endif // PASS_VULKAN_TEXTURE_H
