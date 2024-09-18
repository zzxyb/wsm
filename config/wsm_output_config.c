#include "wsm_log.h"
#include "wsm_server.h"
#include "wsm_scene.h"
#include "wsm_seat.h"
#include "wsm_cursor.h"
#include "wsm_output.h"
#include "wsm_output_config.h"
#include "wsm_input_manager.h"

#include <stdlib.h>
#include <limits.h>

#include <drm_fourcc.h>

#include <wlr/config.h>
#include <wlr/backend.h>
#include <wlr/render/color.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/backend/drm.h>
#include <wlr/types/wlr_output_swapchain_manager.h>

struct search_context {
	struct wlr_output_swapchain_manager *swapchain_mgr;
	struct wlr_backend_output_state *states;
	struct matched_output_config *configs;
	size_t configs_len;
	bool degrade_to_off;
};

static void default_output_config(struct output_config *oc,
		struct wlr_output *wlr_output) {
	oc->enabled = 1;
	oc->power = 1;
	struct wlr_output_mode *mode = wlr_output_preferred_mode(wlr_output);
	if (mode != NULL) {
		oc->width = mode->width;
		oc->height = mode->height;
		oc->refresh_rate = mode->refresh / 1000.f;
	}
	oc->x = oc->y = -1;
	oc->scale = 0; // auto
	oc->scale_filter = SCALE_FILTER_DEFAULT;
	struct wsm_output *output = wlr_output->data;
	oc->subpixel = output->detected_subpixel;
	oc->transform = WL_OUTPUT_TRANSFORM_NORMAL;
	oc->max_render_time = 0;
}

static bool output_config_is_disabling(struct output_config *oc) {
	return oc && (!oc->enabled || oc->power == 0);
}

static int compare_matched_output_config_priority(const void *a, const void *b) {
	const struct matched_output_config *amc = a;
	const struct matched_output_config *bmc = b;
	bool a_disabling = output_config_is_disabling(amc->config);
	bool b_disabling = output_config_is_disabling(bmc->config);
	bool a_enabled = amc->output->enabled;
	bool b_enabled = bmc->output->enabled;

	if (a_enabled && !a_disabling) {
		return -1;
	} else if (b_enabled && !b_disabling) {
		return 1;
	} else if (b_disabling && !a_disabling) {
		return -1;
	} else if (a_disabling && !b_disabling) {
		return 1;
	}
	return 0;
}

void sort_output_configs_by_priority(struct matched_output_config *configs,
		size_t configs_len) {
	qsort(configs, configs_len, sizeof(*configs), compare_matched_output_config_priority);
}

void apply_all_output_configs(void) {
	size_t configs_len = wl_list_length(&global_server.wsm_scene->all_outputs);
	struct matched_output_config *configs = calloc(configs_len, sizeof(*configs));
	if (!configs) {
		return;
	}

	int config_idx = 0;
	struct wsm_output *wsm_output;
	wl_list_for_each(wsm_output, &global_server.wsm_scene->all_outputs, link) {
		if (wsm_output == global_server.wsm_scene->fallback_output) {
			configs_len--;
			continue;
		}

		struct matched_output_config *config = &configs[config_idx++];
		config->output = wsm_output;
		config->config = find_output_config(wsm_output);
	}

	sort_output_configs_by_priority(configs, configs_len);
	apply_output_configs(configs, configs_len, false, true);
	for (size_t idx = 0; idx < configs_len; idx++) {
		struct matched_output_config *cfg = &configs[idx];
		free_output_config(cfg->config);
	}
	free(configs);
}

struct output_config *find_output_config(struct wsm_output *output) {
	const char *name = output->wlr_output->name;

	struct output_config *result = new_output_config(name);
	default_output_config(result, output->wlr_output);

	return result;
}

struct output_config *new_output_config(const char *name) {
	struct output_config *oc = calloc(1, sizeof(struct output_config));
	if (oc == NULL) {
		return NULL;
	}
	oc->name = strdup(name);
	if (oc->name == NULL) {
		free(oc);
		return NULL;
	}
	oc->enabled = -1;
	oc->width = oc->height = -1;
	oc->refresh_rate = -1;
	oc->custom_mode = -1;
	oc->drm_mode.type = -1;
	oc->x = oc->y = -1;
	oc->scale = -1;
	oc->scale_filter = SCALE_FILTER_DEFAULT;
	oc->transform = -1;
	oc->subpixel = WL_OUTPUT_SUBPIXEL_UNKNOWN;
	oc->max_render_time = -1;
	oc->adaptive_sync = -1;
	oc->render_bit_depth = RENDER_BIT_DEPTH_DEFAULT;
	oc->set_color_transform = false;
	oc->color_transform = NULL;
	oc->power = -1;
	return oc;
}

