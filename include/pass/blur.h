/**
 * @file        blur.h
 * @brief       Blur render pass interface.
 * @details     Defines Dual Kawase filter options, renderer-owned effect
 *              resources, and helpers for capturing and filtering render targets.
 * @author      YaoBing Xiao
 * @date        2026-11-02
 * @version     v1.0
 * @par Copyright(c):
 * @par History:
 *      version: v1.0, YaoBing Xiao, 2026-11-02, initial version\n
 */

#ifndef EFFECTS_WSM_BLUR_BUFFER_PASS_H
#define EFFECTS_WSM_BLUR_BUFFER_PASS_H

#include <wlr/render/pass.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/render/wlr_texture.h>

#include <pixman.h>

/** Maximum supported number of down/up sampling iterations. */
#define WSM_BLUR_BUFFER_MAX_PASSES 8

/**
 * @brief Source texture, geometry, and appearance for a blur draw.
 * @details Options and referenced objects are borrowed for the call. Textures and
 *          render output use premultiplied alpha; tint uses straight RGBA.
 *          Iteration counts are capped by WSM_BLUR_BUFFER_MAX_PASSES and image size.
 */
struct wsm_render_blur_buffer_options {
	struct wlr_texture *texture; /**< Borrowed source texture from the pass renderer. */
	int padding; /**< Source margin cropped after filtering; 0 to 4096 target pixels. */
	struct wlr_fbox src_box; /**< Source rectangle in texture pixels; empty selects the whole texture. */
	struct wlr_box dst_box; /**< Destination rectangle in render-target pixels. */
	const float *alpha; /**< Borrowed opacity multiplier, or NULL for 1. */
	const pixman_region32_t *clip; /**< Borrowed clip region in render-target coordinates, or NULL. */
	enum wl_output_transform transform; /**< Transform applied to the source texture. */
	enum wlr_scale_filter_mode filter_mode; /**< Reserved; blur sampling uses bilinear filtering. */
	enum wlr_render_blend_mode blend_mode; /**< Blend mode for final composition. */
	float top_left_radius; /**< Top-left corner radius in target pixels. */
	float top_right_radius; /**< Top-right corner radius in target pixels. */
	float bottom_left_radius; /**< Bottom-left corner radius in target pixels. */
	float bottom_right_radius; /**< Bottom-right corner radius in target pixels. */
	float corner_smoothness; /**< Reserved; texture clipping uses exponent 4. */
	int iterations; /**< Requested iteration count; nonpositive values select 4. */
	float offset; /**< Nonnegative sampling offset; 0 selects 1.8. */
	float saturation; /**< Saturation multiplier; 0 selects 1.35. */
	float brightness; /**< Brightness multiplier; 0 selects 1.06. */
	float noise_amount; /**< Noise amplitude; 0 disables noise. */
	float material_contrast; /**< Highlight and shadow adjustment strength; 0 disables it. */
	float tint[4]; /**< Straight RGBA tint; alpha controls the tint mixture. */
	struct wlr_drm_syncobj_timeline *wait_timeline; /**< Optional source acquire timeline. */
	uint64_t wait_point; /**< Acquire point on wait_timeline. */
};

/**
 * @brief Captures the entire current render target.
 * @param target Active render pass providing the renderer and its blur resources.
 * @return Owned snapshot texture, or NULL on failure.
 */
struct wlr_texture *wsm_render_pass_capture_blur(struct wlr_render_pass *target);

/**
 * @brief Creates or updates a cached render-target snapshot.
 * @details Existing textures must belong to the target renderer and match the extent.
 *          The caller owns the resulting texture and must destroy it when no longer used.
 * @param target Active render pass providing the renderer and its blur resources.
 * @param texture Input/output owned snapshot; NULL contents allocate a new texture.
 * @param box Snapshot extent in target-buffer coordinates, or NULL for the whole target.
 * @param damage Region to copy in target-buffer coordinates, or NULL for the full extent.
 * @return true on success, false for incompatible inputs or capture failure.
 */
bool wsm_render_pass_update_blur_capture(struct wlr_render_pass *target,
	struct wlr_texture **texture, const struct wlr_box *box, const pixman_region32_t *damage);

/**
 * @brief Filters a texture and composites it onto the render target.
 * @details Invalid or incompatible inputs are ignored. Corner radii are clamped
 *          to the destination size before dispatching to the backend.
 * @param target Active render pass providing the renderer and its blur resources.
 * @param options Borrowed source, geometry, synchronization, and appearance options.
 */
void wsm_render_pass_add_blur(struct wlr_render_pass *target,
	const struct wsm_render_blur_buffer_options *options);

#endif // EFFECTS_WSM_BLUR_BUFFER_PASS_H
