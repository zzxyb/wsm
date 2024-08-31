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

#include "wsm_server.h"
#include "wsm_log.h"
#include "wsm_xdg_shell.h"
#include "../config.h"
#include "wsm_scene.h"
#include "wsm_output.h"
#include "wsm_input_manager.h"
#include "wsm_output_manager.h"
#include "wsm_server_decoration_manager.h"
#include "wsm_xdg_decoration_manager.h"
#include "wsm_layer_shell.h"
#include "wsm_output.h"
#include "wsm_list.h"
#include "wsm_seat.h"
#include "wsm_config.h"
#include "wsm_cursor.h"
#include "wsm_session_lock.h"
#include "wsm_desktop.h"

#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include <wayland-util.h>
#include <wayland-server-core.h>

#include <xf86drm.h>

#include <wlr/config.h>
#include <wlr/types/wlr_drm.h>
#include <wlr/backend/multi.h>
#if HAVE_XWAYLAND
#include <wlr/xwayland/shell.h>
#include <wlr/xwayland/xwayland.h>
#include <wlr/xwayland/server.h>
#endif
#include <wlr/types/wlr_scene.h>
#include <wlr/backend/wayland.h>
#include <wlr/render/allocator.h>
#include <wlr/backend/headless.h>
#include <wlr/xwayland/xwayland.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_viewporter.h>
#include <wlr/types/wlr_data_device.h>
#include <wlr/types/wlr_drm_lease_v1.h>
#include <wlr/types/wlr_xdg_output_v1.h>
#include <wlr/types/wlr_screencopy_v1.h>
#include <wlr/types/wlr_subcompositor.h>
#include <wlr/types/wlr_idle_notify_v1.h>
#include <wlr/types/wlr_xdg_foreign_v1.h>
#include <wlr/types/wlr_xdg_foreign_v2.h>
#include <wlr/types/wlr_data_control_v1.h>
#include <wlr/types/wlr_linux_dmabuf_v1.h>
#include <wlr/types/wlr_linux_dmabuf_v1.h>
#include <wlr/types/wlr_export_dmabuf_v1.h>
#include <wlr/types/wlr_xcursor_manager.h>
#include <wlr/types/wlr_presentation_time.h>
#include <wlr/types/wlr_xdg_foreign_registry.h>
#include <wlr/types/wlr_single_pixel_buffer_v1.h>
#include <wlr/types/wlr_security_context_v1.h>
#include <wlr/types/wlr_output_management_v1.h>
#include <wlr/types/wlr_gamma_control_v1.h>
#include <wlr/types/wlr_output_power_management_v1.h>
#include <wlr/types/wlr_content_type_v1.h>
#include <wlr/types/wlr_fractional_scale_v1.h>
#include <wlr/types/wlr_cursor_shape_v1.h>
#include <wlr/types/wlr_xdg_activation_v1.h>
#include <wlr/types/wlr_relative_pointer_v1.h>
#include <wlr/types/wlr_pointer_constraints_v1.h>
#include <wlr/types/wlr_foreign_toplevel_management_v1.h>
#include <wlr/types/wlr_ext_foreign_toplevel_list_v1.h>
#include <wlr/types/wlr_primary_selection_v1.h>
#include <wlr/types/wlr_input_method_v2.h>
#include <wlr/types/wlr_text_input_v3.h>

#define WSM_XDG_SHELL_VERSION 5
#define WSM_LAYER_SHELL_VERSION 4
#define WSM_WLR_FRACTIONAL_SCALE_V1_VERSION 1
#define WSM_FOREIGN_TOPLEVEL_LIST_VERSION 1
#define WINDOW_TITLE "Wsm Compositor"

static void handle_pointer_constraint_set_region(struct wl_listener *listener,
												 void *data) {
	struct wsm_pointer_constraint *wsm_constraint =
			wl_container_of(listener, wsm_constraint, set_region);
	struct wsm_cursor *cursor = wsm_constraint->cursor;
	
	cursor->active_confine_requires_warp = true;
}

