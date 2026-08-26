#include "wsm_server.h"
#include "wsm_log.h"
#include "wsm_output.h"
#include "wsm_session_lock.h"
#include "wsm_output_manager.h"
#include "wsm_output_config.h"
#include "wsm_output_manager_config.h"
#include "wsm_output_memory.h"

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include <wayland-server-core.h>

#include <wlr/config.h>
#include <wlr/backend/headless.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/backend/wayland.h>
#include <wlr/backend/drm.h>
#include <wlr/types/wlr_xdg_output_v1.h>
#include <wlr/types/wlr_gamma_control_v1.h>
#include <wlr/types/wlr_output_management_v1.h>
#include <wlr/types/wlr_output_power_management_v1.h>
#include <wlr/types/wlr_drm_lease_v1.h>

static unsigned int last_headless_num = 0;

static void handle_new_output(struct wl_listener *listener, void *data) {
	struct wsm_output_manager *output_manager =
		wl_container_of(listener, output_manager, new_output);
	struct wlr_output *wlr_output = data;

	if (global_server.scene_state.fallback_output &&
			wlr_output == global_server.scene_state.fallback_output->wlr_output) {
		return;
	}

	if (wlr_output_is_headless(wlr_output)) {
		char name[64];
		snprintf(name, sizeof(name), "HEADLESS-%u", ++last_headless_num);
		wlr_output_set_name(wlr_output, name);
	}

	wsm_log(WSM_DEBUG, "New output %p: %s (non-desktop: %d)",
		wlr_output, wlr_output->name, wlr_output->non_desktop);

	if (wlr_output->non_desktop) {
		wsm_log(WSM_DEBUG, "Not configuring non-desktop output");
		struct wsm_output_non_desktop *non_desktop = output_non_desktop_create(wlr_output);
		if (!non_desktop) {
			return;
		}
#if WLR_HAS_DRM_BACKEND
		if (global_server.drm_lease_manager) {
			wlr_drm_lease_v1_manager_offer_output(global_server.drm_lease_manager,
				wlr_output);
		}
#endif
		wsm_list_add(global_server.scene_state.non_desktop_outputs, non_desktop);
		return;
	}

	if (!wlr_output_init_render(wlr_output, global_server.wlr_allocator,
			global_server.wlr_renderer)) {
		wsm_log(WSM_ERROR, "Failed to init output render");
		return;
	}

	struct wlr_scene_output *scene_output =
		wlr_scene_output_create(global_server.scene, wlr_output);
	if (!scene_output) {
		wsm_log(WSM_ERROR, "Failed to create a scene output");
		return;
	}

	struct wsm_output *output = wsm_ouput_create(wlr_output);
	if (!output) {
		wsm_log(WSM_ERROR, "Failed to create a wsm_output");
		wlr_scene_output_destroy(scene_output);
		return;
	}

	output->scene_output = scene_output;

	if (global_server.session_lock.lock) {
		wsm_session_lock_add_output(global_server.session_lock.lock, output);
	}

	request_modeset();
}

void update_output_manager_config(struct wsm_server *server) {
	if (!server || !server->output_manager ||
			!server->output_manager->output_manager_v1_wlr) {
		return;
	}

	struct wlr_output_configuration_v1 *config =
		wlr_output_configuration_v1_create();
	if (!config) {
		wsm_log(WSM_ERROR, "Could not create output configuration");
		return;
	}

	struct wsm_output *output;
	wl_list_for_each(output, &global_server.scene_state.all_outputs, link) {
		if (output == global_server.scene_state.fallback_output ||
				!output->wlr_output) {
			continue;
		}
		struct wlr_output_configuration_head_v1 *config_head =
			wlr_output_configuration_head_v1_create(config, output->wlr_output);
		if (!config_head) {
			wsm_log(WSM_ERROR, "Could not create output configuration head");
			wlr_output_configuration_v1_destroy(config);
			return;
		}
		struct wlr_box output_box;
		wlr_output_layout_get_box(global_server.scene_state.output_layout,
			output->wlr_output, &output_box);
		config_head->state.enabled = !wlr_box_empty(&output_box);
		config_head->state.x = output_box.x;
		config_head->state.y = output_box.y;
	}

	wlr_output_manager_v1_set_configuration(global_server.output_manager->output_manager_v1_wlr, config);
}

