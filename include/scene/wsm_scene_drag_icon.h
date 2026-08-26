/**
 * @file        wsm_scene_drag_icon.h
 * @brief       Scene helper for drag icons.
 * @details     This file defines the scene state and creation helper for a
 *              drag icon and all of its sub-surfaces.
 * @author      YaoBing Xiao
 * @date        2026-08-23
 * @version     v1.0
 * @par Copyright(c):
 * @par History:
 *      version: v1.0, YaoBing Xiao, 2026-08-23, initial version\n
 */

#ifndef SCENE_WSM_SCENE_DRAG_ICON_H
#define SCENE_WSM_SCENE_DRAG_ICON_H

#include <wayland-server-core.h>

struct wlr_drag_icon;
struct wsm_scene_tree;

struct wsm_scene_drag_icon {
	struct wsm_scene_tree *tree; /**< Scene tree containing the drag icon. */
	struct wsm_scene_tree *surface_tree; /**< Tree for the icon surface. */
	struct wlr_drag_icon *drag_icon; /**< Associated wlroots drag icon. */

	struct wl_listener tree_destroy; /**< Scene tree destroy listener. */
	struct wl_listener drag_icon_surface_commit; /**< Surface commit listener. */
	struct wl_listener drag_icon_destroy; /**< Drag icon destroy listener. */
};

/**
 * @brief Adds a drag icon and its sub-surfaces to the scene-graph.
 * @param parent Parent scene tree.
 * @param drag_icon Drag icon to display.
 * @return Scene tree for the drag icon, or NULL on failure.
 */
struct wsm_scene_drag_icon *wsm_scene_drag_icon_create(
	struct wsm_scene_tree *parent, struct wlr_drag_icon *drag_icon);

#endif // SCENE_WSM_SCENE_DRAG_ICON_H
