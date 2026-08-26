/**
 * @file        pass.h
 * @brief       GLES2 render pass state and creation helpers.
 * @details     This file defines the render pass used to render into a GLES2
 *              buffer and optionally signal a DRM synchronization timeline.
 * @author      YaoBing Xiao
 * @date        2026-08-23
 * @version     v1.0
 * @par Copyright(c):
 * @par History:
 *      version: v1.0, YaoBing Xiao, 2026-08-23, initial version\n
 */

#ifndef PASS_GLES_RENDERPASS_H
#define PASS_GLES_RENDERPASS_H

#include "renderer/gles/egl.h"
#include "buffer/gles/buffer.h"
#include "renderer/gles/render_timer.h"

#include <stdbool.h>

#include <wlr/render/interface.h>
#include <wlr/render/drm_syncobj.h>

/**
 * @brief GLES2 render pass state.
 */
struct wsm_gles2_render_pass {
	struct wlr_render_pass base; /**< Generic wlroots render pass interface. */
	struct wsm_gles2_buffer *buffer; /**< Destination buffer. */
	float projection_matrix[9]; /**< Projection matrix for the destination. */
	struct wsm_egl_context prev_ctx; /**< EGL context saved before the pass. */
	struct wsm_gles2_render_timer *timer; /**< Optional render timer. */
	struct wlr_drm_syncobj_timeline *signal_timeline; /**< Optional signal timeline. */
	uint64_t signal_point; /**< Timeline point to signal on completion. */
};

/**
 * @brief Begins a GLES2 buffer render pass.
 * @param buffer Destination GLES2 buffer.
 * @param prev_ctx EGL context state to restore when the pass ends.
 * @param timer Optional GLES2 render timer.
 * @param signal_timeline Optional DRM synchronization timeline.
 * @param signal_point Timeline point to signal.
 * @return Newly allocated render pass, or NULL on failure.
 */
struct wsm_gles2_render_pass *wsm_render_pass_begin_gles2_buffer_pass(struct wsm_gles2_buffer *buffer,
	struct wsm_egl_context *prev_ctx, struct wsm_gles2_render_timer *timer,
	struct wlr_drm_syncobj_timeline *signal_timeline, uint64_t signal_point);

/**
 * @brief Checks whether a render pass is a wsm GLES2 render pass.
 * @param wlr_pass Render pass to inspect.
 * @return true when the render pass is GLES2-backed, false otherwise.
 */
bool wlr_render_pass_is_wsm_gles2(const struct wlr_render_pass *wlr_pass);

/**
 * @brief Casts a generic render pass to a GLES2 render pass.
 * @param wlr_pass Render pass known to be GLES2-backed.
 * @return Enclosing GLES2 render pass, or NULL when the type does not match.
 */
struct wsm_gles2_render_pass *wlr_gles2_render_pass_from_render_pass(
	struct wlr_render_pass *wlr_pass);

#endif // PASS_GLES_RENDERPASS_H