static struct output_config *output_config_for_config_head(
		struct wlr_output_configuration_head_v1 *config_head,
		struct wsm_output *output) {
	struct output_config *oc = new_output_config(output->wlr_output->name);
	if (!oc) {
		return NULL;
	}
	oc->enabled = config_head->state.enabled;
	if (!oc->enabled) {
		return oc;
	}

	if (config_head->state.mode != NULL) {
		struct wlr_output_mode *mode = config_head->state.mode;
		oc->width = mode->width;
		oc->height = mode->height;
		oc->refresh_rate = mode->refresh / 1000.f;
	} else {
		oc->width = config_head->state.custom_mode.width;
		oc->height = config_head->state.custom_mode.height;
		oc->refresh_rate =
			config_head->state.custom_mode.refresh / 1000.f;
	}
	oc->x = config_head->state.x;
	oc->y = config_head->state.y;
	oc->transform = config_head->state.transform;
	oc->scale = config_head->state.scale;
	oc->adaptive_sync = config_head->state.adaptive_sync_enabled;
	return oc;
}

static void output_manager_apply(struct wsm_server *server,
		struct wlr_output_configuration_v1 *config, bool test_only) {
	size_t configs_len = wl_list_length(&server->scene_state.all_outputs);
	if (configs_len == 0) {
		wlr_output_configuration_v1_send_succeeded(config);
		wlr_output_configuration_v1_destroy(config);
		return;
	}
	struct matched_output_config *configs = calloc(configs_len, sizeof(struct matched_output_config));
	if (!configs) {
		wsm_log(WSM_ERROR, "Could not create matched_output_config: allocation failed!");
		wlr_output_configuration_v1_send_failed(config);
		wlr_output_configuration_v1_destroy(config);
		return;
	}

	int config_idx = 0;
	struct wsm_output *sway_output;
	wl_list_for_each(sway_output, &server->scene_state.all_outputs, link) {
		if (sway_output == server->scene_state.fallback_output) {
			configs_len--;
			continue;
		}

		struct matched_output_config *cfg = &configs[config_idx++];
		cfg->output = sway_output;

		struct wlr_output_configuration_head_v1 *config_head;
		wl_list_for_each(config_head, &config->heads, link) {
			if (config_head->state.output == sway_output->wlr_output) {
				cfg->config = output_config_for_config_head(config_head, sway_output);
				break;
			}
		}
		if (!cfg->config) {
			cfg->config = find_output_config(sway_output);
		}
	}

	sort_output_configs_by_priority(configs, configs_len);
	bool ok = apply_output_configs(configs, configs_len, test_only, false);
	for (size_t idx = 0; idx < configs_len; idx++) {
		struct matched_output_config *cfg = &configs[idx];
		bool store_config = false;
		if (!test_only && ok) {
			struct wlr_output_configuration_head_v1 *config_head;
			wl_list_for_each(config_head, &config->heads, link) {
				if (config_head->state.output == cfg->output->wlr_output) {
					store_config = true;
					break;
				}
			}
		}
		if (store_config) {
			store_output_config(cfg->config);
		} else {
			free_output_config(cfg->config);
		}
	}
	free(configs);

	if (ok) {
		wlr_output_configuration_v1_send_succeeded(config);
	} else {
		wlr_output_configuration_v1_send_failed(config);
	}
	wlr_output_configuration_v1_destroy(config);

	if (!test_only) {
		update_output_manager_config(server);
	}
}

static void handle_output_manager_apply(struct wl_listener *listener, void *data) {
	struct wsm_output_manager *output_manager =
		wl_container_of(listener, output_manager, output_manager_apply);
	struct wlr_output_configuration_v1 *config = data;

	output_manager_apply(&global_server, config, false);
}

static void handle_output_manager_test(struct wl_listener *listener, void *data) {
	struct wsm_output_manager *output_manager =
		wl_container_of(listener, output_manager, output_manager_test);
	struct wlr_output_configuration_v1 *config = data;
	output_manager_apply(&global_server, config, true);
}

static void handle_output_power_manager_set_mode(struct wl_listener *listener, void *data) {
	struct wlr_output_power_v1_set_mode_event *event = data;
	if (!event || !event->output) {
		return;
	}
	struct wsm_output *output = event->output->data;
	if (!output || !output->wlr_output) {
		return;
	}

	struct output_config *oc = new_output_config(output->wlr_output->name);
	if (!oc) {
		return;
	}
	switch (event->mode) {
	case ZWLR_OUTPUT_POWER_V1_MODE_OFF:
		oc->power = 0;
		break;
	case ZWLR_OUTPUT_POWER_V1_MODE_ON:
		oc->power = 1;
		break;
	}
	store_output_config(oc);
	request_modeset();
}

