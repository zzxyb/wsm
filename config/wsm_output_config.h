#ifndef WSM_OUTPUT_CONFIG_H
#define WSM_OUTPUT_CONFIG_H

#include "wsm_output.h"

#include <stdbool.h>

#include <wayland-server-protocol.h>

#include <xf86drmMode.h>

enum render_bit_depth {
	RENDER_BIT_DEPTH_DEFAULT, // the default is currently 8
	RENDER_BIT_DEPTH_8,
	RENDER_BIT_DEPTH_10,
};

struct output_config {
	drmModeModeInfo drm_mode;
	struct wlr_color_transform *color_transform;
	char *name;
	int enabled;
	int power;
	int width, height;
	float refresh_rate;
	int custom_mode;
	int x, y;
	float scale;
	enum scale_filter_mode scale_filter;
	int32_t transform;
	enum wl_output_subpixel subpixel;
	int max_render_time; // In milliseconds
	int adaptive_sync;
	enum render_bit_depth render_bit_depth;

	char *background;
	char *background_option;
	char *background_fallback;
	bool set_color_transform;
};

struct matched_output_config {
	struct wsm_output *output;
	struct output_config *config;
};

void apply_all_output_configs(void);
struct output_config *find_output_config(struct wsm_output *output);
struct output_config *new_output_config(const char *name);
void sort_output_configs_by_priority(struct matched_output_config *configs,
	size_t configs_len);
void store_output_config(struct output_config *oc);
void free_output_config(struct output_config *oc);
bool apply_output_configs(struct matched_output_config *configs,
	size_t configs_len, bool test_only, bool degrade_to_off);
void output_get_identifier(char *identifier, size_t len,
	struct wsm_output *output);

#endif
