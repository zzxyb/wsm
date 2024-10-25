#ifndef WSM_XDG_POPUP_H
#define WSM_XDG_POPUP_H

#include "wsm_view.h"

struct wlr_scene_tree;
struct wlr_xdg_popup;

struct wsm_xdg_popup {
	struct wl_listener surface_commit;
	struct wl_listener new_popup;
	struct wl_listener reposition;
	struct wl_listener destroy;

	struct wsm_popup_desc desc;

	struct wsm_view *view;

	struct wlr_scene_tree *scene_tree;
	struct wlr_scene_tree *xdg_surface_tree;
	struct wlr_xdg_popup *xdg_popup_wlr;
};

struct wsm_xdg_popup *wsm_xdg_popup_create(struct wlr_xdg_popup *wlr_popup,
	struct wsm_view *view, struct wlr_scene_tree *parent);
void wsm_xdg_popup_unconstrain(struct wsm_xdg_popup *popup);

#endif
