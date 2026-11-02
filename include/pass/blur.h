/**
 * @file        blur.h
 * @brief       Blur render pass interface.
 * @details     Defines Dual Kawase filter options, renderer-owned effect
 *              resources, and helpers for capturing and filtering render targets.
 * @author      YaoBing Xiao
 * @date        2026-11-02
 * @version     v1.0
 */

#ifndef EFFECTS_WSM_BLUR_BUFFER_PASS_H
#define EFFECTS_WSM_BLUR_BUFFER_PASS_H

#include <wayland-server-core.h>

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

struct wsm_render_blur_buffer_pass;

/**
 * @brief Backend callbacks for blur capture, rendering, and destruction.
 */
struct wsm_render_blur_buffer_pass_impl {
	struct wlr_texture *(*capture)(struct wlr_render_pass *target, struct wlr_texture *texture,
		const struct wlr_box *box, const pixman_region32_t *damage); /**< Captures pixels in target coordinates. */
	void (*destroy)(struct wsm_render_blur_buffer_pass *pass); /**< Releases resources and frees the pass. */
	void (*render)(struct wlr_render_pass *target, struct wsm_render_blur_buffer_pass *pass,
		const struct wsm_render_blur_buffer_options *options); /**< Draws with validated blur options. */
};

/**
 * @brief Blur resources owned by a renderer and destroyed with it.
 */
struct wsm_render_blur_buffer_pass {
	const struct wsm_render_blur_buffer_pass_impl *impl; /**< Backend implementation. */
	struct wlr_renderer *renderer; /**< Renderer owning these effect resources. */
	struct wl_listener renderer_destroy; /**< Renderer destruction listener. */
	struct {
		struct wl_signal destroy; /**< Emitted with NULL before backend resources are destroyed. */
	} events; /**< Pass lifecycle signals. */
};

/**
 * @brief Captures the entire current render target.
 * @param target Active render pass belonging to the blur pass renderer.
 * @param pass Blur pass providing the capture backend.
 * @return Owned snapshot texture, or NULL on failure.
 */
struct wlr_texture *wsm_render_blur_buffer_pass_capture(
	struct wlr_render_pass *target, struct wsm_render_blur_buffer_pass *pass);

/**
 * @brief Creates or updates a cached render-target snapshot.
 * @details Existing textures must belong to the pass renderer and match the extent.
 *          The caller owns the resulting texture and must destroy it when no longer used.
 * @param target Active render pass belonging to the blur pass renderer.
 * @param pass Blur pass providing the capture backend.
 * @param texture Input/output owned snapshot; NULL contents allocate a new texture.
 * @param box Snapshot extent in target-buffer coordinates, or NULL for the whole target.
 * @param damage Region to copy in target-buffer coordinates, or NULL for the full extent.
 * @return true on success, false for incompatible inputs or capture failure.
 */
bool wsm_render_blur_buffer_pass_update_capture(struct wlr_render_pass *target,
	struct wsm_render_blur_buffer_pass *pass, struct wlr_texture **texture,
	const struct wlr_box *box, const pixman_region32_t *damage);

/**
 * @brief Initializes the common state of a backend blur pass.
 * @details Renderer ownership and the destruction listener are attached by
 *          wsm_render_blur_buffer_pass_create after backend creation.
 * @param pass Pass storage to initialize.
 * @param impl Backend implementation with a render callback.
 */
void wsm_render_blur_buffer_pass_init(
	struct wsm_render_blur_buffer_pass *pass, const struct wsm_render_blur_buffer_pass_impl *impl);

/**
 * @brief Emits the destruction signal and releases blur resources.
 * @param pass Blur pass to destroy, or NULL.
 */
void wsm_render_blur_buffer_pass_destroy(struct wsm_render_blur_buffer_pass *pass);

/**
 * @brief Creates blur resources for a wsm GLES2 or Pixman renderer.
 * @param renderer Renderer that owns and automatically destroys the pass.
 * @return New blur pass, or NULL for an unsupported renderer or creation failure.
 */
struct wsm_render_blur_buffer_pass *wsm_render_blur_buffer_pass_create(
	struct wlr_renderer *renderer);

/**
 * @brief Filters a texture and composites it onto the render target.
 * @details Invalid or incompatible inputs are ignored. Corner radii are clamped
 *          to the destination size before dispatching to the backend.
 * @param target Active render pass belonging to the blur pass renderer.
 * @param pass Blur resources to use.
 * @param options Borrowed source, geometry, synchronization, and appearance options.
 */
void wsm_render_blur_buffer_pass_render(struct wlr_render_pass *target,
	struct wsm_render_blur_buffer_pass *pass, const struct wsm_render_blur_buffer_options *options);

#endif // EFFECTS_WSM_BLUR_BUFFER_PASS_H
