#ifndef WSM_OUTPUT_MANAGER_H
#define WSM_OUTPUT_MANAGER_H

#include <wayland-server-core.h>

struct wl_listener;
struct wlr_output_layout;
struct wlr_output_manager_v1;
struct wlr_gamma_control_manager_v1;
struct wlr_output_power_manager_v1;

struct wsm_server;
struct wsm_output_manager_config;

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

/**
 * @brief Structure representing the output manager in the WSM
 */
struct wsm_output_manager {
	struct wl_listener new_output; /**< Listener for handling new output events */
	struct wl_listener output_layout_change; /**< Listener for handling output layout change events */
	struct wl_listener output_manager_apply; /**< Listener for applying output manager settings */
	struct wl_listener output_manager_test; /**< Listener for testing output manager settings */
	struct wl_listener wsm_output_power_manager_set_mode; /**< Listener for setting output power manager mode */
	struct wl_listener gamma_control_set_gamma; /**< Listener for setting gamma control */

	struct wl_list outputs; /**< List of outputs managed by the output manager */

	struct wlr_output_manager_v1 *output_manager_v1_wlr; /**< Pointer to the WLR output manager instance */
	struct wlr_output_power_manager_v1 *output_power_manager_v1; /**< Pointer to the WLR output power manager instance */
	struct wlr_gamma_control_manager_v1 *gamma_control_manager_v1; /**< Pointer to the WLR gamma control manager instance */
	struct wsm_output_manager_config *output_manager_config; /**< Pointer to the output manager configuration */
};

/**
 * @brief Creates a new wsm_output_manager instance
 * @param server Pointer to the WSM server that will manage the outputs
 * @return Pointer to the newly created wsm_output_manager instance
 */
struct wsm_output_manager *wsm_output_manager_create(const struct wsm_server *server);

/**
 * @brief Destroys the specified wsm_output_manager instance
 * @param manager Pointer to the wsm_output_manager instance to be destroyed
 */
void wsm_output_manager_destory(struct wsm_output_manager *manager);

/**
 * @brief Updates the configuration of the output manager
 * @param server Pointer to the WSM server whose output manager configuration will be updated
 */
void update_output_manager_config(struct wsm_server *server);

#endif
