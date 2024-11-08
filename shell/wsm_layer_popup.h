#ifndef WSM_LAYER_POPUP_H
#define WSM_LAYER_POPUP_H

#include <wayland-server-core.h>

struct wlr_xdg_popup;
struct wlr_scene_tree;
struct wsm_layer_surface;

/**
 * @brief Structure representing a layer popup in the WSM
 */
struct wsm_layer_popup {
	struct wl_listener destroy; /**< Listener for handling destruction events of the popup */
	struct wl_listener new_popup; /**< Listener for handling new popup events */
	struct wl_listener commit; /**< Listener for handling commit events of the popup */

	struct wlr_xdg_popup *xdg_popup; /**< Pointer to the associated WLR XDG popup instance */
	struct wlr_scene_tree *scene; /**< Pointer to the scene tree containing this layer popup */
	struct wsm_layer_surface *toplevel; /**< Pointer to the associated top-level layer surface */
};

/**
 * @brief Creates a new wsm_layer_popup instance
 * @param wlr_popup Pointer to the WLR XDG popup instance to be associated with the new layer popup
 * @param toplevel Pointer to the top-level layer surface associated with the new layer popup
 * @param parent Pointer to the parent scene tree in which the layer popup will be created
 * @return Pointer to the newly created wsm_layer_popup instance
 */
struct wsm_layer_popup *wsm_layer_popup_create(struct wlr_xdg_popup *wlr_popup,
	struct wsm_layer_surface *toplevel, struct wlr_scene_tree *parent);

/**
 * @brief Unconstrains the specified wsm_layer_popup instance
 * @param popup Pointer to the wsm_layer_popup instance to be unconstrained
 */
void wsm_layer_popup_unconstrain(struct wsm_layer_popup *popup);

#endif
