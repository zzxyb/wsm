/**
 * @file        copy.h
 * @brief       Texture copy render pass interface.
 * @details     Defines texture copy options, backend callbacks, and validated dispatch.
 * @author      YaoBing Xiao
 * @date        2026-11-04
 * @version     v1.0
 * @par Copyright(c):
 * @par History:
 *      version: v1.0, YaoBing Xiao, 2026-11-04, initial version\n
 */

#ifndef PASS_COPY_H
#define PASS_COPY_H

#include <wlr/render/pass.h>
#include <wlr/render/wlr_texture.h>
#include <wlr/render/wlr_renderer.h>

/**
 * @brief Source, destination, geometry, and synchronization for a texture copy.
 * @details Textures are borrowed and must remain alive until pass submission.
 *          Coordinates and extents are expressed in texture pixels.
 */
struct wsm_copy_texture_options {
	struct wlr_texture *texture; /**< Borrowed source texture from the pass renderer. */
	struct wlr_texture *dst_texture; /**< Borrowed writable destination texture from the same renderer. */
	int src_x, src_y; /**< Nonnegative source origin in texture pixels. */
	int dst_x, dst_y; /**< Nonnegative destination origin in texture pixels. */
	int width, height; /**< Positive copy extent, within both textures. */
	uint32_t src_level; /**< Source mipmap level; currently must be zero. */
	uint32_t dst_level; /**< Destination mipmap level; currently must be zero. */
	struct wlr_drm_syncobj_timeline *wait_timeline; /**< Borrowed source acquire timeline, or NULL. */
	uint64_t wait_point; /**< Acquire point on wait_timeline. */
};

struct wsm_render_pass_copy;

/**
 * @brief Backend callbacks for copying textures and destroying copy passes.
 * @details Implementation tables remain private to each backend.
 */
struct wsm_copy_texture_impl {
	bool (*copy)(struct wlr_render_pass *target,
		const struct wsm_copy_texture_options *options); /**< Copies validated regions; false on failure. */
	void (*destroy)(struct wsm_render_pass_copy *pass); /**< Releases backend resources and frees the pass. */
};

/**
 * @brief Renderer-owned texture copy pass.
 * @details Automatically destroyed with its renderer, or explicitly by the caller.
 */
struct wsm_render_pass_copy {
	const struct wsm_copy_texture_impl *impl; /**< Private backend implementation. */
	struct wlr_renderer *renderer; /**< Borrowed renderer owning the copy pass. */
	struct wl_listener renderer_destroy; /**< Destroys the copy pass with its renderer. */
};

/**
 * @brief Initializes common copy pass state and renderer ownership.
 * @param pass Backend-allocated copy pass storage.
 * @param impl Private backend callbacks; copy and destroy must be provided.
 * @param renderer Renderer owning the pass and its textures.
 */
void wsm_render_pass_copy_init(struct wsm_render_pass_copy *pass,
	const struct wsm_copy_texture_impl *impl, struct wlr_renderer *renderer);

/**
 * @brief Creates a texture copy pass for a wsm GLES2 or Pixman renderer.
 * @param renderer Renderer owning and automatically destroying the copy pass.
 * @return New copy pass, or NULL for an unsupported renderer or allocation failure.
 */
struct wsm_render_pass_copy *wsm_render_pass_copy_autocreate(struct wlr_renderer *renderer);

/**
 * @brief Removes renderer ownership and releases the copy pass.
 * @param pass Copy pass to destroy, or NULL.
 */
void wsm_render_pass_copy_destroy(struct wsm_render_pass_copy *pass);

/**
 * @brief Copies a texture region through the active pass backend.
 * @details Copies without scaling, blending, filtering, or applying sampling state.
 *          Both textures must have the same format and belong to the pass renderer.
 *          Source and destination storage must not alias each other or the pass target.
 *          GLES2 requires GL_TEXTURE_2D textures and EXT_copy_image or OES_copy_image.
 *          Pixman requires a writable destination mapping or owned image and does
 *          not support explicit timelines. Only mipmap level zero is supported.
 * @param target Active render pass belonging to the copy pass renderer.
 * @param pass Renderer-owned copy pass providing the backend implementation.
 * @param options Borrowed textures, geometry, and optional acquire synchronization.
 * @return true on success, false for invalid options, an unsupported backend, or copy failure.
 */
bool wsm_render_pass_copy(struct wlr_render_pass *target, struct wsm_render_pass_copy *pass,
	const struct wsm_copy_texture_options *options);

#endif // PASS_COPY_H
