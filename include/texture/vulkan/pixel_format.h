/* SPDX-License-Identifier: MIT */

/**
 * @file        pixel_format.h
 * @brief       Vulkan pixel format mappings and device capabilities.
 * @details     Defines DRM format metadata, Vulkan format mappings, and format modifier capability
 *              queries.
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

#ifndef TEXTURE_VULKAN_PIXEL_FORMAT_H
#define TEXTURE_VULKAN_PIXEL_FORMAT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <wayland-server-protocol.h>

#include <vulkan/vulkan.h>

struct wsm_vk_device;

/**
 * @brief Mapping from DRM fourcc to linear and sRGB Vulkan formats.
 */
struct wsm_vk_format {
	uint32_t drm; /**< DRM fourcc format identifier. */
	VkFormat vk; /**< Vulkan format used for linear image access. */
	VkFormat vk_srgb; /**< sRGB variant of the Vulkan format, or VK_FORMAT_UNDEFINED if unavailable. */
};

/**
 * @brief Device capabilities for one DRM format modifier.
 */
struct wsm_vk_format_modifier_props {
	VkImageUsageFlags usage; /**< Image usage flags validated together for this format and modifier. */
	VkDrmFormatModifierPropertiesEXT props; /**< DRM format modifier properties reported by Vulkan. */
	VkExtent2D max_extent; /**< Maximum supported image extent. */
	bool has_mutable_srgb; /**< Whether an sRGB view can be created for the image. */
};

/**
 * @brief Device support for shared-memory and DMA-BUF use of a format.
 */
struct wsm_vk_format_props {
	struct wsm_vk_format format; /**< Pixel format mapping used by the image. */

	struct {
		VkExtent2D max_extent; /**< Maximum supported image extent. */
		VkFormatFeatureFlags features; /**< Supported Vulkan format features. */
		bool has_mutable_srgb; /**< Whether an sRGB view can be created for the image. */
	} shm; /**< Shared-memory image upload capabilities. */

	struct {
		uint32_t render_mod_count; /**< Number of renderable DRM modifiers. */
		struct wsm_vk_format_modifier_props *render_mods; /**< Modifier capabilities for render targets. */

		uint32_t texture_mod_count; /**< Number of sampleable DRM modifiers. */
		struct wsm_vk_format_modifier_props *texture_mods; /**< Modifier capabilities for textures. */
	} dmabuf; /**< DMA-BUF rendering and sampling capabilities. */
};

/**
 * @brief Pixel block storage metadata for a DRM fourcc format.
 *
 * A pixel format is identified via its DRM four character code (see <drm_fourcc.h>).
 *
 * Simple formats have a block size of 1×1 pixels and bytes_per_block contains
 * the number of bytes per pixel (including padding).
 *
 * Tiled formats (e.g. sub-sampled YCbCr) are described with a block size
 * greater than 1×1 pixels. A block is a rectangle of pixels which are stored
 * next to each other in a byte-aligned memory region.
 */
struct wsm_pixel_format {
	uint32_t drm_format; /**< DRM fourcc format identifier. */

	uint32_t bytes_per_block; /**< Bytes per storage block, including padding. */
	uint32_t block_width, block_height; /**< Storage block width and height in pixels. */
};

/**
 * @brief Gets pixel storage metadata for a DRM format.
 * @param fmt DRM fourcc format identifier.
 * @return Static format metadata, or NULL for an unknown format.
 */
const struct wsm_pixel_format *wsm_vk_drm_get_pixel_format(uint32_t fmt);

/**
 * @brief Gets the number of pixels stored in a format block.
 * @param info Pixel format metadata.
 * @return Pixels per block, treating unspecified block dimensions as one pixel.
 */
uint32_t wsm_vk_pixel_format_pixels_per_block(const struct wsm_pixel_format *info);

/**
 * @brief Computes the minimum byte stride for a pixel format.
 * @param info Pixel format metadata.
 * @param width Image width in pixels.
 * @return Minimum stride in bytes, or zero on overflow.
 */
int32_t wsm_vk_pixel_format_min_stride(const struct wsm_pixel_format *info, int32_t width);

