/**
 * @file        pixel_format.h
 * @brief       GLES2 pixel format descriptions and lookup helpers.
 * @details     This file maps DRM formats to the GLES2 format and type values
 *              required for texture upload and framebuffer operations.
 * @author      YaoBing Xiao
 * @date        2026-08-23
 * @version     v1.0
 * @par Copyright(c):
 * @par History:
 *      version: v1.0, YaoBing Xiao, 2026-08-23, initial version\n
 */

#ifndef RENDERER_GLES_PIXEL_FORMAT_H
#define RENDERER_GLES_PIXEL_FORMAT_H

#include <stdbool.h>

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

struct wsm_gles2_renderer;
struct wlr_gles2_pixel_format;
struct wlr_drm_format_set;

/**
 * @brief Mapping between a DRM format and GLES2 upload values.
 */
struct wlr_gles2_pixel_format {
	uint32_t drm_format; /**< DRM fourcc format identifier. */
	/** GLES internal format, or zero to use @ref gl_format. */
	GLint gl_internalformat;
	GLint gl_format; /**< GLES format used for upload. */
	GLint gl_type; /**< GLES data type used for upload. */
};

/**
 * @brief Checks whether a GLES2 format is supported by a renderer.
 * @param renderer GLES2 renderer to query.
 * @param format Pixel format to check.
 * @return true when the format is supported, false otherwise.
 */
bool is_gles2_pixel_format_supported(const struct wsm_gles2_renderer *renderer,
	const struct wlr_gles2_pixel_format *format);

/**
 * @brief Gets a GLES2 format description from a DRM format.
 * @param fmt DRM fourcc format identifier.
 * @return Matching format, or NULL when unsupported.
 */
const struct wlr_gles2_pixel_format *get_gles2_format_from_drm(uint32_t fmt);

/**
 * @brief Gets a GLES2 format description from GLES format and type values.
 * @param gl_format GLES format value.
 * @param gl_type GLES data type value.
 * @param alpha Whether the format includes alpha.
 * @return Matching format, or NULL when unsupported.
 */
const struct wlr_gles2_pixel_format *get_gles2_format_from_gl(
	GLint gl_format, GLint gl_type, bool alpha);

/**
 * @brief Fills a format set with supported shared-memory formats.
 * @param renderer GLES2 renderer to query.
 * @param out Format set to populate.
 */
void get_gles2_shm_formats(const struct wsm_gles2_renderer *renderer,
	struct wlr_drm_format_set *out);

#endif // RENDERER_GLES_PIXEL_FORMAT_H
