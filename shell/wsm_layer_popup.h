#ifndef WSM_LAYER_POPUP_H
#define WSM_LAYER_POPUP_H

#include <wayland-server-core.h>

struct wlr_xdg_popup;
struct wlr_scene_tree;
struct wsm_layer_surface;

struct wsm_layer_popup {
	struct wl_listener destroy;
	struct wl_listener new_popup;
	struct wl_listener commit;

	struct wlr_xdg_popup *wlr_popup;
	struct wlr_scene_tree *scene;
	struct wsm_layer_surface *toplevel;
};

struct wsm_layer_popup *wsm_layer_popup_create(struct wlr_xdg_popup *wlr_popup,
	struct wsm_layer_surface *toplevel, struct wlr_scene_tree *parent);
void wsm_layer_popup_unconstrain(struct wsm_layer_popup *popup);

#endif
