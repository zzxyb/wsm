#include "wsm_server.h"
#include "wsm_log.h"
#include "wsm_xdg_shell.h"
#include "../config.h"
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
#include "wsm_brightness_control_v1.h"
#include "wsm_keyboard_group_v1.h"
#include "wsm_transaction.h"
#include "wsm_workspace.h"
#include "wsm_output_memory.h"
#include "wsm_input_memory.h"
#include "node/wsm_node_descriptor.h"

#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>

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
#include <wlr/types/wlr_output_layout.h>
#include <wlr/util/box.h>
#include <wlr/backend/wayland.h>
#include <wlr/render/allocator.h>
#include <wlr/backend/headless.h>
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
#include <wlr/types/wlr_export_dmabuf_v1.h>
#include <wlr/types/wlr_xcursor_manager.h>
#include <wlr/types/wlr_presentation_time.h>
#include <wlr/types/wlr_xdg_foreign_registry.h>
#include <wlr/types/wlr_single_pixel_buffer_v1.h>
#include <wlr/types/wlr_security_context_v1.h>
#include <wlr/types/wlr_alpha_modifier_v1.h>
#include <wlr/types/wlr_session_lock_v1.h>
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
#define WSM_LAYER_SHELL_VERSION 5
#define WSM_WLR_FRACTIONAL_SCALE_V1_VERSION 1
#define WSM_FOREIGN_TOPLEVEL_LIST_VERSION 1
#define WINDOW_TITLE "Wsm Compositor"
#define THEME_CHECK_INTERVAL_MS 1000

static void remove_listener(struct wl_listener *listener) {
	if (listener && listener->link.next) {
		wl_list_remove(&listener->link);
		wl_list_init(&listener->link);
	}
}

void wsm_server_run_startup_command(struct wsm_server *server) {
	if (!server || !server->startup_command ||
			server->startup_command_started ||
			!server->scene_state.outputs ||
			server->scene_state.outputs->length == 0) {
		return;
	}

	server->startup_command_started = true;
	pid_t child = fork();
	if (child < 0) {
		wsm_log_errno(WSM_ERROR, "Failed to fork startup command");
		return;
	}
	if (child == 0) {
		execl("/bin/sh", "/bin/sh", "-c", server->startup_command,
			(void *)NULL);
		_exit(EXIT_FAILURE);
	}

	wsm_log(WSM_DEBUG, "Started startup command: %s", server->startup_command);
}

static void detach_backend_listeners(struct wsm_server *server) {
	if (server->output_manager) {
		remove_listener(&server->output_manager->new_output);
	}
	if (server->input_manager) {
		remove_listener(&server->input_manager->new_input);
	}
}

static void detach_display_listeners(struct wsm_server *server) {
	remove_listener(&server->pointer_constraint);

	if (server->input_manager) {
		remove_listener(&server->input_manager->virtual_keyboard_new);
		remove_listener(&server->input_manager->virtual_pointer_new);
		remove_listener(&server->input_manager->keyboard_shortcuts_inhibit_new_inhibitor);
	}

	remove_listener(&server->idle_inhibit_manager_v1.new_idle_inhibitor_v1);
	remove_listener(&server->idle_inhibit_manager_v1.manager_destroy);
	remove_listener(&server->session_lock.new_lock);
	remove_listener(&server->session_lock.manager_destroy);

	if (server->layer_shell) {
		remove_listener(&server->layer_shell->layer_shell_surface);
	}
	if (server->xdg_shell) {
		remove_listener(&server->xdg_shell->xdg_shell_toplevel);
		remove_listener(&server->xdg_shell->xdg_activation_request);
		remove_listener(&server->xdg_shell->xdg_activation_v1_request_activate);
		remove_listener(&server->xdg_shell->xdg_activation_v1_new_token);
	}
#if HAVE_SSD
	if (server->server_decoration_manager) {
		remove_listener(&server->server_decoration_manager->server_decoration);
	}
	if (server->xdg_decoration_manager) {
		remove_listener(&server->xdg_decoration_manager->xdg_decoration);
	}
#endif
#if WLR_HAS_DRM_BACKEND
	remove_listener(&server->drm_lease_request);
#endif
}

