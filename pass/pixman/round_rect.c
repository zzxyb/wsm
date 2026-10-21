#include "pass/pixman/round_rect.h"

#include <math.h>
#include <wlr/render/interface.h>

static void add_colored_rect(struct wlr_render_pass *pass,
		const pixman_region32_t *clip, const struct wlr_box *box,
		const float color[static 4], float coverage) {
	if (box->width <= 0 || box->height <= 0 || coverage <= 0) {
		return;
	}
	wlr_render_pass_add_rect(pass, &(struct wlr_render_rect_options){
		.box = *box,
		.color = {
			.r = color[0] * coverage, .g = color[1] * coverage,
			.b = color[2] * coverage, .a = color[3] * coverage,
		},
		.clip = clip,
	});
}

/* A small supersampled edge keeps the same shape on GLES and Pixman passes. */
static float corner_coverage(int x, int y, int width, int height, float radius, float exponent,
		bool right, bool bottom) {
	int inside = 0;
	for (int sy = 0; sy < 4; sy++) {
		for (int sx = 0; sx < 4; sx++) {
			float px = right ? width - x - (sx + .5f) / 4.f : x + (sx + .5f) / 4.f;
			float py = bottom ? height - y - (sy + .5f) / 4.f : y + (sy + .5f) / 4.f;
			float dx = fmaxf(radius - px, 0.f) / radius;
			float dy = fmaxf(radius - py, 0.f) / radius;
			inside += powf(dx, exponent) + powf(dy, exponent) <= 1.f;
		}
	}
	return inside / 16.f;
}

static void render_round_rect(struct wlr_render_pass *pass,
		const pixman_region32_t *clip, const struct wlr_box *box,
		const float color[static 4], const float radii[static 4], float exponent) {
	if (box->width <= 0 || box->height <= 0) {
		return;
	}
	pixman_region32_t solid;
	pixman_region32_init_rect(&solid, box->x, box->y, box->width, box->height);
	for (int corner = 0; corner < 4; corner++) {
		float radius = radii[corner];
		if (radius <= 0) {
			continue;
		}
		bool right = corner & 1, bottom = corner & 2;
		int width = fminf(ceilf(radius), right ? box->width / 2 : (box->width + 1) / 2);
		int height = fminf(ceilf(radius), bottom ? box->height / 2 : (box->height + 1) / 2);
		int ox = box->x + (right ? box->width - width : 0);
		int oy = box->y + (bottom ? box->height - height : 0);
		pixman_region32_t corner_region;
		pixman_region32_init_rect(&corner_region, ox, oy, width, height);
		pixman_region32_subtract(&solid, &solid, &corner_region);
		pixman_region32_fini(&corner_region);
	}
	int count;
	const pixman_box32_t *rects = pixman_region32_rectangles(&solid, &count);
	for (int i = 0; i < count; i++) {
		struct wlr_box span = {
			.x = rects[i].x1, .y = rects[i].y1,
			.width = rects[i].x2 - rects[i].x1,
			.height = rects[i].y2 - rects[i].y1,
		};
		add_colored_rect(pass, clip, &span, color, 1);
	}
	pixman_region32_fini(&solid);

	for (int corner = 0; corner < 4; corner++) {
		float radius = radii[corner];
		if (radius <= 0) {
			continue;
		}
		bool right = corner & 1, bottom = corner & 2;
		int width = fminf(ceilf(radius), right ? box->width / 2 : (box->width + 1) / 2);
		int height = fminf(ceilf(radius), bottom ? box->height / 2 : (box->height + 1) / 2);
		int ox = box->x + (right ? box->width - width : 0);
		int oy = box->y + (bottom ? box->height - height : 0);
		for (int y = 0; y < height; y++) {
			for (int step = 0; step < width; step++) {
				int x = right ? width - 1 - step : step;
				float coverage = corner_coverage(x, y, width, height, radius,
					exponent, right, bottom);
				if (coverage == 1.f) {
					struct wlr_box span = {
						.x = ox + (right ? 0 : x), .y = oy + y,
						.width = width - step, .height = 1,
					};
					add_colored_rect(pass, clip, &span, color, 1);
					break;
				}
				if (coverage > 0 && coverage < 1) {
					struct wlr_box pixel = { ox + x, oy + y, 1, 1 };
					add_colored_rect(pass, clip, &pixel, color, coverage);
				}
			}
		}
	}
}

void wsm_pixman_render_pass_add_round_rect(struct wlr_render_pass *pass,
		const struct wsm_render_round_rect_options *options) {
	const float color[4] = {
		options->color.r, options->color.g, options->color.b, options->color.a,
	};
	float exponent = options->corner_smoothness > 0 ?
		fmaxf(1.f, options->corner_smoothness) : 4.f;
	render_round_rect(pass, options->clip, &options->box, color, options->radii, exponent);
}
