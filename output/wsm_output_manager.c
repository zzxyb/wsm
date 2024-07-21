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
#include "wsm_session_lock.h"
#include "wsm_output_manager.h"
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

static bool verify_output_config_v1(const struct wlr_output_configuration_v1 *config)
{
    const char *err_msg = NULL;
    struct wlr_output_configuration_head_v1 *head;
    wl_list_for_each(head, &config->heads, link) {
        if (!head->state.enabled) {
            continue;
        }

        /* Handle custom modes */
        if (!head->state.mode) {
            int32_t refresh = head->state.custom_mode.refresh;

            if (wlr_output_is_drm(head->state.output) && refresh == 0) {
                err_msg = "DRM backend does not support a refresh rate of 0";
                goto custom_mode_failed;
            }

            if (wlr_output_is_wl(head->state.output) && refresh != 0) {
                err_msg = "Wayland backend refresh rates unsupported";
                goto custom_mode_failed;
            }
        }

        if (wlr_output_is_wl(head->state.output)
            && !head->state.adaptive_sync_enabled) {
            err_msg = "Wayland backend requires adaptive sync";
            goto custom_mode_failed;
        }

        struct wlr_output_state output_state;
        wlr_output_state_init(&output_state);
        wlr_output_head_v1_state_apply(&head->state, &output_state);

        if (!wlr_output_test_state(head->state.output, &output_state)) {
            wlr_output_state_finish(&output_state);
            return false;
        }
        wlr_output_state_finish(&output_state);
    }

    return true;

custom_mode_failed:
    assert(err_msg);
    wsm_log(WSM_INFO, "%s (%s: %dx%d@%d)",
            err_msg,
            head->state.output->name,
            head->state.custom_mode.width,
            head->state.custom_mode.height,
            head->state.custom_mode.refresh);
    return false;
}

// static void add_output_to_layout(struct wsm_output *output)
// {
//     struct wlr_output *wlr_output = output->wlr_output;
//     struct wlr_output_layout_output *layout_output =
//         wlr_output_layout_add_auto(global_server.wsm_scene->output_layout, wlr_output);
//     if (!layout_output) {
//         wsm_log(WSM_ERROR, "unable to add output to layout");
//         return;
//     }

//     if (!output->scene_output) {
//         output->scene_output =
//             wlr_scene_output_create(global_server.wsm_scene->root_scene, wlr_output);
//         if (!output->scene_output) {
//             wsm_log(WSM_ERROR, "unable to create scene output");
//             return;
//         }
//         // wlr_scene_output_layout_add_output(global_server.wsm_scene->wlr_scene_output_layout, layout_output, output->scene_output);
//     }
// }

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

static bool output_config_apply(struct wsm_output_manager *output_manager, struct wlr_output_configuration_v1 *config)
{
    // bool success = true;
    // output_manager->pending_output_layout_change++;

    // struct wlr_output_configuration_head_v1 *head;
    // wl_list_for_each(head, &config->heads, link) {
    //     struct wlr_output *o = head->state.output;
    //     struct wsm_output *output = wsm_output_from_wlr_output(o);
    //     bool output_enabled = head->state.enabled && !output->leased;
    //     bool need_to_add = output_enabled && !o->enabled;
    //     bool need_to_remove = !output_enabled && o->enabled;

    //     wlr_output_enable(o, output_enabled);
    //     if (output_enabled) {
    //         /* Output specific actions only */
    //         if (head->state.mode) {
    //             wlr_output_set_mode(o, head->state.mode);
    //         } else {
    //             int32_t width = head->state.custom_mode.width;
    //             int32_t height = head->state.custom_mode.height;
    //             int32_t refresh = head->state.custom_mode.refresh;
    //             wlr_output_set_custom_mode(o, width,
    //                                        height, refresh);
    //         }
    //         wlr_output_set_scale(o, head->state.scale);
    //         wlr_output_set_transform(o, head->state.transform);
    //         wsm_output_set_enable_adaptive_sync(o, head->state.adaptive_sync_enabled);
    //     }
    //     if (!wlr_output_commit(o)) {
    //         wsm_log(WSM_INFO, "Output config commit failed: %s", o->name);
    //         success = false;
    //         break;
    //     }

    //     if (need_to_add) {
    //         add_output_to_layout(output);
    //     }

    //     if (output_enabled) {
    //         struct wlr_box pos = {0};
    //         wlr_output_layout_get_box(global_server.wsm_scene->output_layout, o, &pos);
    //         if (pos.x != head->state.x || pos.y != head->state.y) {
    //             wlr_output_layout_add(global_server.wsm_scene->output_layout, o,
    //                                   head->state.x, head->state.y);
    //         }
    //     }

    //     if (need_to_remove) {
    //         // regions_evacuate_output(output);
    //         wlr_scene_output_destroy(output->scene_output);
    //         wlr_output_layout_remove(global_server.wsm_scene->output_layout, o);
    //         output->scene_output = NULL;
    //     }
    // }

    // output_manager->pending_output_layout_change--;
    // update_output_manager_config(&global_server);
    // return success;
    return false;
}

static void handle_output_manager_apply(struct wl_listener *listener, void *data) {
    struct wsm_output_manager *output_manager =
        wl_container_of(listener, output_manager, output_manager_apply);
    struct wlr_output_configuration_v1 *config = data;

    bool config_is_good = verify_output_config_v1(config);
    if (config_is_good && output_config_apply(output_manager, config)) {
        wlr_output_configuration_v1_send_succeeded(config);
    } else {
        wlr_output_configuration_v1_send_failed(config);
    }
    wlr_output_configuration_v1_destroy(config);
    struct wsm_output *output;
    wl_list_for_each(output, &output_manager->outputs, link) {
        // wlr_xcursor_manager_load(server->seat.xcursor_manager,
        //                          output->wlr_output->scale);
    }
}

static void handle_output_manager_test(struct wl_listener *listener, void *data) {
    struct wsm_output_manager *output_manager =
        wl_container_of(listener, output_manager, output_manager_test);
    struct wlr_output_configuration_v1 *config = data;
    if (verify_output_config_v1(config)) {
        wlr_output_configuration_v1_send_succeeded(config);
    } else {
        wlr_output_configuration_v1_send_failed(config);
    }
    wlr_output_configuration_v1_destroy(config);
}

static void handle_output_power_manager_set_mode(struct wl_listener *listener, void *data) {
    // struct wsm_output_manager *output_manager = wl_container_of(listener, output_manager,
    //                                         wsm_output_power_manager_set_mode);
    // struct wlr_output_power_v1_set_mode_event *event = data;
    // wsm_output_power_manager_set_mode(event);
    // struct wlr_output_power_v1_set_mode_event *event = data;
    // struct wsm_output *output = event->output->data;

    // struct output_config *oc = new_output_config(output->wlr_output->name);
    // switch (event->mode) {
    // case ZWLR_OUTPUT_POWER_V1_MODE_OFF:
    //     oc->power = 0;
    //     break;
    // case ZWLR_OUTPUT_POWER_V1_MODE_ON:
    //     oc->power = 1;
    //     break;
    // }
    // store_output_config(oc);
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
