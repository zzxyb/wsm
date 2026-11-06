/**
 * @file        blit.h
 * @brief       GLES2 texture blit backend.
 * @details     Declares backend creation through the common blit pass interface.
 * @author      YaoBing Xiao
 * @date        2026-11-06
 * @version     v1.0
 * @par Copyright(c):
 * @par History:
 *      version: v1.0, YaoBing Xiao, 2026-11-06, initial version\n
 */

#ifndef PASS_GLES_BLIT_H
#define PASS_GLES_BLIT_H

#include "pass/blit.h"

/**
 * @brief GLES2 texture blit pass state.
 */
struct wsm_render_pass_blit_gles {
	struct wsm_render_pass_blit base; /**< Common blit pass state and renderer ownership. */
};

/**
 * @brief Creates a GLES texture blit pass.
 * @details Allocates backend state and initializes common renderer ownership.
 * @param renderer wsm GLES renderer owning the pass.
 * @return New blit pass, or NULL for an incompatible renderer or creation failure.
 */
struct wsm_render_pass_blit *wsm_render_pass_blit_gles_create(struct wlr_renderer *renderer);

/**
 * @brief Checks whether a blit pass uses the GLES2 backend.
 * @param pass Blit pass to inspect, or NULL.
 * @return true for a GLES2 blit pass, false otherwise.
 */
bool wsm_render_pass_blit_is_gles(const struct wsm_render_pass_blit *pass);

/**
 * @brief Converts a common blit pass to its GLES2 backend state.
 * @param pass Non-NULL blit pass known to use the GLES2 backend.
 * @return Enclosing GLES2 blit pass.
 * @pre wsm_render_pass_blit_is_gles(pass) must be true.
 */
struct wsm_render_pass_blit_gles *wsm_render_pass_blit_gles_from_pass(
	struct wsm_render_pass_blit *pass);

#endif // PASS_GLES_BLIT_H
