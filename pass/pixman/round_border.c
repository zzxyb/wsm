#include "pass/pixman/round_border.h"
#include "pass/pixman/pass.h"

#include <math.h>
#include <stdint.h>

static bool inside_rounded_rect(float x, float y, float width, float height,
		const float radii[static 4], float exponent) {
	if (x < 0 || y < 0 || x >= width || y >= height) {
		return false;
	}
	bool right = x >= width / 2.f;
	bool bottom = y >= height / 2.f;
	float radius = radii[right + 2 * bottom];
	if (radius <= 0) {
		return true;
	}
	float dx = right ? width - x : x;
	float dy = bottom ? height - y : y;
	if (dx >= radius || dy >= radius) {
		return true;
	}
	float qx = 1.f - dx / radius;
	float qy = 1.f - dy / radius;
	return powf(qx, exponent) + powf(qy, exponent) <= 1.f;
}

void wsm_pixman_render_pass_add_round_border(struct wlr_render_pass *target,
		const struct wsm_render_round_border_options *options) {
	struct wsm_pixman_render_pass *pass = wsm_pixman_render_pass_from_render_pass(target);
	const struct wlr_box *box = &options->box;
	pixman_region32_t bounds;
	pixman_region32_init_rect(&bounds, box->x, box->y, box->width, box->height);
	if (options->clip != NULL) {
		pixman_region32_intersect(&bounds, &bounds, options->clip);
	}
	if (pixman_region32_empty(&bounds)) {
		pixman_region32_fini(&bounds);
		return;
	}
	pixman_region32_fini(&bounds);

	pixman_image_t *mask = pixman_image_create_bits(PIXMAN_a8,
		box->width, box->height, NULL, 0);
	if (mask == NULL) {
		return;
	}
	uint8_t *pixels = (uint8_t *)pixman_image_get_data(mask);
	int stride = pixman_image_get_stride(mask);
	float inner_width = box->width - 2.f * options->border_width;
	float inner_height = box->height - 2.f * options->border_width;
	float inner_radii[4];
	for (int i = 0; i < 4; i++) {
		inner_radii[i] = fmaxf(options->radii[i] - options->border_width, 0.f);
	}
	for (int y = 0; y < box->height; y++) {
		for (int x = 0; x < box->width; x++) {
			int covered = 0;
			for (int sy = 0; sy < 4; sy++) {
				for (int sx = 0; sx < 4; sx++) {
					float px = x + (sx + .5f) / 4.f;
					float py = y + (sy + .5f) / 4.f;
					bool outer = inside_rounded_rect(px, py,
						box->width, box->height, options->radii,
						options->corner_smoothness);
					bool inner = inside_rounded_rect(px - options->border_width,
						py - options->border_width, inner_width, inner_height,
						inner_radii, options->corner_smoothness);
					covered += outer && !inner;
				}
			}
			pixels[y * stride + x] = (covered * 255 + 8) / 16;
		}
	}

	struct pixman_color color = {
		.red = options->color.r * 0xFFFF,
		.green = options->color.g * 0xFFFF,
		.blue = options->color.b * 0xFFFF,
		.alpha = options->color.a * 0xFFFF,
	};
	pixman_image_t *fill = pixman_image_create_solid_fill(&color);
	if (fill != NULL) {
		pixman_image_set_clip_region32(pass->buffer->image, options->clip);
		pixman_image_composite32(PIXMAN_OP_OVER, fill, mask, pass->buffer->image,
			0, 0, 0, 0, box->x, box->y, box->width, box->height);
		pixman_image_set_clip_region32(pass->buffer->image, NULL);
		pixman_image_unref(fill);
	}
	pixman_image_unref(mask);
}
