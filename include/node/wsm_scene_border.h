/**
 * @file        wsm_scene_border.h
 * @brief       Rounded rectangle border scene node.
 * @details     This file defines a border scene node with four independent
 *              corner radii and helpers for updating its geometry and color.
 * @author      YaoBing Xiao
 * @date        2026-10-21
 * @version     v1.0
 * @par Copyright(c):
 * @par History:
 *      version: v1.0, YaoBing Xiao, 2026-10-21, initial version
 */

#ifndef NODE_WSM_SCENE_BORDER_H
#define NODE_WSM_SCENE_BORDER_H

#include "node/wsm_scene_node.h"

/**
 * @brief Rounded outline scene node with independent corner radii.
 */
struct wsm_scene_border {
	struct wsm_scene_node node; /**< Base scene node. */
	int width, height; /**< Rectangle dimensions in logical pixels. */
	float color[4]; /**< Premultiplied RGBA color. */
	float top_left_radius, top_right_radius; /**< Top corner radii in logical pixels. */
	float bottom_left_radius, bottom_right_radius; /**< Bottom corner radii in logical pixels. */
	float border_width; /**< Inward border width in logical pixels. */
};

/**
 * @brief Adds a rounded border to a scene tree.
 * @param parent Parent scene tree.
 * @param width Rectangle width in logical pixels.
 * @param height Rectangle height in logical pixels.
 * @param top_left_radius Top-left corner radius.
 * @param top_right_radius Top-right corner radius.
 * @param bottom_left_radius Bottom-left corner radius.
 * @param bottom_right_radius Bottom-right corner radius.
 * @param border_width Inward border width in logical pixels.
 * @param color Premultiplied RGBA color.
 * @return Newly created node, or NULL on allocation failure.
 */
struct wsm_scene_border *wsm_scene_border_create(struct wsm_scene_tree *parent,
	int width, int height, float top_left_radius, float top_right_radius,
	float bottom_left_radius, float bottom_right_radius, float border_width,
	const float color[static 4]);

/**
 * @brief Changes the rectangle size and clamps its radii to the new size.
 * @param rect Rounded border node to update.
 * @param width New width in logical pixels.
 * @param height New height in logical pixels.
 */
void wsm_scene_border_set_size(struct wsm_scene_border *rect, int width, int height);

/**
 * @brief Changes the rectangle color.
 * @param rect Rounded border node to update.
 * @param color New premultiplied RGBA color.
 */
void wsm_scene_border_set_color(struct wsm_scene_border *rect, const float color[static 4]);

/**
 * @brief Changes the inward border width.
 * @param rect Rounded border node to update.
 * @param border_width New width in logical pixels, clamped to half the smaller dimension.
 */
void wsm_scene_border_set_border_width(struct wsm_scene_border *rect,
	float border_width);

/**
 * @brief Changes the top-left corner radius.
 * @param rect Rounded border node to update.
 * @param radius New radius in logical pixels, clamped to half the smaller dimension.
 * @return true if the effective radius changed, false otherwise.
 */
bool wsm_scene_border_set_top_left_radius(struct wsm_scene_border *rect, float radius);

/**
 * @brief Changes the top-right corner radius.
 * @param rect Rounded border node to update.
 * @param radius New radius in logical pixels, clamped to half the smaller dimension.
 * @return true if the effective radius changed, false otherwise.
 */
bool wsm_scene_border_set_top_right_radius(struct wsm_scene_border *rect, float radius);

/**
 * @brief Changes the bottom-left corner radius.
 * @param rect Rounded border node to update.
 * @param radius New radius in logical pixels, clamped to half the smaller dimension.
 * @return true if the effective radius changed, false otherwise.
 */
bool wsm_scene_border_set_bottom_left_radius(struct wsm_scene_border *rect, float radius);

/**
 * @brief Changes the bottom-right corner radius.
 * @param rect Rounded border node to update.
 * @param radius New radius in logical pixels, clamped to half the smaller dimension.
 * @return true if the effective radius changed, false otherwise.
 */
bool wsm_scene_border_set_bottom_right_radius(struct wsm_scene_border *rect, float radius);

/**
 * @brief Checks whether a scene node is a rounded border.
 * @param node Scene node to inspect.
 * @return true when the node is a rounded border, false otherwise.
 */
bool wsm_scene_node_is_border(const struct wsm_scene_node *node);

/**
 * @brief Casts a rounded border scene node to its containing object.
 * @param node Node known to represent a rounded border.
 * @return Enclosing rounded border node.
 */
struct wsm_scene_border *wsm_scene_border_from_node(struct wsm_scene_node *node);

#endif // NODE_WSM_SCENE_BORDER_H
