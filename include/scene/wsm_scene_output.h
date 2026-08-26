/**
 * @file        wsm_scene_output.h
 * @brief       Scene output state, damage, and rendering helpers.
 * @details     This file defines scene output state, output events, render
 *              options, frame commits, and visible-buffer traversal.
 * @author      YaoBing Xiao
 * @date        2026-08-23
 * @version     v1.0
 * @par Copyright(c):
 * @par History:
 *      version: v1.0, YaoBing Xiao, 2026-08-23, initial version\n
 */

#ifndef WSM_SCENE_OUTPUT_H
#define WSM_SCENE_OUTPUT_H

#include <pixman.h>
#include <time.h>

#include <wayland-server-core.h>

#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_damage_ring.h>
#include <wlr/types/wlr_linux_dmabuf_v1.h>
#include <wlr/util/addon.h>
#include <wlr/util/box.h>

struct wlr_output;
struct wlr_output_layout;
struct wlr_output_layout_output;
struct wlr_xdg_surface;
struct wlr_layer_surface_v1;
struct wlr_surface;

struct wsm_scene_buffer;
struct wsm_scene_output_layout;

struct wlr_presentation;
struct wlr_linux_dmabuf_v1;
struct wlr_gamma_control_manager_v1;
struct wlr_color_manager_v1;
struct wlr_output_state;

/**
 * @brief Callback invoked for each buffer visible on an output.
 * @param buffer Buffer node being visited.
 * @param sx Buffer x coordinate in the scene.
 * @param sy Buffer y coordinate in the scene.
 * @param user_data Caller-provided data.
 */
typedef void (*wsm_scene_buffer_iterator_func_t)(
	struct wsm_scene_buffer *buffer, int sx, int sy, void *user_data);

/**
 * @brief Event describing the active outputs for a scene buffer.
 */
struct wsm_scene_outputs_update_event {
	struct wsm_scene_output **active; /**< Active scene outputs. */
	size_t size; /**< Number of active outputs. */
};

/**
 * @brief Event describing a buffer sample on a scene output.
 */
struct wsm_scene_output_sample_event {
	struct wsm_scene_output *output; /**< Output that sampled the buffer. */
	bool direct_scanout; /**< Whether direct scanout was used. */
	struct wlr_drm_syncobj_timeline *release_timeline; /**< Release synchronization timeline. */
	uint64_t release_point; /**< Release timeline point. */
};

/**
 * @brief Scene-graph viewport for an output.
 */
struct wsm_scene_output {
	struct wlr_output *output; /**< Associated wlroots output. */
	struct wl_list link; /**< Link in wsm_scene.outputs. */
	struct wsm_scene *scene; /**< Owning scene. */
	struct wlr_addon addon; /**< Addon linking the output to the scene. */

	struct wlr_damage_ring damage_ring; /**< Damage ring for the output. */

	int x, y; /**< Output position in scene coordinates. */

	struct {
		struct wl_signal destroy; /**< Emitted when the scene output is destroyed. */
	} events; /**< Output events. */

	struct {
		pixman_region32_t pending_commit_damage; /**< Damage pending the next commit. */

		uint8_t index; /**< Output index in scene output bitsets. */

		/**
		 * @brief Debounces scanout and composition DMA-BUF feedback.
		 * @details The value increases while scanout is applicable and decreases
		 *          when composition is required.
		 */
		uint8_t dmabuf_feedback_debounce; /**< Remaining DMA-BUF feedback debounce frames. */
		bool prev_scanout; /**< Whether the previous frame used scanout. */

		bool gamma_lut_changed; /**< Whether the gamma LUT changed. */
		struct wlr_gamma_control_v1 *gamma_lut; /**< Current gamma LUT. */
		struct wlr_color_transform *gamma_lut_color_transform; /**< Gamma LUT transform. */

		struct wlr_color_transform *prev_gamma_lut_color_transform; /**< Previous gamma transform. */
		struct wlr_color_transform *prev_supplied_color_transform; /**< Previous supplied transform. */
		struct wlr_color_transform *combined_color_transform; /**< Combined output transform. */

