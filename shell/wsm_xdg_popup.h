#ifndef WSM_XDG_POPUP_H
#define WSM_XDG_POPUP_H

#include "wsm_view.h"

struct wlr_scene_tree;
struct wlr_xdg_popup;

/**
 * @brief Structure representing an XDG popup in the WSM
 */
struct wsm_xdg_popup {
	struct wl_listener surface_commit; /**< Listener for handling surface commit events */
	struct wl_listener new_popup; /**< Listener for handling new popup events */
	struct wl_listener reposition; /**< Listener for handling reposition events */
	struct wl_listener destroy; /**< Listener for handling destruction events */

	struct wsm_popup_desc desc; /**< Description of the popup associated with this XDG popup */

	struct wsm_view *view; /**< Pointer to the associated WSM view */

	struct wlr_scene_tree *scene_tree; /**< Pointer to the scene tree containing this popup */
	struct wlr_scene_tree *xdg_surface_tree; /**< Pointer to the scene tree for the XDG surface */
	struct wlr_xdg_popup *xdg_popup_wlr; /**< Pointer to the WLR XDG popup instance */
};

/**
 * @brief Creates a new wsm_xdg_popup instance
 * @param wlr_popup Pointer to the WLR XDG popup instance to be associated with the new popup
 * @param view Pointer to the WSM view that the popup will be associated with
 * @param parent Pointer to the parent scene tree in which the popup will be created
 * @return Pointer to the newly created wsm_xdg_popup instance
 */
struct wsm_xdg_popup *wsm_xdg_popup_create(struct wlr_xdg_popup *wlr_popup,
	struct wsm_view *view, struct wlr_scene_tree *parent);

/**
 * @brief Unconstrains the specified wsm_xdg_popup instance
 * @param popup Pointer to the wsm_xdg_popup instance to be unconstrained
 */
void wsm_xdg_popup_unconstrain(struct wsm_xdg_popup *popup);

#endif
