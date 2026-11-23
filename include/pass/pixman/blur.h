/**
 * @file        blur.h
 * @brief       Pixman blur render pass backend.
 * @details     Declares software Dual Kawase rendering and capture operations.
 * @author      YaoBing Xiao
 * @date        2026-11-02
 * @version     v1.0
 * @par Copyright(c):
 * @par History:
 *      version: v1.0, YaoBing Xiao, 2026-11-02, initial version\n
 */

#ifndef WSM_PIXMAN_BLUR_H
#define WSM_PIXMAN_BLUR_H

#include "pass/blur.h"

/**
 * @brief Applies Dual Kawase blur and composites the result onto the render target.
 * @param pass Active Pixman render pass receiving the blurred texture.
 * @param options Borrowed blur options with a source texture from the pass renderer.
 */
void wsm_pixman_render_pass_add_blur(struct wlr_render_pass *pass,
	const struct wsm_render_blur_buffer_options *options);

/**
 * @brief Copies pixels from the active render target into a snapshot texture.
 * @details Copies only the intersection of the snapshot extent, target bounds,
 *          and damage region. Copied pixels are stored relative to the snapshot
 *          origin. The caller owns the returned texture.
 * @param pass Active Pixman render pass providing the source pixels.
 * @param previous Existing snapshot from the same renderer with matching dimensions,
 *                 or NULL to allocate a new texture.
 * @param box Snapshot extent in render-target pixels, or NULL for the whole target.
 * @param damage Region to copy in render-target coordinates, or NULL for the full extent.
 * @return The updated or newly allocated snapshot, or NULL on failure.
 */
struct wlr_texture *wsm_pixman_render_pass_capture_blur(struct wlr_render_pass *pass,
	struct wlr_texture *previous, const struct wlr_box *box, const pixman_region32_t *damage);

#endif // WSM_PIXMAN_BLUR_H
