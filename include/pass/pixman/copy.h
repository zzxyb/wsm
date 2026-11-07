/**
 * @file        copy.h
 * @brief       Pixman texture copy backend.
 * @details     Declares texture region copies with geometry and synchronization
 *              options on the active Pixman render pass.
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
 * @brief Copy texture regions on the active pixman render pass.
 * @param pass Active backend render pass.
 * @param options Validated texture regions and synchronization options.
 * @return true on success, false on unsupported operations or backend failure.
 */
bool wsm_pixman_render_pass_copy(struct wlr_render_pass *pass,
	const struct wsm_copy_texture_options *options);

#endif // PASS_PIXMAN_COPY_H
