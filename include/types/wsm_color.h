/**
 * @file        wsm_color.h
 * @brief       Color transform types and helpers for the wsm renderer.
 * @details     This file defines the color transform hierarchy and utilities
 *              for composing, evaluating, and querying color transforms.
 * @author      YaoBing Xiao
 * @date        2026-08-23
 * @version     v1.0
 * @par Copyright(c):
 * @par History:
 *      version: v1.0, YaoBing Xiao, 2026-08-23, initial version\n
 */

#ifndef RENDER_COLOR_H
#define RENDER_COLOR_H

#include <stdint.h>

#include <wlr/render/color.h>
#include <wlr/util/addon.h>
#include <wlr/render/color.h>

/**
 * @brief Initializes a color transform.
 * @param tr Transform to initialize.
 * @param type Transform implementation type.
 */
void wsm_color_transform_init(struct wlr_color_transform *tr,
	enum wlr_color_transform_type type);

/**
 * @brief Gets an LCMS2 transform from a generic transform.
 * @param tr Generic transform whose type must be COLOR_TRANSFORM_LCMS2.
 * @return Enclosing LCMS2 transform.
 */
struct wlr_color_transform_lcms2 *color_transform_lcms2_from_base(
	struct wlr_color_transform *tr);

/**
 * @brief Finishes an LCMS2 transform.
 * @param tr LCMS2 transform to release.
 */
void color_transform_lcms2_finish(struct wlr_color_transform_lcms2 *tr);

/**
 * @brief Evaluates an LCMS2 transform for an RGB triplet.
 * @param tr LCMS2 transform to evaluate.
 * @param out Destination RGB triplet.
 * @param in Source RGB triplet.
 */
void color_transform_lcms2_eval(struct wlr_color_transform_lcms2 *tr,
	float out[static 3], const float in[static 3]);

/**
 * @brief Gets an inverse-EOTF transform from a generic transform.
 * @param tr Generic transform whose type must be COLOR_TRANSFORM_INVERSE_EOTF.
 * @return Enclosing inverse-EOTF transform.
 */
struct wlr_color_transform_inverse_eotf *wsm_color_transform_inverse_eotf_from_base(
	struct wlr_color_transform *tr);

/**
 * @brief Gets a three-lookup-table transform from a generic transform.
 * @param tr Generic transform whose type must be COLOR_TRANSFORM_LUT_3X1D.
 * @return Enclosing lookup-table transform.
 */
struct wlr_color_transform_lut_3x1d *color_transform_lut_3x1d_from_base(
	struct wlr_color_transform *tr);

/**
 * @brief Creates a simplified and normalized color transform pipeline.
 * @param result Destination for the composed transform.
 * @param transforms Input transforms; NULL entries represent identity transforms.
 * @param len Number of input transforms.
 * @return true on success, false on allocation or composition failure.
 */
bool color_transform_compose(struct wlr_color_transform **result,
	struct wlr_color_transform **transforms, size_t len);

/**
 * @brief Computes the matrix from RGB values to CIE 1931 XYZ.
 * @param primaries Color primaries used by the source RGB space.
 * @param matrix Destination three-by-three matrix.
 */
void wsm_color_primaries_to_xyz(const struct wlr_color_primaries *primaries, float matrix[static 9]);

/**
 * @brief Gets default luminances for a transfer function.
 * @param tf Transfer function to query.
 * @param lum Destination luminance values.
 */
void wsm_color_transfer_function_get_default_luminance(enum wlr_color_transfer_function tf,
	struct wlr_color_luminances *lum);

#endif