void store_output_config(struct output_config *oc) {

}

void free_output_config(struct output_config *oc) {
	if (!oc) {
		return;
	}
	free(oc->name);
	free(oc->background);
	free(oc->background_option);
	wlr_color_transform_unref(oc->color_transform);
	free(oc);
}

const char *wsm_output_scale_filter_to_string(enum scale_filter_mode scale_filter) {
	switch (scale_filter) {
	case SCALE_FILTER_DEFAULT:
		return "smart";
	case SCALE_FILTER_LINEAR:
		return "linear";
	case SCALE_FILTER_NEAREST:
		return "nearest";
	case SCALE_FILTER_SMART:
		return "smart";
	}
	wsm_assert(false, "Unknown value for scale_filter.");
	return NULL;
}

static bool finalize_output_config(struct output_config *oc, struct wsm_output *output) {
	if (output == global_server.wsm_scene->fallback_output) {
		return false;
	}

	struct wlr_output *wlr_output = output->wlr_output;
	if (oc && !oc->enabled) {
		wsm_log(WSM_DEBUG, "Disabling output %s", oc->name);
		if (output->enabled) {
			output_disable(output);
			wlr_output_layout_remove(global_server.wsm_scene->output_layout, wlr_output);
		}
		return true;
	}

	if (oc) {
		enum scale_filter_mode scale_filter_old = output->scale_filter;
		switch (oc->scale_filter) {
		case SCALE_FILTER_DEFAULT:
		case SCALE_FILTER_SMART:
			output->scale_filter = ceilf(wlr_output->scale) == wlr_output->scale ?
										   SCALE_FILTER_NEAREST : SCALE_FILTER_LINEAR;
			break;
		case SCALE_FILTER_LINEAR:
		case SCALE_FILTER_NEAREST:
			output->scale_filter = oc->scale_filter;
			break;
		}
		if (scale_filter_old != output->scale_filter) {
			wsm_log(WSM_DEBUG, "Set %s scale_filter to %s", oc->name,
					wsm_output_scale_filter_to_string(output->scale_filter));
			wlr_damage_ring_add_whole(&output->scene_output->damage_ring);
		}
	}

	if (oc && (oc->x != -1 || oc->y != -1)) {
		wsm_log(WSM_DEBUG, "Set %s position to %d, %d", oc->name, oc->x, oc->y);
		wlr_output_layout_add(global_server.wsm_scene->output_layout, wlr_output, oc->x, oc->y);
	} else {
		wlr_output_layout_add_auto(global_server.wsm_scene->output_layout, wlr_output);
	}

	struct wlr_box output_box;
	wlr_output_layout_get_box(global_server.wsm_scene->output_layout, wlr_output, &output_box);
	output->lx = output_box.x;
	output->ly = output_box.y;
	output->width = output_box.width;
	output->height = output_box.height;

	if (!output->enabled) {
		output_enable(output);
	}

	if (oc && oc->max_render_time >= 0) {
		wsm_log(WSM_DEBUG, "Set %s max render time to %d",
			oc->name, oc->max_render_time);
		output->max_render_time = oc->max_render_time;
	}

	if (oc && oc->set_color_transform) {
		if (oc->color_transform) {
			wlr_color_transform_ref(oc->color_transform);
		}
		wlr_color_transform_unref(output->color_transform);
		output->color_transform = oc->color_transform;
	}
	return true;
}

static void set_modeline(struct wlr_output *output,
		struct wlr_output_state *pending, drmModeModeInfo *drm_mode) {
#if WLR_HAS_DRM_BACKEND
	if (!wlr_output_is_drm(output)) {
		wsm_log(WSM_ERROR, "Modeline can only be set to DRM output");
		return;
	}
	wsm_log(WSM_DEBUG, "Assigning custom modeline to %s", output->name);
	struct wlr_output_mode *mode = wlr_drm_connector_add_mode(output, drm_mode);
	if (mode) {
		wlr_output_state_set_mode(pending, mode);
	}
#else
	wsm_log(WSM_ERROR, "Modeline can only be set to DRM output");
#endif
}

