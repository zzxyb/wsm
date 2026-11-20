/* SPDX-License-Identifier: MIT */

/**
 * @file        shadow.h
 * @brief       Vulkan nine-slice shadow cache creation.
 * @author      YaoBing Xiao
 * @date        2026-11-20
 * @version     v1.0
 * @par Copyright(c):
 *              See LICENSE for wsm copyright and license.
 * @par History:
 *      version: v1.0, YaoBing Xiao, 2026-11-20, initial version\n
 */

#ifndef PASS_VULKAN_SHADOW_H
#define PASS_VULKAN_SHADOW_H

#include "node/wsm_scene_shadow.h"

/**
 * @brief Creates a nine-slice shadow cache backed by an offscreen Vulkan texture.
 * @param renderer wsm Vulkan renderer recording the shadow draw.
 * @param margins Outer margins added to the corner extent.
 * @param corner_extent Extent reserved on each side for the rounded corners.
 * @param radii Top-left, top-right, bottom-left, and bottom-right corner radii.
 * @param inner_offset Offset of the inner shadow edge.
 * @param color Shadow color as four RGBA components.
 * @return An owned shadow cache, or NULL on failure.
 */
struct wsm_scene_shadow_cache *wsm_vk_shadow_cache_create(
	struct wlr_renderer *renderer, const struct wsm_scene_shadow_slices *margins,
	float corner_extent, const float radii[static 4], float inner_offset,
	const float color[static 4]);

#endif // PASS_VULKAN_SHADOW_H
