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
