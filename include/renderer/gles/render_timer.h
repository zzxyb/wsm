/**
 * @file        render_timer.h
 * @brief       GLES2 render timer implementation.
 * @details     This file defines the timer state used to measure CPU and GPU
 *              work performed by the GLES2 renderer.
 * @author      YaoBing Xiao
 * @date        2026-08-23
 * @version     v1.0
 * @par Copyright(c):
 * @par History:
 *      version: v1.0, YaoBing Xiao, 2026-08-23, initial version\n
 */

#ifndef RENDERER_GLES_RENDER_TIMER_H
#define RENDERER_GLES_RENDER_TIMER_H

#include <GLES2/gl2.h>

#include <wlr/render/interface.h>

struct wsm_gles2_renderer;

/**
 * @brief GLES2 render timer state.
 */
struct wsm_gles2_render_timer {
	struct wlr_render_timer base; /**< Generic wlroots timer interface. */
	struct wsm_gles2_renderer *renderer; /**< GLES2 renderer being measured. */
	struct timespec cpu_start; /**< CPU timestamp at timer start. */
	struct timespec cpu_end; /**< CPU timestamp at timer completion. */
	GLuint id; /**< GLES query object name. */
	GLint64 gl_cpu_end; /**< GPU timestamp corresponding to CPU completion. */
};

/**
 * @brief Creates a GLES2 render timer.
 * @param wlr_renderer Renderer whose work will be measured.
 * @return Generic render timer, or NULL on failure.
 */
struct wsm_gles2_render_timer *wsm_gles2_render_timer_create(struct wlr_renderer *wlr_renderer);

/**
 * @brief Checks whether a render timer belongs to the GLES2 backend.
 * @param timer Timer to inspect.
 * @return true when the timer is a GLES2 timer, false otherwise.
 */
bool wlr_render_timer_is_wsm_gles2(const struct wlr_render_timer *timer);

/**
 * @brief Casts a generic render timer to a GLES2 timer.
 * @param timer Timer known to be a GLES2 timer.
 * @return Enclosing GLES2 timer, or NULL when the type does not match.
 */
struct wsm_gles2_render_timer *wsm_gles2_render_timer_from_timer(
	struct wlr_render_timer *timer);

#endif // RENDERER_GLES_RENDER_TIMER_H