static void set_mode(struct wlr_output *output, struct wlr_output_state *pending,
		int width, int height, float refresh_rate, bool custom) {
	int mhz = (int)roundf(refresh_rate * 1000);
	mhz = mhz <= 0 ? INT_MAX : mhz;

	if (wl_list_empty(&output->modes) || custom) {
		wsm_log(WSM_DEBUG, "Assigning custom mode to %s", output->name);
		wlr_output_state_set_custom_mode(pending, width, height,
			refresh_rate > 0 ? mhz : 0);
		return;
	}

	struct wlr_output_mode *mode, *best = NULL;
	int best_diff_mhz = INT_MAX;
	wl_list_for_each(mode, &output->modes, link) {
		if (mode->width == width && mode->height == height) {
			int diff_mhz = abs(mode->refresh - mhz);
			if (diff_mhz < best_diff_mhz) {
				best_diff_mhz = diff_mhz;
				best = mode;
				if (best_diff_mhz == 0) {
					break;
				}
			}
		}
	}
	if (best) {
		wsm_log(WSM_INFO, "Assigning configured mode (%dx%d@%.3fHz) to %s",
			best->width, best->height, best->refresh / 1000.f, output->name);
	} else {
		best = wlr_output_preferred_mode(output);
		wsm_log(WSM_INFO, "Configured mode (%dx%d@%.3fHz) not available, "
			"applying preferred mode (%dx%d@%.3fHz)",
			width, height, refresh_rate,
			best->width, best->height, best->refresh / 1000.f);
	}
	wlr_output_state_set_mode(pending, best);
}

const char *wl_output_subpixel_to_string(enum wl_output_subpixel subpixel) {
	switch (subpixel) {
	case WL_OUTPUT_SUBPIXEL_UNKNOWN:
		return "unknown";
	case WL_OUTPUT_SUBPIXEL_NONE:
		return "none";
	case WL_OUTPUT_SUBPIXEL_HORIZONTAL_RGB:
		return "rgb";
	case WL_OUTPUT_SUBPIXEL_HORIZONTAL_BGR:
		return "bgr";
	case WL_OUTPUT_SUBPIXEL_VERTICAL_RGB:
		return "vrgb";
	case WL_OUTPUT_SUBPIXEL_VERTICAL_BGR:
		return "vbgr";
	}
	wsm_assert(false, "Unknown value for wl_output_subpixel.");
	return NULL;
}

// The minimum DPI at which we turn on a scale of 2
#define HIDPI_DPI_LIMIT (2 * 96)
// The minimum screen height at which we turn on a scale of 2
#define HIDPI_MIN_HEIGHT 1200
// 1 inch = 25.4 mm
#define MM_PER_INCH 25.

static bool phys_size_is_aspect_ratio(struct wlr_output *output) {
	return (output->phys_width == 1600 && output->phys_height == 900) ||
		   (output->phys_width == 1600 && output->phys_height == 1000) ||
		   (output->phys_width == 160 && output->phys_height == 90) ||
		   (output->phys_width == 160 && output->phys_height == 100) ||
		   (output->phys_width == 16 && output->phys_height == 9) ||
		   (output->phys_width == 16 && output->phys_height == 10);
}

static int compute_default_scale(struct wlr_output *output,
		struct wlr_output_state *pending) {
	struct wlr_box box = { .width = output->width, .height = output->height };
	if (pending->committed & WLR_OUTPUT_STATE_MODE) {
		switch (pending->mode_type) {
		case WLR_OUTPUT_STATE_MODE_FIXED:
			box.width = pending->mode->width;
			box.height = pending->mode->height;
			break;
		case WLR_OUTPUT_STATE_MODE_CUSTOM:
			box.width = pending->custom_mode.width;
			box.height = pending->custom_mode.height;
			break;
		}
	}
	enum wl_output_transform transform = output->transform;
	if (pending->committed & WLR_OUTPUT_STATE_TRANSFORM) {
		transform = pending->transform;
	}
	wlr_box_transform(&box, &box, transform, box.width, box.height);

	int width = box.width;
	int height = box.height;

	if (height < HIDPI_MIN_HEIGHT) {
		return 1;
	}

	if (output->phys_width == 0 || output->phys_height == 0) {
		return 1;
	}

	if (phys_size_is_aspect_ratio(output)) {
		return 1;
	}

	double dpi_x = (double) width / (output->phys_width / MM_PER_INCH);
	double dpi_y = (double) height / (output->phys_height / MM_PER_INCH);
	wsm_log(WSM_DEBUG, "Output DPI: %fx%f", dpi_x, dpi_y);
	if (dpi_x <= HIDPI_DPI_LIMIT || dpi_y <= HIDPI_DPI_LIMIT) {
		return 1;
	}

	return 2;
}

