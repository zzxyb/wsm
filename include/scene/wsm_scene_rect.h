/**
 * @file        wsm_scene_rect.h
 * @brief       Solid rectangle scene node.
 * @details     This file defines the rectangle node state and helpers for
 *              creating and updating solid-colored scene rectangles.
 * @author      YaoBing Xiao
 * @date        2026-08-23
 * @version     v1.0
 * @par Copyright(c):
 * @par History:
 *      version: v1.0, YaoBing Xiao, 2026-08-23, initial version\n
 */

#ifndef SCENE_WSM_SCENE_RECT_H
#define SCENE_WSM_SCENE_RECT_H

#include "scene/wsm_scene_node.h"

struct wsm_scene_rect {
	struct wsm_scene_node node; /**< Base scene node. */
	int width, height; /**< Rectangle dimensions in pixels. */
	float color[4]; /**< Premultiplied RGBA color. */
};

/**
 * @brief Adds a solid-colored rectangle to the scene-graph.
 * @param parent Parent scene tree.
 * @param width Rectangle width in pixels.
 * @param height Rectangle height in pixels.
 * @param color Premultiplied RGBA color.
 * @return Newly created rectangle node, or NULL on failure.
 */
struct wsm_scene_rect *wsm_scene_rect_create(struct wsm_scene_tree *parent,
	int width, int height, const float color[static 4]);

/**
 * @brief Changes the size of a rectangle node.
 * @param rect Rectangle node to update.
 * @param width New width in pixels.
 * @param height New height in pixels.
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

#endif // SCENE_WSM_SCENE_RECT_H