		struct wl_listener output_commit; /**< Output commit listener. */
		struct wl_listener output_damage; /**< Output damage listener. */
		struct wl_listener output_needs_frame; /**< Frame request listener. */

		struct wl_list damage_highlight_regions; /**< Debug damage regions. */

		struct wl_array render_list; /**< Render-list entries for the frame. */

		struct wlr_drm_syncobj_timeline *in_timeline; /**< Input synchronization timeline. */
		uint64_t in_point; /**< Input timeline point. */
		struct wlr_drm_syncobj_timeline *out_timeline; /**< Output synchronization timeline. */
		uint64_t out_point; /**< Output timeline point. */
	} WLR_PRIVATE; /**< Private output state. */
};

/**
 * @brief Adds an output viewport to a scene.
 * @param scene Scene that owns the viewport.
 * @param output Output to add.
 * @return Newly created scene output, or NULL on failure.
 */
struct wsm_scene_output *wsm_scene_output_create(struct wsm_scene *scene,
	struct wlr_output *output);
/**
 * @brief Destroys a scene output.
 * @param scene_output Scene output to destroy.
 */
void wsm_scene_output_destroy(struct wsm_scene_output *scene_output);
/**
 * @brief Sets an output's position in scene coordinates.
 * @param scene_output Scene output to update.
 * @param lx New layout x coordinate.
 * @param ly New layout y coordinate.
 */
void wsm_scene_output_set_position(struct wsm_scene_output *scene_output,
	int lx, int ly);

/**
 * @brief Optional state used while rendering or committing an output.
 */
struct wsm_scene_output_state_options {
	struct wsm_scene_timer *timer; /**< Optional scene render timer. */

	/** @brief Color transform applied before the output transform. */
	struct wlr_color_transform *color_transform; /**< Optional pre-output transform. */

	/** @brief Custom swapchain used for the output configuration. */
	struct wlr_swapchain *swapchain; /**< Optional custom swapchain. */
};

/**
 * @brief Checks whether an output needs a new frame.
 * @param scene_output Scene output to query.
 * @return true when rendering is needed, false otherwise.
 */
bool wsm_scene_output_needs_frame(struct wsm_scene_output *scene_output);

/**
 * @brief Renders and commits an output.
 * @param scene_output Scene output to render.
 * @param options Render options, or NULL for defaults.
 * @return true on success, false when rendering or commit fails.
 */
bool wsm_scene_output_commit(struct wsm_scene_output *scene_output,
	const struct wsm_scene_output_state_options *options);

/**
 * @brief Renders an output and populates an output state.
 * @param scene_output Scene output to render.
 * @param state Output state to populate.
 * @param options Render options, or NULL for defaults.
 * @return true on success, false when rendering fails.
 */
bool wsm_scene_output_build_state(struct wsm_scene_output *scene_output,
	struct wlr_output_state *state, const struct wsm_scene_output_state_options *options);
/**
 * @brief Sends frame completion to surfaces rendered by an output.
 * @param scene_output Scene output whose surfaces receive completion.
 * @param now Frame completion timestamp.
 */
void wsm_scene_output_send_frame_done(struct wsm_scene_output *scene_output,
	struct timespec *now);
/**
 * @brief Visits each buffer visible on an output in render order.
 * @param scene_output Scene output to traverse.
 * @param iterator Callback invoked for each visible buffer.
 * @param user_data Caller-provided data.
 */
void wsm_scene_output_for_each_buffer(struct wsm_scene_output *scene_output,
	wsm_scene_buffer_iterator_func_t iterator, void *user_data);
/**
 * @brief Gets the scene output associated with a wlroots output.
 * @param scene Scene to search.
 * @param output Output to find.
 * @return Matching scene output, or NULL when it is not attached.
 */
struct wsm_scene_output *wsm_scene_get_scene_output(struct wsm_scene *scene,
	struct wlr_output *output);

#endif // WSM_SCENE_OUTPUT_H
