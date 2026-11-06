/**
 * @file        blit.h
 * @brief       Texture blit render pass interface.
 * @details     Defines texture blit options, backend callbacks, and validated dispatch.
 * @author      YaoBing Xiao
 * @date        2026-11-06
 * @version     v1.0
 * @par Copyright(c):
 * @par History:
 *      version: v1.0, YaoBing Xiao, 2026-11-06, initial version\n
 */

#ifndef PASS_BLIT_H
#define PASS_BLIT_H

#include <wlr/util/box.h>
#include <wlr/render/pass.h>
#include <wlr/render/wlr_texture.h>
#include <wlr/render/wlr_renderer.h>

/**
 * @brief Source, destination, geometry, and synchronization for a texture blit.
 * @details Textures are borrowed and must remain alive until pass submission.
 *          Coordinates and extents are expressed in texture pixels.
 */
struct wsm_blit_texture_options {
	struct wlr_texture *texture; /**< Borrowed source texture from the pass renderer. */
	struct wlr_texture *dst_texture; /**< Borrowed writable destination texture from the same renderer. */
	struct wlr_box src_box; /**< Source rectangle; negative extents reverse an axis. */
	struct wlr_box dst_box; /**< Destination rectangle; negative extents reverse an axis. */
	enum wlr_scale_filter_mode filter_mode; /**< Bilinear (default) or nearest filtering. */
	struct wlr_drm_syncobj_timeline *wait_timeline; /**< Borrowed source acquire timeline, or NULL. */
	uint64_t wait_point; /**< Acquire point on wait_timeline. */
};

struct wsm_render_pass_blit;

/**
 * @brief Backend callbacks for blitting textures and destroying blit passes.
 * @details Implementation tables remain private to each backend.
 */
struct wsm_blit_texture_impl {
	bool (*blit)(struct wlr_render_pass *target,
		const struct wsm_blit_texture_options *options); /**< Blits validated regions; false on failure. */
	void (*destroy)(struct wsm_render_pass_blit *pass); /**< Releases backend resources and frees the pass. */
};

/**
 * @brief Renderer-owned texture blit pass.
 * @details Automatically destroyed with its renderer, or explicitly by the caller.
 */
struct wsm_render_pass_blit {
	const struct wsm_blit_texture_impl *impl; /**< Private backend implementation. */
	struct wlr_renderer *renderer; /**< Borrowed renderer owning the blit pass. */
	struct wl_listener renderer_destroy; /**< Destroys the blit pass with its renderer. */
};

/**
 * @brief Initializes common blit pass state and renderer ownership.
 * @param pass Backend-allocated blit pass storage.
 * @param impl Private backend callbacks; blit and destroy must be provided.
 * @param renderer Renderer owning the pass and its textures.
 */
void wsm_render_pass_blit_init(struct wsm_render_pass_blit *pass,
	const struct wsm_blit_texture_impl *impl, struct wlr_renderer *renderer);

/**
 * @brief Creates a texture blit pass for a wsm GLES2 or Pixman renderer.
 * @param renderer Renderer owning and automatically destroying the blit pass.
 * @return New blit pass, or NULL for an unsupported renderer or allocation failure.
 */
struct wsm_render_pass_blit *wsm_render_pass_blit_autocreate(struct wlr_renderer *renderer);

/**
 * @brief Removes renderer ownership and releases the blit pass.
 * @param pass Blit pass to destroy, or NULL.
 */
void wsm_render_pass_blit_destroy(struct wsm_render_pass_blit *pass);

/**
 * @brief Blits a color texture rectangle, with scaling, filtering, and axis reversal.
 * @details Coordinates refer to texture pixels, with the same origin as texture data.
 *          Each rectangle maps (x,y) to (x+width,y+height); both endpoints must be
 *          within the texture and extents must be nonzero. Negative extents flip
 *          the corresponding axis. No blending or texture sampling state is applied.
 *          Source and destination storage must not alias each other or the pass target.
 *          Only mipmap level zero and single-sample color textures are supported.
 *          GLES requires framebuffer-renderable GL_TEXTURE_2D textures and ES 3
 *          or NV_framebuffer_blit. ANGLE_framebuffer_blit only supports equal-size,
 *          nearest blits without relative axis reversal and with matching formats.
 *          Other format conversions depend on backend framebuffer support.
 *          Pixman requires writable destination storage and rejects explicit timelines.
 *          Borrowed textures must remain alive until pass submission. The caller
 *          must arrange destination synchronization before reusing its storage.
 * @param target Active render pass belonging to the blit pass renderer.
 * @param pass Renderer-owned blit pass providing the backend implementation.
 * @param options Borrowed textures, geometry, and optional acquire synchronization.
 * @return true on success, false for invalid options, an unsupported backend, or blit failure.
 */
bool wsm_render_pass_blit(struct wlr_render_pass *target, struct wsm_render_pass_blit *pass,
	const struct wsm_blit_texture_options *options);

#endif // PASS_BLIT_H
