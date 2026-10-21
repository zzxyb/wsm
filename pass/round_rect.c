#include "pass/round_rect.h"
#include "pass/gles/pass.h"
#include "pass/pixman/pass.h"
#include "pass/gles/round_rect.h"
#include "pass/pixman/round_rect.h"
#include <math.h>

bool wsm_render_pass_add_round_rect(struct wlr_render_pass *pass,
		const struct wsm_render_round_rect_options *options) {
	if (pass == NULL || options == NULL) {
		return false;
	}
	if (options->box.width <= 0 || options->box.height <= 0) {
		return true;
	}
	struct wsm_render_round_rect_options normalized = *options;
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
	if (wlr_render_pass_is_wsm_gles2(pass)) {
		wsm_gles2_render_pass_add_round_rect(pass, &normalized);
		return true;
	}
	if (wlr_render_pass_is_wsm_pixman(pass)) {
		wsm_pixman_render_pass_add_round_rect(pass, &normalized);
		return true;
	}
	return false;
}
