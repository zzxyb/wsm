/**
 * @file        wsm_scene_subsurface_tree.h
 * @brief       Scene tree helper for surfaces and sub-surfaces.
 * @details     This file defines the state and clipping helper for a surface
 *              together with all of its child sub-surfaces.
 * @author      YaoBing Xiao
 * @date        2026-08-23
 * @version     v1.0
 * @par Copyright(c):
 * @par History:
 *      version: v1.0, YaoBing Xiao, 2026-08-23, initial version\n
 */

#ifndef SCENE_WSM_SUBSURFACE_TREE_H
#define SCENE_WSM_SUBSURFACE_TREE_H

#include <wayland-server-core.h>

#include <wlr/util/box.h>
#include <wlr/util/addon.h>

struct wlr_surface;
struct wsm_scene_node;
struct wsm_scene_tree;

/**
 * @brief Scene tree for a surface and all of its child sub-surfaces.
 */
struct wsm_scene_subsurface_tree {
	struct wsm_scene_tree *tree; /**< Scene tree containing the surface. */
	struct wlr_surface *surface; /**< Root surface. */
	struct wsm_scene_surface *scene_surface; /**< Scene surface node for the root. */

	struct wl_listener surface_destroy; /**< Surface destroy listener. */
	struct wl_listener surface_commit; /**< Surface commit listener. */
	struct wl_listener surface_map; /**< Surface map listener. */
	struct wl_listener surface_unmap; /**< Surface unmap listener. */
	struct wl_listener surface_new_subsurface; /**< New sub-surface listener. */

	struct wsm_scene_subsurface_tree *parent; /**< Parent tree, or NULL at the root. */

	struct wlr_addon scene_addon; /**< Addon linking the tree to its scene node. */

	struct wlr_box clip; /**< Crop rectangle in root-surface coordinates. */

	struct wlr_addon surface_addon; /**< Addon used for sub-surface association. */

	struct wl_listener subsurface_destroy; /**< Sub-surface destroy listener. */
};

/**
 * @brief Adds a surface and all of its sub-surfaces to the scene-graph.
 * @param parent Parent scene tree.
 * @param surface Root surface to display.
 * @return Scene tree for the surface hierarchy, or NULL on failure.
 */
struct wsm_scene_subsurface_tree *wsm_scene_subsurface_tree_create(
	struct wsm_scene_tree *parent, struct wlr_surface *surface);

/**
 * @brief Sets the cropping region for a sub-surface tree.
 * @param node Scene node whose sub-surface children are clipped.
 * @param clip Crop rectangle in root-surface coordinates, or NULL to disable clipping.
 */
void wsm_scene_subsurface_tree_set_clip(struct wsm_scene_node *node,
	const struct wlr_box *clip);

#endif // SCENE_WSM_SUBSURFACE_TREE_H
