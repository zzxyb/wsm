/**
 * @file        wsm_scene_blur.h
 * @brief       Blur scene node with backdrop or buffer input.
 * @details     Defines a rounded blur node, per-output background captures,
 *              and helpers for updating filter geometry and appearance.
 * @author      YaoBing Xiao
 * @date        2026-11-02
 * @version     v1.0
 */

#ifndef WSM_SCENE_BLUR_H
#define WSM_SCENE_BLUR_H

#include "pass/blur.h"

#include <pixman.h>
#include <stdbool.h>
#include "node/wsm_scene_node.h"
#include <wlr/types/wlr_buffer.h>

/**
 * @brief Input sampled by a blur node.
 */
enum wsm_scene_blur_source {
	WSM_SCENE_BLUR_SOURCE_BACKDROP, /**< Capture preceding scene content. */
	WSM_SCENE_BLUR_SOURCE_BUFFER, /**< Sample the supplied buffer, if any. */
};

/**
 * @brief Rounded filter over preceding scene content or a supplied buffer.
 * @details Background captures are cached per output and updated from capture damage.
 *          Occluded source content is retained for filtering. The borrowed pass
 *          is detached automatically when it is destroyed.
 */
struct wsm_scene_blur {
	struct wsm_scene_node node; /**< Base scene node. */
	int width, height; /**< Destination size in logical pixels. */
	float opacity; /**< Opacity multiplier. */
	float top_left_radius; /**< Top-left corner radius in logical pixels. */
	float top_right_radius; /**< Top-right corner radius in logical pixels. */
	float bottom_left_radius; /**< Bottom-left corner radius in logical pixels. */
	float bottom_right_radius; /**< Bottom-right corner radius in logical pixels. */
	struct wlr_buffer *buffer; /**< Locked source buffer, or NULL. */
	enum wsm_scene_blur_source source; /**< Input mode, changed through the source setters. */
	struct wsm_render_blur_buffer_pass *pass; /**< Borrowed blur pass, or NULL when detached. */
	struct wl_listener pass_destroy; /**< Blur pass destruction listener. */
	struct wl_list captures; /**< List of per-output blur_capture caches. */
	int iterations; /**< Requested Dual Kawase down/up sampling iterations. */
	float offset; /**< Blur sampling offset. */
	float saturation; /**< Saturation multiplier; 1 preserves saturation. */
	float brightness; /**< Brightness multiplier; 1 preserves brightness. */
	float noise_amount; /**< Noise amplitude; 0 disables noise. */
	float material_contrast; /**< Highlight and shadow adjustment strength. */
	float tint[4]; /**< Straight RGBA tint mixed with the blurred content. */
};

/**
 * @brief Adds a backdrop blur node with square corners to a scene tree.
 * @details Set rounded clipping through wsm_scene_blur_set_corner_radius.
 * @param parent Parent scene tree.
 * @param width Nonnegative filter width in logical pixels.
 * @param height Nonnegative filter height in logical pixels.
 * @return New blur node, or NULL on allocation failure.
 */
struct wsm_scene_blur *wsm_scene_blur_create(
	struct wsm_scene_tree *parent, int width, int height);

/**
 * @brief Changes the filter destination size.
 * @details Buffer input uses its native dimensions unless both dimensions are positive.
 *          Backdrop input uses the supplied dimensions, including zero.
 * @param blur Blur node to update.
 * @param width Nonnegative destination width in logical pixels.
 * @param height Nonnegative destination height in logical pixels.
 */
void wsm_scene_blur_set_size(struct wsm_scene_blur *blur, int width, int height);

/**
 * @brief Sets destination corner radii in logical pixels.
 * @param blur Blur node to update.
 * @param top_left Nonnegative top-left radius.
 * @param top_right Nonnegative top-right radius.
 * @param bottom_left Nonnegative bottom-left radius.
 * @param bottom_right Nonnegative bottom-right radius.
 */
void wsm_scene_blur_set_corner_radius(struct wsm_scene_blur *blur,
	float top_left, float top_right, float bottom_left, float bottom_right);

/**
 * @brief Replaces the borrowed blur pass and damages the node.
 * @param blur Blur node to update.
 * @param pass New pass to borrow, or NULL to detach the filter.
 */
void wsm_scene_blur_set_pass(struct wsm_scene_blur *blur, struct wsm_render_blur_buffer_pass *pass);

