#ifndef WSM_LAYER_SHELL_H
#define WSM_LAYER_SHELL_H

#include "wsm_view.h"

#include <wlr/util/box.h>
#include <wlr/types/wlr_layer_shell_v1.h>

#include <wayland-server-core.h>

struct wlr_subsurface;
struct wlr_xdg_popup;
struct wlr_layer_shell_v1;
struct wlr_scene_layer_surface_v1;

struct wsm_seat;
struct wsm_output;
struct wsm_server;

/**
 * @brief Enum for the types of layer parents
 */
enum layer_parent_type {
	LAYER_PARENT_LAYER, /**< Indicates a layer parent type */
	LAYER_PARENT_POPUP, /**< Indicates a popup parent type */
};

/**
 * @brief Structure representing a layer surface in the WSM
 */
struct wsm_layer_surface {
	struct wl_listener map; /**< Listener for handling map events */
	struct wl_listener unmap; /**< Listener for handling unmap events */
	struct wl_listener surface_commit; /**< Listener for handling surface commit events */
	struct wl_listener output_destroy; /**< Listener for handling output destruction events */
	struct wl_listener node_destroy; /**< Listener for handling node destruction events */
	struct wl_listener new_popup; /**< Listener for handling new popup events */

	struct wsm_popup_desc desc; /**< Description of the popup associated with this layer surface */

	struct wlr_scene_tree *popups; /**< Pointer to the scene tree containing popups */

	struct wsm_output *output; /**< Pointer to the associated WSM output */
	struct wlr_scene_layer_surface_v1 *scene; /**< Pointer to the WLR scene layer surface */
	struct wlr_scene_tree *tree; /**< Pointer to the scene tree for this layer surface */
	struct wlr_layer_surface_v1 *layer_surface_wlr; /**< Pointer to the WLR layer surface instance */

	bool mapped; /**< Indicates whether the layer surface is currently mapped */
};

/**
 * @brief Structure representing the layer shell in the WSM
 */
struct wsm_layer_shell {
	struct wl_listener layer_shell_surface; /**< Listener for handling layer shell surface events */
	struct wlr_layer_shell_v1 *wlr_layer_shell; /**< Pointer to the WLR layer shell instance */
};

/**
 * @brief Creates a new wsm_layer_shell instance
 * @param server Pointer to the WSM server that will manage the layer shell
 * @return Pointer to the newly created wsm_layer_shell instance
 */
struct wsm_layer_shell *wsm_layer_shell_create(const struct wsm_server *server);

/**
 * @brief Destroys the specified wsm_layer_shell instance
 * @param shell Pointer to the wsm_layer_shell instance to be destroyed
 */
void wsm_layer_shell_destroy(struct wsm_layer_shell *shell);

/**
 * @brief Retrieves the toplevel layer surface from a given WLR surface
 * @param surface Pointer to the WLR surface from which to retrieve the toplevel layer surface
 * @return Pointer to the corresponding toplevel layer surface, or NULL if not found
 */
struct wlr_layer_surface_v1 *toplevel_layer_surface_from_surface(
	struct wlr_surface *surface);

#endif
