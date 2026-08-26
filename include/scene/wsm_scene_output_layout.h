/**
 * @file        wsm_scene_output_layout.h
 * @brief       Synchronization between scene outputs and output layouts.
 * @details     This file defines the associations and listeners used to keep
 *              scene output positions synchronized with a wlroots layout.
 * @author      YaoBing Xiao
 * @date        2026-08-23
 * @version     v1.0
 * @par Copyright(c):
 * @par History:
 *      version: v1.0, YaoBing Xiao, 2026-08-23, initial version\n
 */

#ifndef WSM_OUTPUT_LAYOUT_H
#define WSM_OUTPUT_LAYOUT_H

#include <pixman.h>
#include <time.h>

#include <wayland-server-core.h>

#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_damage_ring.h>
#include <wlr/types/wlr_linux_dmabuf_v1.h>
#include <wlr/util/addon.h>
#include <wlr/util/box.h>

/**
 * @brief Association between a scene and an output layout.
 */
struct wsm_scene_output_layout {
	struct wlr_output_layout *layout; /**< Associated output layout. */
	struct wsm_scene *scene; /**< Scene whose outputs are synchronized. */

	struct wl_list outputs; /**< Scene output layout associations. */

	struct wl_listener layout_change; /**< Layout change listener. */
	struct wl_listener layout_destroy; /**< Layout destroy listener. */
	struct wl_listener scene_destroy; /**< Scene destroy listener. */
};

/**
 * @brief Association between one layout output and one scene output.
 */
struct wsm_scene_output_layout_output {
	struct wlr_output_layout_output *layout_output; /**< Layout output association. */
	struct wsm_scene_output *scene_output; /**< Scene output association. */

	struct wl_list link; /**< Link in wsm_scene_output_layout.outputs. */

	struct wl_listener layout_output_destroy; /**< Layout output destroy listener. */
	struct wl_listener scene_output_destroy; /**< Scene output destroy listener. */
};

/**
 * @brief Attaches an output layout to a scene.
 * @param scene Scene whose output positions are synchronized.
 * @param output_layout Output layout to attach.
 * @return New scene output layout, or NULL on failure.
 */
struct wsm_scene_output_layout *wsm_scene_attach_output_layout(struct wsm_scene *scene,
	struct wlr_output_layout *output_layout);

/**
 * @brief Associates a layout output with a scene output.
 * @param sol Scene output layout.
 * @param lo Layout output to associate.
 * @param so Scene output to reposition.
 */
void wsm_scene_output_layout_add_output(struct wsm_scene_output_layout *sol,
	struct wlr_output_layout_output *lo, struct wsm_scene_output *so);

#endif // WSM_OUTPUT_LAYOUT_H
