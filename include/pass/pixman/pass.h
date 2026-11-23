/* SPDX-License-Identifier: MIT */

/**
 * @file        pass.h
 * @brief       Pixman render pass state and creation helpers.
 * @details     This file defines the render pass used to render into a
 *              pixman-backed buffer through the shared wsm render pass interface.
 *
 * @par Upstream:
 *              Derived from wlroots; see LICENSE.wlroots in the project root.
 *
 * @author      YaoBing Xiao (wsm adaptations)
 * @date        2026-08-23
 * @version     v1.0
 * @par Copyright(c):
 *              Upstream copyright notices and MIT license: LICENSE.wlroots
 *              in the project root. See LICENSE for wsm copyright and license.
 * @par History:
 *      version: v1.0, YaoBing Xiao, 2026-08-23, initial version\n
 */

#ifndef PASS_PIXMAN_PASS_H
#define PASS_PIXMAN_PASS_H

#include "buffer/pixman/buffer.h"

#include "pass/pass.h"

#include <stdbool.h>

#include <wlr/render/interface.h>

/**
 * @brief Pixman render pass state.
 */
struct wsm_pixman_render_pass {
	struct wsm_render_pass base; /**< Embedded wsm base exposing wlroots and extended render operations. */
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
 * @return Borrowed pointer to the enclosing pixman render pass.
 * @pre The pass must be non-NULL and use the wsm pixman backend.
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

#endif // PASS_PIXMAN_PASS_H
