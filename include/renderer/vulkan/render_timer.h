/* SPDX-License-Identifier: MIT */

/**
 * @file        render_timer.h
 * @brief       Vulkan GPU render timing resources.
 * @details     Defines timestamp query state and helpers for creating and identifying render
 *              timers.
 *
 * @par Upstream:
 *              Derived from wlroots; see LICENSE.wlroots in the project root.
 *
 * @author      YaoBing Xiao (wsm adaptations)
 * @date        2026-11-19
 * @version     v1.0
 * @par Copyright(c):
 *              Upstream copyright notices and MIT license: LICENSE.wlroots
 *              in the project root. See LICENSE for wsm copyright and license.
 * @par History:
 *      version: v1.0, YaoBing Xiao, 2026-11-19, initial version\n
 */

#ifndef RENDERER_VULKAN_RENDER_TIMER_H
#define RENDERER_VULKAN_RENDER_TIMER_H

#include <wlr/render/interface.h>

#include <vulkan/vulkan.h>

/**
 * @brief GPU timestamp query resources for measuring a render pass.
 */
struct wsm_vk_render_timer {
	struct wlr_render_timer base; /**< Generic wlroots interface. */
	struct wsm_vk_renderer *renderer; /**< Renderer owning these resources. */
	VkQueryPool query_pool; /**< Query pool containing start and end timestamps. */
};

/**
 * @brief Creates a timer backed by two GPU timestamp queries.
 * @param wlr_renderer Vulkan renderer providing the query pool.
 * @return Render timer, or NULL on failure or unsupported timestamp queries.
 */
struct wsm_vk_render_timer *wsm_vk_render_timer_create(
		struct wlr_renderer *wlr_renderer);

/**
 * @brief Checks whether a timer uses the Vulkan implementation.
 * @param wlr_timer Timer to inspect; may be NULL.
 * @return true for a Vulkan timer, false otherwise.
 */
bool wlr_render_timer_is_wsm_vk(const struct wlr_render_timer *wlr_timer);

/**
 * @brief Casts a generic render timer to a Vulkan render timer.
 * @details Asserts that the timer has the expected implementation.
 * @param wlr_timer Timer known to use the Vulkan implementation.
 * @return Enclosing Vulkan timer.
 */
struct wsm_vk_render_timer *wlr_vk_render_timer_from_timer(
	struct wlr_render_timer *wlr_timer);

#endif // RENDERER_VULKAN_RENDER_TIMER_H
