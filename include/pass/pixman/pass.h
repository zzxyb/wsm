/**
 * @file        pass.h
 * @brief       Pixman render pass state and creation helpers.
 * @details     This file defines the render pass used to render into a
 *              pixman-backed buffer.
 * @author      YaoBing Xiao
 * @date        2026-08-23
 * @version     v1.0
 * @par Copyright(c):
 * @par History:
 *      version: v1.0, YaoBing Xiao, 2026-08-23, initial version\n
 */

#ifndef PASS_PIXMAN_RENDER_PASS_H
#define PASS_PIXMAN_RENDER_PASS_H

#include "buffer/pixman/buffer.h"

#include <stdbool.h>

#include <wlr/render/interface.h>

/**
 * @brief Pixman render pass state.
 */
struct wsm_pixman_render_pass {
	struct wlr_render_pass base; /**< Generic wlroots render pass interface. */
	struct wsm_pixman_buffer *buffer; /**< Destination pixman buffer. */
};

/**
 * @brief Checks whether a render pass is a wsm pixman render pass.
 * @param wlr_pass Render pass to inspect.
 * @return true when the render pass is pixman-backed, false otherwise.
 */
bool wlr_render_pass_is_wsm_pixman(const struct wlr_render_pass *wlr_pass);

/**
 * @brief Casts a generic render pass to a pixman render pass.
 * @param wlr_pass Render pass known to be pixman-backed.
 * @return Enclosing pixman render pass, or NULL when the type does not match.
 */
struct wsm_pixman_render_pass *wsm_pixman_render_pass_from_render_pass(
	struct wlr_render_pass *wlr_pass);

/**
 * @brief Begins a pixman render pass.
 * @param buffer Destination pixman buffer.
 * @return Newly allocated render pass, or NULL on failure.
 */
struct wsm_pixman_render_pass *wsm_render_pass_begin_pixman_render_pass(
	struct wsm_pixman_buffer *buffer);

#endif // PASS_PIXMAN_RENDER_PASS_H
