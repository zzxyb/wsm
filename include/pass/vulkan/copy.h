/* SPDX-License-Identifier: MIT */

/**
 * @file        copy.h
 * @brief       Vulkan ordered texture copies and image transfer helpers.
 * @author      YaoBing Xiao
 * @date        2026-11-20
 * @version     v1.0
 * @par Copyright(c):
 *              See LICENSE for wsm copyright and license.
 * @par History:
 *      version: v1.0, YaoBing Xiao, 2026-11-20, initial version\n
 */

#ifndef PASS_VULKAN_COPY_H
#define PASS_VULKAN_COPY_H

#include "pass/copy.h"

struct wsm_vk_render_pass;
struct wsm_vk_texture;

/**
 * @brief Records an ordered texture copy.
 * @details Both textures must use matching formats and separate storage from the render target.
 * @param target Active wsm Vulkan render pass.
 * @param options Borrowed textures, geometry, and optional source synchronization.
 * @return true if the operation was recorded, false otherwise.
 */
bool wsm_vk_render_pass_copy(struct wlr_render_pass *target,
	const struct wsm_copy_texture_options *options);

/**
 * @brief Checks common image transfer requirements.
 * @details DMA-BUF destination usage is negotiated per modifier.
 * @param pass Active Vulkan render pass.
 * @param src Source texture with transfer-source usage.
 * @param dst Separate destination texture with transfer-destination usage.
 * @param allow_target_source Whether the source may alias the pass target.
 * @return true if the textures meet the common requirements, false otherwise.
 */
bool wsm_vk_transfer_validate(struct wsm_vk_render_pass *pass,
	struct wsm_vk_texture *src, struct wsm_vk_texture *dst, bool allow_target_source);

/**
 * @brief Checks whether the source shares storage with the render target.
 * @param pass Vulkan render pass holding the target.
 * @param src Texture to check.
 * @return true if the images alias or DMA-BUF aliasing cannot be ruled out.
 */
bool wsm_vk_transfer_uses_target(struct wsm_vk_render_pass *pass, struct wsm_vk_texture *src);

/**
 * @brief Records the image layout transitions around a transfer.
 * @param pass Suspended Vulkan render pass whose command buffer records the barriers.
 * @param src Source image to transition.
 * @param dst Destination image to transition.
 * @param before true to enter transfer layouts, false to restore the render and sampling layouts.
 */
void wsm_vk_transfer_barriers(struct wsm_vk_render_pass *pass,
	struct wsm_vk_texture *src, struct wsm_vk_texture *dst, bool before);

#endif // PASS_VULKAN_COPY_H
