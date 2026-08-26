/**
 * @file        buffer.h
 * @brief       GLES2 buffer wrapper for the wsm renderer.
 * @details     This file defines the buffer object used to associate a wlroots
 *              buffer with an EGL image and GLES framebuffer resources.
 * @author      YaoBing Xiao
 * @date        2026-08-23
 * @version     v1.0
 * @par Copyright(c):
 * @par History:
 *      version: v1.0, YaoBing Xiao, 2026-08-23, initial version\n
 */

#ifndef BUFFER_GLES2_BUFFER_H
#define BUFFER_GLES2_BUFFER_H

#include "renderer/gles/renderer.h"

#include <stdbool.h>

#include <wayland-util.h>

#include <GLES2/gl2.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>

#include <wlr/util/addon.h>
#include <wlr/types/wlr_buffer.h>

/**
 * @brief GLES2 resources associated with a wlroots buffer.
 */
struct wsm_gles2_buffer {
	struct wlr_buffer *base; /**< Wrapped wlroots buffer. */
	struct wsm_gles2_renderer *renderer; /**< Renderer that owns the wrapper. */
	struct wl_list link; /**< Link in wsm_gles2_renderer.buffers. */
	bool external_only; /**< Whether the image can only be sampled externally. */

	EGLImageKHR image; /**< EGL image imported from the buffer. */
	GLuint rbo; /**< GLES renderbuffer associated with the image. */
	GLuint fbo; /**< GLES framebuffer associated with the image. */
	GLuint tex; /**< GLES texture associated with the image. */

	struct wlr_addon addon; /**< Addon used to cache this wrapper on the buffer. */
};

/**
 * @brief Gets or creates a GLES2 wrapper for a wlroots buffer.
 * @param renderer GLES2 renderer that owns the wrapper.
 * @param wlr_buffer Buffer to wrap.
 * @return Existing or newly created wrapper, or NULL on failure.
 */
struct wsm_gles2_buffer *wsm_gles2_buffer_get_or_create(struct wsm_gles2_renderer *renderer,
	struct wlr_buffer *wlr_buffer);

/**
 * @brief Destroys a GLES2 buffer wrapper.
 * @param buffer Buffer wrapper to destroy.
 */
void wsm_gles2_buffer_destroy(struct wsm_gles2_buffer *buffer);

/**
 * @brief Gets the framebuffer object used to render to a buffer.
 * @param buffer GLES2 buffer wrapper to inspect.
 * @return GLES framebuffer object name.
 */
GLuint wsm_gles2_buffer_get_fbo(struct wsm_gles2_buffer *buffer);

#endif // BUFFER_GLES2_BUFFER_H
