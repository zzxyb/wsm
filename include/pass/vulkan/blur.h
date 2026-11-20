/* SPDX-License-Identifier: MIT */

/**
 * @file        blur.h
 * @brief       Vulkan background capture and Dual Kawase blur.
 * @author      YaoBing Xiao
 * @date        2026-11-20
 * @version     v1.0
 * @par Copyright(c):
 *              See LICENSE for wsm copyright and license.
 * @par History:
 *      version: v1.0, YaoBing Xiao, 2026-11-20, initial version\n
 */

#ifndef PASS_VULKAN_BLUR_H
#define PASS_VULKAN_BLUR_H

#include "pass/blur.h"

/**
 * @brief Adds a Dual Kawase blur draw.
 * @param target Active wsm Vulkan render pass.
 * @param options Source texture, blur settings, and destination geometry.
 */
void wsm_vk_render_pass_add_blur(struct wlr_render_pass *target,
	const struct wsm_render_blur_buffer_options *options);

/**
 * @brief Captures the current render pass content for background blur.
 * @param target Active wsm Vulkan render pass.
 * @param previous Existing Vulkan capture texture of matching size, or NULL to allocate one.
 * @param box Capture bounds in target coordinates, or NULL for the whole target.
 * @param damage Region to update in target coordinates, or NULL to update the full capture.
 * @return The reused or newly allocated caller-owned capture texture, or NULL on failure.
 */
struct wlr_texture *wsm_vk_render_pass_capture_blur(struct wlr_render_pass *target,
	struct wlr_texture *previous, const struct wlr_box *box, const pixman_region32_t *damage);
struct wsm_vk_renderer;

/**
 * @brief Releases the renderer's cached blur textures.
 * @param renderer Renderer owning the blur cache.
 */
void wsm_vk_blur_finish(struct wsm_vk_renderer *renderer);

#endif // PASS_VULKAN_BLUR_H