static bool render_format_is_10bit(uint32_t render_format) {
	return render_format == DRM_FORMAT_XRGB2101010 ||
		   render_format == DRM_FORMAT_XBGR2101010;
}

static void queue_output_config(struct output_config *oc,
		struct wsm_output *output, struct wlr_output_state *pending) {
	if (output == global_server.wsm_scene->fallback_output) {
		return;
	}

	struct wlr_output *wlr_output = output->wlr_output;

	if (output_config_is_disabling(oc)) {
		wsm_log(WSM_DEBUG, "Turning off output %s", wlr_output->name);
		wlr_output_state_set_enabled(pending, false);
		return;
	}

	wsm_log(WSM_DEBUG, "Turning on output %s", wlr_output->name);
	wlr_output_state_set_enabled(pending, true);

	if (oc && oc->drm_mode.type != 0 && oc->drm_mode.type != (uint32_t) -1) {
		wsm_log(WSM_DEBUG, "Set %s modeline",
			wlr_output->name);
		set_modeline(wlr_output, pending, &oc->drm_mode);
	} else if (oc && oc->width > 0 && oc->height > 0) {
		wsm_log(WSM_DEBUG, "Set %s mode to %dx%d (%f Hz)",
			wlr_output->name, oc->width, oc->height, oc->refresh_rate);
		set_mode(wlr_output, pending, oc->width, oc->height,
				oc->refresh_rate, oc->custom_mode == 1);
	} else if (!wl_list_empty(&wlr_output->modes)) {
		wsm_log(WSM_DEBUG, "Set preferred mode");
		struct wlr_output_mode *preferred_mode =
			wlr_output_preferred_mode(wlr_output);
		wlr_output_state_set_mode(pending, preferred_mode);
	}

	if (oc && (oc->subpixel != WL_OUTPUT_SUBPIXEL_UNKNOWN)) {
		wsm_log(WSM_DEBUG, "Set %s subpixel to %s", oc->name,
			wl_output_subpixel_to_string(oc->subpixel));
		wlr_output_state_set_subpixel(pending, oc->subpixel);
	}

	enum wl_output_transform tr = WL_OUTPUT_TRANSFORM_NORMAL;
	if (oc && oc->transform >= 0) {
		tr = oc->transform;
#if WLR_HAS_DRM_BACKEND
	} else if (wlr_output_is_drm(wlr_output)) {
		tr = wlr_drm_connector_get_panel_orientation(wlr_output);
		wsm_log(WSM_DEBUG, "Auto-detected output transform: %d", tr);
#endif
	}
	if (wlr_output->transform != tr) {
		wsm_log(WSM_DEBUG, "Set %s transform to %d", wlr_output->name, tr);
		wlr_output_state_set_transform(pending, tr);
	}

	float scale;
	if (oc && oc->scale > 0) {
		scale = oc->scale;
		float adjusted_scale = round(scale * 120) / 120;
		if (scale != adjusted_scale) {
			wsm_log(WSM_INFO, "Adjusting output scale from %f to %f",
				scale, adjusted_scale);
			scale = adjusted_scale;
		}
	} else {
		scale = compute_default_scale(wlr_output, pending);
		wsm_log(WSM_DEBUG, "Auto-detected output scale: %f", scale);
	}
	if (scale != wlr_output->scale) {
		wsm_log(WSM_DEBUG, "Set %s scale to %f", wlr_output->name, scale);
		wlr_output_state_set_scale(pending, scale);
	}
	
	if (oc && oc->adaptive_sync != -1) {
		wsm_log(WSM_DEBUG, "Set %s adaptive sync to %d", wlr_output->name,
			oc->adaptive_sync);
		wlr_output_state_set_adaptive_sync_enabled(pending, oc->adaptive_sync == 1);
	}

	if (oc && oc->render_bit_depth != RENDER_BIT_DEPTH_DEFAULT) {
		if (oc->render_bit_depth == RENDER_BIT_DEPTH_10 &&
			render_format_is_10bit(output->wlr_output->render_format)) {
			wlr_output_state_set_render_format(pending, output->wlr_output->render_format);
		} else if (oc->render_bit_depth == RENDER_BIT_DEPTH_10) {
			wlr_output_state_set_render_format(pending, DRM_FORMAT_XRGB2101010);
		} else {
			wlr_output_state_set_render_format(pending, DRM_FORMAT_XRGB8888);
		}
	}
}