static void handle_output_layout_change(struct wl_listener *listener, void *data) {
	update_output_manager_config(&global_server);
	wsm_output_memory_store_all();
}

struct wsm_output_manager *wsm_output_manager_create(const struct wsm_server *server) {
	struct wsm_output_manager *output_manager = calloc(1, sizeof(struct wsm_output_manager));
	if (!output_manager) {
		wsm_log(WSM_ERROR, "Could not create wsm_output_manager: allocation failed!");
		return NULL;
	}

	wl_list_init(&output_manager->outputs);

	output_manager->output_manager_config = wsm_output_manager_config_create(output_manager);
	if (!output_manager->output_manager_config) {
		wsm_log(WSM_ERROR, "Failed to create output manager configuration");
		free(output_manager);
		return NULL;
	}

	output_manager->new_output.notify = handle_new_output;
	wl_signal_add(&server->backend->events.new_output, &output_manager->new_output);
	output_manager->output_layout_change.notify = handle_output_layout_change;
	wl_signal_add(&global_server.scene_state.output_layout->events.change,
		&output_manager->output_layout_change);

	output_manager->xdg_output_manager_v1 =
		wlr_xdg_output_manager_v1_create(server->wl_display,
			global_server.scene_state.output_layout);
	if (!output_manager->xdg_output_manager_v1) {
		wsm_log(WSM_ERROR, "Failed to create XDG output manager");
		goto fail;
	}

	output_manager->output_manager_v1_wlr = wlr_output_manager_v1_create(server->wl_display);
	if (!output_manager->output_manager_v1_wlr) {
		wsm_log(WSM_ERROR, "Failed to create output manager");
		goto fail;
	}
	output_manager->output_manager_apply.notify = handle_output_manager_apply;
	wl_signal_add(&output_manager->output_manager_v1_wlr->events.apply,
		&output_manager->output_manager_apply);
	output_manager->output_manager_test.notify = handle_output_manager_test;
	wl_signal_add(&output_manager->output_manager_v1_wlr->events.test,
		&output_manager->output_manager_test);

	output_manager->output_power_manager_v1 = wlr_output_power_manager_v1_create(server->wl_display);
	if (!output_manager->output_power_manager_v1) {
		wsm_log(WSM_ERROR, "Failed to create output power manager");
		goto fail;
	}
	output_manager->wsm_output_power_manager_set_mode.notify = handle_output_power_manager_set_mode;
	wl_signal_add(&output_manager->output_power_manager_v1->events.set_mode,
		&output_manager->wsm_output_power_manager_set_mode);

	output_manager->gamma_control_manager_v1 =
		wlr_gamma_control_manager_v1_create(server->wl_display);
	if (!output_manager->gamma_control_manager_v1) {
		wsm_log(WSM_ERROR, "Failed to create gamma control manager");
		goto fail;
	}
	output_manager->gamma_control_manager_v1->fallback_gamma_size = 512;
	wlr_scene_set_gamma_control_manager_v1(server->scene,
		output_manager->gamma_control_manager_v1);
	return output_manager;

fail:
	if (output_manager->output_manager_v1_wlr) {
		wl_list_remove(&output_manager->output_manager_apply.link);
		wl_list_remove(&output_manager->output_manager_test.link);
	}
	if (output_manager->output_power_manager_v1) {
		wl_list_remove(&output_manager->wsm_output_power_manager_set_mode.link);
	}
	if (output_manager->output_layout_change.link.next) {
		wl_list_remove(&output_manager->output_layout_change.link);
	}
	if (output_manager->new_output.link.next) {
		wl_list_remove(&output_manager->new_output.link);
	}
	wwsm_output_manager_config_destory(output_manager->output_manager_config);
	free(output_manager);
	return NULL;
}

void wsm_output_manager_destory(struct wsm_output_manager *manager) {
	if (!manager) {
		return;
	}

	if (manager->new_output.link.next) {
		wl_list_remove(&manager->new_output.link);
	}
	if (manager->output_layout_change.link.next) {
		wl_list_remove(&manager->output_layout_change.link);
	}
	if (manager->output_manager_apply.link.next) {
		wl_list_remove(&manager->output_manager_apply.link);
	}
	if (manager->output_manager_test.link.next) {
		wl_list_remove(&manager->output_manager_test.link);
	}
	if (manager->wsm_output_power_manager_set_mode.link.next) {
		wl_list_remove(&manager->wsm_output_power_manager_set_mode.link);
	}
	wwsm_output_manager_config_destory(manager->output_manager_config);
	free(manager);
}
