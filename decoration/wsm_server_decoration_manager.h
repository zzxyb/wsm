#ifndef WSM_DECORATION_MANAGER_H
#define WSM_DECORATION_MANAGER_H

#include <wayland-server-core.h>

struct wsm_server;
struct wlr_server_decoration_manager;

struct wsm_server_decoration_manager {
	struct wl_listener server_decoration;

	struct wl_list decorations;

	struct wlr_server_decoration_manager *server_decoration_manager_wlr;
};

void decoration_manager_init();
struct wsm_server_decoration_manager *wsm_server_decoration_manager_create(const struct wsm_server* server);

#endif
