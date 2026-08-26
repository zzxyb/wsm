/**
 * @file        wsm_matrix.h
 * @brief       Two-dimensional matrix utility functions.
 * @details     This file provides helpers for identity, translation, scale,
 *              transform, projection, multiplication, and inversion matrices.
 * @author      YaoBing Xiao
 * @date        2026-08-23
 * @version     v1.0
 * @par Copyright(c):
 * @par History:
 *      version: v1.0, YaoBing Xiao, 2026-08-23, initial version\n
 */

#ifndef UTIL_WSM_MATRIX_H
#define UTIL_WSM_MATRIX_H

#include <wayland-server-protocol.h>

struct wlr_box;

/**
 * @brief Writes the identity matrix into an output matrix.
 * @param mat Destination three-by-three matrix.
 */
void wlr_matrix_identity(float mat[static 9]);

/**
 * @brief Multiplies two three-by-three matrices.
 * @param mat Destination matrix.
 * @param a Left-hand matrix.
 * @param b Right-hand matrix.
 */
void wlr_matrix_multiply(float mat[static 9], const float a[static 9],
	const float b[static 9]);

/**
 * @brief Writes a two-dimensional translation matrix.
 * @param mat Destination matrix.
 * @param x Translation along the x axis.
 * @param y Translation along the y axis.
 */
void wlr_matrix_translate(float mat[static 9], float x, float y);

/**
 * @brief Writes a two-dimensional scale matrix.
 * @param mat Destination matrix.
 * @param x Scale along the x axis.
 * @param y Scale along the y axis.
 */
void wlr_matrix_scale(float mat[static 9], float x, float y);

/**
 * @brief Writes a matrix for a Wayland output transform.
 * @param mat Destination matrix.
 * @param transform Output transform to apply.
 */
void wlr_matrix_transform(float mat[static 9],
	enum wl_output_transform transform);

/**
 * @brief Builds a matrix that projects a box into normalized coordinates.
 * @param mat Destination matrix.
 * @param box Box to project.
 * @param transform Output transform to apply.
 * @param projection Orthographic projection matrix.
 */
void wlr_matrix_project_box(float mat[static 9], const struct wlr_box *box,
	enum wl_output_transform transform, const float projection[static 9]);

/**
 * @brief Writes a two-dimensional orthographic projection matrix.
 * @param mat Destination matrix.
 * @param width Projection width.
 * @param height Projection height.
 * @param transform Output transform to apply.
 */
void matrix_projection(float mat[static 9], int width, int height,
	enum wl_output_transform transform);

/**
 * @brief Computes the inverse of a three-by-three matrix.
 * @param out Destination inverse matrix.
 * @param m Matrix to invert; it must be invertible.
 */
void matrix_invert(float out[static 9], float m[static 9]);

#endif // UTIL_WSM_MATRIX_H
