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

/** \file
 *
 *  \brief Service core, manages global resources and initialization.
 *
 */

#ifndef WSM_XDG_DECORATION_H
#define WSM_XDG_DECORATION_H

#include <wayland-server-core.h>

struct wlr_surface;
struct wlr_xdg_toplevel_decoration_v1;

struct wsm_view;

struct wsm_xdg_decoration {
	struct wl_listener destroy;
	struct wl_listener request_mode;

	struct wl_list link;

	struct wlr_xdg_toplevel_decoration_v1 *wlr_xdg_decoration;

	struct wsm_view *view;
};

void handle_xdg_decoration(struct wl_listener *listener, void *data);
struct wsm_xdg_decoration *wsm_server_decoration_from_surface(
	struct wlr_surface *surface);
void set_xdg_decoration_mode(struct wsm_xdg_decoration *deco);

#endif
