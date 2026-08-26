/**
 * @file        buffer.h
 * @brief       Pixman buffer wrapper for the wsm renderer.
 * @details     This file defines the pixman-backed buffer objects used to
 *              access wlroots buffers and read-only CPU pixel data.
 * @author      YaoBing Xiao
 * @date        2026-08-23
 * @version     v1.0
 * @par Copyright(c):
 * @par History:
 *      version: v1.0, YaoBing Xiao, 2026-08-23, initial version\n
 */

#ifndef BUFFER_PIXMAN_BUFFER_H
#define BUFFER_PIXMAN_BUFFER_H

#include "renderer/pixman/renderer.h"

#include <stdint.h>
#include <stdbool.h>

#include <wayland-server-core.h>

#include <pixman-1/pixman.h>

#include <wlr/types/wlr_buffer.h>

/**
 * @brief Pixman resources associated with a wlroots buffer.
 */
struct wsm_pixman_buffer {
	struct wlr_buffer *base; /**< Wrapped wlroots buffer. */
	struct wsm_pixman_renderer *renderer; /**< Renderer that owns the wrapper. */

	pixman_image_t *image; /**< Pixman image backed by the buffer. */

	struct wl_listener buffer_destroy; /**< Listener for buffer destruction. */
	struct wl_list link; /**< Link in wlr_pixman_renderer.buffers. */
};

/**
 * @brief Generic buffer backed by read-only CPU pixel data.
 */
struct wsm_readonly_data_buffer {
	struct wlr_buffer base; /**< Generic wlroots buffer interface. */

	const void *data; /**< Read-only pixel data. */
	uint32_t format; /**< DRM pixel format of the data. */
	size_t stride; /**< Row stride in bytes. */

	void *saved_data; /**< Temporary writable mapping, when present. */
};

/**
 * @brief Creates a pixman wrapper for a wlroots buffer.
 * @param renderer Pixman renderer that owns the wrapper.
 * @param wlr_buffer Buffer to wrap.
 * @return Newly created wrapper, or NULL on failure.
 */
struct wsm_pixman_buffer *wsm_pixman_buffer_create(
	struct wsm_pixman_renderer *renderer, struct wlr_buffer *wlr_buffer);

/**
 * @brief Destroys a pixman buffer wrapper.
 * @param buffer Buffer wrapper to destroy.
 */
void wsm_pixman_buffer_destroy(struct wsm_pixman_buffer *buffer);

/**
 * @brief Checks whether a wlroots buffer is a pixman buffer owned by a renderer.
 * @param renderer Pixman renderer to search.
 * @param wlr_buffer Buffer to inspect.
 * @return true when the buffer is a matching pixman buffer, false otherwise.
 */
bool wlr_buffer_is_wsm_pixman(struct wsm_pixman_renderer *renderer,
	const struct wlr_buffer *wlr_buffer);

/**
 * @brief Gets the pixman wrapper for a wlroots buffer.
 * @param renderer Pixman renderer to search.
 * @param wlr_buffer Buffer to look up.
 * @return Matching wrapper, or NULL when the buffer is not wrapped.
 */
struct wsm_pixman_buffer *wsm_pixman_buffer_from_buffer(
	struct wsm_pixman_renderer *renderer, const struct wlr_buffer *wlr_buffer);

/**
 * @brief Begins CPU access to a pixman buffer's image.
 * @param buffer Buffer to access.
 * @param image_ptr Destination for the pixman image pointer.
 * @param flags Access flags requested by the caller.
 * @return true on success, false when access cannot be started.
 */
bool wsm_pixman_buffer_begin_data_ptr_access(struct wlr_buffer *buffer,
	pixman_image_t **image_ptr, uint32_t flags);

/**
 * @brief Checks whether a buffer is a read-only data buffer.
 * @param wlr_buffer Buffer to inspect.
 * @return true when the buffer is a read-only data buffer, false otherwise.
 */
bool wlr_buffer_is_wsm_readonly_data(const struct wlr_buffer *wlr_buffer);

/**
 * @brief Gets the read-only data wrapper for a wlroots buffer.
 * @param wlr_buffer Buffer to inspect.
 * @return Read-only data wrapper, or NULL when the type does not match.
 */
struct wsm_readonly_data_buffer *wsm_readonly_data_buffer_from_buffer(
	struct wlr_buffer *wlr_buffer);

/**
 * @brief Creates a buffer backed by read-only CPU pixel data.
 * @param format DRM pixel format of the data.
 * @param stride Row stride in bytes.
 * @param width Buffer width in pixels.
 * @param height Buffer height in pixels.
 * @param data Pixel data owned by the caller.
 * @return Newly created buffer, or NULL on failure.
 */
struct wsm_readonly_data_buffer *wsm_readonly_data_buffer_create(uint32_t format,
	size_t stride, uint32_t width, uint32_t height, const void *data);

/**
 * @brief Drops a read-only data buffer reference.
 * @param buffer Buffer to release.
 * @return true when the buffer was released, false otherwise.
 */
bool wsm_readonly_data_buffer_drop(struct wsm_readonly_data_buffer *buffer);

#endif // BUFFER_PIXMAN_BUFFER_H
