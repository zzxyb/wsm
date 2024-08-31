/*
MIT License

Copyright (c) 2024 YaoBing Xiao

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

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

enum layer_parent_type {
	LAYER_PARENT_LAYER,
	LAYER_PARENT_POPUP,
};

struct wsm_layer_surface {
	struct wl_listener map;
	struct wl_listener unmap;
	struct wl_listener surface_commit;
	struct wl_listener output_destroy;
	struct wl_listener node_destroy;
	struct wl_listener new_popup;
	
	bool mapped;
	
	struct wlr_scene_tree *popups;
	struct wsm_popup_desc desc;
	
	struct wsm_output *output;
	struct wlr_scene_layer_surface_v1 *scene;
	struct wlr_scene_tree *tree;
	struct wlr_layer_surface_v1 *layer_surface;
};

struct wsm_layer_shell {
	struct wlr_layer_shell_v1 *wlr_layer_shell;
	struct wl_listener layer_shell_surface;
};

struct wsm_layer_shell *wsm_layer_shell_create(const struct wsm_server *server);
void wsm_layer_shell_destroy(struct wsm_layer_shell *shell);
struct wlr_layer_surface_v1 *toplevel_layer_surface_from_surface(
		struct wlr_surface *surface);

#endif
