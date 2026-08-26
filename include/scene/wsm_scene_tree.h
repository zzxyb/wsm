/**
 * @file        wsm_scene_tree.h
 * @brief       Scene tree node and tree creation helpers.
 * @details     This file defines tree state and helpers for creating root and
 *              child scene trees.
 * @author      YaoBing Xiao
 * @date        2026-08-23
 * @version     v1.0
 * @par Copyright(c):
 * @par History:
 *      version: v1.0, YaoBing Xiao, 2026-08-23, initial version\n
 */

#ifndef SCENE_WSM_SCENE_TREE_H
#define SCENE_WSM_SCENE_TREE_H

#include "scene/wsm_scene_node.h"

#include <stdbool.h>

#include <wayland-server-core.h>

struct wlr_drag_icon;
struct wlr_xdg_surface;
struct wlr_layer_surface_v1;
struct wlr_surface;

struct wsm_scene_tree {
	struct wsm_scene_node node; /**< Base scene node. */

	struct wl_list children; /**< Child nodes. */
};

/**
 * @brief Creates a scene tree containing only its children.
 * @param parent Parent scene tree.
 * @return Newly created scene tree, or NULL on failure.
 */
struct wsm_scene_tree *wsm_scene_tree_create(struct wsm_scene_tree *parent);

/**
 * @brief Creates the root scene tree for a scene.
 * @param scene Scene that owns the root tree.
 * @return Newly created root tree, or NULL on failure.
 */
struct wsm_scene_tree *wsm_root_scene_tree_create(struct wsm_scene *scene);

/**
 * @brief Checks whether a scene node is a tree node.
 * @param node Scene node to inspect.
 * @return true when the node is a tree, false otherwise.
 */
bool wsm_scene_node_is_tree(const struct wsm_scene_node *node);

/**
 * @brief Casts a scene node to a scene tree.
 * @param node Node known to represent a scene tree.
 * @return Enclosing scene tree.
 */
struct wsm_scene_tree *wsm_scene_tree_from_node(struct wsm_scene_node *node);

#endif // SCENE_WSM_SCENE_TREE_H
