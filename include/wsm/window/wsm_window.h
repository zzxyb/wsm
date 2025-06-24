#ifndef WINDOW_WSM_WINDOW_H
#define WINDOW_WSM_WINDOW_H

#include "config.h"

#include <stdint.h>

#include <wayland-server-core.h>

#include <wlr/types/wlr_box.h>

enum wsm_window_type {
	WSM_WINDOW_XDG_SHELL, /**< XDG shell view type */
	WSM_WINDOW_LAYER_SHELL, /**< Layer shell view type */
	WSM_DR_WINDOW, 		 /**< Direct Render window type */
	WSM_WINDOW_XWAYLAND, /**< XWayland view type */
};

enum wsm_window_prop {
	WINDOW_PROP_TITLE,
	WINDOW_PROP_APP_ID,
	WINDOW_PROP_CLASS,
	WINDOW_PROP_INSTANCE,
	WINDOW_PROP_WINDOW_TYPE,
	WINDOW_PROP_WINDOW_ROLE,
	WINDOW_PROP_X11_WINDOW_ID,
	WINDOW_PROP_X11_PARENT_ID,
};

struct wsm_window;

struct wsm_window_impl {
	uint32_t (*configure)(struct wsm_window *window, double lx, double ly,
		int width, int height);
	const char *(*get_string_prop)(struct wsm_window *window,
		enum wsm_window_prop prop);
	uint32_t (*get_int_prop)(struct wsm_window *window, enum wsm_window_prop prop);
	void (*set_activated)(struct wsm_window *window, bool activated);
	void (*set_tiled)(struct wsm_window *window, bool tiled);
	void (*set_fullscreen)(struct wsm_window *window, bool fullscreen);
	void (*set_resizing)(struct wsm_window *window, bool resizing);
	void (*maximize)(struct wsm_window *window, bool maximize);
	void (*minimize)(struct wsm_window *window, bool minimize);
	void (*close)(struct wsm_window *window);
	void (*close_popups)(struct wsm_window *window);
	void (*destroy)(struct wsm_window *window);
};

struct wsm_window {
	struct wsm_window_impl *impl; /**< Implementation specific data */

	struct wl_listener foreign_activate_request;
	struct wl_listener foreign_fullscreen_request;
	struct wl_listener foreign_close_request;
	struct wl_listener foreign_destroy;

	struct wlr_surface *surface;

	struct wlr_ext_foreign_toplevel_handle_v1 *ext_foreign_toplevel;
	struct wlr_foreign_toplevel_handle_v1 *foreign_toplevel;

	char *title_format;
	char *app_id;
	char *app_icon_path;

	struct wlr_box geometry;

	pid_t pid;
	enum wsm_window_type type;
};

bool wsm_window_init(struct wsm_window *window, enum wsm_window_type type,
	const struct wsm_window_impl *impl);
void wsm_window_finish(struct wsm_window *window);
const char *wsm_window_get_title(const struct wsm_window *window);
const char *wsm_window_get_app_id(const struct wsm_window *window);
const char *wsm_window_get_window_role(const struct wsm_window *window);
uint32_t wsm_window_get_window_type(const struct wsm_window *window);

#endif // WINDOW_WSM_WINDOW_H
