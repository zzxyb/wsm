#ifndef WSM_OUTPUT_CONFIG_H
#define WSM_OUTPUT_CONFIG_H

#include "wsm_output.h"

#include <stdbool.h>

#include <wayland-server-protocol.h>

#include <xf86drmMode.h>

/**
 * @brief Enumeration of render bit depths
 */
enum render_bit_depth {
	RENDER_BIT_DEPTH_DEFAULT, /**< Default render bit depth (currently 8) */
	RENDER_BIT_DEPTH_8, /**< 8-bit render depth */
	RENDER_BIT_DEPTH_10, /**< 10-bit render depth */
};

/**
 * @brief Structure representing the configuration for an output
 */
struct output_config {
	drmModeModeInfo drm_mode; /**< DRM mode information for the output */
	struct wlr_color_transform *color_transform; /**< Pointer to the color transformation settings */
	char *name; /**< Name of the output */
	int enabled; /**< Flag indicating if the output is enabled */
	int power; /**< Power state of the output */
	int width; /**< Width of the output */
	int height; /**< Height of the output */
	float refresh_rate; /**< Refresh rate of the output */
	int custom_mode; /**< Custom mode identifier */
	int x; /**< X position of the output */
	int y; /**< Y position of the output */
	float scale; /**< Scale factor for the output */
	enum scale_filter_mode scale_filter; /**< Scale filter mode for the output */
	int32_t transform; /**< Transformation applied to the output */
	enum wl_output_subpixel subpixel; /**< Subpixel layout for the output */
	int max_render_time; /**< Maximum render time in milliseconds */
	int adaptive_sync; /**< Flag for adaptive sync support */
	enum render_bit_depth render_bit_depth; /**< Render bit depth setting */

	char *background; /**< Background image for the output */
	char *background_option; /**< Options for the background */
	char *background_fallback; /**< Fallback background image */
	bool set_color_transform; /**< Flag indicating if color transform is set */
};

/**
 * @brief Structure representing a matched output configuration
 */
struct matched_output_config {
	struct wsm_output *output; /**< Pointer to the associated WSM output */
	struct output_config *config; /**< Pointer to the associated output configuration */
};

/**
 * @brief Applies all output configurations
 */
void apply_all_output_configs(void);

/**
 * @brief Finds the output configuration for the specified output
 * @param output Pointer to the WSM output to find the configuration for
 * @return Pointer to the associated output_config instance, or NULL if not found
 */
struct output_config *find_output_config(struct wsm_output *output);

/**
 * @brief Creates a new output configuration with the specified name
 * @param name Name for the new output configuration
 * @return Pointer to the newly created output_config instance
 */
struct output_config *new_output_config(const char *name);

/**
 * @brief Sorts output configurations by priority
 * @param configs Pointer to the matched_output_config array to sort
 * @param configs_len Length of the matched_output_config array
 */
void sort_output_configs_by_priority(struct matched_output_config *configs,
	size_t configs_len);

/**
 * @brief Stores the specified output configuration
 * @param oc Pointer to the output_config instance to store
 */
void store_output_config(struct output_config *oc);

/**
 * @brief Frees the specified output configuration
 * @param oc Pointer to the output_config instance to free
 */
void free_output_config(struct output_config *oc);

/**
 * @brief Applies the specified output configurations
 * @param configs Pointer to the matched_output_config array to apply
 * @param configs_len Length of the matched_output_config array
 * @param test_only Flag indicating if the application is a test only
 * @param degrade_to_off Flag indicating if the output should degrade to off
 * @return true if the application was successful, false otherwise
 */
bool apply_output_configs(struct matched_output_config *configs,
	size_t configs_len, bool test_only, bool degrade_to_off);

/**
 * @brief Retrieves the identifier for the specified output
 * @param identifier Pointer to the buffer to store the identifier
 * @param len Length of the identifier buffer
 * @param output Pointer to the WSM output to get the identifier for
 */
void output_get_identifier(char *identifier, size_t len,
	struct wsm_output *output);

#endif
