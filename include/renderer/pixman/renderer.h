/**
 * @file        renderer.h
 * @brief       Pixman renderer state and helper functions.
 * @details     This file defines the software renderer state and helpers for
 *              accessing pixman images backed by wlroots buffers.
 * @author      YaoBing Xiao
 * @date        2026-08-23
 * @version     v1.0
 * @par Copyright(c):
 * @par History:
 *      version: v1.0, YaoBing Xiao, 2026-08-23, initial version\n
 */

#ifndef RENDERER_PIXMAN_RENDERER_H
#define RENDERER_PIXMAN_RENDERER_H

#include <stdint.h>
#include <stdbool.h>

#include <wayland-server-core.h>

#include <pixman-1/pixman.h>

#include <wlr/render/wlr_renderer.h>
#include <wlr/render/drm_format_set.h>

/**
 * @brief Pixman renderer state and resource lists.
 */
struct wsm_pixman_renderer {
	struct wlr_renderer base; /**< Generic wlroots renderer interface. */
	void *data; /**< Backend-specific renderer data. */

	struct wl_list buffers; /**< List of wsm_pixman_buffer.link entries. */
	struct wl_list textures; /**< List of wsm_pixman_texture.link entries. */

	struct wsm_pixman_buffer *current_buffer; /**< Buffer currently mapped for rendering. */

	struct wlr_drm_format_set drm_formats; /**< Formats supported by the renderer. */
};

/**
 * @brief Creates a pixman renderer.
 * @return pixman renderer, or NULL on failure.
 */
struct wsm_pixman_renderer *wsm_pixman_renderer_create(void);

/**
 * @brief Checks whether a renderer is a wsm pixman renderer.
 * @param wlr_renderer Renderer to inspect.
 * @return true when the renderer is pixman-backed, false otherwise.
 */
bool wlr_renderer_is_wsm_pixman(const struct wlr_renderer *wlr_renderer);

/**
 * @brief Casts a generic renderer to a pixman renderer.
 * @param renderer Renderer known to be pixman-backed.
 * @return Enclosing pixman renderer, or NULL when the type does not match.
 */
struct wsm_pixman_renderer *wsm_pixman_renderer_from_renderer(struct wlr_renderer *renderer);

/**
 * @brief Gets the pixman image associated with a wlroots buffer.
 * @param wlr_renderer Renderer used to find the buffer.
 * @param wlr_buffer Buffer to inspect.
 * @return Pixman image, or NULL when the buffer cannot be accessed.
 */
pixman_image_t *wsm_pixman_renderer_get_buffer_image(
	struct wlr_renderer *wlr_renderer, struct wlr_buffer *wlr_buffer);

/**
 * @brief Begins data-pointer access to a pixman buffer.
 * @param wlr_buffer Buffer to access.
 * @param image_ptr Destination for the pixman image pointer.
 * @param flags Access flags requested by the caller.
 * @return true on success, false on failure.
 */
bool wsm_renderer_begin_pixman_data_ptr_access(struct wlr_buffer *wlr_buffer,
	pixman_image_t **image_ptr, uint32_t flags);

#endif // RENDERER_PIXMAN_RENDERER_H
