#include "pass/blur.h"
#include "pass/pass.h"

#include <limits.h>
#include <math.h>

static struct wlr_texture *capture(struct wlr_render_pass *target,
		struct wlr_texture *texture, const struct wlr_box *box, const pixman_region32_t *damage) {
	const struct wsm_render_pass_impl *impl = wsm_render_pass_get_impl(target);
	return impl && impl->capture ?
		impl->capture(target, texture, box, damage) : NULL;
}

struct wlr_texture *wsm_render_pass_capture_blur(struct wlr_render_pass *target) {
	return capture(target, NULL, NULL, NULL);
}

bool wsm_render_pass_update_blur_capture(struct wlr_render_pass *target,
	struct wlr_texture **texture, const struct wlr_box *box, const pixman_region32_t *damage) {
	if (box && (box->width <= 0 || box->height <= 0)) {
		return false;
	}
	if (*texture && box &&
		((*texture)->width != (uint32_t)box->width ||
			(*texture)->height != (uint32_t)box->height)) {
		return false;
	}
	if (*texture && damage && pixman_region32_empty(damage)) {
		return true;
	}
	struct wlr_texture *updated = capture(target, *texture, box, damage);
	if (!updated) {
		return false;
	}
	*texture = updated;
	return true;
}

void wsm_render_pass_add_blur(struct wlr_render_pass *target,
	const struct wsm_render_blur_buffer_options *options) {
	if (!options->texture || options->dst_box.width <= 0 ||
		options->dst_box.height <= 0 || !isfinite(options->offset) || options->offset < 0 ||
		options->padding < 0 || options->padding > 4096 ||
		options->dst_box.width > INT_MAX - 2 * options->padding ||
		options->dst_box.height > INT_MAX - 2 * options->padding) {
		return;
	}
	if (!isfinite(options->src_box.x) || !isfinite(options->src_box.y) ||
		!isfinite(options->src_box.width) || !isfinite(options->src_box.height)) {
		return;
	}
	if (!isfinite(options->saturation) || !isfinite(options->brightness) ||
		!isfinite(options->noise_amount) || !isfinite(options->material_contrast) ||
		!isfinite(options->corner_smoothness) || (options->alpha && !isfinite(*options->alpha))) {
		return;
	}
	for (int i = 0; i < 4; i++) {
		if (!isfinite(options->tint[i])) {
			return;
		}
	}
	struct wsm_render_blur_buffer_options normalized = *options;
	float radii[4] = {options->top_left_radius, options->top_right_radius,
		options->bottom_left_radius, options->bottom_right_radius};
	float max_radius = fminf(options->dst_box.width, options->dst_box.height) * .5f;
	for (int i = 0; i < 4; i++) {
		radii[i] = isfinite(radii[i]) ? fminf(max_radius, fmaxf(0, radii[i])) : 0;
	}
	normalized.top_left_radius = radii[0];
	normalized.top_right_radius = radii[1];
	normalized.bottom_left_radius = radii[2];
	normalized.bottom_right_radius = radii[3];
	const struct wsm_render_pass_impl *impl = wsm_render_pass_get_impl(target);
	if (impl->add_blur) {
		impl->add_blur(target, &normalized);
	}
}