static bool server_init_scene(struct wsm_server *server) {
	server->scene = wlr_scene_create();
	if (!server->scene) {
		wsm_log(WSM_ERROR, "Could not create wlr_scene");
		return false;
	}

	node_init(&server->scene_state.node, N_ROOT, server);

	bool failed = false;
	server->scene_state.staging = alloc_scene_tree(
		&server->scene->tree, &failed);
	server->scene_state.layer_tree = alloc_scene_tree(
		&server->scene->tree, &failed);
	server->scene_state.layers.shell_background = alloc_scene_tree(
		server->scene_state.layer_tree, &failed);
	server->scene_state.layers.shell_bottom = alloc_scene_tree(
		server->scene_state.layer_tree, &failed);
	server->scene_state.layers.tiling = alloc_scene_tree(
		server->scene_state.layer_tree, &failed);
	server->scene_state.layers.floating = alloc_scene_tree(
		server->scene_state.layer_tree, &failed);
	server->scene_state.layers.shell_top = alloc_scene_tree(
		server->scene_state.layer_tree, &failed);
	server->scene_state.layers.fullscreen = alloc_scene_tree(
		server->scene_state.layer_tree, &failed);
	server->scene_state.layers.fullscreen_global = alloc_scene_tree(
		server->scene_state.layer_tree, &failed);
#if HAVE_XWAYLAND
	server->scene_state.layers.unmanaged = alloc_scene_tree(
		server->scene_state.layer_tree, &failed);
#endif
	server->scene_state.layers.shell_overlay = alloc_scene_tree(
		server->scene_state.layer_tree, &failed);
	server->scene_state.layers.animation = alloc_scene_tree(
		server->scene_state.layer_tree, &failed);
	server->scene_state.layers.popup = alloc_scene_tree(
		server->scene_state.layer_tree, &failed);
	server->scene_state.layers.seat = alloc_scene_tree(
		server->scene_state.layer_tree, &failed);
	server->scene_state.layers.session_lock = alloc_scene_tree(
		server->scene_state.layer_tree, &failed);

	if (!failed && !wsm_scene_descriptor_assign(
			&server->scene_state.layers.seat->node,
			WSM_SCENE_DESC_NON_INTERACTIVE, (void *)1)) {
		failed = true;
	}
	if (!failed && !wsm_scene_descriptor_assign(
			&server->scene_state.layers.animation->node,
			WSM_SCENE_DESC_NON_INTERACTIVE, (void *)1)) {
		failed = true;
	}

	if (failed) {
		wlr_scene_node_destroy(&server->scene->tree.node);
		server->scene = NULL;
		return false;
	}

	wlr_scene_node_set_enabled(&server->scene_state.staging->node, false);
	server->scene_state.output_layout =
		wlr_output_layout_create(server->wl_display);
	server->scene_state.outputs = wsm_list_create();
	server->scene_state.non_desktop_outputs = wsm_list_create();
	server->scene_state.scratchpad = wsm_list_create();
	if (!server->scene_state.output_layout ||
			!server->scene_state.outputs ||
			!server->scene_state.non_desktop_outputs ||
			!server->scene_state.scratchpad) {
		wsm_log(WSM_ERROR, "Could not initialize WSM scene state");
		wsm_list_destroy(server->scene_state.scratchpad);
		wsm_list_destroy(server->scene_state.non_desktop_outputs);
		wsm_list_destroy(server->scene_state.outputs);
		wlr_output_layout_destroy(server->scene_state.output_layout);
		server->scene_state.scratchpad = NULL;
		server->scene_state.non_desktop_outputs = NULL;
		server->scene_state.outputs = NULL;
		server->scene_state.output_layout = NULL;
		wlr_scene_node_destroy(&server->scene->tree.node);
		server->scene = NULL;
		return false;
	}

	wl_list_init(&server->scene_state.all_outputs);
	wl_signal_init(&server->scene_state.events.new_node);
	return true;
}

void root_get_box(struct wlr_box *box) {
	if (!box) {
		return;
	}
	box->x = global_server.scene_state.x;
	box->y = global_server.scene_state.y;
	box->width = global_server.scene_state.width;
	box->height = global_server.scene_state.height;
}

