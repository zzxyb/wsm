/**
 * @file        wsm_scene.h
 * @brief       Scene-graph root and scene-wide configuration.
 * @details     This file defines the root scene object, debug options, render
 *              timing state, and scene-wide protocol integration helpers.
 * @author      YaoBing Xiao
 * @date        2026-08-23
 * @version     v1.0
 * @par Copyright(c):
 * @par History:
 *      version: v1.0, YaoBing Xiao, 2026-08-23, initial version\n
 */

#ifndef SCENE_WSM_SCENE_H
#define SCENE_WSM_SCENE_H

#include <wayland-server-core.h>

#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_damage_ring.h>
#include <wlr/types/wlr_linux_dmabuf_v1.h>

struct wsm_scene_node;

struct wlr_linux_dmabuf_v1;
struct wlr_gamma_control_manager_v1;
struct wlr_color_manager_v1;

enum wsm_scene_debug_damage_option {
	WLR_SCENE_DEBUG_DAMAGE_NONE, /**< Do not visualize damage. */
	WLR_SCENE_DEBUG_DAMAGE_RERENDER, /**< Re-render damaged regions for debugging. */
	WLR_SCENE_DEBUG_DAMAGE_HIGHLIGHT /**< Highlight damaged regions. */
};

/**
 * @brief Root scene-graph object.
 */
struct wsm_scene {
	struct wsm_scene_tree *tree; /**< Root scene tree. */

	struct wl_list outputs; /**< List of scene output links. */

	struct wlr_linux_dmabuf_v1 *linux_dmabuf_v1; /**< Optional DMA-BUF manager. */
	struct wlr_gamma_control_manager_v1 *gamma_control_manager_v1; /**< Optional gamma manager. */
	struct wlr_color_manager_v1 *color_manager_v1; /**< Optional color manager. */

	bool restack_xwayland_surfaces; /**< Whether Xwayland surfaces are restacked. */

	struct {
		struct wl_listener linux_dmabuf_v1_destroy; /**< DMA-BUF manager destroy listener. */
		struct wl_listener gamma_control_manager_v1_destroy; /**< Gamma manager destroy listener. */
		struct wl_listener gamma_control_manager_v1_set_gamma; /**< Gamma update listener. */
		struct wl_listener color_manager_v1_destroy; /**< Color manager destroy listener. */

		enum wsm_scene_debug_damage_option debug_damage_option; /**< Damage debug mode. */
		bool direct_scanout; /**< Whether direct scanout is enabled. */
		bool calculate_visibility; /**< Whether visibility is calculated. */
		bool highlight_transparent_region; /**< Whether transparent regions are highlighted. */
	} WLR_PRIVATE;
};

/**
 * @brief Timing state for scene rendering.
 */
struct wsm_scene_timer {
	int64_t pre_render_duration; /**< Time spent before rendering, in nanoseconds. */
	struct wlr_render_timer *render_timer; /**< Backend render timer. */
};

/**
 * @brief Creates a new scene-graph.
 * @return Newly created scene, or NULL on allocation failure.
 */
struct wsm_scene *wsm_scene_create(void);

/**
 * @brief Destroys a scene-graph and its associated resources.
 * @param scene Scene to destroy.
 */
void wsm_scene_destroy(struct wsm_scene *scene);

/**
 * @brief Enables linux_dmabuf_v1 feedback for all scene surfaces.
 * @param scene Scene to configure.
 * @param linux_dmabuf_v1 DMA-BUF manager used for feedback.
 */
void wsm_scene_set_linux_dmabuf_v1(struct wsm_scene *scene,
	struct wlr_linux_dmabuf_v1 *linux_dmabuf_v1);

/**
 * @brief Enables gamma-control handling for all scene outputs.
 * @param scene Scene to configure.
 * @param gamma_control Gamma-control manager used by the scene.
 */
void wsm_scene_set_gamma_control_manager_v1(struct wsm_scene *scene,
	struct wlr_gamma_control_manager_v1 *gamma_control);

/**
 * @brief Enables color-management feedback for all scene surfaces.
 * @param scene Scene to configure.
 * @param manager Color manager used by the scene.
 */
void wsm_scene_set_color_manager_v1(struct wsm_scene *scene, struct wlr_color_manager_v1 *manager);

/**
 * @brief Gets the duration of the most recent scene render.
 * @param timer Scene timer to query.
 * @return Duration in nanoseconds, or -1 when unavailable.
 */
int64_t wsm_scene_timer_get_duration_ns(struct wsm_scene_timer *timer);

/**
 * @brief Finishes and releases the backend timer associated with a scene timer.
 * @param timer Scene timer to finish.
 */
void wsm_scene_timer_finish(struct wsm_scene_timer *timer);

/**
 * @brief Gets the root scene from a scene node.
 * @param node Scene node belonging to the scene.
 * @return Root scene, or NULL when the node is NULL or unattached.
 */
struct wsm_scene *wsm_scene_node_get_root(struct wsm_scene_node *node);

#endif // SCENE_WSM_SCENE_H
