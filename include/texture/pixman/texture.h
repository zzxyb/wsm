/**
 * @file        texture.h
 * @brief       Pixman texture implementation for the wsm renderer.
 * @details     This file defines the pixman texture state and helpers for
 *              creating textures from pixel data and wlroots buffers.
 * @author      YaoBing Xiao
 * @date        2026-08-23
 * @version     v1.0
 * @par Copyright(c):
 * @par History:
 *      version: v1.0, YaoBing Xiao, 2026-08-23, initial version\n
 */

#ifndef PIXMAN_TEXTURE_H
#define PIXMAN_TEXTURE_H

#include "renderer/pixman/renderer.h"
#include "texture/wsm_pixel_format_info.h"

#include <stdint.h>

#include <wayland-server-core.h>

#include <pixman-1/pixman.h>

#include <wlr/render/wlr_texture.h>

/**
 * @brief Pixman texture state.
 */
struct wsm_pixman_texture {
	struct wlr_texture base; /**< Generic wlroots texture interface. */
	struct wsm_pixman_renderer *renderer; /**< Renderer that owns the texture. */
	struct wl_list link; /**< Link in wlr_pixman_renderer.textures. */

	pixman_image_t *image; /**< Pixman image containing the texture data. */
	const struct wsm_pixel_format_info *format_info; /**< Pixel format metadata. */

	struct wlr_buffer *buffer; /**< Source buffer, when the texture is buffer-backed. */
	void *user_data; /**< Backend-specific user data. */
	pixman_format_code_t format; /**< Pixman format of the image. */
};

/**
 * @brief Creates an empty pixman texture.
 * @param renderer Pixman renderer that owns the texture.
 * @param drm_format DRM fourcc format of the texture.
 * @param width Texture width in pixels.
 * @param height Texture height in pixels.
 * @return Newly created texture, or NULL on failure.
 */
struct wsm_pixman_texture *wsm_pixman_texture_create(
	struct wsm_pixman_renderer *renderer, uint32_t drm_format,
	uint32_t width, uint32_t height);

/**
 * @brief Destroys a pixman texture.
 * @param texture Texture to destroy.
 */
void wsm_pixman_texture_destroy(struct wsm_pixman_texture *texture);

/**
 * @brief Creates a pixman texture from a wlroots buffer.
 * @param pixman_renderer Pixman renderer that owns the texture.
 * @param buffer Buffer to import.
 * @return Generic texture, or NULL on failure.
 */
struct wsm_pixman_texture *wsm_pixman_texture_from_buffer(
	struct wsm_pixman_renderer *pixman_renderer, struct wlr_buffer *buffer);

/**
 * @brief Checks whether a texture is a wsm pixman texture.
 * @param texture Texture to inspect.
 * @return true when the texture is pixman-backed, false otherwise.
 */
bool wlr_texture_is_wsm_pixman(const struct wlr_texture *texture);

/**
 * @brief Casts a generic texture to a pixman texture.
 * @param texture Texture known to be pixman-backed.
 * @return Enclosing pixman texture, or NULL when the type does not match.
 */
struct wsm_pixman_texture *wsm_pixman_texture_from_texture(struct wlr_texture *texture);

#endif // PIXMAN_TEXTURE_H