void handle_constraint_destroy(struct wl_listener *listener, void *data) {
	struct wsm_pointer_constraint *wsm_constraint =
			wl_container_of(listener, wsm_constraint, destroy);
	struct wlr_pointer_constraint_v1 *constraint = data;
	struct wsm_cursor *cursor = wsm_constraint->cursor;
	
	wl_list_remove(&wsm_constraint->set_region.link);
	wl_list_remove(&wsm_constraint->destroy.link);
	
	if (cursor->active_constraint == constraint) {
		warp_to_constraint_cursor_hint(cursor);
		
		if (cursor->constraint_commit.link.next != NULL) {
			wl_list_remove(&cursor->constraint_commit.link);
		}
		wl_list_init(&cursor->constraint_commit.link);
		cursor->active_constraint = NULL;
	}
	
	free(wsm_constraint);
}

void handle_pointer_constraint(struct wl_listener *listener, void *data) {
	struct wlr_pointer_constraint_v1 *constraint = data;
	struct wsm_seat *seat = constraint->seat->data;
	
	struct wsm_pointer_constraint *wsm_constraint =
			calloc(1, sizeof(struct wsm_pointer_constraint));
	wsm_constraint->cursor = seat->wsm_cursor;
	wsm_constraint->constraint = constraint;
	
	wsm_constraint->set_region.notify = handle_pointer_constraint_set_region;
	wl_signal_add(&constraint->events.set_region, &wsm_constraint->set_region);
	
	wsm_constraint->destroy.notify = handle_constraint_destroy;
	wl_signal_add(&constraint->events.destroy, &wsm_constraint->destroy);
	
	struct wlr_surface *surface = seat->wlr_seat->keyboard_state.focused_surface;
	if (surface && surface == constraint->surface) {
		wsm_cursor_constrain(seat->wsm_cursor, constraint);
	}
}

#if WLR_HAS_DRM_BACKEND
static void handle_drm_lease_request(struct wl_listener *listener, void *data) {
	struct wlr_drm_lease_request_v1 *req = data;
	struct wlr_drm_lease_v1 *lease = wlr_drm_lease_request_v1_grant(req);
	if (!lease) {
		wsm_log(WSM_ERROR, "Failed to grant lease request");
		wlr_drm_lease_request_v1_reject(req);
	}
}
#endif

static bool is_privileged(const struct wl_global *global, const struct wsm_server *server) {
#if WLR_HAS_DRM_BACKEND
	if (server->drm_lease_manager != NULL) {
		struct wlr_drm_lease_device_v1 *drm_lease_dev;
		wl_list_for_each(drm_lease_dev, &server->drm_lease_manager->devices, link) {
			if (drm_lease_dev->global == global) {
				return true;
			}
		}
	}
#endif
	
	return true;
}

static bool filter_global(const struct wl_client *client,
						  const struct wl_global *global, void *data) {
	struct wsm_server *server = data;
#if HAVE_XWAYLAND
	if (global_server.xwayland_enabled) {
		struct wlr_xwayland *xwayland = server->xwayland.wlr_xwayland;
		if (xwayland && global == xwayland->shell_v1->global) {
			return xwayland->server != NULL && client == xwayland->server->client;
		}
	}
#endif
	
		   // Restrict usage of privileged protocols to unsandboxed clients
		   // TODO: add a way for users to configure an allow-list
	const struct wlr_security_context_v1_state *security_context =
			wlr_security_context_manager_v1_lookup_client(
					server->security_context_manager_v1, (struct wl_client *)client);
	
	if (is_privileged(global, server)) {
		return security_context == NULL;
	}
	
	return true;
}

