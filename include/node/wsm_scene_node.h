/**
 * @file        wsm_scene_node.h
 * @brief       Base scene-node interface and traversal helpers.
 * @details     This file defines scene-node state, implementation callbacks,
 *              render-list data, geometry traversal, and node management APIs.
 * @author      YaoBing Xiao
 * @date        2026-08-23
 * @version     v1.0
 * @par Copyright(c):
 * @par History:
 *      version: v1.0, YaoBing Xiao, 2026-08-23, initial version\n
 */

#ifndef SCENE_WSM_SCENE_NODE_H
#define SCENE_WSM_SCENE_NODE_H

#include "scene/wsm_scene.h"

#include <time.h>

#include <pixman.h>

#include <wayland-server-core.h>

#include <wlr/util/addon.h>
#include <wlr/util/box.h>
#include <wlr/config.h>
#if WLR_HAS_XWAYLAND
#include <wlr/xwayland/xwayland.h>
#endif

struct wsm_scene_node;
struct wsm_scene_output;
struct wsm_render_list_entry;
struct wsm_render_data;
struct wsm_scene_buffer;
struct wlr_linux_dmabuf_feedback_v1_init_options;

/**
 * @brief Callback used while traversing nodes that intersect a box.
 * @param node Node currently being visited.
 * @param sx Node-local x coordinate.
 * @param sy Node-local y coordinate.
 * @param data User data supplied to the traversal.
 * @return true to continue traversal, false to stop.
 */
typedef bool (*scene_node_box_iterator_func_t)(struct wsm_scene_node *node,
	int sx, int sy, void *data);

/**
 * @brief State used while updating scene nodes.
 */
struct wsm_scene_update_data {
	pixman_region32_t *visible; /**< Visible region being updated. */
	const pixman_region32_t *update_region; /**< Region requiring an update. */
	struct wlr_box update_box; /**< Bounding box of the update. */
	struct wl_list *outputs; /**< Scene outputs being traversed. */
	bool calculate_visibility; /**< Whether visibility should be recalculated. */
	bool restack_xwayland_surfaces; /**< Whether Xwayland surfaces are restacked. */

#if WLR_HAS_XWAYLAND
	struct wlr_xwayland_surface *restack_above; /**< Surface to restack above. */
#endif
};

/**
 * @brief State used while rendering a scene output.
 */
struct wsm_render_data {
	enum wl_output_transform transform; /**< Output transform. */
	float scale; /**< Output scale. */
	struct wlr_box logical; /**< Logical output rectangle. */
	int trans_width, trans_height; /**< Transformed output dimensions. */

	struct wsm_scene_output *output; /**< Output being rendered. */

	struct wlr_render_pass *render_pass; /**< Render pass receiving the scene. */
	pixman_region32_t damage; /**< Damage region for the frame. */
};

/**
 * @brief State used while constructing a render list.
 */
struct wsm_render_list_constructor_data {
	struct wlr_box box; /**< Output box used for construction. */
	struct wl_array *render_list; /**< Destination render-list array. */
	bool calculate_visibility; /**< Whether visibility should be calculated. */
	bool highlight_transparent_region; /**< Whether transparent regions are highlighted. */
	bool fractional_scale; /**< Whether fractional scaling is active. */
};

/**
 * @brief One scene node entry in a render list.
 */
struct wsm_render_list_entry {
	struct wsm_scene_node *node; /**< Node represented by the entry. */
	bool highlight_transparent_region; /**< Whether transparent regions are highlighted. */
	int x, y; /**< Node position in output coordinates. */
};

/**
 * @brief Virtual operations implemented by a scene-node type.
 */