static void server_finish_scene(struct wsm_server *server) {
	if (server->scene) {
		wlr_scene_node_destroy(&server->scene->tree.node);
		server->scene = NULL;
	}
	if (server->scene_state.output_layout) {
		wlr_output_layout_destroy(server->scene_state.output_layout);
		server->scene_state.output_layout = NULL;
	}
	wsm_list_destroy(server->scene_state.scratchpad);
	wsm_list_destroy(server->scene_state.non_desktop_outputs);
	wsm_list_destroy(server->scene_state.outputs);
	server->scene_state.scratchpad = NULL;
	server->scene_state.non_desktop_outputs = NULL;
	server->scene_state.outputs = NULL;
	server->scene_state.fallback_output = NULL;
}

static void mark_container_dirty(struct wsm_container *con, void *data) {
	node_set_dirty(&con->node);
}

static void handle_desktop_theme_change(
	struct wl_listener *listener, void *data) {
	if (!global_server.scene) {
		return;
	}

	root_for_each_container(mark_container_dirty, NULL);
	transaction_commit_dirty();
}

static int handle_theme_check_timer(void *data) {
	struct wsm_server *server = data;
	wsm_desktop_interface_refresh_system_settings(
		server->desktop_interface);
	wl_event_source_timer_update(
		server->theme_check_timer, THEME_CHECK_INTERVAL_MS);
	return 0;
}

static void handle_pointer_constraint_set_region(
	struct wl_listener *listener, void *data) {
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

	if (cursor->active_constraint_wlr == constraint) {
		warp_to_constraint_cursor_hint(cursor);

		remove_listener(&cursor->constraint_commit);
		cursor->active_constraint_wlr = NULL;
	}

	free(wsm_constraint);
}