static void detect_proprietary(struct wlr_backend *backend, void *data) {
	int drm_fd = wlr_backend_get_drm_fd(backend);
	if (drm_fd < 0) {
		return;
	}
	
	drmVersion *version = drmGetVersion(drm_fd);
	if (version == NULL) {
		wsm_log(WSM_ERROR, "drmGetVersion() failed");
		return;
	}
	
	bool is_unsupported = false;
	if (strcmp(version->name, "nvidia-drm") == 0) {
		is_unsupported = true;
		wsm_log(WSM_ERROR, "!!! Proprietary Nvidia drivers are in use !!!");
		wsm_log(WSM_ERROR, "Use drivers Nouveau instead");
	}
	
	if (strcmp(version->name, "evdi") == 0) {
		is_unsupported = true;
		wsm_log(WSM_ERROR, "!!! Proprietary DisplayLink drivers are in use !!!");
	}
	
	if (is_unsupported) {
		wsm_log(WSM_ERROR,
				"Proprietary drivers are NOT supported. To launch wsm anyway, "
				"launch with --unsupported-gpu and DO NOT report issues.");
		exit(EXIT_FAILURE);
	}
	
	drmFreeVersion(version);
}

/**
 * @brief wsm_server_init initialize the wayland compositor core
 * @param server
 * @return successed return true
 */
