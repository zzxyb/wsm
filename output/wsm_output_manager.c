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
#include "wsm_output.h"
#include "wsm_scene.h"
#include "wsm_scene.h"
#include "wsm_session_lock.h"
#include "wsm_output_manager.h"
#include "wsm_output_config.h"
#include "wsm_output_manager_config.h"

#include <stdlib.h>
#include <assert.h>

#include <wayland-server-core.h>

#include <wlr/config.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/backend/wayland.h>
#include <wlr/backend/drm.h>
#include <wlr/types/wlr_xdg_output_v1.h>
#include <wlr/types/wlr_gamma_control_v1.h>
#include <wlr/types/wlr_output_management_v1.h>
#include <wlr/types/wlr_output_power_management_v1.h>
#include <wlr/types/wlr_drm_lease_v1.h>

static void handle_new_output(struct wl_listener *listener, void *data) {
	struct wsm_output_manager *output_manager = wl_container_of(listener, output_manager, new_output);
	struct wlr_output *wlr_output = data;
	
	if (wlr_output == global_server.wsm_scene->fallback_output->wlr_output) {
		return;
	}
	
	wsm_log(WSM_DEBUG, "New output %p: %s (non-desktop: %d)",
			wlr_output, wlr_output->name, wlr_output->non_desktop);
	
	if (wlr_output->non_desktop) {
		wsm_log(WSM_DEBUG, "Not configuring non-desktop output");
		struct wsm_output_non_desktop *non_desktop = output_non_desktop_create(wlr_output);
#if WLR_HAS_DRM_BACKEND
		if (global_server.drm_lease_manager) {
			wlr_drm_lease_v1_manager_offer_output(global_server.drm_lease_manager,
												  wlr_output);
		}
#endif
		list_add(global_server.wsm_scene->non_desktop_outputs, non_desktop);
		return;
	}
	
	if (!wlr_output_init_render(wlr_output, global_server.wlr_allocator,
								global_server.wlr_renderer)) {
		wsm_log(WSM_ERROR, "Failed to init output render");
		return;
	}
	
	struct wlr_scene_output *scene_output =
			wlr_scene_output_create(global_server.wsm_scene->root_scene, wlr_output);
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

void update_output_manager_config(struct wsm_server *server)
{
	struct wlr_output_configuration_v1 *config =
			wlr_output_configuration_v1_create();
	
	struct wsm_output *output;
	wl_list_for_each(output, &global_server.wsm_scene->all_outputs, link) {
		if (output == global_server.wsm_scene->fallback_output) {
			continue;
		}
		struct wlr_output_configuration_head_v1 *config_head =
				wlr_output_configuration_head_v1_create(config, output->wlr_output);
		struct wlr_box output_box;
		wlr_output_layout_get_box(global_server.wsm_scene->output_layout,
								  output->wlr_output, &output_box);
		// We mark the output enabled when it's switched off but not disabled
		config_head->state.enabled = !wlr_box_empty(&output_box);
		config_head->state.x = output_box.x;
		config_head->state.y = output_box.y;
	}
	
	wlr_output_manager_v1_set_configuration(global_server.wsm_output_manager->wlr_output_manager_v1, config);
}

static struct output_config *output_config_for_config_head(
		struct wlr_output_configuration_head_v1 *config_head,
		struct wsm_output *output) {
	struct output_config *oc = new_output_config(output->wlr_output->name);
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
	struct wsm_scene *root = server->wsm_scene;
	size_t configs_len = wl_list_length(&root->all_outputs);
	struct matched_output_config *configs = calloc(configs_len, sizeof(struct matched_output_config));
	if (!configs) {
		return;
	}
	
	int config_idx = 0;
	struct wsm_output *sway_output;
	wl_list_for_each(sway_output, &root->all_outputs, link) {
		if (sway_output == root->fallback_output) {
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
	struct wsm_output *output = event->output->data;
	
	struct output_config *oc = new_output_config(output->wlr_output->name);
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
}

static void handle_gamma_control_set_gamma(struct wl_listener *listener, void *data) {
	struct wsm_output_manager *output_manager = wl_container_of(listener, output_manager,
																gamma_control_set_gamma);
	const struct wlr_gamma_control_manager_v1_set_gamma_event *event = data;
	struct wsm_output *output = event->output->data;
	if (!wsm_output_is_usable(output)) {
		wsm_log(WSM_ERROR, "Error, wsm_output is not available!");
		return;
	}
	
	output->gamma_lut_changed = true;
	wlr_output_schedule_frame(output->wlr_output);
}

struct wsm_output_manager *wsm_output_manager_create(const struct wsm_server *server) {
	struct wsm_output_manager *output_manager = calloc(1, sizeof(struct wsm_output_manager));
	if (!wsm_assert(output_manager, "Could not create wsm_output_manager: allocation failed!")) {
		return NULL;
	}
	
	wl_list_init(&output_manager->outputs);
	
	output_manager->wsm_output_manager_config = wsm_output_manager_config_create(output_manager);
	
	output_manager->new_output.notify = handle_new_output;
	wl_signal_add(&server->backend->events.new_output, &output_manager->new_output);
	output_manager->output_layout_change.notify = handle_output_layout_change;
	wl_signal_add(&global_server.wsm_scene->output_layout->events.change,
				  &output_manager->output_layout_change);
	
	wlr_xdg_output_manager_v1_create(server->wl_display, global_server.wsm_scene->output_layout);
	
	output_manager->wlr_output_manager_v1 = wlr_output_manager_v1_create(server->wl_display);
	output_manager->output_manager_apply.notify = handle_output_manager_apply;
	wl_signal_add(&output_manager->wlr_output_manager_v1->events.apply,
				  &output_manager->output_manager_apply);
	output_manager->output_manager_test.notify = handle_output_manager_test;
	wl_signal_add(&output_manager->wlr_output_manager_v1->events.test,
				  &output_manager->output_manager_test);
	
	output_manager->wlr_output_power_manager_v1 = wlr_output_power_manager_v1_create(server->wl_display);
	output_manager->wsm_output_power_manager_set_mode.notify = handle_output_power_manager_set_mode;
	wl_signal_add(&output_manager->wlr_output_power_manager_v1->events.set_mode,
				  &output_manager->wsm_output_power_manager_set_mode);
	
	output_manager->wlr_gamma_control_manager_v1 =
			wlr_gamma_control_manager_v1_create(server->wl_display);
	
	output_manager->gamma_control_set_gamma.notify = handle_gamma_control_set_gamma;
	wl_signal_add(&output_manager->wlr_gamma_control_manager_v1->events.set_gamma,
				  &output_manager->gamma_control_set_gamma);
	return output_manager;
}

void wsm_output_manager_destory(struct wsm_output_manager *manager) {
	if (!manager) {
		return;
	}
	
	free(manager);
}
