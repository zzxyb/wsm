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

/**
 * @brief Copies a texture region through the active pass backend.
 * @details Copies without scaling, blending, filtering, or applying sampling state.
 *          Both textures must have the same format and belong to the pass renderer.
 *          Source and destination storage must not alias each other or the pass target.
 *          GLES2 requires GL_TEXTURE_2D textures and EXT_copy_image or OES_copy_image.
 *          Pixman requires a writable destination mapping or owned image and does
 *          not support explicit timelines. Only mipmap level zero is supported.
 * @param target Active render pass.
 * @param options Borrowed textures, geometry, and optional acquire synchronization.
 * @return true on success, false for invalid options, an unsupported backend, or copy failure.
 */
bool wsm_render_pass_copy(struct wlr_render_pass *target,
	const struct wsm_copy_texture_options *options);

#endif // PASS_COPY_H
