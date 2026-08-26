/**
 * @file        pixel_format.h
 * @brief       Pixman pixel format descriptions and lookup helpers.
 * @details     This file maps DRM fourcc formats to pixman format codes and
 *              exposes the formats supported by the pixman backend.
 * @author      YaoBing Xiao
 * @date        2026-08-23
 * @version     v1.0
 * @par Copyright(c):
 * @par History:
 *      version: v1.0, YaoBing Xiao, 2026-08-23, initial version\n
 */

#ifndef RENDERER_PIXMAN_PIXEL_FORMAT_H
#define RENDERER_PIXMAN_PIXEL_FORMAT_H

#include <stdint.h>
#include <stdbool.h>

#include <pixman-1/pixman.h>

#include <wlr/render/drm_format_set.h>

/**
 * @brief Mapping between a DRM format and a pixman format code.
 */
struct wsm_pixman_pixel_format {
	uint32_t drm_format; /**< DRM fourcc format identifier. */
	pixman_format_code_t pixman_format; /**< Corresponding pixman format code. */
};

/**
 * @brief Converts a DRM format to a pixman format.
 * @param fmt DRM fourcc format identifier.
 * @return Pixman format code, or zero when unsupported.
 */
pixman_format_code_t get_pixman_format_from_drm(uint32_t fmt);

/**
 * @brief Converts a pixman format to a DRM format.
 * @param fmt Pixman format code.
 * @return DRM fourcc format identifier, or zero when unsupported.
 */
uint32_t get_drm_format_from_pixman(pixman_format_code_t fmt);

/**
 * @brief Gets the DRM formats supported by pixman.
 * @param len Destination for the number of formats.
 * @return Renderer-owned array of DRM format identifiers.
 */
const uint32_t *get_pixman_drm_formats(size_t *len);

#endif // RENDERER_PIXMAN_PIXEL_FORMAT_H
