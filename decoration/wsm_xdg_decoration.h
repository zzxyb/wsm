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

	struct wlr_xdg_toplevel_decoration_v1 *xdg_decoration_wlr;

	struct wsm_view *view;
};

void handle_xdg_decoration(struct wl_listener *listener, void *data);
struct wsm_xdg_decoration *wsm_server_decoration_from_surface(
	struct wlr_surface *surface);
void set_xdg_decoration_mode(struct wsm_xdg_decoration *deco);

#endif