struct wsm_scene_node_impl {
	void (*destroy)(struct wsm_scene_node *node); /**< Destroys the node implementation. */
	void (*set_enabled)(struct wsm_scene_node *node, bool enabled); /**< Changes enabled state. */
	void (*set_position)(struct wsm_scene_node *node, int x, int y); /**< Changes position. */
	void (*bounds)(struct wsm_scene_node *node,
		int x, int y, pixman_region32_t *visible); /**< Computes visible bounds. */
	void (*get_size)(struct wsm_scene_node *node,
		int *width, int *height); /**< Gets node size. */
	bool (*coords)(struct wsm_scene_node *node, int *lx_ptr, int *ly_ptr); /**< Converts coordinates. */
	struct wsm_scene_node *(*at)(struct wsm_scene_node *node,
		double lx, double ly, double *nx, double *ny); /**< Finds the node at a point. */
	bool (*in_box)(struct wsm_scene_node *node, struct wlr_box *box,
		scene_node_box_iterator_func_t iterator, void *user_data); /**< Traverses nodes in a box. */
	void (*opaque_region)(struct wsm_scene_node *node, int x, int y,
		pixman_region32_t *opaque); /**< Computes opaque region. */
	void (*update_outputs)(struct wsm_scene_node *node,
		struct wl_list *outputs, struct wsm_scene_output *ignore,
		struct wsm_scene_output *force); /**< Updates output membership. */
	void (*update)(struct wsm_scene_node *node,
		pixman_region32_t *damage); /**< Updates node damage. */
	void (*visibility)(struct wsm_scene_node *node,
		pixman_region32_t *visible); /**< Computes node visibility. */
	void (*frame_done)(struct wsm_scene_node *node,
		struct wsm_scene_output *scene_output, struct timespec *now); /**< Sends frame completion. */
	bool (*invisible)(struct wsm_scene_node *node); /**< Checks whether a node is invisible. */
	bool (*construct_render_list_iterator)(struct wsm_scene_node *node,
		int lx, int ly, void *_data); /**< Adds a node to a render list. */
	void (*render)(struct wsm_render_list_entry *entry, const struct wsm_render_data *data); /**< Renders a node. */
	void (*dmabuf_feedback)(struct wsm_render_list_entry *entry,
		struct wsm_scene_output *scene_output); /**< Updates DMA-BUF feedback. */
	void (*get_extents)(struct wsm_scene_node *node, int lx, int ly,
		int *x_min, int *y_min, int *x_max, int *y_max); /**< Gets node extents. */
	struct wl_list *(*get_children)(struct wsm_scene_node *node); /**< Gets child list. */
	void (*restack_xwayland_surface)(struct wsm_scene_node *node,
		struct wlr_box *box, struct wsm_scene_update_data *data); /**< Restacks Xwayland surfaces. */
	void (*cleanup_when_disabled)(struct wsm_scene_node *node,
		bool xwayland_restack, struct wl_list *outputs); /**< Cleans up disabled state. */
};

/**
 * @brief Base object shared by all scene-node types.
 */
struct wsm_scene_node {
	const struct wsm_scene_node_impl *impl; /**< Node implementation callbacks. */

	struct wsm_scene_tree *parent; /**< Parent scene tree. */
	struct wsm_scene *scene; /**< Owning scene. */

	struct wl_list link; /**< Link in the parent's children list. */

	bool enabled; /**< Whether the node is enabled. */
	int x, y; /**< Position relative to the parent. */

	struct {
		struct wl_signal destroy; /**< Emitted when the node is destroyed. */
	} events; /**< Node events. */

	void *data; /**< User data associated with the node. */

	struct wlr_addon_set addons; /**< Addons attached to the node. */

	struct {
		pixman_region32_t visible; /**< Cached visible region. */
	} WLR_PRIVATE; /**< Private node state. */

	struct wl_list children; /**< Child nodes. */
};

/**
 * @brief Temporary state used by point-hit testing.
 */
struct node_at_data {
	double lx, ly; /**< Layout coordinates. */
	double rx, ry; /**< Relative coordinates. */
	struct wsm_scene_node *node; /**< Node found during traversal. */
};

/**
 * @brief Initializes a scene node.
 * @param node Node to initialize.
 * @param impl Node implementation callbacks.
 * @param parent Parent scene tree.
 */