static void dump_output_state(struct wlr_output *wlr_output, struct wlr_output_state *state) {
	wsm_log(WSM_DEBUG, "Output state for %s", wlr_output->name);
	if (state->committed & WLR_OUTPUT_STATE_ENABLED) {
		wsm_log(WSM_DEBUG, "    enabled:       %s", state->enabled ? "yes" : "no");
	}
	if (state->committed & WLR_OUTPUT_STATE_RENDER_FORMAT) {
		wsm_log(WSM_DEBUG, "    render_format: %d", state->render_format);
	}
	if (state->committed & WLR_OUTPUT_STATE_MODE) {
		if (state->mode_type == WLR_OUTPUT_STATE_MODE_CUSTOM) {
			wsm_log(WSM_DEBUG, "    custom mode:   %dx%d@%dmHz",
				state->custom_mode.width, state->custom_mode.height, state->custom_mode.refresh);
		} else {
			wsm_log(WSM_DEBUG, "    mode:          %dx%d@%dmHz%s",
				state->mode->width, state->mode->height, state->mode->refresh,
				state->mode->preferred ? " (preferred)" : "");
		}
	}
	if (state->committed & WLR_OUTPUT_STATE_ADAPTIVE_SYNC_ENABLED) {
		wsm_log(WSM_DEBUG, "    adaptive_sync: %s",
			state->adaptive_sync_enabled ? "enabled": "disabled");
	}
}

static bool search_valid_config(struct search_context *ctx, size_t output_idx);

static void reset_output_state(struct wlr_output_state *state) {
	wlr_output_state_finish(state);
	wlr_output_state_init(state);
	state->committed = 0;
}

static void clear_later_output_states(struct wlr_backend_output_state *states,
		size_t configs_len, size_t output_idx) {
	for (size_t idx = output_idx+1; idx < configs_len; idx++) {
		struct wlr_backend_output_state *backend_state = &states[idx];
		struct wlr_output_state *state = &backend_state->base;

		reset_output_state(state);
		wlr_output_state_set_enabled(state, false);
	}
}

static bool search_finish(struct search_context *ctx, size_t output_idx) {
	struct wlr_backend_output_state *backend_state = &ctx->states[output_idx];
	struct wlr_output_state *state = &backend_state->base;
	struct wlr_output *wlr_output = backend_state->output;

	clear_later_output_states(ctx->states, ctx->configs_len, output_idx);
	dump_output_state(wlr_output, state);
	return wlr_output_swapchain_manager_prepare(ctx->swapchain_mgr, ctx->states, ctx->configs_len) &&
		search_valid_config(ctx, output_idx+1);
}

static bool render_format_is_bgr(uint32_t fmt) {
	return fmt == DRM_FORMAT_XBGR2101010 || fmt == DRM_FORMAT_XBGR8888;
}

static bool search_adaptive_sync(struct search_context *ctx, size_t output_idx) {
	struct matched_output_config *cfg = &ctx->configs[output_idx];
	struct wlr_backend_output_state *backend_state = &ctx->states[output_idx];
	struct wlr_output_state *state = &backend_state->base;

	if (!backend_state->output->adaptive_sync_supported) {
		return search_finish(ctx, output_idx);
	}

	if (cfg->config && cfg->config->adaptive_sync == 1) {
		wlr_output_state_set_adaptive_sync_enabled(state, true);
		if (search_finish(ctx, output_idx)) {
			return true;
		}
	}

	wlr_output_state_set_adaptive_sync_enabled(state, false);
	return search_finish(ctx, output_idx);
}

