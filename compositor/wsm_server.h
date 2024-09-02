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

/** \file
 *
 *  \brief Service core, manages global resources and initialization.
 *
 */

#ifndef WSM_SERVER_H
#define WSM_SERVER_H

#include "wsm_idle_inhibit_v1.h"

#include "../config.h"
#ifdef HAVE_XWAYLAND
#include "wsm_xwayland.h"
#endif
#include <wayland-server-core.h>

struct wlr_idle;
struct wl_display;
struct wsm_output;
struct wlr_backend;
struct wlr_surface;
struct wlr_renderer;
struct wlr_allocator;
struct wlr_compositor;
struct wlr_presentation;
struct wlr_output_layout;
struct wlr_session_lock_v1;
struct wlr_linux_dmabuf_v1;
struct wlr_xcursor_manager;
struct wlr_idle_notifier_v1;
struct wlr_output_manager_v1;
struct wlr_data_device_manager;
struct wlr_drm_lease_v1_manager;
struct wlr_pointer_constraint_v1;
struct wlr_security_context_manager_v1;
struct wlr_xdg_activation_v1;
struct wlr_content_type_manager_v1;
struct wlr_data_control_manager_v1;
struct wlr_screencopy_manager_v1;
struct wlr_pointer_constraints_v1;
struct wlr_export_dmabuf_manager_v1;
struct wlr_session_lock_manager_v1;
struct wlr_relative_pointer_manager_v1;
struct wlr_foreign_toplevel_manager_v1;
struct wlr_ext_foreign_toplevel_list_v1;
struct wlr_input_method_manager_v2;
struct wlr_text_input_manager_v3;

struct wsm_font;
struct wsm_list;
struct wsm_xdg_shell;
struct wsm_layer_shell;
struct wsm_scene;
struct wsm_cursor;
struct wsm_transaction;
struct wsm_session_lock;
struct wsm_input_manager;
struct wsm_output_manager;
struct wsm_desktop_interface;
struct wsm_xdg_decoration_manager;
struct wsm_server_decoration_manager;

/**
 * @brief server global server object
 */
extern struct wsm_server global_server;

struct wsm_pointer_constraint {
	struct wl_listener set_region;
	struct wl_listener destroy;

	struct wsm_cursor *cursor;
	struct wlr_pointer_constraint_v1 *constraint;
};

struct wsm_server {
#ifdef HAVE_XWAYLAND
	struct wsm_xwayland xwayland;
	struct wl_listener xwayland_surface;
	struct wl_listener xwayland_ready;
#endif

	struct wl_listener pointer_constraint;
	struct wl_listener drm_lease_request;

	struct {
		struct wl_listener new_lock;
		struct wl_listener manager_destroy;

		struct wsm_session_lock *lock;
		struct wlr_session_lock_manager_v1 *manager;
	} session_lock;

	const char *socket;
	struct wl_display *wl_display;
	struct wl_event_loop *wl_event_loop;
	struct wlr_backend *backend;
	struct wlr_session *wlr_session;
	struct wlr_backend *headless_backend;
	struct wlr_renderer *wlr_renderer;
	struct wlr_allocator *wlr_allocator;
	struct wlr_compositor *wlr_compositor;
	struct wlr_relative_pointer_manager_v1 *wlr_relative_pointer_manager;

	struct wlr_data_device_manager *data_device_manager;
	struct wlr_linux_dmabuf_v1 *linux_dmabuf_v1;
	struct wlr_security_context_manager_v1 *security_context_manager_v1;
	struct wlr_idle_notifier_v1 *idle_notifier_v1;
	struct wlr_presentation *presentation;
	struct wlr_ext_foreign_toplevel_list_v1 *foreign_toplevel_list;
	struct wlr_foreign_toplevel_manager_v1 *foreign_toplevel_manager;
	struct wlr_drm_lease_v1_manager *drm_lease_manager;
	struct wlr_content_type_manager_v1 *content_type_manager_v1;
	struct wlr_data_control_manager_v1 *data_control_manager_v1;
	struct wlr_screencopy_manager_v1 *screencopy_manager_v1;
	struct wlr_export_dmabuf_manager_v1 *export_dmabuf_manager_v1;
	struct wlr_xdg_activation_v1 *xdg_activation_v1;
	struct wlr_pointer_constraints_v1 *pointer_constraints;
	struct wlr_input_method_manager_v2 *input_method;
	struct wlr_text_input_manager_v3 *text_input;

#ifdef HAVE_XWAYLAND
	struct wlr_xcursor_manager *xcursor_manager;
#endif

	struct wsm_scene *wsm_scene;
	struct wsm_xdg_shell *wsm_xdg_shell;
	struct wsm_layer_shell *wsm_layer_shell;
	struct wsm_input_manager *wsm_input_manager;
	struct wsm_output_manager *wsm_output_manager;
	struct wsm_server_decoration_manager *wsm_server_decoration_manager;
	struct wsm_xdg_decoration_manager *wsm_xdg_decoration_manager;
	struct wsm_idle_inhibit_manager_v1 wsm_idle_inhibit_manager_v1;
	struct wsm_desktop_interface *desktop_interface;

	size_t txn_timeout_ms;
	struct wsm_transaction *queued_transaction;
	struct wsm_transaction *pending_transaction;
	struct wsm_list *dirty_nodes;
	struct wl_event_source *delayed_modeset;
	bool xwayland_enabled;
};

bool wsm_server_init(struct wsm_server *server);
void server_finish(struct wsm_server *server);

#endif
