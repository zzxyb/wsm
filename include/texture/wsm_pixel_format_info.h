/**
 * @file        wsm_pixel_format_info.h
 * @brief       DRM pixel format metadata and conversion helpers.
 * @details     This file describes pixel block sizes, strides, alpha support,
 *              and conversions between DRM and Wayland shared-memory formats.
 * @author      YaoBing Xiao
 * @date        2026-08-23
 * @version     v1.0
 * @par Copyright(c):
 * @par History:
 *      version: v1.0, YaoBing Xiao, 2026-08-23, initial version\n
 */

#ifndef TEXTURE_WSM_PIXEL_FORMAT_INFO_H
#define TEXTURE_WSM_PIXEL_FORMAT_INFO_H

#include <stdint.h>

#include <wayland-server-protocol.h>

/**
 * @brief Information about a pixel format.
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
struct wsm_pixel_format_info {
	uint32_t drm_format; /**< DRM fourcc format identifier. */

	/** Equivalent opaque format, or DRM_FORMAT_INVALID when unavailable. */
	uint32_t opaque_substitute;

	uint32_t bytes_per_block; /**< Bytes per block, including padding. */
	/** Block dimensions in pixels; zero denotes the simple 1x1 case. */
	uint32_t block_width, block_height;
};

/**
 * @brief Gets pixel format information from a DRM fourcc.
 * @param fmt DRM fourcc format identifier.
 * @return Format metadata, or NULL when the format is unknown.
 */
const struct wsm_pixel_format_info *drm_get_pixel_format_info(uint32_t fmt);

/**
 * @brief Gets the number of pixels per block.
 * @param info Pixel format metadata.
 * @return Number of pixels in one block.
 */
uint32_t pixel_format_info_pixels_per_block(const struct wsm_pixel_format_info *info);

/**
 * @brief Gets the minimum stride for a pixel format and width.
 * @param info Pixel format metadata.
 * @param width Width in pixels.
 * @return Minimum stride in bytes, or a negative value on overflow/error.
 */
int32_t pixel_format_info_min_stride(const struct wsm_pixel_format_info *info, int32_t width);

/**
 * @brief Checks whether a stride is sufficient for a pixel format and width.
 * @param info Pixel format metadata.
 * @param stride Row stride in bytes.
 * @param width Width in pixels.
 * @return true when the stride is sufficient, false otherwise.
 */
bool pixel_format_info_check_stride(const struct wsm_pixel_format_info *info,
	int32_t stride, int32_t width);

/**
 * @brief Converts a Wayland shared-memory format to a DRM fourcc.
 * @param fmt Wayland shared-memory format.
 * @return Corresponding DRM format, or DRM_FORMAT_INVALID when unsupported.
 */
uint32_t convert_wl_shm_format_to_drm(enum wl_shm_format fmt);

/**
 * @brief Converts a DRM fourcc to a Wayland shared-memory format.
 * @param fmt DRM fourcc format identifier.
 * @return Corresponding Wayland format, or WL_SHM_FORMAT_INVALID when unsupported.
 */
enum wl_shm_format convert_drm_format_to_wl_shm(uint32_t fmt);

/**
 * @brief Checks whether a DRM fourcc has an alpha channel.
 * @param fmt DRM fourcc format identifier.
 * @return true when alpha is present, false otherwise.
 */
bool pixel_format_has_alpha(uint32_t fmt);

#endif // TEXTURE_WSM_PIXEL_FORMAT_INFO_H
