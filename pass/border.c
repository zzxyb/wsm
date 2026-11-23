#include "pass/border.h"
#include "pass/pass.h"

#include <math.h>

bool wsm_render_pass_add_border(struct wlr_render_pass *pass,
		const struct wsm_render_border_options *options) {
	if (pass == NULL || options == NULL) {
		return false;
	}
	const struct wsm_render_pass_impl *impl = wsm_render_pass_get_impl(pass);
	if (!impl || !impl->add_border) {
		return false;
	}
	if (options->box.width <= 0 || options->box.height <= 0 ||
			options->color.a <= 0 || !isfinite(options->border_width) ||
			options->border_width <= 0) {
		return true;
	}
	struct wsm_render_border_options normalized = *options;
	float max_width = fminf(options->box.width, options->box.height) / 2.f;
	normalized.border_width = fminf(options->border_width, max_width);
	for (int i = 0; i < 4; i++) {
		float radius = options->radii[i];
		normalized.radii[i] = isfinite(radius) && radius > 0 ?
			fminf(radius, max_width) : 0;
	}
	normalized.corner_smoothness = isfinite(options->corner_smoothness) &&
		options->corner_smoothness > 0 ?
		fmaxf(1.f, options->corner_smoothness) : 4.f;
	impl->add_border(pass, &normalized);
	return true;
}