/**
 * @brief Changes the blur sampling parameters.
 * @details The offset must be finite and nonnegative. Iterations are capped
 *          at WSM_BLUR_BUFFER_MAX_PASSES. Backdrop offsets are scaled per output.
 * @param blur Blur node to update.
 * @param iterations Requested iteration count; nonpositive values select 4.
 * @param offset Sampling offset; 0 selects 1.8.
 */
void wsm_scene_blur_set_blur(struct wsm_scene_blur *blur, int iterations, float offset);

/**
 * @brief Changes saturation, brightness, and tint.
 * @param blur Blur node to update.
 * @param saturation Saturation multiplier; 0 selects the render default of 1.35.
 * @param brightness Brightness multiplier; 0 selects the render default of 1.06.
 * @param tint Straight RGBA tint copied into the node.
 */
void wsm_scene_blur_set_appearance(
	struct wsm_scene_blur *blur, float saturation, float brightness, const float tint[4]);

/**
 * @brief Changes the noise amplitude.
 * @param blur Blur node to update.
 * @param noise_amount New amplitude, clamped to at least 0; 0 disables noise.
 */
void wsm_scene_blur_set_noise(struct wsm_scene_blur *blur, float noise_amount);

/**
 * @brief Changes the highlight and shadow adjustment strength.
 * @param blur Blur node to update.
 * @param contrast New strength, clamped to at least 0; 0 disables the adjustment.
 */
void wsm_scene_blur_set_material_contrast(struct wsm_scene_blur *blur, float contrast);

/**
 * @brief Marks cached background pixels for recapture.
 * @param blur Backdrop blur node to update.
 * @param damage Source damage in output-buffer coordinates, clipped to each capture.
 */
void wsm_scene_blur_add_capture_damage(
	struct wsm_scene_blur *blur, const pixman_region32_t *damage);

/**
 * @brief Computes the blur sampling margin for damage expansion.
 * @param blur Backdrop blur node to inspect.
 * @param scale Output scale factor.
 * @return Conservative margin in output-buffer pixels, or 0 for nonpositive parameters.
 */
int wsm_scene_blur_get_damage_expansion(const struct wsm_scene_blur *blur, float scale);

/**
 * @brief Prepares capture state and expands pending output damage.
 * @details Call before scene output commit or state construction. Prepare multiple
 *          blur nodes in scene order, from back to front.
 * @param blur Backdrop blur node, or NULL for no work.
 * @param output Scene output to prepare.
 * @return true on success or when no work is needed, false on capture allocation failure.
 */
bool wsm_scene_blur_prepare_output(struct wsm_scene_blur *blur, struct wsm_scene_output *output);

/**
 * @brief Propagates source damage through a backdrop filter.
 * @details Prepares the capture cache and expands damage for sampling and filtering.
 *          Call for blur entries in scene order, from back to front.
 * @param entry Render list entry for a blur node; buffer sources require no preparation.
 * @param data Current output rendering state.
 * @param damage Input/output damage region in output-buffer coordinates.
 * @return true on success or when no capture is needed, false on capture allocation failure.
 */
bool wsm_scene_blur_prepare(struct wsm_render_list_entry *entry, const struct wsm_render_data *data,
	pixman_region32_t *damage);

/**
 * @brief Checks whether a scene node is a blur.
 * @param node Scene node to inspect, or NULL.
 * @return true when the node is a blur in either input mode, false otherwise.
 */
bool wsm_scene_node_is_blur(const struct wsm_scene_node *node);

/**
 * @brief Casts a blur scene node to its containing object.
 * @param node Node known to represent a blur.
 * @return Enclosing blur node.
 */
struct wsm_scene_blur *wsm_scene_blur_from_node(struct wsm_scene_node *node);

/**
 * @brief Selects a supplied buffer as the blur source and damages the whole node.
 * @details Unless both destination dimensions are positive, native buffer
 *          dimensions are used. Background captures are released. Blur nodes
 *          do not accept input or emit buffer frame and sampling events.
 * @param blur Blur node to update.
 * @param buffer Source buffer to lock, or NULL for an empty buffer source.
 */
void wsm_scene_blur_set_buffer(struct wsm_scene_blur *blur, struct wlr_buffer *buffer);

/**
 * @brief Selects preceding scene content as the blur source.
 * @details Releases the supplied buffer and resets background capture caches.
 *          Destination dimensions and appearance are preserved. Backdrop nodes
 *          do not accept input or emit buffer frame and sampling events.
 * @param blur Blur node to update.
 */
void wsm_scene_blur_set_backdrop(struct wsm_scene_blur *blur);

#endif // WSM_SCENE_BLUR_H