static bool config_has_auto_mode(struct output_config *oc) {
	if (!oc) {
		return true;
	}
	if (oc->drm_mode.type != 0 && oc->drm_mode.type != (uint32_t)-1) {
		return true;
	} else if (oc->width > 0 && oc->height > 0) {
		return true;
	}
	return false;
}

static bool search_mode(struct search_context *ctx, size_t output_idx) {
	struct matched_output_config *cfg = &ctx->configs[output_idx];
	struct wlr_backend_output_state *backend_state = &ctx->states[output_idx];
	struct wlr_output_state *state = &backend_state->base;
	struct wlr_output *wlr_output = backend_state->output;

	if (!config_has_auto_mode(cfg->config)) {
		return search_adaptive_sync(ctx, output_idx);
	}

	struct wlr_output_mode *preferred_mode = wlr_output_preferred_mode(wlr_output);
	if (preferred_mode) {
		wlr_output_state_set_mode(state, preferred_mode);
		if (search_adaptive_sync(ctx, output_idx)) {
			return true;
		}
	}

	if (wl_list_empty(&wlr_output->modes)) {
		state->committed &= ~WLR_OUTPUT_STATE_MODE;
		return search_adaptive_sync(ctx, output_idx);
	}

	struct wlr_output_mode *mode;
	wl_list_for_each(mode, &backend_state->output->modes, link) {
		if (mode == preferred_mode) {
			continue;
		}
		wlr_output_state_set_mode(state, mode);
		if (search_adaptive_sync(ctx, output_idx)) {
			return true;
		}
	}

	return false;
}

static bool search_render_format(struct search_context *ctx, size_t output_idx) {
	struct matched_output_config *cfg = &ctx->configs[output_idx];
	struct wlr_backend_output_state *backend_state = &ctx->states[output_idx];
	struct wlr_output_state *state = &backend_state->base;
	struct wlr_output *wlr_output = backend_state->output;

	uint32_t fmts[] = {
		DRM_FORMAT_XRGB2101010,
		DRM_FORMAT_XBGR2101010,
		DRM_FORMAT_XRGB8888,
		DRM_FORMAT_INVALID,
	};
	if (render_format_is_bgr(wlr_output->render_format)) {
		// Start with BGR in the unlikely event that we previously required it.
		fmts[0] = DRM_FORMAT_XBGR2101010;
		fmts[1] = DRM_FORMAT_XRGB2101010;
	}

	const struct wlr_drm_format_set *primary_formats =
			wlr_output_get_primary_formats(wlr_output, WLR_BUFFER_CAP_DMABUF);
	bool need_10bit = cfg->config && cfg->config->render_bit_depth == RENDER_BIT_DEPTH_10;
	for (size_t idx = 0; fmts[idx] != DRM_FORMAT_INVALID; idx++) {
		if (!need_10bit && render_format_is_10bit(fmts[idx])) {
			continue;
		}
		if (!wlr_drm_format_set_get(primary_formats, fmts[idx])) {
			// This is not a supported format for this output
			continue;
		}
		wlr_output_state_set_render_format(state, fmts[idx]);
		if (search_mode(ctx, output_idx)) {
			return true;
		}
	}
	return false;
}

static bool search_valid_config(struct search_context *ctx, size_t output_idx) {
	if (output_idx >= ctx->configs_len) {
		// We reached the end of the search, all good!
		return true;
	}

	struct matched_output_config *cfg = &ctx->configs[output_idx];
	struct wlr_backend_output_state *backend_state = &ctx->states[output_idx];
	struct wlr_output_state *state = &backend_state->base;
	struct wlr_output *wlr_output = backend_state->output;

	if (!output_config_is_disabling(cfg->config)) {
		// Search through our possible configurations, doing a depth-first
		// through render_format, modes, adaptive_sync and the next output's
		// config.
		queue_output_config(cfg->config, cfg->output, &backend_state->base);
		if (search_render_format(ctx, output_idx)) {
			return true;
		} else if (!ctx->degrade_to_off) {
			return false;
		}
		wsm_log(WSM_DEBUG, "Unable to find valid config with output %s, disabling",
				wlr_output->name);
		reset_output_state(state);
	}
	
	wlr_output_state_set_enabled(state, false);
	return search_finish(ctx, output_idx);
}

