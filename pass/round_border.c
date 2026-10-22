#include "pass/round_border.h"
#include "pass/gles/round_border.h"
#include "pass/gles/pass.h"
#include "pass/pixman/round_border.h"
#include "pass/pixman/pass.h"

#include <math.h>

bool wsm_render_pass_add_round_border(struct wlr_render_pass *pass,
		const struct wsm_render_round_border_options *options) {
	if (pass == NULL || options == NULL) {
		return false;
	}
	bool gles = wlr_render_pass_is_wsm_gles2(pass);
	bool pixman = wlr_render_pass_is_wsm_pixman(pass);
	if (!gles && !pixman) {
		return false;
	}
	if (options->box.width <= 0 || options->box.height <= 0 ||
			options->color.a <= 0 || !isfinite(options->border_width) ||
			options->border_width <= 0) {
		return true;
	}
	struct wsm_render_round_border_options normalized = *options;
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
	if (gles) {
		wsm_gles2_render_pass_add_round_border(pass, &normalized);
		return true;
	}
	if (pixman) {
		wsm_pixman_render_pass_add_round_border(pass, &normalized);
		return true;
	}
	return false;
}
