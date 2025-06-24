#ifndef SHELL_WSM_XDG_POPUP_H
#define SHELL_WSM_XDG_POPUP_H

#include "wsm/shell/wsm_popup.h"

#include <wayland-server-core.h>

struct wsm_window;

struct wsm_xdg_popup {
	struct wl_listener surface_commit;
	struct wl_listener new_popup;
	struct wl_listener reposition;
	struct wl_listener destroy;

	struct wsm_popup_description desc;

	struct wlr_scene_tree *scene_tree;
	struct wlr_scene_tree *xdg_surface_tree;
	struct wlr_xdg_popup *base;
};

struct wsm_xdg_popup *wsm_xdg_popup_create(struct wlr_xdg_popup *wlr_popup,
	struct wsm_window *window, struct wlr_scene_tree *parent);
void wsm_xdg_popup_unconstrain(struct wsm_xdg_popup *popup);

#endif // SHELL_WSM_XDG_POPUP_H
