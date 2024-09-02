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

#ifndef WSM_XWAYLAND_H
#define WSM_XWAYLAND_H

#include "../config.h"

#include <stdbool.h>

#ifdef HAVE_XWAYLAND

#include <xcb/xproto.h>

struct wlr_xwayland_surface;

struct wsm_server;
struct wsm_view;
struct wsm_xwayland_view;

enum atom_name {
	NET_WM_WINDOW_TYPE_NORMAL,
	NET_WM_WINDOW_TYPE_DIALOG,
	NET_WM_WINDOW_TYPE_UTILITY,
	NET_WM_WINDOW_TYPE_TOOLBAR,
	NET_WM_WINDOW_TYPE_SPLASH,
	NET_WM_WINDOW_TYPE_MENU,
	NET_WM_WINDOW_TYPE_DROPDOWN_MENU,
	NET_WM_WINDOW_TYPE_POPUP_MENU,
	NET_WM_WINDOW_TYPE_TOOLTIP,
	NET_WM_WINDOW_TYPE_NOTIFICATION,
	NET_WM_STATE_MODAL,
	NET_WM_ICON,
	NET_WM_WINDOW_OPACITY,
	GTK_APPLICATION_ID,
	UTF8_STRING,
	ATOM_LAST,
};

struct wsm_xwayland {
	xcb_atom_t atoms[ATOM_LAST];
	struct wlr_xwayland *wlr_xwayland;
	struct wlr_xcursor_manager *xcursor_manager;

	xcb_connection_t *xcb_conn;
};

bool xwayland_start(struct wsm_server *server);
struct wsm_view *view_from_wlr_xwayland_surface(
	struct wlr_xwayland_surface *xsurface);
const char *get_xwayland_surface_app_id(xcb_get_property_reply_t *reply);

#endif

#endif
