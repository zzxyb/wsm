/* SPDX-License-Identifier: MIT */

/**
 * @file        blit.h
 * @brief       Vulkan filtered image blits.
 * @author      YaoBing Xiao
 * @date        2026-11-20
 * @version     v1.0
 * @par Copyright(c):
 *              See LICENSE for wsm copyright and license.
 * @par History:
 *      version: v1.0, YaoBing Xiao, 2026-11-20, initial version\n
 */

#ifndef PASS_VULKAN_BLIT_H
#define PASS_VULKAN_BLIT_H

#include "pass/blit.h"

/**
 * @brief Records an ordered texture blit.
 * @details A source aliasing the render target requires a single-pass render target.
 * @param target Active wsm Vulkan render pass.
 * @param options Borrowed textures, geometry, and optional source synchronization.
 * @return true if the operation was recorded, false otherwise.
 */
bool wsm_vk_render_pass_blit(struct wlr_render_pass *target,
	const struct wsm_blit_texture_options *options);

#endif // PASS_VULKAN_BLIT_H