void wsm_scene_node_init(struct wsm_scene_node *node,
	const struct wsm_scene_node_impl *impl, struct wsm_scene_tree *parent);

/**
 * @brief Destroys a scene node.
 * @param node Node to destroy.
 */
void wsm_scene_node_destroy(struct wsm_scene_node *node);

/**
 * @brief Enables or disables a scene node.
 * @param node Node to update.
 * @param enabled New enabled state.
 */
void wsm_scene_node_set_enabled(struct wsm_scene_node *node, bool enabled);

/**
 * @brief Sets a node's position relative to its parent.
 * @param node Node to update.
 * @param x New x coordinate.
 * @param y New y coordinate.
 */
void wsm_scene_node_set_position(struct wsm_scene_node *node, int x, int y);

/**
 * @brief Places a node immediately above a sibling.
 * @param node Node to move.
 * @param sibling Sibling that will be below the node.
 */
void wsm_scene_node_place_above(struct wsm_scene_node *node,
	struct wsm_scene_node *sibling);

/**
 * @brief Places a node immediately below a sibling.
 * @param node Node to move.
 * @param sibling Sibling that will be above the node.
 */
void wsm_scene_node_place_below(struct wsm_scene_node *node,
	struct wsm_scene_node *sibling);

/**
 * @brief Raises a node to the top of its parent's child list.
 * @param node Node to move.
 */
void wsm_scene_node_raise_to_top(struct wsm_scene_node *node);

/**
 * @brief Lowers a node to the bottom of its parent's child list.
 * @param node Node to move.
 */
void wsm_scene_node_lower_to_bottom(struct wsm_scene_node *node);

/**
 * @brief Reparents a scene node.
 * @param node Node to move.
 * @param new_parent New parent scene tree.
 */
void wsm_scene_node_reparent(struct wsm_scene_node *node,
	struct wsm_scene_tree *new_parent);

/**
 * @brief Computes the node bounds at a position.
 * @param node Node to query.
 * @param x Node x coordinate.
 * @param y Node y coordinate.
 * @param visible Destination region for the bounds.
 */
void wsm_scene_node_bounds(struct wsm_scene_node *node,
	int x, int y, pixman_region32_t *visible);

/**
 * @brief Gets a node's dimensions.
 * @param node Node to query.
 * @param width Destination for the width.
 * @param height Destination for the height.
 */
void wsm_scene_node_get_size(struct wsm_scene_node *node,
	int *width, int *height);

/**
 * @brief Converts node-local coordinates to layout coordinates.
 * @param node Node to query.
 * @param lx_ptr Destination for the layout x coordinate.
 * @param ly_ptr Destination for the layout y coordinate.
 * @return true on success, false when the node is not attached.
 */
bool wsm_scene_node_coords(struct wsm_scene_node *node, int *lx_ptr, int *ly_ptr);

/**
 * @brief Finds the deepest node at a layout coordinate.
 * @param node Root node for the search.
 * @param lx Layout x coordinate.
 * @param ly Layout y coordinate.
 * @param nx Destination for the node-local x coordinate.
 * @param ny Destination for the node-local y coordinate.
 * @return Matching node, or NULL when no node contains the point.
 */
struct wsm_scene_node *wsm_scene_node_at(struct wsm_scene_node *node,
	double lx, double ly, double *nx, double *ny);

/**
 * @brief Visits nodes intersecting a box.
 * @param node Root node for the traversal.
 * @param box Box to intersect.
 * @param iterator Callback invoked for matching nodes.
 * @param user_data User data passed to the callback.
 * @return true when traversal completed, false when stopped by the callback.
 */
bool wsm_scene_node_nodes_in_box(struct wsm_scene_node *node, struct wlr_box *box,
	scene_node_box_iterator_func_t iterator, void *user_data);

/**
 * @brief Computes the opaque region of a node.
 * @param node Node to query.
 * @param x Node x coordinate.
 * @param y Node y coordinate.
 * @param opaque Destination opaque region.
 */
void wsm_scene_node_opaque_region(struct wsm_scene_node *node, int x, int y,
	pixman_region32_t *opaque);

