/**
 * @file        wsm_scene_xdg_surface.h
 * @brief       Scene helper for xdg surfaces.
 * @details     This file defines xdg-surface scene state and the helper for
 *              adding a window and its sub-surfaces to a scene tree.
 * @author      YaoBing Xiao
 * @date        2026-08-23
 * @version     v1.0
 * @par Copyright(c):
 * @par History:
 *      version: v1.0, YaoBing Xiao, 2026-08-23, initial version\n
 */

#ifndef SCENE_WSM_SCENE_XDG_SHELL_H
#define SCENE_WSM_SCENE_XDG_SHELL_H

#include <wayland-server-core.h>

#include <wlr/util/box.h>
#include <wlr/util/addon.h>

struct wsm_scene_tree;
struct wlr_xdg_surface;

struct wsm_scene_xdg_surface {
	struct wsm_scene_tree *tree; /**< Scene tree containing the xdg surface. */
	struct wlr_xdg_surface *xdg_surface; /**< Associated xdg surface. */
	struct wsm_scene_tree *surface_tree; /**< Tree for the surface contents. */

	struct wl_listener tree_destroy; /**< Scene tree destroy listener. */
	struct wl_listener xdg_surface_destroy; /**< Xdg surface destroy listener. */
	struct wl_listener xdg_surface_commit; /**< Xdg surface commit listener. */
};

/**
 * @brief Adds an xdg surface and its sub-surfaces to the scene-graph.
 * @param parent Parent scene tree.
 * @param xdg_surface Xdg surface to display.
 * @return Scene tree for the xdg surface, or NULL on failure.
 */
struct wsm_scene_tree *wsm_scene_xdg_surface_create(
	struct wsm_scene_tree *parent, struct wlr_xdg_surface *xdg_surface);

#endif // SCENE_WSM_SCENE_XDG_SHELL_H
