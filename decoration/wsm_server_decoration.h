#ifndef WSM_DECORATION_H
#define WSM_DECORATION_H

#include <wayland-server-core.h>

struct wlr_surface;
struct wlr_server_decoration;

struct wsm_server_decoration {
	struct wl_listener destroy;
	struct wl_listener mode;

	struct wl_list link;

	struct wlr_server_decoration *server_decoration_wlr;
};

void handle_server_decoration(struct wl_listener *listener, void *data);
struct wsm_server_decoration *decoration_from_surface(
	struct wlr_surface *surface);

#endif
