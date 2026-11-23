/* SPDX-License-Identifier: MIT */

/**
 * @file        pass.h
 * @brief       Shared render pass interface and implementation callbacks.
 * @details     Defines the base embedded by GLES2, Pixman, and Vulkan passes,
 *              extended rendering operations, and wlroots conversion helpers.
 * @author      YaoBing Xiao
 * @date        2026-11-23
 * @version     v1.0
 * @par Copyright(c):
 *              See LICENSE for wsm copyright and license.
 * @par History:
 *      version: v1.0, YaoBing Xiao, 2026-11-23, initial version\n
 */

#ifndef PASS_PASS_H
#define PASS_PASS_H

#include <wlr/render/interface.h>
#include "pass/blit.h"
#include "pass/blur.h"
#include "pass/border.h"
#include "pass/copy.h"
#include "pass/rect.h"
#include "pass/shadow.h"
#include "pass/texture.h"

/**
 * @brief Virtual operations implemented by a render pass backend.
 * @details The embedded wlroots implementation is used directly without an
 *          adapter table. Callbacks borrow their arguments; submit consumes
 *          and frees the pass. Extension callbacks may be NULL when unsupported.
 */
struct wsm_render_pass_impl {
	struct wlr_render_pass_impl base; /**< Base submission, texture, and rectangle callbacks. */
	bool (*blit)(struct wlr_render_pass *pass,
		const struct wsm_blit_texture_options *options); /**< Blits a texture region with scaling or axis reversal. */
	bool (*copy)(struct wlr_render_pass *pass,
		const struct wsm_copy_texture_options *options); /**< Copies a texture region without scaling. */
	void (*add_blur)(struct wlr_render_pass *pass,
		const struct wsm_render_blur_buffer_options *options); /**< Filters and composites a texture with blur. */
	struct wlr_texture *(*capture)(struct wlr_render_pass *pass,
		struct wlr_texture *previous, const struct wlr_box *box,
		const pixman_region32_t *damage); /**< Creates or updates a snapshot of the render target. */
	void (*add_border)(struct wlr_render_pass *pass,
		const struct wsm_render_border_options *options); /**< Draws an inset rounded border. */
	void (*add_rect)(struct wlr_render_pass *pass,
		const struct wsm_render_rect_options *options); /**< Draws a rounded rectangle. */
	void (*add_texture)(struct wlr_render_pass *pass,
		const struct wlr_render_texture_options *options, const float *radii); /**< Draws a texture with rounded corners. */
	void (*add_shadow)(struct wsm_scene_shadow *shadow,
		const struct wsm_render_data *data, const pixman_region32_t *clip, int x, int y); /**< Draws a cached shadow using scene render state. */
};

/**
 * @brief Base object embedded by each wsm render pass backend.
 */
struct wsm_render_pass {
	struct wlr_render_pass base; /**< Embedded wlroots pass using the backend implementation. */
};

/**
 * @brief Initializes the base of a backend render pass.
 * @param pass Render pass to initialize.
 * @param impl Backend callbacks, which must remain valid for the pass lifetime.
 * @pre The base submit, add_texture, and add_rect callbacks must be provided.
 */
void wsm_render_pass_init(struct wsm_render_pass *pass,
	const struct wsm_render_pass_impl *impl);

/**
 * @brief Gets the extended implementation of a wlroots render pass.
 * @param pass Render pass initialized by wsm_render_pass_init.
 * @return Borrowed pointer to the backend implementation callbacks.
 * @pre The pass must be non-NULL and use a wsm render pass implementation.
 */
const struct wsm_render_pass_impl *wsm_render_pass_get_impl(const struct wlr_render_pass *pass);

/**
 * @brief Gets the wsm base containing a wlroots render pass.
 * @param pass Embedded wlroots render pass.
 * @return Borrowed pointer to the enclosing wsm render pass.
 * @pre The pass must be non-NULL and embedded in a wsm_render_pass.
 */
struct wsm_render_pass *wsm_render_pass_from_base(struct wlr_render_pass *pass);

#endif
