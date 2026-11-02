/**
 * @file        wsm_scene_rect.h
 * @brief       Solid rectangle scene node with optional rounded corners.
 * @details     This file defines the rectangle node state and helpers for
 *              creating and updating solid-colored scene rectangles and corner radii.
 * @author      YaoBing Xiao
 * @date        2026-08-23
 * @version     v1.0
 * @par Copyright(c):
 * @par History:
 *      version: v1.0, YaoBing Xiao, 2026-08-23, initial version\n
 */

#ifndef SCENE_WSM_SCENE_RECT_H
#define SCENE_WSM_SCENE_RECT_H

#include "node/wsm_scene_node.h"

/**
 * @brief Solid rectangle with four independent corner radii.
 * @details Zero radii use the plain rectangle pass. Rounded corners use the
 *          round-rect pass, with conservative corner cutouts for opaque culling.
 *          Translucent rectangles do not occlude preceding scene content.
 */
struct wsm_scene_rect {
	struct wsm_scene_node node; /**< Base scene node. */
	int width, height; /**< Rectangle dimensions in logical pixels. */
	float color[4]; /**< Premultiplied RGBA color. */
	float top_left_radius, top_right_radius; /**< Top corner radii in logical pixels. */
	float bottom_left_radius, bottom_right_radius; /**< Bottom corner radii in logical pixels. */
};

/**
 * @brief Adds a solid-colored rectangle with zero corner radii to the scene-graph.
 * @param parent Parent scene tree.
 * @param width Rectangle width in logical pixels.
 * @param height Rectangle height in logical pixels.
 * @param color Premultiplied RGBA color.
 * @return Newly created rectangle node, or NULL on failure.
 */
struct wsm_scene_rect *wsm_scene_rect_create(struct wsm_scene_tree *parent,
	int width, int height, const float color[static 4]);

/**
 * @brief Changes the rectangle size and clamps its radii to the new size.
 * @param rect Rectangle node to update.
 * @param width New width in logical pixels.
 * @param height New height in logical pixels.
 */
void wsm_scene_rect_set_size(struct wsm_scene_rect *rect, int width, int height);

/**
 * @brief Changes the color of a rectangle node.
 * @param rect Rectangle node to update.
 * @param color New premultiplied RGBA color.
 */
void wsm_scene_rect_set_color(struct wsm_scene_rect *rect, const float color[static 4]);

/**
 * @brief Checks whether a scene node is a rectangle node.
 * @param node Scene node to inspect.
 * @return true when the node is a rectangle, false otherwise.
 */
bool wsm_scene_node_is_rect(const struct wsm_scene_node *node);

/**
 * @brief Casts a scene node to a rectangle node.
 * @param node Node known to represent a rectangle.
 * @return Enclosing rectangle node.
 */
struct wsm_scene_rect *wsm_scene_rect_from_node(struct wsm_scene_node *node);

/**
 * @brief Changes the top-left corner radius.
 * @param rect Rectangle node to update.
 * @param radius New radius in logical pixels, clamped to half the smaller dimension.
 * @return true if the effective radius changed, false otherwise.
 */
bool wsm_scene_rect_set_top_left_radius(struct wsm_scene_rect *rect, float radius);

/**
 * @brief Changes the top-right corner radius.
 * @param rect Rectangle node to update.
 * @param radius New radius in logical pixels, clamped to half the smaller dimension.
 * @return true if the effective radius changed, false otherwise.
 */
bool wsm_scene_rect_set_top_right_radius(struct wsm_scene_rect *rect, float radius);

/**
 * @brief Changes the bottom-left corner radius.
 * @param rect Rectangle node to update.
 * @param radius New radius in logical pixels, clamped to half the smaller dimension.
 * @return true if the effective radius changed, false otherwise.
 */
bool wsm_scene_rect_set_bottom_left_radius(struct wsm_scene_rect *rect, float radius);

/**
 * @brief Changes the bottom-right corner radius.
 * @param rect Rectangle node to update.
 * @param radius New radius in logical pixels, clamped to half the smaller dimension.
 * @return true if the effective radius changed, false otherwise.
 */
bool wsm_scene_rect_set_bottom_right_radius(struct wsm_scene_rect *rect, float radius);

#endif // SCENE_WSM_SCENE_RECT_H
