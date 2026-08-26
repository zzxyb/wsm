/**
 * @file        wsm_scene_surface.h
 * @brief       Scene node helper for a single Wayland surface.
 * @details     This file defines the surface scene state and helpers for
 *              creating, clipping, and completing frames for a surface node.
 * @author      YaoBing Xiao
 * @date        2026-08-23
 * @version     v1.0
 * @par Copyright(c):
 * @par History:
 *      version: v1.0, YaoBing Xiao, 2026-08-23, initial version\n
 */

#ifndef SCENE_WSM_SCENE_SURFACE_H
#define SCENE_WSM_SCENE_SURFACE_H

#include <time.h>

#include <wayland-server-core.h>

#include <wlr/util/addon.h>
#include <wlr/util/box.h>

struct wlr_surface;

struct wsm_scene_tree;
struct wsm_scene_buffer;
struct wsm_scene_output_layout;

/**
 * @brief Scene-graph node displaying a single surface.
 */
struct wsm_scene_surface {
	struct wsm_scene_buffer *buffer; /**< Buffer node associated with the surface. */
	struct wlr_surface *surface; /**< Wayland surface displayed by the node. */

	struct {
		struct wlr_box clip; /**< Surface clipping rectangle. */

		struct wlr_addon addon; /**< Addon linking the surface to the scene. */

		struct wl_listener outputs_update; /**< Outputs update listener. */
		struct wl_listener output_sample; /**< Output sample listener. */
		struct wl_listener frame_done; /**< Frame completion listener. */
		struct wl_listener surface_destroy; /**< Surface destroy listener. */
		struct wl_listener surface_commit; /**< Surface commit listener. */
	} WLR_PRIVATE; /**< Private surface state. */
};

/**
 * @brief Adds a scene node displaying a single surface.
 * @param parent Parent scene tree.
 * @param surface Surface to display.
 * @return Newly created scene surface, or NULL on failure.
 * @note Child sub-surfaces are ignored; use the sub-surface tree helper when
 *       they must be included.
 */
struct wsm_scene_surface *wsm_scene_surface_create(struct wsm_scene_tree *parent,
	struct wlr_surface *surface);

/**
 * @brief Gets the surface node associated with a scene buffer.
 * @param scene_buffer Scene buffer to inspect.
 * @return Associated surface node, or NULL when the buffer is not surface-backed.
 */
struct wsm_scene_surface *wsm_scene_surface_try_from_buffer(
	struct wsm_scene_buffer *scene_buffer);

/**
 * @brief Sends frame completion to a visible surface.
 * @param scene_surface Surface node receiving the completion.
 * @param when Frame completion timestamp.
 */
void wsm_scene_surface_send_frame_done(struct wsm_scene_surface *scene_surface,
	const struct timespec *when);

/**
 * @brief Sets the clipping rectangle for a surface node.
 * @param surface Surface node to update.
 * @param clip Clipping rectangle.
 */
void wsm_scene_surface_set_clip(struct wsm_scene_surface *surface, struct wlr_box *clip);

#endif // SCENE_WSM_SCENE_SURFACE_H