/**
 * @brief Checks whether a stride can hold a row of pixels.
 * @param info Pixel format metadata.
 * @param stride Row stride in bytes.
 * @param width Image width in pixels.
 * @return true when the stride is sufficient, false otherwise.
 */
bool wsm_vk_pixel_format_check_stride(const struct wsm_pixel_format *info,
	int32_t stride, int32_t width);

/**
 * @brief Converts a Wayland shared-memory format to DRM fourcc.
 * @param fmt Wayland shared-memory format.
 * @return Corresponding DRM fourcc identifier.
 */
uint32_t wsm_vk_convert_wl_shm_format_to_drm(enum wl_shm_format fmt);

/**
 * @brief Converts a DRM fourcc to a Wayland shared-memory format.
 * @param fmt DRM fourcc format identifier.
 * @return Corresponding Wayland shared-memory format.
 */
enum wl_shm_format wsm_vk_convert_drm_format_to_wl_shm(uint32_t fmt);

/**
 * @brief Checks whether a DRM format is known to be opaque.
 * @param fmt DRM fourcc format identifier.
 * @return true for a known opaque format, false otherwise.
 */
bool wsm_vk_pixel_format_is_opaque(uint32_t fmt);

/**
 * @brief Checks whether a DRM format may contain an alpha channel.
 * @param fmt DRM fourcc format identifier.
 * @return false for a known opaque format, true otherwise.
 */
bool wsm_vk_pixel_format_has_alpha(uint32_t fmt);

/**
 * @brief Checks whether a DRM format belongs to a YCbCr family.
 * @param fmt DRM fourcc format identifier.
 * @return true for a recognized YCbCr format, false otherwise.
 */
bool wsm_vk_pixel_format_is_ycbcr(uint32_t fmt);

/**
 * @brief Gets the opaque equivalent of an alpha-bearing DRM format.
 * @param fmt DRM fourcc format identifier.
 * @return Opaque DRM format, or DRM_FORMAT_INVALID when no substitute exists.
 */
uint32_t wsm_vk_pixel_format_get_opaque_substitute(uint32_t fmt);

/**
 * @brief Gets all known DRM-to-Vulkan format mappings.
 * @param len Receives the number of mappings.
 * @return Static array of mappings; individual device support must be queried separately.
 */
const struct wsm_vk_format *vulkan_get_format_list(size_t *len);

/**
 * @brief Looks up a Vulkan mapping for a DRM format.
 * @param drm_format DRM fourcc format identifier.
 * @return Static format mapping, or NULL when no mapping exists.
 */
const struct wsm_vk_format *vulkan_get_format_from_drm(uint32_t drm_format);

/**
 * @brief Queries and records device capabilities for a format mapping.
 * @param dev Device receiving supported formats and modifiers.
 * @param format Format mapping to query.
 */
void vulkan_format_props_query(struct wsm_vk_device *dev,
	const struct wsm_vk_format *format);

/**
 * @brief Finds a supported modifier for rendering or texturing.
 * @param props Queried device format properties.
 * @param mod DRM format modifier to find.
 * @param render Whether to search rendering rather than texturing capabilities.
 * @return Matching modifier properties, or NULL when unsupported.
 */
const struct wsm_vk_format_modifier_props *vulkan_format_props_find_modifier(
	const struct wsm_vk_format_props *props, uint64_t mod, bool render);

/**
 * @brief Releases modifier arrays owned by queried format properties.
 * @param props Format properties to finish.
 */
void vulkan_format_props_finish(struct wsm_vk_format_props *props);

/**
 * @brief Checks whether a Vulkan format mapping describes YCbCr data.
 * @param format Format mapping to inspect.
 * @return true for a YCbCr mapping, false otherwise.
 */
bool vulkan_format_is_ycbcr(const struct wsm_vk_format *format);

/**
 * @brief Looks up queried format properties on a device.
 * @param dev Device whose cached properties are searched.
 * @param drm_format DRM fourcc format identifier.
 * @return Device-owned format properties, or NULL when unavailable.
 */
struct wsm_vk_format_props *vulkan_format_props_from_drm(
	struct wsm_vk_device *dev, uint32_t drm_format);

#endif
