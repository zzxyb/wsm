#include "pass/texture.h"
#include "pass/gles/pass.h"
#include "pass/gles/texture.h"
#include "pass/pixman/pass.h"
#include "pass/pixman/texture.h"
#include <math.h>

bool wsm_render_pass_add_texture(struct wlr_render_pass *pass,
	const struct wlr_render_texture_options *options, const float radii[static 4]) {
	if (radii[0] == 0 && radii[1] == 0 && radii[2] == 0 && radii[3] == 0) {
		wlr_render_pass_add_texture(pass, options);
	} else if (wlr_render_pass_is_wsm_gles2(pass)) {
		wsm_gles2_render_pass_add_texture(pass, options, radii);
	} else if (wlr_render_pass_is_wsm_pixman(pass)) {
		wsm_pixman_render_pass_add_texture(pass, options, radii);
	} else {
		return false;
	}
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
