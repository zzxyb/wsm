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

#ifndef WSM_VIEW_ITEM_H
#define WSM_VIEW_ITEM_H

#include "../config.h"

#include <wayland-server-core.h>

struct wlr_surface;
struct wlr_xdg_toplevel;
struct wlr_xwayland_surface;

enum wsm_view_type {
    WSM_LAYER_XDG_SHELL,
    WSM_VIEW_XDG_SHELL,
#ifdef HAVE_XWAYLAND
    WSM_VIEW_XWAYLAND,
#endif
};

enum wsm_view_prop {
    VIEW_PROP_TITLE,
    VIEW_PROP_APP_ID,
    VIEW_PROP_CLASS,
    VIEW_PROP_INSTANCE,
    VIEW_PROP_WINDOW_TYPE,
    VIEW_PROP_WINDOW_ROLE,
#ifdef HAVE_XWAYLAND
    VIEW_PROP_X11_WINDOW_ID,
    VIEW_PROP_X11_PARENT_ID,
#endif
};

struct wsm_view_item {
    enum wsm_view_type type;
    struct wlr_surface *surface;

    union {
        struct wlr_xdg_toplevel *wlr_xdg_toplevel;
#ifdef HAVE_XWAYLAND
        struct wlr_xwayland_surface *wlr_xwayland_surface;
#endif
    };

    struct {
        struct wl_signal unmap;
    } events;

    struct wl_listener surface_new_subsurface;
};

#endif
