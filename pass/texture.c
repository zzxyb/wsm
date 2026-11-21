#include "pass/texture.h"
#include "pass/pass.h"
#include "rounded.h"

#include <math.h>

bool wsm_render_pass_add_texture(struct wlr_render_pass *pass,
	const struct wlr_render_texture_options *options, const float radii[static 4]) {
	if (radii[0] == 0 && radii[1] == 0 && radii[2] == 0 && radii[3] == 0) {
		wlr_render_pass_add_texture(pass, options);
		return true;
	}

	struct wlr_box box;
	wlr_render_texture_options_get_dst_box(options, &box);
	if (box.width <= 0 || box.height <= 0) {
		return true;
	}
	float max_radius = fminf(box.width, box.height) * .5f;
	float normalized[4];
	bool square = true;
	for (int i = 0; i < 4; i++) {
		normalized[i] = fminf(fmaxf(radii[i], 0), max_radius);
		square &= normalized[i] == 0;
	}
	if (square) {
		wlr_render_pass_add_texture(pass, options);
		return true;
	}

	const struct wsm_render_pass_impl *impl = wsm_render_pass_get_impl(pass);
	if (!impl || !impl->add_texture) {
		return false;
	}

	pixman_region32_t solid, corners;
	split_rounded_region(&box, normalized, options->clip, &solid, &corners);
	struct wlr_render_texture_options clipped = *options;
	/* Rounded texture draws have always used premultiplied blending. */
	clipped.blend_mode = WLR_RENDER_BLEND_MODE_PREMULTIPLIED;
	if (pixman_region32_not_empty(&solid)) {
		clipped.clip = &solid;
		wlr_render_pass_add_texture(pass, &clipped);
	}
	if (pixman_region32_not_empty(&corners)) {
		clipped.clip = &corners;
		impl->add_texture(pass, &clipped, normalized);
	}
	pixman_region32_fini(&corners);
	pixman_region32_fini(&solid);
	return true;
}

float wsm_render_texture_corner_coverage(int width, int height,
	const float radii[static 4], float x, float y) {
	float px = x - width * .5f, py = y - height * .5f;
	float r = fminf(fmaxf(0, radii[(px >= 0) + 2 * (py >= 0)]),
		fminf(width, height) * .5f);
	float qx = fabsf(px) - width * .5f + r;
	float qy = fabsf(py) - height * .5f + r;
	if (r == 0 || qx <= 0 || qy <= 0) {
		return 1;
	}
	float dist = powf(powf(qx, 4) + powf(qy, 4), .25f) - r;
	float t = fminf(1, fmaxf(0, (dist + 1) * .5f));
	return 1 - t * t * (3 - 2 * t);
}
