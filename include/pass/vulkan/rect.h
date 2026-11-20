/* SPDX-License-Identifier: MIT */

/**
 * @file        rect.h
 * @brief       Vulkan rounded rectangle rendering.
 * @details     This file defines the Vulkan render pass entry point for drawing
 *              clipped rounded rectangles.
 *
 * @author      YaoBing Xiao
 * @date        2026-11-20
 * @version     v1.0
 * @par Copyright(c):
 *              See LICENSE for wsm copyright and license.
 * @par History:
 *      version: v1.0, YaoBing Xiao, 2026-11-20, initial version\n
 */

#ifndef PASS_VULKAN_RECT_H
#define PASS_VULKAN_RECT_H

#include "pass/rect.h"

/**
 * @brief Records a rounded rectangle draw in an active Vulkan render pass.
 * @details The draw is clipped to the target buffer and the optional clip
 *          region. Failure to obtain a pipeline or staging space marks the
 *          pass as failed.
 * @param target Active wsm Vulkan render pass.
 * @param options Rectangle geometry, fill color, corner radii, and clipping options.
 */
void wsm_vk_render_pass_add_rect(struct wlr_render_pass *target,
	const struct wsm_render_rect_options *options);

#endif // PASS_VULKAN_RECT_H
