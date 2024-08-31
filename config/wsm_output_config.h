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
	char *name;
	int enabled;
	int power;
	int width, height;
	float refresh_rate;
	int custom_mode;
	drmModeModeInfo drm_mode;
	int x, y;
	float scale;
	enum scale_filter_mode scale_filter;
	int32_t transform;
	enum wl_output_subpixel subpixel;
	int max_render_time; // In milliseconds
	int adaptive_sync;
	enum render_bit_depth render_bit_depth;
	bool set_color_transform;
	struct wlr_color_transform *color_transform;

	char *background;
	char *background_option;
	char *background_fallback;
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
