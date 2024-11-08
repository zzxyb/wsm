#ifndef WSM_XDG_SHELL_H
#define WSM_XDG_SHELL_H

#include <wayland-server-core.h>

struct wlr_xdg_shell;
struct wlr_xdg_surface;
struct wlr_xdg_toplevel;
struct wlr_xdg_activation_v1;

struct wsm_view;
struct wsm_server;
struct wsm_xdg_shell_view;

struct wsm_xdg_shell {
	struct wl_listener xdg_activation_request;
	struct wl_listener xdg_shell_toplevel;
	struct wl_listener xdg_activation_v1_request_activate;
	struct wl_listener xdg_activation_v1_new_token;

	struct wlr_xdg_shell *xdg_shell_wlr;
	struct wlr_xdg_activation_v1 *xdg_activation_v1;
};

/**
 * @brief Creates a new wsm_xdg_shell instance
 * @param server Pointer to the WSM server
 * @return Pointer to the newly created wsm_xdg_shell instance
 */
struct wsm_xdg_shell *wsm_xdg_shell_create(const struct wsm_server* server);

/**
 * @brief Destroys the specified wsm_xdg_shell instance
 * @param shell Pointer to the wsm_xdg_shell instance to be destroyed
 */
void wsm_xdg_shell_destroy(struct wsm_xdg_shell *shell);

/**
 * @brief Retrieves a WSM view from a WLR XDG surface
 * @param xdg_surface Pointer to the XDG surface
 * @return Pointer to the corresponding WSM view
 */
struct wsm_view *view_from_wlr_xdg_surface(
	struct wlr_xdg_surface *xdg_surface);

#endif