bool wsm_server_init(struct wsm_server *server)
{
	server->desktop_interface = wsm_desktop_interface_create();
	wsm_config_init();
	
	server->wl_display = wl_display_create();
	server->wl_event_loop = wl_display_get_event_loop(server->wl_display);
	
	wl_display_set_global_filter(server->wl_display, filter_global, server);
	
	server->backend = wlr_backend_autocreate(server->wl_event_loop, &server->wlr_session);
	if (server->backend == NULL) {
		wsm_log(WSM_ERROR, "failed to create wlr_backend");
		return false;
	}
	
	wlr_multi_for_each_backend(server->backend, detect_proprietary, NULL);
	
	server->wlr_renderer = wlr_renderer_autocreate(server->backend);
	if (!server->wlr_renderer) {
		wsm_log(WSM_ERROR, "Failed to create renderer");
		return false;
	}
	
	wlr_renderer_init_wl_shm(server->wlr_renderer, server->wl_display);
	
	if (wlr_renderer_get_texture_formats(server->wlr_renderer, WLR_BUFFER_CAP_DMABUF) != NULL) {
		server->linux_dmabuf_v1 = wlr_linux_dmabuf_v1_create_with_renderer(
				server->wl_display, 4, server->wlr_renderer);
		// wlr_drm_create(server->wl_display, server->wlr_renderer);
	}
	
	server->wlr_allocator = wlr_allocator_autocreate(server->backend, server->wlr_renderer);
	if (!server->wlr_allocator) {
		wsm_log(WSM_ERROR, "Failed to create allocator");
		return false;
	}
	
	server->wlr_compositor = wlr_compositor_create(server->wl_display, 6,
												   server->wlr_renderer);
	wlr_subcompositor_create(server->wl_display);
	server->wsm_scene = wsm_scene_create(server);
	
	server->xcursor_manager = wlr_xcursor_manager_create(NULL, 24);
	server->data_device_manager = wlr_data_device_manager_create(server->wl_display);
	server->wsm_output_manager = wsm_output_manager_create(server);
	
	wsm_idle_inhibit_manager_v1_init();
	
	server->wsm_layer_shell = wsm_layer_shell_create(server);
	server->wsm_xdg_shell = wsm_xdg_shell_create(server);
	server->idle_notifier_v1 = wlr_idle_notifier_v1_create(server->wl_display);
	server->wsm_server_decoration_manager = wsm_server_decoration_manager_create(server);
	server->wsm_xdg_decoration_manager = xdg_decoration_manager_create(server);
	server->wlr_relative_pointer_manager =
			wlr_relative_pointer_manager_v1_create(server->wl_display);
	
	server->pointer_constraints =
			wlr_pointer_constraints_v1_create(server->wl_display);
	server->pointer_constraint.notify = handle_pointer_constraint;
	wl_signal_add(&server->pointer_constraints->events.new_constraint,
				  &server->pointer_constraint);
	
	server->presentation = wlr_presentation_create(server->wl_display, server->backend);
	server->input_method = wlr_input_method_manager_v2_create(server->wl_display);
	server->text_input = wlr_text_input_manager_v3_create(server->wl_display);
	server->foreign_toplevel_list =
			wlr_ext_foreign_toplevel_list_v1_create(server->wl_display, WSM_FOREIGN_TOPLEVEL_LIST_VERSION);
	server->foreign_toplevel_manager =
			wlr_foreign_toplevel_manager_v1_create(server->wl_display);
	
	wsm_session_lock_init();
#if WLR_HAS_DRM_BACKEND
	server->drm_lease_manager=
			wlr_drm_lease_v1_manager_create(server->wl_display, server->backend);
	if (server->drm_lease_manager) {
		server->drm_lease_request.notify = handle_drm_lease_request;
		wl_signal_add(&server->drm_lease_manager->events.request,
					  &server->drm_lease_request);
	} else {
		wsm_log(WSM_ERROR, "Failed to create wlr_drm_lease_device_v1");
		wsm_log(WSM_ERROR, "VR will not be available");
	}
#endif
	
	server->export_dmabuf_manager_v1 = wlr_export_dmabuf_manager_v1_create(server->wl_display);
	server->screencopy_manager_v1 = wlr_screencopy_manager_v1_create(server->wl_display);
	server->data_control_manager_v1 = wlr_data_control_manager_v1_create(server->wl_display);
	wlr_viewporter_create(server->wl_display);
	wlr_single_pixel_buffer_manager_v1_create(server->wl_display);
	wlr_fractional_scale_manager_v1_create(server->wl_display,
										   WSM_WLR_FRACTIONAL_SCALE_V1_VERSION);
	server->content_type_manager_v1 =
			wlr_content_type_manager_v1_create(server->wl_display, 1);
	
	struct wlr_xdg_foreign_registry *foreign_registry =
			wlr_xdg_foreign_registry_create(server->wl_display);
	wlr_xdg_foreign_v1_create(server->wl_display, foreign_registry);
	wlr_xdg_foreign_v2_create(server->wl_display, foreign_registry);
	
	char name_candidate[16];
	for (unsigned int i = 1; i <= 32; ++i) {
		snprintf(name_candidate, sizeof(name_candidate), "wayland-%u", i);
		if (wl_display_add_socket(server->wl_display, name_candidate) >= 0) {
			server->socket = strdup(name_candidate);
			break;
		}
	}
	
	if (!server->socket) {
		wsm_log(WSM_ERROR, "Unable to open wayland socket");
		wlr_backend_destroy(server->backend);
		return false;
	}
	
	server->headless_backend = wlr_headless_backend_create(server->wl_event_loop);
	if (!server->headless_backend) {
		wsm_log(WSM_ERROR, "Failed to create secondary headless backend");
		wlr_backend_destroy(server->backend);
		return false;
	} else {
		wlr_multi_backend_add(server->backend, server->headless_backend);
	}
	
	struct wlr_output *wlr_output =
			wlr_headless_add_output(server->headless_backend, 800, 600);
	wlr_output_set_name(wlr_output, "FALLBACK");
	server->wsm_scene->fallback_output = wsm_ouput_create(wlr_output);
	
	if (!server->txn_timeout_ms) {
		server->txn_timeout_ms = 200;
	}
	
	server->dirty_nodes = create_list();
	
	server->wsm_input_manager = wsm_input_manager_create(server);
	input_manager_get_default_seat();
	
	if (global_config.primary_selection)
		wlr_primary_selection_v1_device_manager_create(server->wl_display);
	
	return true;
}

void server_finish(struct wsm_server *server) {
#if HAVE_XWAYLAND
	wlr_xwayland_destroy(server->xwayland.wlr_xwayland);
#endif
	wl_display_destroy_clients(server->wl_display);
	wlr_backend_destroy(server->backend);
	wl_display_destroy(server->wl_display);
	list_free(server->dirty_nodes);
}