void handle_pointer_constraint(struct wl_listener *listener, void *data) {
	struct wlr_pointer_constraint_v1 *constraint = data;
	struct wsm_seat *seat = constraint->seat->data;
	struct wsm_pointer_constraint *wsm_constraint =
		calloc(1, sizeof(struct wsm_pointer_constraint));
	if (!wsm_constraint) {
		wsm_log(WSM_ERROR,
			"Unable to allocate wsm_pointer_constraint: allocation "
			"failed!");
		return;
	}

	wsm_constraint->cursor = seat->cursor;
	wsm_constraint->constraint = constraint;

	wsm_constraint->set_region.notify =
		handle_pointer_constraint_set_region;
	wl_signal_add(
		&constraint->events.set_region, &wsm_constraint->set_region);

	wsm_constraint->destroy.notify = handle_constraint_destroy;
	wl_signal_add(&constraint->events.destroy, &wsm_constraint->destroy);

	struct wlr_surface *surface =
		seat->seat->keyboard_state.focused_surface;
	if (surface && surface == constraint->surface) {
		wsm_cursor_constrain(seat->cursor, constraint);
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

static bool is_privileged(
	const struct wl_global *global, const struct wsm_server *server) {
#if WLR_HAS_DRM_BACKEND
	if (server->drm_lease_manager != NULL) {
		struct wlr_drm_lease_device_v1 *drm_lease_dev;
		wl_list_for_each(drm_lease_dev,
			&server->drm_lease_manager->devices, link) {
			if (drm_lease_dev->global == global) {
				return true;
			}
		}
	}
#endif

	if (server->output_manager) {
		if (server->output_manager->output_manager_v1_wlr &&
				global == server->output_manager->output_manager_v1_wlr->global) {
			return true;
		}
		if (server->output_manager->output_power_manager_v1 &&
				global == server->output_manager->output_power_manager_v1->global) {
			return true;
		}
		if (server->output_manager->xdg_output_manager_v1 &&
				global == server->output_manager->xdg_output_manager_v1->global) {
			return true;
		}
		if (server->output_manager->gamma_control_manager_v1 &&
				global == server->output_manager->gamma_control_manager_v1->global) {
			return true;
		}
	}

	if (server->input_method && global == server->input_method->global) {
		return true;
	}
	if (server->foreign_toplevel_list &&
			global == server->foreign_toplevel_list->global) {
		return true;
	}
	if (server->foreign_toplevel_manager &&
			global == server->foreign_toplevel_manager->global) {
		return true;
	}
	if (server->data_control_manager_v1 &&
			global == server->data_control_manager_v1->global) {
		return true;
	}
	if (server->screencopy_manager_v1 &&
			global == server->screencopy_manager_v1->global) {
		return true;
	}
	if (server->export_dmabuf_manager_v1 &&
			global == server->export_dmabuf_manager_v1->global) {
		return true;
	}
	if (server->security_context_manager_v1 &&
			global == server->security_context_manager_v1->global) {
		return true;
	}
	if (server->layer_shell && server->layer_shell->wlr_layer_shell &&
			global == server->layer_shell->wlr_layer_shell->global) {
		return true;
	}
	if (server->session_lock.manager &&
			global == server->session_lock.manager->global) {
		return true;
	}
	if (server->input_manager) {
		if (server->input_manager->keyboard_shortcuts_inhibit_wlr &&
				global == server->input_manager->keyboard_shortcuts_inhibit_wlr->global) {
			return true;
		}
		if (server->input_manager->virtual_keyboard_manager_wlr &&
				global == server->input_manager->virtual_keyboard_manager_wlr->global) {
			return true;
		}
		if (server->input_manager->virtual_pointer_manager_wlr &&
				global == server->input_manager->virtual_pointer_manager_wlr->global) {
			return true;
		}
	}

	return false;
}

static bool filter_global(const struct wl_client *client,
	const struct wl_global *global, void *data) {
	struct wsm_server *server = data;
#if HAVE_XWAYLAND
	if (global_server.xwayland_enabled) {
		struct wlr_xwayland *xwayland = server->xwayland.xwayland_wlr;
		if (xwayland && xwayland->shell_v1 &&
				global == xwayland->shell_v1->global) {
			return xwayland->server != NULL &&
				client == xwayland->server->client;
		}
	}
#endif

	const struct wlr_security_context_v1_state *security_context = NULL;
	if (server->security_context_manager_v1) {
		security_context = wlr_security_context_manager_v1_lookup_client(
			server->security_context_manager_v1, (struct wl_client *)client);
	}

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
		wsm_log(WSM_ERROR,
			"!!! Proprietary Nvidia drivers are in use !!!");
		wsm_log(WSM_ERROR, "Use drivers Nouveau instead");
	}

	if (strcmp(version->name, "evdi") == 0) {
		is_unsupported = true;
		wsm_log(WSM_ERROR,
			"!!! Proprietary DisplayLink drivers are in use !!!");
	}

	if (is_unsupported) {
		wsm_log(WSM_ERROR,
			"Proprietary drivers are NOT supported. To launch wsm "
			"anyway, "
			"launch with --unsupported-gpu and DO NOT report "
			"issues.");
		exit(EXIT_FAILURE);
	}

	drmFreeVersion(version);
}

/**
 * @brief wsm_server_init initialize the wayland compositor core
 * @param server
 * @return successed return true
 */
bool wsm_server_init(struct wsm_server *server) {
	server->desktop_interface = wsm_desktop_interface_create();
	if (!server->desktop_interface) {
		wsm_log(WSM_ERROR, "Failed to create desktop interface");
		return false;
	}
	wsm_config_init();

	server->wl_display = wl_display_create();
	if (!server->wl_display) {
		wsm_log(WSM_ERROR, "Failed to create Wayland display");
		return false;
	}
	server->wl_event_loop = wl_display_get_event_loop(server->wl_display);

	wl_display_set_global_filter(server->wl_display, filter_global, server);

	server->backend = wlr_backend_autocreate(
		server->wl_event_loop, &server->wlr_session);
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

	if (wlr_renderer_get_drm_fd(server->wlr_renderer) >= 0 &&
			wlr_renderer_get_texture_formats(
				server->wlr_renderer, WLR_BUFFER_CAP_DMABUF) != NULL) {
		server->linux_dmabuf_v1 =
			wlr_linux_dmabuf_v1_create_with_renderer(
				server->wl_display, 5, server->wlr_renderer);
		if (!server->linux_dmabuf_v1) {
			wsm_log(WSM_ERROR, "Failed to create linux-dmabuf v1");
			return false;
		}
	}

	server->wlr_allocator =
		wlr_allocator_autocreate(server->backend, server->wlr_renderer);
	if (!server->wlr_allocator) {
		wsm_log(WSM_ERROR, "Failed to create allocator");
		return false;
	}

	server->wlr_compositor = wlr_compositor_create(
		server->wl_display, 6, server->wlr_renderer);
	if (!server->wlr_compositor) {
		wsm_log(WSM_ERROR, "Failed to create compositor");
		return false;
	}
	if (!wlr_subcompositor_create(server->wl_display)) {
		wsm_log(WSM_ERROR, "Failed to create subcompositor");
		return false;
	}
	if (!server_init_scene(server)) {
		return false;
	}
	if (server->linux_dmabuf_v1) {
		wlr_scene_set_linux_dmabuf_v1(server->scene, server->linux_dmabuf_v1);
	}
	if (!wlr_alpha_modifier_v1_create(server->wl_display)) {
		wsm_log(WSM_ERROR, "Failed to create alpha modifier manager");
		return false;
	}
	server->icon_theme_change.notify = handle_desktop_theme_change;
	wl_signal_add(&server->desktop_interface->events.icon_theme_change,
		&server->icon_theme_change);
	server->color_theme_change.notify = handle_desktop_theme_change;
	wl_signal_add(&server->desktop_interface->events.color_theme_change,
		&server->color_theme_change);
	server->theme_check_timer = wl_event_loop_add_timer(
		server->wl_event_loop, handle_theme_check_timer, server);
	if (server->theme_check_timer) {
		wl_event_source_timer_update(
			server->theme_check_timer, THEME_CHECK_INTERVAL_MS);
	}

	server->xcursor_manager = wlr_xcursor_manager_create(NULL, 24);
	if (!server->xcursor_manager) {
		wsm_log(WSM_ERROR, "Failed to create xcursor manager");
		return false;
	}
	server->data_device_manager =
		wlr_data_device_manager_create(server->wl_display);
	if (!server->data_device_manager) {
		wsm_log(WSM_ERROR, "Failed to create data device manager");
		return false;
	}
	server->output_manager = wsm_output_manager_create(server);
	if (!server->output_manager) {
		return false;
	}

	server->layer_shell = wsm_layer_shell_create(server);
	server->idle_notifier_v1 =
		wlr_idle_notifier_v1_create(server->wl_display);
	if (!server->idle_notifier_v1) {
		wsm_log(WSM_ERROR, "Failed to create idle notifier");
		return false;
	}
	if (!wsm_idle_inhibit_manager_v1_init()) {
		wsm_log(WSM_ERROR, "Failed to initialize idle inhibit manager");
		return false;
	}
	if (!server->layer_shell) {
		wsm_log(WSM_ERROR, "Failed to create layer shell");
		return false;
	}
	server->xdg_shell = wsm_xdg_shell_create(server);
	if (!server->xdg_shell) {
		wsm_log(WSM_ERROR, "Failed to create XDG shell");
		return false;
	}
#if HAVE_SSD
	server->server_decoration_manager =
		wsm_server_decoration_manager_create(server);
	server->xdg_decoration_manager = xdg_decoration_manager_create(server);
#endif
	server->wlr_relative_pointer_manager =
		wlr_relative_pointer_manager_v1_create(server->wl_display);
	if (!server->wlr_relative_pointer_manager) {
		wsm_log(WSM_ERROR, "Failed to create relative pointer manager");
		return false;
	}

	server->pointer_constraints =
		wlr_pointer_constraints_v1_create(server->wl_display);
	if (!server->pointer_constraints) {
		wsm_log(WSM_ERROR, "Failed to create pointer constraints");
		return false;
	}
	server->pointer_constraint.notify = handle_pointer_constraint;
	wl_signal_add(&server->pointer_constraints->events.new_constraint,
		&server->pointer_constraint);

	server->presentation =
		wlr_presentation_create(server->wl_display, server->backend, 2);
	if (!server->presentation) {
		wsm_log(WSM_ERROR, "Failed to create presentation manager");
		return false;
	}
	server->input_method =
		wlr_input_method_manager_v2_create(server->wl_display);
	if (!server->input_method) {
		wsm_log(WSM_ERROR, "Failed to create input method manager");
		return false;
	}
	server->text_input =
		wlr_text_input_manager_v3_create(server->wl_display);
	if (!server->text_input) {
		wsm_log(WSM_ERROR, "Failed to create text input manager");
		return false;
	}
	server->foreign_toplevel_list = wlr_ext_foreign_toplevel_list_v1_create(
		server->wl_display, WSM_FOREIGN_TOPLEVEL_LIST_VERSION);
	if (!server->foreign_toplevel_list) {
		wsm_log(WSM_ERROR, "Failed to create foreign toplevel list");
		return false;
	}
	server->foreign_toplevel_manager =
		wlr_foreign_toplevel_manager_v1_create(server->wl_display);
	if (!server->foreign_toplevel_manager) {
		wsm_log(WSM_ERROR, "Failed to create foreign toplevel manager");
		return false;
	}

	if (!wsm_session_lock_init()) {
		return false;
	}
#if WLR_HAS_DRM_BACKEND
	server->drm_lease_manager = wlr_drm_lease_v1_manager_create(
		server->wl_display, server->backend);
	if (server->drm_lease_manager) {
		server->drm_lease_request.notify = handle_drm_lease_request;
		wl_signal_add(&server->drm_lease_manager->events.request,
			&server->drm_lease_request);
	} else {
		wsm_log(WSM_DEBUG, "Failed to create wlr_drm_lease_device_v1");
		wsm_log(WSM_INFO, "VR will not be available");
	}
#endif

	server->export_dmabuf_manager_v1 =
		wlr_export_dmabuf_manager_v1_create(server->wl_display);
	if (!server->export_dmabuf_manager_v1) {
		wsm_log(WSM_ERROR, "Failed to create export dmabuf manager");
		return false;
	}
	server->screencopy_manager_v1 =
		wlr_screencopy_manager_v1_create(server->wl_display);
	if (!server->screencopy_manager_v1) {
		wsm_log(WSM_ERROR, "Failed to create screencopy manager");
		return false;
	}
	server->data_control_manager_v1 =
		wlr_data_control_manager_v1_create(server->wl_display);
	if (!server->data_control_manager_v1) {
		wsm_log(WSM_ERROR, "Failed to create data control manager");
		return false;
	}
	if (!wlr_viewporter_create(server->wl_display)) {
		wsm_log(WSM_ERROR, "Failed to create viewporter");
		return false;
	}
	if (!wlr_single_pixel_buffer_manager_v1_create(server->wl_display)) {
		wsm_log(WSM_ERROR, "Failed to create single pixel buffer manager");
		return false;
	}
	if (!wlr_fractional_scale_manager_v1_create(
			server->wl_display, WSM_WLR_FRACTIONAL_SCALE_V1_VERSION)) {
		wsm_log(WSM_ERROR, "Failed to create fractional scale manager");
		return false;
	}
	server->content_type_manager_v1 =
		wlr_content_type_manager_v1_create(server->wl_display, 1);
	if (!server->content_type_manager_v1) {
		wsm_log(WSM_ERROR, "Failed to create content type manager");
		return false;
	}
	server->security_context_manager_v1 =
		wlr_security_context_manager_v1_create(server->wl_display);
	if (!server->security_context_manager_v1) {
		wsm_log(WSM_ERROR, "Failed to create security context manager");
		return false;
	}

	struct wlr_xdg_foreign_registry *foreign_registry =
		wlr_xdg_foreign_registry_create(server->wl_display);
	if (!foreign_registry) {
		wsm_log(WSM_ERROR, "Failed to create XDG foreign registry");
		return false;
	}
	if (!wlr_xdg_foreign_v1_create(server->wl_display, foreign_registry) ||
			!wlr_xdg_foreign_v2_create(server->wl_display, foreign_registry)) {
		wsm_log(WSM_ERROR, "Failed to create XDG foreign protocol");
		return false;
	}

	char name_candidate[16];
	for (unsigned int i = 1; i <= 32; ++i) {
		snprintf(name_candidate, sizeof(name_candidate), "wayland-%u",
			i);
		if (wl_display_add_socket(server->wl_display, name_candidate) >=
			0) {
			server->socket = strdup(name_candidate);
			break;
		}
	}

	if (!server->socket) {
		wsm_log(WSM_ERROR, "Unable to open wayland socket");
		detach_backend_listeners(server);
		wlr_backend_destroy(server->backend);
		server->backend = NULL;
		return false;
	}

	server->headless_backend =
		wlr_headless_backend_create(server->wl_event_loop);
	if (!server->headless_backend) {
		wsm_log(WSM_ERROR,
			"Failed to create secondary headless backend");
		detach_backend_listeners(server);
		wlr_backend_destroy(server->backend);
		server->backend = NULL;
		return false;
	} else {
		wlr_multi_backend_add(
			server->backend, server->headless_backend);
	}

	struct wlr_output *wlr_output =
		wlr_headless_add_output(server->headless_backend, 800, 600);
	if (!wlr_output) {
		wsm_log(WSM_ERROR, "Failed to create fallback headless output");
		return false;
	}
	wlr_output_set_name(wlr_output, "FALLBACK");
	server->scene_state.fallback_output = wsm_ouput_create(wlr_output);
	if (!server->scene_state.fallback_output) {
		wsm_log(WSM_ERROR, "Failed to create fallback output");
		return false;
	}

	if (!server->txn_timeout_ms) {
		server->txn_timeout_ms = 200;
	}

	server->dirty_nodes = wsm_list_create();
	if (!server->dirty_nodes) {
		wsm_log(WSM_ERROR, "Failed to create dirty node list");
		return false;
	}
	server->input_manager = wsm_input_manager_create(server);
	if (!server->input_manager || !input_manager_get_default_seat()) {
		wsm_log(WSM_ERROR, "Failed to create input manager or default seat");
		return false;
	}

	if (global_config.primary_selection)
		wlr_primary_selection_v1_device_manager_create(
			server->wl_display);

	wsm_brightness_control_manager_v1_create(server->wl_display);
	if (!wsm_keyboard_group_manager_v1_create(server->wl_display)) {
		wsm_log(WSM_ERROR, "Failed to create keyboard group manager");
		return false;
	}
	return true;
}

void server_finish(struct wsm_server *server) {
	server->shutting_down = true;
	if (server->icon_theme_change.link.next) {
		wl_list_remove(&server->icon_theme_change.link);
		wl_list_init(&server->icon_theme_change.link);
	}
	if (server->color_theme_change.link.next) {
		wl_list_remove(&server->color_theme_change.link);
		wl_list_init(&server->color_theme_change.link);
	}
	if (server->theme_check_timer) {
		wl_event_source_remove(server->theme_check_timer);
		server->theme_check_timer = NULL;
	}
	if (server->delayed_modeset) {
		wl_event_source_remove(server->delayed_modeset);
		server->delayed_modeset = NULL;
	}

	/* wlr_backend_finish() requires all backend event listeners to be gone. */
	detach_backend_listeners(server);
	detach_display_listeners(server);
#if HAVE_XWAYLAND
	if (server->xwayland.xwayland_wlr) {
		wlr_xwayland_destroy(server->xwayland.xwayland_wlr);
		server->xwayland.xwayland_wlr = NULL;
	}
#endif
	if (server->wl_display) {
		wl_display_destroy_clients(server->wl_display);
	}
	if (server->backend) {
		wlr_backend_destroy(server->backend);
		server->backend = NULL;
	}
	if (server->input_manager) {
		while (!wl_list_empty(&server->input_manager->seats)) {
			struct wsm_seat *seat = wl_container_of(
				server->input_manager->seats.next, seat, link);
			wlr_seat_destroy(seat->seat);
		}
	}
	if (server->output_manager) {
		wsm_output_manager_destory(server->output_manager);
		server->output_manager = NULL;
	}
	server_finish_scene(server);
	if (server->wl_display) {
		wl_display_destroy(server->wl_display);
		server->wl_display = NULL;
	}
	if (server->xcursor_manager) {
		wlr_xcursor_manager_destroy(server->xcursor_manager);
		server->xcursor_manager = NULL;
	}
	if (server->dirty_nodes) {
		wsm_list_destroy(server->dirty_nodes);
		server->dirty_nodes = NULL;
	}
	free((char *)server->socket);
	server->socket = NULL;
	wsm_output_memory_finish();
	wsm_input_memory_finish();
}
