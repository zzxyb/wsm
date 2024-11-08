#ifndef WSM_SCENE_H
#define WSM_SCENE_H

#include "../config.h"
#include "node/wsm_node.h"

#include <stdbool.h>

struct wlr_scene;
struct wlr_scene_tree;
struct wlr_scene_output;
struct wlr_output_state;
struct wlr_output_layout;
struct wlr_scene_output_layout;
struct wlr_scene_output_state_options;

struct wsm_server;

/**
 * @brief Scene render control.
 *
 * @details The order in which the scene-trees below are created determines the
 * z-order for nodes that cover the whole work area. This is applicable for per-output scene-trees.
 *
 * | Type              | isWindow          | Scene Tree       | Per Output | Example
 * | ----------------- | ----------------- | ---------------- | ---------- | -------------------------------
 * | server-node       | No(display node)  | black-screen     | Yes        | Black screen transition
 * | server-node       | No(display node)  | screen-water-mark| Yes        | Show employee number information
 * | server-node       | No(display node)  | drag display node| No         | Show drag icon
 * | layer-shell       | No(display node)  | osd_overlay_layer| No         | Screenshot preview window
 * | xdg-popups        | Yes               | xdg-popups(layer)| Yes        | Server decides layer surface popup (wlr_xdg_popup)
 * | layer-shell       | Yes               | notify-osd       | Yes        | Notify bubble or OSD
 * | layer-shell       | Yes               | overlay-layer    | Yes        | Lock screen
 * | xdg-popups        | Yes               | xdg-popups       | No         | Server decides (wlr_xdg_popup)
 * | xwayland-OR       | Yes               | unmanaged        | No         | Like Qt::X11BypassWindowManagerHint, override_redirect appear above
 * | layer-shell       | Yes               | top-layer        | Yes        | Launcher, dock, fullscreen window
 * | toplevel windows   | Yes               | always-on-top    | No         | X or Wayland, like Qt::WindowStaysOnTopHint
 * | toplevel windows   | Yes               | normal           | No         | X or Wayland normal window
 * | toplevel windows   | Yes               | always-on-bottom | No         | X or Wayland, like Qt::WindowStaysOnBottomHint
 * | layer-shell       | Yes               | bottom-layer     | Yes        | Placeholder for custom desktop widget
 * | layer-shell       | Yes               | background-layer | Yes        | Desktop
 */
struct wsm_scene {
	struct {
		struct wlr_scene_tree *shell_background; /**< Scene tree for the background layer */
		struct wlr_scene_tree *shell_bottom; /**< Scene tree for the bottom layer */
		struct wlr_scene_tree *tiling; /**< Scene tree for tiled windows */
		struct wlr_scene_tree *floating; /**< Scene tree for floating windows */
		struct wlr_scene_tree *shell_top; /**< Scene tree for the top layer */
		struct wlr_scene_tree *fullscreen; /**< Scene tree for fullscreen windows */
		struct wlr_scene_tree *fullscreen_global; /**< Scene tree for globally fullscreen windows */
#ifdef HAVE_XWAYLAND
		struct wlr_scene_tree *unmanaged; /**< Scene tree for unmanaged windows (if XWayland is enabled) */
#endif
		struct wlr_scene_tree *shell_overlay; /**< Scene tree for overlay layers */
		struct wlr_scene_tree *popup; /**< Scene tree for popups */
		struct wlr_scene_tree *seat; /**< Scene tree for seat management */
		struct wlr_scene_tree *session_lock; /**< Scene tree for session lock */
	} layers;

	struct wsm_node node; /**< Node representing the scene */

	struct {
		struct wl_signal new_node; /**< Signal emitted when a new node is added to the scene */
	} events;

	struct wl_list all_outputs; /**< List of all outputs associated with the scene */

	struct wlr_output_layout *output_layout; /**< Layout for managing output arrangements */

	struct wlr_scene *root_scene; /**< Pointer to the root scene */
	struct wlr_scene_tree *staging; /**< Pointer to the staging scene tree */

	struct wlr_scene_tree *layer_tree; /**< Pointer to the layer scene tree */

	struct wsm_list *outputs; /**< List of outputs associated with the scene */
	struct wsm_list *non_desktop_outputs; /**< List of non-desktop outputs */
	struct wsm_list *scratchpad; /**< List for scratchpad windows */

	struct wsm_output *fallback_output; /**< Pointer to the fallback output */
	struct wsm_container *fullscreen_global; /**< Pointer to the global fullscreen container */

	double x, y; /**< Position coordinates of the scene */
	double width, height; /**< Dimensions of the scene */
};

/**
 * @brief Creates a new wsm_scene instance
 * @param server Pointer to the WSM server that will manage the scene
 * @return Pointer to the newly created wsm_scene instance
 */
struct wsm_scene *wsm_scene_create(const struct wsm_server* server);

/**
 * @brief Commits the output state for the specified scene output
 * @param scene_output Pointer to the wlr_scene_output to be committed
 * @param options Pointer to the options for the scene output state
 * @return true if the commit was successful, false otherwise
 */
bool wsm_scene_output_commit(struct wlr_scene_output *scene_output,
	const struct wlr_scene_output_state_options *options);

/**
 * @brief Builds the output state for the specified scene output
 * @param scene_output Pointer to the wlr_scene_output for which to build the state
 * @param state Pointer to the wlr_output_state to be populated
 * @param options Pointer to the options for the scene output state
 * @return true if the state was built successfully, false otherwise
 */
bool wsm_scene_output_build_state(struct wlr_scene_output *scene_output,
	struct wlr_output_state *state, const struct wlr_scene_output_state_options *options);

/**
 * @brief Retrieves the bounding box of the root scene
 * @param root Pointer to the wsm_scene whose box is to be retrieved
 * @param box Pointer to the wlr_box to be populated with the dimensions
 */
void root_get_box(struct wsm_scene *root, struct wlr_box *box);

/**
 * @brief Shows the scratchpad for the specified container
 * @param con Pointer to the wsm_container whose scratchpad will be shown
 */
void root_scratchpad_show(struct wsm_container *con);

#endif
