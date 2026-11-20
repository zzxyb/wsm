/* SPDX-License-Identifier: MIT */

/**
 * @file        pass.h
 * @brief       Vulkan render pass state and creation.
 * @details     Defines render pass resources, texture dependencies, and output synchronization
 *              state.
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

#ifndef PASS_VULKAN_PASS_H
#define PASS_VULKAN_PASS_H

#include <stdbool.h>
#include <stdint.h>
#include <vulkan/vulkan.h>
#include <wayland-util.h>
#include <wlr/render/interface.h>
#include "renderer/vulkan/rect_union.h"

struct wsm_vk_renderer;
struct wsm_vk_texture;
struct wsm_vk_render_buffer;
struct wsm_vk_render_buffer_out;
struct wsm_vk_render_format_setup;
struct wsm_vk_command_buffer;
struct wsm_vk_render_timer;
struct wlr_color_transform;
struct wlr_drm_syncobj_timeline;

/**
 * @brief Texture dependency and optional explicit acquisition timeline.
 */
struct wsm_vk_render_pass_texture {
	struct wsm_vk_texture *texture; /**< Texture used by the render pass. */

	struct wlr_drm_syncobj_timeline *wait_timeline; /**< Optional explicit synchronization timeline; may be NULL. */
	uint64_t wait_point; /**< Timeline point required before texture access. */
};

/**
 * @brief Recorded Vulkan render pass and its synchronization dependencies.
 */
struct wsm_vk_render_pass {
	struct wlr_render_pass base; /**< Generic wlroots interface. */
	struct wsm_vk_renderer *renderer; /**< Renderer owning these resources. */
	struct wsm_vk_render_buffer *render_buffer; /**< Destination render buffer. */
	struct wsm_vk_render_buffer_out *render_buffer_out; /**< Selected output framebuffer and view. */
	struct wsm_vk_render_format_setup *render_setup; /**< Render passes and pipelines used for this target. */
	struct wsm_vk_command_buffer *command_buffer; /**< Command buffer recording this render pass. */
	struct rect_union updated_region; /**< Accumulated output damage rectangles. */
	VkPipeline bound_pipeline; /**< Currently bound Vulkan graphics pipeline. */
	float projection[9]; /**< Projection matrix for destination coordinates. */
	bool failed; /**< Whether recording has encountered a failure. */
	bool two_pass; /**< Whether rendering uses an intermediate blending image. */
	struct wlr_color_transform *color_transform; /**< Color transform retained for rendering; may be NULL. */

	struct wlr_drm_syncobj_timeline *signal_timeline; /**< Optional timeline signaled after rendering; may be NULL. */
	uint64_t signal_point; /**< Timeline point signaled on completion. */

	struct wsm_vk_render_timer *timer; /**< Optional GPU render timer; may be NULL. */

	struct wl_array textures; /**< Array of wsm_vk_render_pass_texture dependencies. */
};

/**
 * @brief Begins recording a render pass for a Vulkan render buffer.
 * @param renderer Renderer recording the pass.
 * @param buffer Destination render buffer.
 * @param options Render pass options.
 * @return Render pass, or NULL on failure.
 */
struct wsm_vk_render_pass *wsm_vk_begin_render_pass(struct wsm_vk_renderer *renderer,
	struct wsm_vk_render_buffer *buffer, const struct wlr_buffer_pass_options *options);

/**
 * @brief Checks whether a render pass uses the wsm Vulkan implementation.
 * @param pass Render pass to check.
 * @return true if the pass uses the wsm Vulkan implementation, false otherwise.
 */
bool wlr_render_pass_is_wsm_vk(const struct wlr_render_pass *pass);

/**
 * @brief Gets the Vulkan state containing a render pass.
 * @param pass Render pass using the wsm Vulkan implementation.
 * @return Borrowed pointer to the containing Vulkan render pass.
 */
struct wsm_vk_render_pass *wsm_vk_render_pass_from_render_pass(struct wlr_render_pass *pass);

/**
 * @brief Ends the active attachment render pass to record ordered effects or transfers.
 * @param pass Active Vulkan render pass.
 */
void wsm_vk_render_pass_suspend(struct wsm_vk_render_pass *pass);

/**
 * @brief Restarts attachment rendering after an ordered effect or transfer.
 * @param pass Suspended Vulkan render pass.
 */
void wsm_vk_render_pass_resume(struct wsm_vk_render_pass *pass);

/**
 * @brief Tracks a texture read and its acquire dependency.
 * @param pass Vulkan render pass recording the read.
 * @param texture Texture used by the draw or transfer.
 * @param timeline Explicit acquire timeline, or NULL to use implicit DMA-BUF synchronization.
 * @param point Point to wait on when a timeline is supplied.
 * @return true if usage was tracked, false on allocation failure.
 */
bool wsm_vk_render_pass_use_texture(struct wsm_vk_render_pass *pass,
	struct wsm_vk_texture *texture, struct wlr_drm_syncobj_timeline *timeline, uint64_t point);

#endif // PASS_VULKAN_PASS_H
