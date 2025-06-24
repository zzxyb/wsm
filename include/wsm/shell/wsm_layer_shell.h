#ifndef SHELL_WSM_LAYER_SHELL_H
#define SHELL_WSM_LAYER_SHELL_H

#include "wsm/shell/wsm_popup.h"

#include <stdbool.h>

#include <wayland-server-core.h>

struct wlr_scene_tree;
struct wlr_scene_layer_surface_v1;
struct wlr_scene_tree;
struct wlr_layer_surface_v1;

struct wsm_outpu;

struct wsm_layer_surface {
	struct wl_listener map;
	struct wl_listener unmap;
	struct wl_listener surface_commit;
	struct wl_listener output_destroy;
	struct wl_listener node_destroy;
	struct wl_listener new_popup;

	struct wsm_popup_description desc;
	struct wlr_scene_tree *popups;
	struct wsm_output *output;
	struct wlr_scene_layer_surface_v1 *scene;
	struct wlr_scene_tree *tree;
	struct wlr_layer_surface_v1 *base;

	bool mapped;
};

#endif // SHELL_WSM_LAYER_SHELL_H
