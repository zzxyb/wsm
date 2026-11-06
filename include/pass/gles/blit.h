/**
 * @file        blit.h
 * @brief       GLES2 texture blit backend.
 * @details     Operates on the active backend render pass.
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
 * @brief Blit texture regions on the active gles render pass.
 * @param pass Active backend render pass.
 * @param options Validated texture regions and synchronization options.
 * @return true on success, false on unsupported operations or backend failure.
 */
bool wsm_gles2_render_pass_blit(struct wlr_render_pass *pass,
	const struct wsm_blit_texture_options *options);

#endif // PASS_GLES_BLIT_H
