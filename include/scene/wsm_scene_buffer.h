/**
 * @file        wsm_scene_buffer.h
 * @brief       Scene node for rendering wlroots buffers.
 * @details     This file defines buffer scene state, buffer update options,
 *              output notifications, and buffer traversal helpers.
 * @author      YaoBing Xiao
 * @date        2026-08-23
 * @version     v1.0
 * @par Copyright(c):
 * @par History:
 *      version: v1.0, YaoBing Xiao, 2026-08-23, initial version\n
 */

#ifndef SCENE_WSM_SCENE_BUFFER_H
#define SCENE_WSM_SCENE_BUFFER_H

#include "scene/wsm_scene_node.h"

#include <wayland-server-core.h>

#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_damage_ring.h>
#include <wlr/types/wlr_linux_dmabuf_v1.h>
#include <wlr/util/addon.h>
#include <wlr/util/box.h>

struct wsm_scene_buffer;

/**
 * @brief Callback that tests whether a point accepts input.
 * @param buffer Buffer node being tested.
 * @param sx In/out surface-local x coordinate.
 * @param sy In/out surface-local y coordinate.
 * @return true when the point accepts input, false otherwise.
 */
typedef bool (*wlr_scene_buffer_point_accepts_input_func_t)(
	struct wsm_scene_buffer *buffer, double *sx, double *sy);

/**
 * @brief Callback invoked for each buffer during scene traversal.
 * @param buffer Buffer node being visited.
 * @param sx Buffer x coordinate in the scene.
 * @param sy Buffer y coordinate in the scene.
 * @param user_data Caller-provided data.
 */
typedef void (*wsm_scene_buffer_iterator_func_t)(
	struct wsm_scene_buffer *buffer, int sx, int sy, void *user_data);

/**
 * @brief Scene-graph node displaying a buffer.
 */
struct wsm_scene_buffer {
	struct wsm_scene_node node; /**< Base scene node. */

	struct wlr_buffer *buffer; /**< Backing buffer, or NULL when unset. */

	struct {
		struct wl_signal outputs_update; /**< Emitted when output membership changes. */
		struct wl_signal output_enter; /**< Emitted when entering an output. */
		struct wl_signal output_leave; /**< Emitted when leaving an output. */
		struct wl_signal output_sample; /**< Emitted when sampled by an output. */
		struct wl_signal frame_done; /**< Emitted when frame completion is due. */
	} events; /**< Buffer node events. */

	wlr_scene_buffer_point_accepts_input_func_t point_accepts_input; /**< Optional input filter. */

	/**
	 * @brief Output showing the largest visible area of this buffer.
	 * @details This may be NULL when the buffer is not displayed. It is the
	 *          output used for frame callbacks and presentation feedback.
	 */
	struct wsm_scene_output *primary_output; /**< Output showing the largest buffer area. */

	float opacity; /**< Buffer opacity. */
	enum wlr_scale_filter_mode filter_mode; /**< Scaling filter. */
	struct wlr_fbox src_box; /**< Source rectangle. */
	int dst_width, dst_height; /**< Destination dimensions. */
	enum wl_output_transform transform; /**< Buffer transform. */
	pixman_region32_t opaque_region; /**< Opaque region hint. */
	enum wlr_color_transfer_function transfer_function; /**< Color transfer function. */
	enum wlr_color_named_primaries primaries; /**< Color primaries. */
	enum wlr_color_encoding color_encoding; /**< Color encoding. */
	enum wlr_color_range color_range; /**< Color range. */


	struct {
		uint64_t active_outputs; /**< Bitset of outputs currently displaying the buffer. */
		struct wlr_texture *texture; /**< Texture created from the backing buffer. */
		struct wlr_linux_dmabuf_feedback_v1_init_options prev_feedback_options; /**< Previous feedback state. */

		bool own_buffer; /**< Whether the scene owns a reference to the buffer. */
		int buffer_width, buffer_height; /**< Backing buffer dimensions. */
		bool buffer_is_opaque; /**< Whether the backing buffer is opaque. */

		struct wlr_drm_syncobj_timeline *wait_timeline; /**< Timeline to wait on. */
		uint64_t wait_point; /**< Timeline point to wait for. */

		struct wl_listener buffer_release; /**< Buffer release listener. */
		struct wl_listener renderer_destroy; /**< Renderer destroy listener. */

		bool is_single_pixel_buffer; /**< Whether the buffer is a single-pixel buffer. */
		/** Single-pixel RGBA color, with UINT32_MAX as the component maximum. */
		uint32_t single_pixel_buffer_color[4];
	} WLR_PRIVATE; /**< Private buffer state. */
};

/**
 * @brief Adds a buffer node to the scene-graph.
 * @param parent Parent scene tree.
 * @param buffer Backing buffer, or NULL for an initially empty node.
 * @return Newly created buffer node, or NULL on failure.
 */
struct wsm_scene_buffer *wsm_scene_buffer_create(struct wsm_scene_tree *parent,
	struct wlr_buffer *buffer);

/**
 * @brief Sets the backing buffer for a scene buffer node.
 * @param scene_buffer Scene buffer node to update.
 * @param buffer New backing buffer, or NULL to hide the node.
 */
void wsm_scene_buffer_set_buffer(struct wsm_scene_buffer *scene_buffer,
	struct wlr_buffer *buffer);

/**
 * @brief Sets the backing buffer with a custom damage region.
 * @param scene_buffer Scene buffer node to update.
 * @param buffer New backing buffer, or NULL to hide the node.
 * @param region Buffer-local damage region, or NULL for the whole buffer.
 */
