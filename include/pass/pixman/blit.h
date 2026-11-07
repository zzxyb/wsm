/**
 * @file        blit.h
 * @brief       Pixman texture blit backend.
 * @details     Declares texture region blits with geometry and synchronization
 *              options on the active Pixman render pass.
 * @author      YaoBing Xiao
 * @date        2026-11-06
 * @version     v1.0
 * @par Copyright(c):
 * @par History:
 *      version: v1.0, YaoBing Xiao, 2026-11-06, initial version\n
 */

#ifndef PASS_PIXMAN_BLIT_H
#define PASS_PIXMAN_BLIT_H

#include "pass/blit.h"

/**
 * @brief Blit texture regions on the active pixman render pass.
 * @param pass Active backend render pass.
 * @param options Validated texture regions and synchronization options.
 * @return true on success, false on unsupported operations or backend failure.
 */
bool wsm_pixman_render_pass_blit(struct wlr_render_pass *pass,
	const struct wsm_blit_texture_options *options);

#endif // PASS_PIXMAN_BLIT_H
