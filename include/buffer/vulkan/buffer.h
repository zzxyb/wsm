/* SPDX-License-Identifier: MIT */

/**
 * @file        buffer.h
 * @brief       Vulkan render buffer resources and synchronization.
 * @details     Defines imported render targets, framebuffer variants, and DMA-BUF acquisition
 *              helpers.
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

#ifndef BUFFER_VULKAN_BUFFER_H
#define BUFFER_VULKAN_BUFFER_H

#include "renderer/vulkan/renderer.h"

/**
 * @brief Imported DMA-BUF render target and its framebuffer variants.
 */
struct wsm_vk_render_buffer {
	struct wlr_buffer *wlr_buffer; /**< Source wlroots buffer associated with this render target. */
	struct wlr_addon addon; /**< Addon attaching these resources to their source object. */
	struct wsm_vk_renderer *renderer; /**< Renderer owning these resources. */
	struct wl_list link; /**< Link in wsm_vk_renderer.render_buffers. */

	VkDeviceMemory memories[WLR_DMABUF_MAX_PLANES]; /**< Memory allocations backing the imported image planes. */
	uint32_t mem_count; /**< Number of valid image memory allocations. */
	VkImage image; /**< Vulkan image handle. */

	// Framebuffer and image view for rendering directly onto the buffer image,
	// without any color transform.
	struct {
		struct wsm_vk_render_buffer_out out; /**< Output image view and framebuffer. */
		struct wsm_vk_render_format_setup *render_setup; /**< Render passes and pipelines used for this target. */
	} linear; /**< Direct rendering without a color transform. */

	// Framebuffer and image view for rendering directly onto the buffer image.
	// This requires that the image support an _SRGB VkFormat, and does
	// not work with color transforms.
	struct {
		struct wsm_vk_render_buffer_out out; /**< Output image view and framebuffer. */
		struct wsm_vk_render_format_setup *render_setup; /**< Render passes and pipelines used for this target. */
	} srgb; /**< Direct rendering through an sRGB image view. */

	// Framebuffer, image view, and blending image to render indirectly
	// onto the buffer image. This works for general image types and permits
	// color transforms.
	struct {
		struct wsm_vk_render_buffer_out out; /**< Output image view and framebuffer. */
		struct wsm_vk_render_format_setup *render_setup; /**< Render passes and pipelines used for this target. */

		VkImage blend_image; /**< Intermediate image used for linear-light blending. */
		VkImageView blend_image_view; /**< View of the intermediate blending image. */
		VkDeviceMemory blend_memory; /**< Memory backing the intermediate blending image. */
		VkDescriptorSet blend_descriptor_set; /**< Descriptor set sampling the blending image. */
		struct wsm_vk_descriptor_pool *blend_attachment_pool; /**< Pool owning the blending image descriptor set. */
		bool blend_transitioned; /**< Whether the blending image has left its initial layout. */
	} two_pass; /**< Intermediate blending and final color conversion resources. */
};

/**
 * @brief Imports a buffer as a Vulkan render target.
 * @param renderer Renderer importing the buffer.
 * @param buffer Source DMA-BUF buffer.
 * @return Render buffer, or NULL on failure.
 */
struct wsm_vk_render_buffer *wsm_vk_render_buffer_create(
		struct wsm_vk_renderer *renderer, struct wlr_buffer *buffer);

/**
 * @brief Destroys an imported render buffer and its Vulkan resources.
 * @param buffer Render buffer to destroy.
 */
void wsm_vk_render_buffer_destroy(struct wsm_vk_render_buffer *buffer);

/**
 * @brief Finds an existing render buffer attached to a source buffer.
 * @param renderer Renderer owning the attachment.
 * @param buffer Source buffer to inspect.
 * @return Existing render buffer, or NULL when none is attached.
 */
struct wsm_vk_render_buffer *wsm_get_vk_render_buffer(
	struct wsm_vk_renderer *renderer, struct wlr_buffer *buffer);

/**
 * @brief Creates a framebuffer for direct rendering into a buffer image.
 * @param buffer Render buffer to configure.
 * @param dmabuf Imported DMA-BUF attributes.
 * @param srgb Whether to use the sRGB framebuffer variant.
 * @return true on success, false on failure.
 */
bool wsm_vk_setup_one_pass_framebuffer(struct wsm_vk_render_buffer *buffer,
		const struct wlr_dmabuf_attributes *dmabuf, bool srgb);

/**
 * @brief Creates blending and output resources for two-pass rendering.
 * @param buffer Render buffer to configure.
 * @param dmabuf Imported DMA-BUF attributes.
 * @return true on success, false on failure.
 */
bool wsm_vk_setup_two_pass_framebuffer(struct wsm_vk_render_buffer *buffer,
		const struct wlr_dmabuf_attributes *dmabuf);

/**
 * @brief Acquires implicit synchronization fences before rendering into a buffer.
 * @details Exported descriptors belong to the caller. Without implicit sync interoperability,
 *          this blocks until the buffer is writable.
 * @param render_buffer Render buffer to acquire.
 * @param sync_file_fds Output plane fence descriptors; initialize entries to -1.
 * @return true when fences are exported or the fallback wait succeeds, false on failure.
 */
bool wsm_vk_sync_render_buffer_acquire(struct wsm_vk_render_buffer *render_buffer,
		int sync_file_fds[static WLR_DMABUF_MAX_PLANES]);

/**
 * @brief Exports per-plane DMA-BUF fences or waits for buffer access.
 * @details The caller must close exported descriptors, including descriptors already exported
 *          if a later plane fails. The blocking fallback leaves the array unchanged.
 * @param renderer Renderer providing synchronization capabilities.
 * @param buffer Buffer whose DMA-BUF fences are acquired.
 * @param flags DMA_BUF_SYNC_READ and/or DMA_BUF_SYNC_WRITE access flags.
 * @param sync_file_fds Output plane fence descriptors; initialize entries to -1.
 * @return true on successful export or fallback wait, false on failure.
 */
bool wsm_buffer_export_sync_file(struct wsm_vk_renderer *renderer, struct wlr_buffer *buffer,
		uint32_t flags, int sync_file_fds[static WLR_DMABUF_MAX_PLANES]);

#endif
