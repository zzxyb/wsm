/**
 * @file        texture.h
 * @brief       Vulkan texture resources and image access.
 * @details     Defines imported textures, image attributes, DMA-BUF synchronization, and pixel
 *              readback helpers.
 * @author      YaoBing Xiao
 * @date        2026-11-19
 * @version     v1.0
 * @par Copyright(c):
 * @par History:
 *      version: v1.0, YaoBing Xiao, 2026-11-19, initial version\n
 */

#ifndef TEXTURE_VULKAN_TEXTURE_H
#define TEXTURE_VULKAN_TEXTURE_H

#include "renderer/vulkan/renderer.h"

/**
 * @brief Borrowed Vulkan image handle, layout, and format.
 */
struct wsm_vk_image_attribs {
	VkImage image; /**< Vulkan image handle. */
	VkImageLayout layout; /**< Image layout expected when accessing the image. */
	VkFormat format; /**< Vulkan format of the image. */
};

/**
 * @brief Vulkan texture storage, ownership, and cached sampling views.
 */
struct wsm_vk_texture {
	struct wlr_texture wlr_texture; /**< Generic wlroots texture interface. */
	struct wsm_vk_renderer *renderer; /**< Renderer owning these resources. */
	uint32_t mem_count; /**< Number of valid image memory allocations. */
	VkDeviceMemory memories[WLR_DMABUF_MAX_PLANES]; /**< Memory allocations backing the imported image planes. */
	VkImage image; /**< Vulkan image handle. */
	const struct wsm_vk_format *format; /**< Pixel format mapping used by the image. */
	struct wsm_vk_command_buffer *last_used_cb; /**< Last command buffer using the texture; may be NULL. */
	bool dmabuf_imported; /**< Whether the image was imported from a DMA-BUF. */
	bool owned; /**< Whether the renderer owns the imported image queue family. */
	bool transitioned; /**< Whether the image has left its initial layout. */
	bool has_alpha; /**< Whether the texture format includes alpha. */
	bool using_mutable_srgb; /**< Whether sampling through an sRGB image view is supported. */
	struct wl_list foreign_link; /**< Link in wsm_vk_renderer.foreign_textures. */
	struct wl_list destroy_link; /**< Link in wsm_vk_command_buffer.destroy_textures. */
	struct wl_list link; /**< Link in wsm_vk_renderer.textures. */

	struct wlr_buffer *buffer; /**< Source buffer for imported textures; may be NULL. */
	struct wlr_addon buffer_addon; /**< Addon associating the texture with its source buffer. */

	struct wl_list views; /**< List of wsm_vk_texture_view.link entries. */
};

/**
 * @brief Imports a DMA-BUF or uploads CPU-accessible buffer pixels.
 * @param wlr_renderer Vulkan renderer importing the texture.
 * @param buffer Source buffer.
 * @return Vulkan texture, or NULL on failure.
 */
struct wsm_vk_texture *wsm_vk_texture_from_buffer(
		struct wlr_renderer *wlr_renderer, struct wlr_buffer *buffer);

/**
 * @brief Imports DMA-BUF attributes as a Vulkan texture.
 * @param renderer Renderer importing the image.
 * @param attribs Source DMA-BUF attributes.
 * @return Vulkan texture, or NULL on failure.
 */
struct wsm_vk_texture *wsm_vk_texture_from_dmabuf(
		struct wsm_vk_renderer *renderer,
		struct wlr_dmabuf_attributes *attribs);

/**
 * @brief Destroys a Vulkan texture and its image resources.
 * @param texture Texture to destroy.
 */
void wsm_vk_texture_destroy(struct wsm_vk_texture *texture);

/**
 * @brief Acquires implicit fences before reading an imported texture.
 * @details The caller owns exported descriptors. Without implicit sync interoperability, this
 *          blocks until the buffer is readable.
 * @param texture Buffer-backed texture to acquire.
 * @param sync_file_fds Output plane fence descriptors; initialize entries to -1.
 * @return true on successful fence export or fallback wait, false on failure.
 */
bool wsm_vk_sync_foreign_texture_acquire(struct wsm_vk_texture *texture,
		int sync_file_fds[static WLR_DMABUF_MAX_PLANES]);

/**
 * @brief Checks whether a texture uses the Vulkan implementation.
 * @param texture Texture to inspect.
 * @return true for a Vulkan texture, false otherwise.
 */
bool wlr_texture_is_wsm_vk(const struct wlr_texture *texture);

/**
 * @brief Casts a generic texture to a Vulkan texture.
 * @details Asserts that the texture has the expected implementation.
 * @param wlr_texture Texture known to use the Vulkan implementation.
 * @return Enclosing Vulkan texture.
 */
struct wsm_vk_texture *wsm_vk_texture_from_texture(struct wlr_texture *wlr_texture);

/**
 * @brief Imports DMA-BUF planes into a Vulkan image.
 * @param renderer Renderer importing the image.
 * @param attribs DMA-BUF image attributes.
 * @param mems Output imported memory allocations.
 * @param n_mems Receives the number of imported memory allocations.
 * @param for_render Whether the image will be used as a render target.
 * @param using_mutable_srgb Receives whether the image permits an sRGB format view.
 * @return Imported image, or VK_NULL_HANDLE on failure.
 */
VkImage wsm_vk_import_dmabuf(struct wsm_vk_renderer *renderer,
		const struct wlr_dmabuf_attributes *attribs,
		VkDeviceMemory mems[static WLR_DMABUF_MAX_PLANES], uint32_t *n_mems,
		bool for_render, bool *using_mutable_srgb);

/**
 * @brief Copies a Vulkan image region into CPU memory.
 * @param vk_renderer Renderer performing the readback.
 * @param src_format Vulkan format of the source image.
 * @param src_image Source image handle.
 * @param drm_format DRM fourcc format for destination pixels.
 * @param stride Destination row stride in bytes.
 * @param width Copy width in pixels.
 * @param height Copy height in pixels.
 * @param src_x Source horizontal offset in pixels.
 * @param src_y Source vertical offset in pixels.
 * @param dst_x Destination horizontal offset in pixels.
 * @param dst_y Destination vertical offset in pixels.
 * @param data Writable destination pixel storage.
 * @param wait_timeline Optional timeline to wait on before reading; may be NULL.
 * @param wait_point Timeline point to wait for.
 * @return true on success, false on failure.
 */
bool wsm_vk_read_pixels(struct wsm_vk_renderer *vk_renderer,
		VkFormat src_format, VkImage src_image,
		uint32_t drm_format, uint32_t stride,
		uint32_t width, uint32_t height, uint32_t src_x, uint32_t src_y,
		uint32_t dst_x, uint32_t dst_y, void *data,
		struct wlr_drm_syncobj_timeline *wait_timeline, uint64_t wait_point);

/**
 * @brief Gets the Vulkan image attributes of a texture.
 * @param texture Texture known to use the Vulkan implementation.
 * @param attribs Output image attributes; image ownership remains with the texture.
 */
void wlr_texture_get_image_attribs_wsm(struct wlr_texture *texture,
		struct wsm_vk_image_attribs *attribs);

/**
 * @brief Checks the alpha metadata of a Vulkan texture.
 * @param texture Texture known to use the Vulkan implementation.
 * @return true when the texture format has alpha, false otherwise.
 */
bool wlr_texture_has_alpha_wsm(struct wlr_texture *texture);

#endif
