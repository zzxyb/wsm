#ifndef SHELL_WSM_XDG_SHELL_H
#define SHELL_WSM_XDG_SHELL_H

#include <wayland-server-core.h>

struct wsm_server;
struct wlr_xdg_shell;
struct wlr_xdg_surface;
struct wlr_xdg_activation_v1;

struct wsm_xdg_shell {
	struct wl_listener xdg_activation_request;
	struct wl_listener xdg_shell_toplevel;
	struct wl_listener xdg_activation_v1_request_activate;
	struct wl_listener xdg_activation_v1_new_token;

	struct wlr_xdg_shell *base;
	struct wlr_xdg_activation_v1 *xdg_activation_v1;
};

struct wsm_xdg_shell *wsm_xdg_shell_create(const struct wsm_server* server);
void wsm_xdg_shell_destroy(struct wsm_xdg_shell *shell);
struct wsm_view *view_from_wlr_xdg_surface(
	struct wlr_xdg_surface *xdg_surface);

#endif // SHELL_WSM_XDG_SHELL_H
