/**
 * @file        wsm_scene_layer_surface.h
 * @brief       Scene helper for layer-shell surfaces.
 * @details     This file defines layer-shell scene state and helpers for
 *              configuring and positioning layer surfaces.
 * @author      YaoBing Xiao
 * @date        2026-08-23
 * @version     v1.0
 * @par Copyright(c):
 * @par History:
 *      version: v1.0, YaoBing Xiao, 2026-08-23, initial version\n
 */

#ifndef SCENE_WSM_SCENE_LAYER_SURFACE_H
#define SCENE_WSM_SCENE_LAYER_SURFACE_H

#include <wayland-server-core.h>

#include <wlr/util/addon.h>
#include <wlr/util/box.h>

struct wlr_layer_surface_v1;

struct wsm_scene_tree;

/**
 * @brief Scene state for a layer-shell surface.
 */
struct wsm_scene_layer_surface_v1 {
	struct wsm_scene_tree *tree; /**< Scene tree for the layer surface. */
	struct wlr_layer_surface_v1 *layer_surface; /**< Associated layer surface. */

	struct {
		struct wl_listener tree_destroy; /**< Scene tree destroy listener. */
		struct wl_listener layer_surface_destroy; /**< Layer surface destroy listener. */
		struct wl_listener layer_surface_map; /**< Layer surface map listener. */
		struct wl_listener layer_surface_unmap; /**< Layer surface unmap listener. */
	} WLR_PRIVATE;
};

/**
 * @brief Adds a layer surface and its sub-surfaces to the scene-graph.
 * @param parent Parent scene tree.
 * @param layer_surface Layer surface to display.
 * @return Layer surface scene state, or NULL on failure.
 */
struct wsm_scene_layer_surface_v1 *wsm_scene_layer_surface_v1_create(
	struct wsm_scene_tree *parent, struct wlr_layer_surface_v1 *layer_surface);

/**
 * @brief Configures and positions a layer surface.
 * @param scene_layer_surface Layer surface scene state.
 * @param full_area Entire area available to the layer surface.
 * @param usable_area Remaining area available to subsequent layer surfaces.
 */
void wsm_scene_layer_surface_v1_configure(
	struct wsm_scene_layer_surface_v1 *scene_layer_surface,
	const struct wlr_box *full_area, struct wlr_box *usable_area);

#endif // SCENE_WSM_SCENE_LAYER_SURFACE_H
