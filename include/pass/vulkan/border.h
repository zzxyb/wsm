/* SPDX-License-Identifier: MIT */

/**
 * @file        border.h
 * @brief       Vulkan superellipse border rendering.
 * @author      YaoBing Xiao
 * @date        2026-11-20
 * @version     v1.0
 * @par Copyright(c):
 *              See LICENSE for wsm copyright and license.
 * @par History:
 *      version: v1.0, YaoBing Xiao, 2026-11-20, initial version\n
 */

#ifndef PASS_VULKAN_BORDER_H
#define PASS_VULKAN_BORDER_H

#include "pass/border.h"

/**
 * @brief Records a superellipse border draw.
 * @param target Active wsm Vulkan render pass.
 * @param options Border geometry, fill color, corner shape, and clipping.
 */
void wsm_vk_render_pass_add_border(struct wlr_render_pass *target,
	const struct wsm_render_border_options *options);

#endif // PASS_VULKAN_BORDER_H
