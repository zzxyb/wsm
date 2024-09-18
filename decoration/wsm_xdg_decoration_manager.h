#ifndef WSM_XDG_DECORATION_MANAGER_H
#define WSM_XDG_DECORATION_MANAGER_H

#include <wayland-server-core.h>

struct wlr_xdg_decoration_manager_v1;

struct wsm_server;

struct wsm_xdg_decoration_manager {
	struct wl_listener xdg_decoration;
	struct wl_list xdg_decorations;
	struct wlr_xdg_decoration_manager_v1 *wlr_xdg_decoration_manager;
};

struct wsm_xdg_decoration_manager *xdg_decoration_manager_create(const struct wsm_server* server);

#endif
