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

#endif

#endif
