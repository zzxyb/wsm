#include "pass/rect.h"
#include "pass/pass.h"
#include "rounded.h"

#include <math.h>

bool wsm_render_pass_add_rect(struct wlr_render_pass *pass,
		const struct wsm_render_rect_options *options) {
	if (pass == NULL || options == NULL) {
		return false;
	}
	if (options->box.width <= 0 || options->box.height <= 0) {
		return true;
	}
	struct wsm_render_rect_options normalized = *options;
	float max_radius = fminf(options->box.width, options->box.height) / 2.f;
	bool square = true;
	for (int i = 0; i < 4; i++) {
		float radius = options->radii[i];
		normalized.radii[i] = isfinite(radius) && radius > 0 ?
			fminf(radius, max_radius) : 0;
		square &= normalized.radii[i] == 0;
	}
	if (square) {
		wlr_render_pass_add_rect(pass, &(struct wlr_render_rect_options){
			.box = options->box,
			.color = options->color,
			.clip = options->clip,
		});
		return true;
	}
	const struct wsm_render_pass_impl *impl = wsm_render_pass_get_impl(pass);
	if (!impl || !impl->add_rect) {
		return false;
	}

	pixman_region32_t solid, corners;
	split_rounded_region(&options->box, normalized.radii, options->clip, &solid, &corners);
	if (pixman_region32_not_empty(&solid)) {
		wlr_render_pass_add_rect(pass, &(struct wlr_render_rect_options){
			.box = options->box,
			.color = options->color,
			.clip = &solid,
		});
	}
	if (pixman_region32_not_empty(&corners)) {
		normalized.clip = &corners;
		impl->add_rect(pass, &normalized);
	}
	pixman_region32_fini(&corners);
	pixman_region32_fini(&solid);
	return true;
}
