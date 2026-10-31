#include "pass/blur.h"
#include "pass/gles/blur.h"
#include "pass/pixman/blur.h"
#include "pass/pixman/pass.h"
#include <limits.h>
#include "renderer/pixman/renderer.h"
#include <assert.h>
#include <math.h>
#include <stdlib.h>

static void handle_renderer_destroy(struct wl_listener *listener, void *data) {
	struct wsm_render_blur_buffer_pass *pass = wl_container_of(listener, pass, renderer_destroy);
	wsm_render_blur_buffer_pass_destroy(pass);
}

void wsm_render_blur_buffer_pass_init(
	struct wsm_render_blur_buffer_pass *pass, const struct wsm_render_blur_buffer_pass_impl *impl) {
	assert(impl->render);
	*pass = (struct wsm_render_blur_buffer_pass){.impl = impl};
	wl_signal_init(&pass->events.destroy);
	wl_list_init(&pass->renderer_destroy.link);
}

void wsm_render_blur_buffer_pass_destroy(struct wsm_render_blur_buffer_pass *pass) {
	if (!pass) {
		return;
	}
	wl_list_remove(&pass->renderer_destroy.link);
	wl_signal_emit_mutable(&pass->events.destroy, NULL);
	assert(wl_list_empty(&pass->events.destroy.listener_list));
	pass->impl->destroy(pass);
}

struct wsm_render_blur_buffer_pass *wsm_render_blur_buffer_pass_create(
	struct wlr_renderer *renderer) {
	struct wsm_render_blur_buffer_pass *pass = NULL;
	if (wlr_renderer_is_wsm_gles2(renderer)) {
		pass = wsm_gles_render_blur_buffer_pass_create(renderer);
	} else if (wlr_renderer_is_wsm_pixman(renderer)) {
		pass = wsm_pixman_render_blur_buffer_pass_create(renderer);
	}
	if (pass) {
		pass->renderer = renderer;
		pass->renderer_destroy.notify = handle_renderer_destroy;
		wl_signal_add(&renderer->events.destroy, &pass->renderer_destroy);
	}
	return pass;
}

static bool matches_target(
	struct wlr_render_pass *target, struct wsm_render_blur_buffer_pass *pass) {
	if (wlr_render_pass_is_wsm_gles2(target)) {
		return &wlr_gles2_render_pass_from_render_pass(target)->buffer->renderer->base ==
			   pass->renderer;
	}
	if (wlr_render_pass_is_wsm_pixman(target)) {
		return &wsm_pixman_render_pass_from_render_pass(target)->buffer->renderer->base ==
			   pass->renderer;
	}
	return false;
}

struct wlr_texture *wsm_render_blur_buffer_pass_capture(
	struct wlr_render_pass *target, struct wsm_render_blur_buffer_pass *pass) {
	return target && pass && matches_target(target, pass)
			   ? pass->impl->capture(target, NULL, NULL, NULL)
			   : NULL;
}

bool wsm_render_blur_buffer_pass_update_capture(struct wlr_render_pass *target,
	struct wsm_render_blur_buffer_pass *pass, struct wlr_texture **texture,
	const struct wlr_box *box, const pixman_region32_t *damage) {
	if (!target || !pass || !texture || !matches_target(target, pass)) {
		return false;
	}
	if (*texture && (*texture)->renderer != pass->renderer) {
		return false;
	}
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
	struct wlr_texture *updated = pass->impl->capture(target, *texture, box, damage);
	if (!updated) {
		return false;
	}
	*texture = updated;
	return true;
}

void wsm_render_blur_buffer_pass_render(struct wlr_render_pass *target,
	struct wsm_render_blur_buffer_pass *pass,
	const struct wsm_render_blur_buffer_options *options) {
	if (!target || !pass || !options || !options->texture ||
		options->texture->renderer != pass->renderer || options->dst_box.width <= 0 ||
		options->dst_box.height <= 0 || !isfinite(options->offset) || options->offset < 0 ||
		options->padding < 0 || options->padding > 4096 ||
		options->dst_box.width > INT_MAX - 2 * options->padding ||
		options->dst_box.height > INT_MAX - 2 * options->padding || !matches_target(target, pass)) {
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
	pass->impl->render(target, pass, &normalized);
}
