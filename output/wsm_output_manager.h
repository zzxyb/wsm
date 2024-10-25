#ifndef WSM_OUTPUT_MANAGER_H
#define WSM_OUTPUT_MANAGER_H

/*
------------------------------------------------------------------------
|                                                                       |
|                          wsm_output_manager                           |
|                                                                       |
|    -------------------------------------------------------------      |
|    |                   |                   |                          |
|    |                   |                   |                          |
|    |     output1       |      output2      |       3、4、5....         |
|    |                   |                   |                          |
|    |                   |                   |                          |
|    --------------------------------------------------------------     |
|                                                                       |
------------------------------------------------------------------------- */

#include <wayland-server-core.h>

struct wl_listener;
struct wlr_output_layout;
struct wlr_output_manager_v1;
struct wlr_gamma_control_manager_v1;
struct wlr_output_power_manager_v1;

struct wsm_server;
struct wsm_output_manager_config;

struct wsm_output_manager {
	struct wl_listener new_output;
	struct wl_listener output_layout_change;
	struct wl_listener output_manager_apply;
	struct wl_listener output_manager_test;
	struct wl_listener wsm_output_power_manager_set_mode;
	struct wl_listener gamma_control_set_gamma;

	struct wl_list outputs;

	struct wlr_output_manager_v1 *output_manager_v1_wlr;
	struct wlr_output_power_manager_v1 *output_power_manager_v1;
	struct wlr_gamma_control_manager_v1 *gamma_control_manager_v1;
	struct wsm_output_manager_config *output_manager_config;
};

struct wsm_output_manager *wsm_output_manager_create(const struct wsm_server *server);
void wsm_output_manager_destory(struct wsm_output_manager *manager);
void update_output_manager_config(struct wsm_server *server);

#endif
