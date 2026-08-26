/**
 * @file        texture.h
 * @brief       GLES2 texture implementation for the wsm renderer.
 * @details     This file defines texture state for pixel uploads, DMA-BUF
 *              imports, framebuffer attachments, and texture ownership.
 * @author      YaoBing Xiao
 * @date        2026-08-23
 * @version     v1.0
 * @par Copyright(c):
 * @par History:
 *      version: v1.0, YaoBing Xiao, 2026-08-23, initial version\n
 */

#ifndef GLES_TEXTURE_H
#define GLES_TEXTURE_H

#include <GLES2/gl2.h>

#include <wlr/types/wlr_buffer.h>
#include <wlr/render/wlr_texture.h>

/**
 * @brief GLES2 texture state.
 */
struct wsm_gles2_texture {
	struct wlr_texture base; /**< Generic wlroots texture interface. */
	struct wsm_gles2_renderer *renderer; /**< Renderer that owns the texture. */
	struct wl_list link; /**< Link in wlr_gles2_renderer.textures. */

	GLenum target; /**< GLES texture target. */

	/** Texture and framebuffer names, borrowed for imported buffers. */
	GLuint tex;
	GLuint fbo;

	bool has_alpha; /**< Whether the texture contains an alpha channel. */

	uint32_t drm_format; /**< DRM format for mutable texture upload data. */
	struct wsm_gles2_buffer *buffer; /**< Imported DMA-BUF buffer, when present. */
};

/**
 * @brief Creates a GLES2 texture from pixel data.
 * @param wlr_renderer Renderer that owns the texture.
 * @param drm_format DRM fourcc format of the data.
 * @param stride Row stride in bytes.
 * @param width Texture width in pixels.
 * @param height Texture height in pixels.
 * @param data Pixel data to upload.
 * @return Generic texture, or NULL on failure.
 */
struct wsm_gles2_texture *wsm_gles2_texture_from_pixels(
	struct wlr_renderer *wlr_renderer,
	uint32_t drm_format, uint32_t stride, uint32_t width,
	uint32_t height, const void *data);

/**
 * @brief Creates a GLES2 texture from a wlroots buffer.
 * @param wlr_renderer Renderer that owns the texture.
 * @param buffer Buffer to import.
 * @return Generic texture, or NULL on failure.
 */
struct wsm_gles2_texture *wsm_gles2_texture_from_buffer(struct wlr_renderer *wlr_renderer,
	struct wlr_buffer *buffer);

/**
 * @brief Destroys a GLES2 texture.
 * @param texture Texture to destroy.
 */
void wsm_gles2_texture_destroy(struct wsm_gles2_texture *texture);

/**
 * @brief Checks whether a texture is a wsm GLES2 texture.
 * @param wlr_texture Texture to inspect.
 * @return true when the texture is GLES2-backed, false otherwise.
 */
bool wlr_texture_is_wsm_gles2(const struct wlr_texture *wlr_texture);

/**
 * @brief Casts a generic texture to a GLES2 texture.
 * @param wlr_texture Texture known to be GLES2-backed.
 * @return Enclosing GLES2 texture, or NULL when the type does not match.
 */
struct wsm_gles2_texture *wsm_gles2_texture_from_texture(
	struct wlr_texture *wlr_texture);

/**
 * @brief Imports a DMA-BUF as a GLES2 texture.
 * @param renderer GLES2 renderer that owns the texture.
 * @param wlr_buffer Buffer associated with the import.
 * @param attribs DMA-BUF attributes describing the planes.
 * @return GLES2 texture, or NULL on failure.
 */
struct wsm_gles2_texture *wsm_gles2_texture_from_dmabuf(
	struct wsm_gles2_renderer *renderer, struct wlr_buffer *wlr_buffer,
	struct wlr_dmabuf_attributes *attribs);

#endif // GLES_TEXTURE_H
