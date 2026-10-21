/**
 * @file        wsm_scene_round_rect.h
 * @brief       Rounded rectangle scene node.
 * @details     This file defines a solid-color scene node with four independent
 *              corner radii and helpers for updating its size, color, and radii.
 * @author      YaoBing Xiao
 * @date        2026-10-21
 * @version     v1.0
 */

#ifndef NODE_WSM_SCENE_ROUND_RECT_H
#define NODE_WSM_SCENE_ROUND_RECT_H

#include "node/wsm_scene_node.h"

/**
 * @brief Solid rectangle scene node with independent corner radii.
 */
struct wsm_scene_round_rect {
	struct wsm_scene_node node; /**< Base scene node. */
	int width, height; /**< Rectangle dimensions in logical pixels. */
	float color[4]; /**< Premultiplied RGBA color. */
	float top_left_radius, top_right_radius; /**< Top corner radii in logical pixels. */
	float bottom_left_radius, bottom_right_radius; /**< Bottom corner radii in logical pixels. */
};

/**
 * @brief Adds a rounded rectangle to a scene tree.
 * @param parent Parent scene tree.
 * @param width Rectangle width in logical pixels.
 * @param height Rectangle height in logical pixels.
 * @param top_left_radius Top-left corner radius.
 * @param top_right_radius Top-right corner radius.
 * @param bottom_left_radius Bottom-left corner radius.
 * @param bottom_right_radius Bottom-right corner radius.
 * @param color Premultiplied RGBA color.
 * @return Newly created node, or NULL on allocation failure.
 */
struct wsm_scene_round_rect *wsm_scene_round_rect_create(struct wsm_scene_tree *parent,
	int width, int height, float top_left_radius, float top_right_radius,
	float bottom_left_radius, float bottom_right_radius, const float color[static 4]);

/**
 * @brief Changes the rectangle size and clamps its radii to the new size.
 * @param rect Rounded rectangle node to update.
 * @param width New width in logical pixels.
 * @param height New height in logical pixels.
 */
void wsm_scene_round_rect_set_size(struct wsm_scene_round_rect *rect, int width, int height);

/**
 * @brief Changes the rectangle color.
 * @param rect Rounded rectangle node to update.
 * @param color New premultiplied RGBA color.
 */
void wsm_scene_round_rect_set_color(struct wsm_scene_round_rect *rect, const float color[static 4]);

/**
 * @brief Changes the top-left corner radius.
 * @param rect Rounded rectangle node to update.
 * @param radius New radius in logical pixels, clamped to half the smaller dimension.
 * @return true if the effective radius changed, false otherwise.
 */
bool wsm_scene_round_rect_set_top_left_radius(struct wsm_scene_round_rect *rect, float radius);

/**
 * @brief Changes the top-right corner radius.
 * @param rect Rounded rectangle node to update.
 * @param radius New radius in logical pixels, clamped to half the smaller dimension.
 * @return true if the effective radius changed, false otherwise.
 */
bool wsm_scene_round_rect_set_top_right_radius(struct wsm_scene_round_rect *rect, float radius);

/**
 * @brief Changes the bottom-left corner radius.
 * @param rect Rounded rectangle node to update.
 * @param radius New radius in logical pixels, clamped to half the smaller dimension.
 * @return true if the effective radius changed, false otherwise.
 */
bool wsm_scene_round_rect_set_bottom_left_radius(struct wsm_scene_round_rect *rect, float radius);

/**
 * @brief Changes the bottom-right corner radius.
 * @param rect Rounded rectangle node to update.
 * @param radius New radius in logical pixels, clamped to half the smaller dimension.
 * @return true if the effective radius changed, false otherwise.
 */
bool wsm_scene_round_rect_set_bottom_right_radius(struct wsm_scene_round_rect *rect, float radius);

/**
 * @brief Checks whether a scene node is a rounded rectangle.
 * @param node Scene node to inspect.
 * @return true when the node is a rounded rectangle, false otherwise.
 */
bool wsm_scene_node_is_round_rect(const struct wsm_scene_node *node);

/**
 * @brief Casts a rounded rectangle scene node to its containing object.
 * @param node Node known to represent a rounded rectangle.
 * @return Enclosing rounded rectangle node.
 */
struct wsm_scene_round_rect *wsm_scene_round_rect_from_node(struct wsm_scene_node *node);

#endif // NODE_WSM_SCENE_ROUND_RECT_H
