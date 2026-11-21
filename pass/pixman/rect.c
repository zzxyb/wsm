#include "pass/pixman/rect.h"
#include "../rounded.h"

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

static void render_rect(struct wlr_render_pass *pass,
		const pixman_region32_t *clip, const struct wlr_box *box,
		const float color[static 4], const float radii[static 4], float exponent) {
	for (int i = 0; i < 4; i++) {
		float radius = radii[i];
		struct wlr_box corner = rounded_corner_box(box, i, radius);
		if (corner.width == 0 || corner.height == 0) {
			continue;
		}
		bool right = i & 1, bottom = i & 2;
		pixman_region32_t visible;
		pixman_region32_init(&visible);
		pixman_region32_intersect_rect(&visible, clip,
			corner.x, corner.y, corner.width, corner.height);
		int count;
		const pixman_box32_t *rects = pixman_region32_rectangles(&visible, &count);
		for (int j = 0; j < count; j++) {
			const pixman_box32_t *rect = &rects[j];
			int width = rect->x2 - rect->x1;
			for (int y = rect->y1; y < rect->y2; y++) {
				for (int step = 0; step < width; step++) {
					int x = right ? rect->x2 - 1 - step : rect->x1 + step;
					float coverage = corner_coverage(x - corner.x, y - corner.y,
						corner.width, corner.height, radius, exponent, right, bottom);
					if (coverage == 1.f) {
						struct wlr_box span = {right ? rect->x1 : x, y, width - step, 1};
						add_colored_rect(pass, NULL, &span, color, 1);
						break;
					}
					struct wlr_box pixel = {x, y, 1, 1};
					add_colored_rect(pass, NULL, &pixel, color, coverage);
				}
			}
		}
		pixman_region32_fini(&visible);
	}
}

void wsm_pixman_render_pass_add_rect(struct wlr_render_pass *pass,
		const struct wsm_render_rect_options *options) {
	const float color[4] = {
		options->color.r, options->color.g, options->color.b, options->color.a,
	};
	float exponent = options->corner_smoothness > 0 ?
		fmaxf(1.f, options->corner_smoothness) : 4.f;
	render_rect(pass, options->clip, &options->box, color, options->radii, exponent);
}
