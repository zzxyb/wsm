/**
 * @file        copy.h
 * @brief       PIXMAN texture copy backend.
 * @details     Declares backend creation through the common copy pass interface.
 * @author      YaoBing Xiao
 * @date        2026-11-04
 * @version     v1.0
 * @par Copyright(c):
 * @par History:
 *      version: v1.0, YaoBing Xiao, 2026-11-04, initial version\n
 */

#ifndef PASS_PIXMAN_COPY_H
#define PASS_PIXMAN_COPY_H

#include "pass/copy.h"

/**
 * @brief Pixman texture copy pass state.
 */
struct wsm_render_pass_copy_pixman {
	struct wsm_render_pass_copy base; /**< Common copy pass state and renderer ownership. */
};

/**
 * @brief Creates a PIXMAN texture copy pass.
 * @details Allocates backend state and initializes common renderer ownership.
 * @param renderer wsm PIXMAN renderer owning the pass.
 * @return New copy pass, or NULL for an incompatible renderer or creation failure.
 */
struct wsm_render_pass_copy *wsm_render_pass_copy_pixman_create(struct wlr_renderer *renderer);

/**
 * @brief Checks whether a copy pass uses the Pixman backend.
 * @param pass Copy pass to inspect, or NULL.
 * @return true for a Pixman copy pass, false otherwise.
 */
bool wsm_render_pass_copy_is_pixman(const struct wsm_render_pass_copy *pass);

/**
 * @brief Converts a common copy pass to its Pixman backend state.
 * @param pass Non-NULL copy pass known to use the Pixman backend.
 * @return Enclosing Pixman copy pass.
 * @pre wsm_render_pass_copy_is_pixman(pass) must be true.
 */
struct wsm_render_pass_copy_pixman *wsm_render_pass_copy_pixman_from_pass(
	struct wsm_render_pass_copy *pass);

#endif // PASS_PIXMAN_COPY_H
