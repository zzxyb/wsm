/**
 * @file        copy.h
 * @brief       GLES2 texture copy backend.
 * @details     Declares backend creation through the common copy pass interface.
 * @author      YaoBing Xiao
 * @date        2026-11-04
 * @version     v1.0
 * @par Copyright(c):
 * @par History:
 *      version: v1.0, YaoBing Xiao, 2026-11-04, initial version\n
 */

#ifndef PASS_GLES_COPY_H
#define PASS_GLES_COPY_H

#include "pass/copy.h"

/**
 * @brief GLES2 texture copy pass state.
 */
struct wsm_render_pass_copy_gles {
	struct wsm_render_pass_copy base; /**< Common copy pass state and renderer ownership. */
};

/**
 * @brief Creates a GLES texture copy pass.
 * @details Allocates backend state and initializes common renderer ownership.
 * @param renderer wsm GLES renderer owning the pass.
 * @return New copy pass, or NULL for an incompatible renderer or creation failure.
 */
struct wsm_render_pass_copy *wsm_render_pass_copy_gles_create(struct wlr_renderer *renderer);

/**
 * @brief Checks whether a copy pass uses the GLES2 backend.
 * @param pass Copy pass to inspect, or NULL.
 * @return true for a GLES2 copy pass, false otherwise.
 */
bool wsm_render_pass_copy_is_gles(const struct wsm_render_pass_copy *pass);

/**
 * @brief Converts a common copy pass to its GLES2 backend state.
 * @param pass Non-NULL copy pass known to use the GLES2 backend.
 * @return Enclosing GLES2 copy pass.
 * @pre wsm_render_pass_copy_is_gles(pass) must be true.
 */
struct wsm_render_pass_copy_gles *wsm_render_pass_copy_gles_from_pass(
	struct wsm_render_pass_copy *pass);

#endif // PASS_GLES_COPY_H
