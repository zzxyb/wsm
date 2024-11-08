#ifndef WSM_XWAYLAND_H
#define WSM_XWAYLAND_H

#include "../config.h"

#include <stdbool.h>

#ifdef HAVE_XWAYLAND

#include <xcb/xproto.h>

struct wlr_xwayland;
struct wlr_xcursor_manager;
struct wlr_xwayland_surface;

struct wsm_server;
struct wsm_view;
struct wsm_xwayland_view;

/**
 * @brief Enum for various window types and states used in X11
 */
enum atom_name {
	NET_WM_WINDOW_TYPE_NORMAL,        /**< Normal window type */
	NET_WM_WINDOW_TYPE_DIALOG,        /**< Dialog window type */
	NET_WM_WINDOW_TYPE_UTILITY,       /**< Utility window type */
	NET_WM_WINDOW_TYPE_TOOLBAR,       /**< Toolbar window type */
	NET_WM_WINDOW_TYPE_SPLASH,        /**< Splash window type */
	NET_WM_WINDOW_TYPE_MENU,          /**< Menu window type */
	NET_WM_WINDOW_TYPE_DROPDOWN_MENU, /**< Dropdown menu type */
	NET_WM_WINDOW_TYPE_POPUP_MENU,    /**< Popup menu type */
	NET_WM_WINDOW_TYPE_TOOLTIP,       /**< Tooltip window type */
	NET_WM_WINDOW_TYPE_NOTIFICATION,  /**< Notification window type */
	NET_WM_STATE_MODAL,               /**< Modal state */
	NET_WM_ICON,                      /**< Icon property */
	NET_WM_WINDOW_OPACITY,            /**< Window opacity property */
	GTK_APPLICATION_ID,               /**< GTK application ID */
	UTF8_STRING,                      /**< UTF-8 string type */
	ATOM_LAST,                        /**< Last atom for array sizing */
};

/**
 * @brief Structure representing the XWayland integration in WSM
 */
struct wsm_xwayland {
	xcb_atom_t atoms[ATOM_LAST];                /**< Array of X11 atoms */
	struct wlr_xwayland *xwayland_wlr;         /**< Pointer to the WLR XWayland instance */
	struct wlr_xcursor_manager *xcursor_manager; /**< Pointer to the cursor manager */

	xcb_connection_t *xcb_conn;                /**< X11 connection for communication */
};

/**
 * @brief Starts the XWayland server
 * @param server Pointer to the WSM server
 * @return true if the server started successfully, false otherwise
 */
bool xwayland_start(struct wsm_server *server);

/**
 * @brief Retrieves a WSM view from a WLR XWayland surface
 * @param xsurface Pointer to the XWayland surface
 * @return Pointer to the corresponding WSM view
 */
struct wsm_view *view_from_wlr_xwayland_surface(
	struct wlr_xwayland_surface *xsurface);

#endif

#endif