void wsm_scene_buffer_set_buffer_with_damage(struct wsm_scene_buffer *scene_buffer,
	struct wlr_buffer *buffer, const pixman_region32_t *region);

/**
 * @brief Options for setting a scene buffer's backing buffer.
 */
struct wsm_scene_buffer_set_buffer_options {
	const pixman_region32_t *damage; /**< Buffer-local damage, or NULL for the whole buffer. */

	struct wlr_drm_syncobj_timeline *wait_timeline; /**< Timeline to wait on before reading. */
	uint64_t wait_point; /**< Timeline point to wait for. */
};

/**
 * @brief Sets the backing buffer with extended options.
 * @param scene_buffer Scene buffer node to update.
 * @param buffer New backing buffer, or NULL to hide the node.
 * @param options Damage and synchronization options, or NULL for defaults.
 */
void wsm_scene_buffer_set_buffer_with_options(struct wsm_scene_buffer *scene_buffer,
	struct wlr_buffer *buffer, const struct wsm_scene_buffer_set_buffer_options *options);

/**
 * @brief Sets the buffer's opaque region optimization hint.
 * @param scene_buffer Scene buffer node to update.
 * @param region Opaque region in buffer-local coordinates.
 */
void wsm_scene_buffer_set_opaque_region(struct wsm_scene_buffer *scene_buffer,
	const pixman_region32_t *region);

/**
 * @brief Sets the source rectangle sampled from the buffer.
 * @param scene_buffer Scene buffer node to update.
 * @param box Source rectangle, or NULL to sample the whole buffer.
 */
void wsm_scene_buffer_set_source_box(struct wsm_scene_buffer *scene_buffer,
	const struct wlr_fbox *box);

/**
 * @brief Sets the destination size used to scale the buffer.
 * @param scene_buffer Scene buffer node to update.
 * @param width Destination width, or zero for the buffer width.
 * @param height Destination height, or zero for the buffer height.
 */
void wsm_scene_buffer_set_dest_size(struct wsm_scene_buffer *scene_buffer,
	int width, int height);

/**
 * @brief Sets the transform applied to the buffer.
 * @param scene_buffer Scene buffer node to update.
 * @param transform Buffer transform.
 */
void wsm_scene_buffer_set_transform(struct wsm_scene_buffer *scene_buffer,
	enum wl_output_transform transform);

/**
 * @brief Sets the opacity of a buffer node.
 * @param scene_buffer Scene buffer node to update.
 * @param opacity New opacity.
 */
void wsm_scene_buffer_set_opacity(struct wsm_scene_buffer *scene_buffer,
	float opacity);

/**
 * @brief Sets the filter mode used when scaling a buffer.
 * @param scene_buffer Scene buffer node to update.
 * @param filter_mode Scaling filter.
 */
void wsm_scene_buffer_set_filter_mode(struct wsm_scene_buffer *scene_buffer,
	enum wlr_scale_filter_mode filter_mode);

/**
 * @brief Frame completion event for a scene buffer.
 */
struct wlr_scene_frame_done_event {
	struct wsm_scene_output *output; /**< Output completing the frame. */
	struct timespec when; /**< Frame completion timestamp. */
};

/**
 * @brief Emits the buffer's frame completion signal.
 * @param scene_buffer Scene buffer node.
 * @param event Frame completion event.
 */
void wsm_scene_buffer_send_frame_done(struct wsm_scene_buffer *scene_buffer,
	struct wlr_scene_frame_done_event *event);

/**
 * @brief Checks whether a scene node is a buffer node.
 * @param node Scene node to inspect.
 * @return true when the node is a buffer, false otherwise.
 */
bool wsm_scene_node_is_buffer(const struct wsm_scene_node *node);

/**
 * @brief Casts a scene node to a buffer node.
 * @param node Node known to represent a buffer.
 * @return Enclosing buffer node.
 */
struct wsm_scene_buffer *wsm_scene_buffer_from_node(struct wsm_scene_node *node);

/**
 * @brief Sets the buffer's transfer function.
 * @param scene_buffer Scene buffer node to update.
 * @param transfer_function Transfer function to use.
 */
void wsm_scene_buffer_set_transfer_function(struct wsm_scene_buffer *scene_buffer,
	enum wlr_color_transfer_function transfer_function);

/**
 * @brief Sets the buffer's color primaries.
 * @param scene_buffer Scene buffer node to update.
 * @param primaries Color primaries to use.
 */
void wsm_scene_buffer_set_primaries(struct wsm_scene_buffer *scene_buffer,
	enum wlr_color_named_primaries primaries);

/**
 * @brief Sets the buffer's color encoding.
 * @param scene_buffer Scene buffer node to update.
 * @param encoding Color encoding to use.
 */
void wsm_scene_buffer_set_color_encoding(struct wsm_scene_buffer *scene_buffer,
	enum wlr_color_encoding encoding);

/**
 * @brief Sets the buffer's color range.
 * @param scene_buffer Scene buffer node to update.
 * @param range Color range to use.
 */
void wsm_scene_buffer_set_color_range(struct wsm_scene_buffer *scene_buffer,
	enum wlr_color_range range);

/**
 * @brief Visits every buffer below a scene node in rendering order.
 * @param node Root node for the traversal.
 * @param iterator Callback invoked for each buffer.
 * @param user_data Caller-provided data.
 */
void wsm_scene_node_for_each_buffer(struct wsm_scene_node *node,
	wsm_scene_buffer_iterator_func_t iterator, void *user_data);

#endif // SCENE_WSM_SCENE_BUFFER_H