/**
 * @brief Updates the outputs associated with a node.
 * @param node Node to update.
 * @param outputs Scene output list.
 * @param ignore Output to ignore.
 * @param force Output to force into the node's set.
 */
void wsm_scene_node_update_outputs(struct wsm_scene_node *node,
	struct wl_list *outputs, struct wsm_scene_output *ignore,
	struct wsm_scene_output *force);

/**
 * @brief Propagates damage through a node.
 * @param node Node to update.
 * @param damage Damage region to update.
 */
void wsm_scene_node_update(struct wsm_scene_node *node,
	pixman_region32_t *damage);

/**
 * @brief Computes the visible region of a node.
 * @param node Node to query.
 * @param visible Destination visible region.
 */
void wsm_scene_node_visibility(struct wsm_scene_node *node,
	pixman_region32_t *visible);

/**
 * @brief Sends frame completion to a node.
 * @param node Node receiving the completion.
 * @param scene_output Output being presented.
 * @param now Presentation timestamp.
 */
void wsm_scene_node_send_frame_done(struct wsm_scene_node *node,
	struct wsm_scene_output *scene_output, struct timespec *now);

/**
 * @brief Checks whether a node is invisible.
 * @param node Node to inspect.
 * @return true when the node is invisible, false otherwise.
 */
bool wsm_scene_node_invisible(struct wsm_scene_node *node);

/**
 * @brief Adds a node to a render list during traversal.
 * @param node Node being visited.
 * @param lx Layout x coordinate.
 * @param ly Layout y coordinate.
 * @param _data Render-list construction data.
 * @return true to continue traversal, false to stop.
 */
bool wsm_scene_node_construct_render_list_iterator(struct wsm_scene_node *node,
	int lx, int ly, void *_data);

/**
 * @brief Renders a render-list entry.
 * @param entry Render-list entry to render.
 * @param data Render state for the output.
 */
void wsm_scene_node_render(struct wsm_render_list_entry *entry, const struct wsm_render_data *data);

/**
 * @brief Updates DMA-BUF feedback for a render-list entry.
 * @param entry Render-list entry to inspect.
 * @param scene_output Output receiving the feedback.
 */
void wsm_scene_node_dmabuf_feedback(struct wsm_render_list_entry *entry,
	struct wsm_scene_output *scene_output);

/**
 * @brief Restacks an Xwayland surface below its peers.
 * @param node Node whose Xwayland surface is restacked.
 */
void wsm_scene_node_restack_xwayland_surface_below(struct wsm_scene_node *node);

/**
 * @brief Gets the extents of a node at a position.
 * @param node Node to query.
 * @param lx Node x coordinate.
 * @param ly Node y coordinate.
 * @param x_min Destination minimum x coordinate.
 * @param y_min Destination minimum y coordinate.
 * @param x_max Destination maximum x coordinate.
 * @param y_max Destination maximum y coordinate.
 */
void wsm_scene_node_get_extents(struct wsm_scene_node *node, int lx, int ly,
	int *x_min, int *y_min, int *x_max, int *y_max);

/**
 * @brief Gets the children list of a scene node.
 * @param node Node to query.
 * @return Children list, or NULL when the node has no child list.
 */
struct wl_list *wsm_scene_node_get_children(struct wsm_scene_node *node);

/**
 * @brief Restacks an Xwayland surface using update state.
 * @param node Node whose surface is restacked.
 * @param box Region used for restacking.
 * @param data Scene update state.
 */
void wsm_scene_node_restack_xwayland_surface(struct wsm_scene_node *node,
	struct wlr_box *box, struct wsm_scene_update_data *data);

/**
 * @brief Cleans up state after a node is disabled.
 * @param node Node being cleaned up.
 * @param xwayland_restack Whether Xwayland restacking is active.
 * @param outputs Scene output list.
 */
void wsm_scene_node_cleanup_when_disabled(struct wsm_scene_node *node,
	bool xwayland_restack, struct wl_list *outputs);

#endif
