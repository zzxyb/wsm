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

#ifndef WSM_XWAYLAND_UNMANAGED_H
#define WSM_XWAYLAND_UNMANAGED_H

#include "../config.h"

#include <wayland-server-core.h>

#ifdef HAVE_XWAYLAND

struct wlr_scene_node;
struct wlr_xwayland_surface;

struct wsm_xwayland_unmanaged {
    struct wlr_xwayland_surface *xwayland_surface;
    struct wlr_scene_node *node;
    struct wl_list link;

    int lx, ly;

    struct wl_listener request_activate;
    struct wl_listener request_configure;
    struct wl_listener request_fullscreen;
    struct wl_listener commit;
    struct wl_listener set_geometry;
    struct wl_listener associate;
    struct wl_listener dissociate;
    struct wl_listener map;
    struct wl_listener unmap;
    struct wl_listener destroy;
    struct wl_listener override_redirect;
};

struct wsm_xwayland_unmanaged *wsm_xwayland_unmanaged_create(struct wlr_xwayland_surface *xsurface, bool mapped);
void destory_wsm_xwayland_unmanaged(struct wsm_xwayland_unmanaged *xwayland_unmanaged);

#endif

#endif