bool apply_output_configs(struct matched_output_config *configs,
		size_t configs_len, bool test_only, bool degrade_to_off) {
	struct wlr_backend_output_state *states = calloc(configs_len, sizeof(struct wlr_backend_output_state));
	if (!states) {
		return false;
	}

	wsm_log(WSM_DEBUG, "Committing %zd outputs", configs_len);
	for (size_t idx = 0; idx < configs_len; idx++) {
		struct matched_output_config *cfg = &configs[idx];
		struct wlr_backend_output_state *backend_state = &states[idx];

		backend_state->output = cfg->output->wlr_output;
		wlr_output_state_init(&backend_state->base);

		wsm_log(WSM_DEBUG, "Preparing config for %s",
			cfg->output->wlr_output->name);
		queue_output_config(cfg->config, cfg->output, &backend_state->base);
	}
	
	struct wlr_output_swapchain_manager swapchain_mgr;
	wlr_output_swapchain_manager_init(&swapchain_mgr, global_server.backend);

	bool ok = wlr_output_swapchain_manager_prepare(&swapchain_mgr, states, configs_len);
	if (!ok) {
		wsm_log(WSM_ERROR, "Requested backend configuration failed, searching for valid fallbacks");
		struct search_context ctx = {
			.swapchain_mgr = &swapchain_mgr,
			.states = states,
			.configs = configs,
			.configs_len = configs_len,
			.degrade_to_off = degrade_to_off,
		};
		if (!search_valid_config(&ctx, 0)) {
			wsm_log(WSM_ERROR, "Search for valid config failed");
			goto out;
		}
	}

	if (test_only) {
		goto out;
	}

	for (size_t idx = 0; idx < configs_len; idx++) {
		struct matched_output_config *cfg = &configs[idx];
		struct wlr_backend_output_state *backend_state = &states[idx];

		struct wlr_scene_output_state_options opts = {
			.swapchain = wlr_output_swapchain_manager_get_swapchain(
				&swapchain_mgr, backend_state->output),
			.color_transform = cfg->output->color_transform,
		};
		struct wlr_scene_output *scene_output = cfg->output->scene_output;
		struct wlr_output_state *state = &backend_state->base;
		if (!wlr_scene_output_build_state(scene_output, state, &opts)) {
			wsm_log(WSM_ERROR, "Building output state for '%s' failed",
				backend_state->output->name);
			goto out;
		}
	}

	ok = wlr_backend_commit(global_server.backend, states, configs_len);
	if (!ok) {
		wsm_log(WSM_ERROR, "Backend commit failed");
		goto out;
	}

	wsm_log(WSM_DEBUG, "Commit of %zd outputs succeeded", configs_len);
	wlr_output_swapchain_manager_apply(&swapchain_mgr);
	for (size_t idx = 0; idx < configs_len; idx++) {
		struct matched_output_config *cfg = &configs[idx];
		wsm_log(WSM_DEBUG, "Finalizing config for %s",
				cfg->output->wlr_output->name);
		finalize_output_config(cfg->config, cfg->output);
	}

out:
	wlr_output_swapchain_manager_finish(&swapchain_mgr);
	for (size_t idx = 0; idx < configs_len; idx++) {
		struct wlr_backend_output_state *backend_state = &states[idx];
		wlr_output_state_finish(&backend_state->base);
	}
	free(states);

	input_manager_configure_all_input_mappings();
	input_manager_configure_xcursor();

	struct wsm_seat *seat;
	wl_list_for_each(seat, &global_server.wsm_input_manager->seats, link) {
		wlr_seat_pointer_notify_clear_focus(seat->wlr_seat);
		cursor_rebase(seat->wsm_cursor);
	}

	return ok;
}

void output_get_identifier(char *identifier, size_t len,
		struct wsm_output *output) {
	struct wlr_output *wlr_output = output->wlr_output;
	snprintf(identifier, len, "%s %s %s",
			 wlr_output->make ? wlr_output->make : "Unknown",
			 wlr_output->model ? wlr_output->model : "Unknown",
			 wlr_output->serial ? wlr_output->serial : "Unknown");
}
