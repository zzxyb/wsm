#include "pass/pixman/blur.h"
#include "pass/pixman/pass.h"
#include "pass/texture.h"
#include "texture/pixman/texture.h"
#include "util/wsm_matrix.h"
#include <math.h>
#include <drm_fourcc.h>
#include <xf86drm.h>
#include <wlr/render/drm_syncobj.h>
#include <stdlib.h>
#include <string.h>
#include <wlr/util/transform.h>

struct level {
	int width, height;
	uint32_t *pixels;
};

static bool allocate(struct level *l, int width, int height) {
	if (width <= 0 || height <= 0 || (size_t)width > SIZE_MAX / 4 / height) {
		return false;
	}
	*l = (struct level){width, height, calloc((size_t)width * height, 4)};
	return l->pixels != NULL;
}

/* Pixel centers and clamp-to-edge match the GLES bilinear sampler. */
static void sample(const struct level *l, float x, float y, float out[4]) {
	x = fminf(fmaxf(x - .5f, 0), l->width - 1);
	y = fminf(fmaxf(y - .5f, 0), l->height - 1);
	int x0 = floorf(x), y0 = floorf(y);
	int x1 = x0 + 1 < l->width ? x0 + 1 : x0;
	int y1 = y0 + 1 < l->height ? y0 + 1 : y0;
	float fx = x - x0, fy = y - y0;
	uint32_t p[4] = {l->pixels[y0 * l->width + x0], l->pixels[y0 * l->width + x1],
		l->pixels[y1 * l->width + x0], l->pixels[y1 * l->width + x1]};
	for (int c = 0; c < 4; c++) {
		float a = (p[0] >> (c * 8)) & 255, b = (p[1] >> (c * 8)) & 255;
		float d = (p[2] >> (c * 8)) & 255, e = (p[3] >> (c * 8)) & 255;
		out[c] = ((a * (1 - fx) + b * fx) * (1 - fy) + (d * (1 - fx) + e * fx) * fy) / 255.f;
	}
}

static uint32_t pack(const float color[4]) {
	uint32_t pixel = 0;
	for (int c = 0; c < 4; c++) {
		pixel |= (uint32_t)lrintf(fminf(1, fmaxf(0, color[c])) * 255) << (c * 8);
	}
	return pixel;
}

static void filter(
	const struct level *src, struct level *dst, bool up, float offset, const float matrix[9]) {
	static const float down[5][3] = {{0, 0, 4}, {-1, -1, 1}, {1, 1, 1}, {1, -1, 1}, {-1, 1, 1}};
	static const float upwards[8][3] = {{-2, 0, 1}, {-1, 1, 2}, {0, 2, 1}, {1, 1, 2}, {2, 0, 1},
		{1, -1, 2}, {0, -2, 1}, {-1, -1, 2}};
	const float (*taps)[3] = up ? upwards : down;
	for (int y = 0; y < dst->height; y++) {
		for (int x = 0; x < dst->width; x++) {
			float u = (x + .5f) / dst->width, v = (y + .5f) / dst->height;
			float sx = (matrix[0] * u + matrix[1] * v + matrix[2]) * src->width;
			float sy = (matrix[3] * u + matrix[4] * v + matrix[5]) * src->height;
			float sum[4] = {0};
			for (int t = 0; t < (up ? 8 : 5); t++) {
				float color[4];
				sample(src, sx + taps[t][0] * .5f * offset, sy + taps[t][1] * .5f * offset, color);
				for (int c = 0; c < 4; c++) {
					sum[c] += color[c] * taps[t][2] / (up ? 12.f : 8.f);
				}
			}
			dst->pixels[y * dst->width + x] = pack(sum);
		}
	}
}

static float smooth(float a, float b, float x) {
	float t = fminf(1, fmaxf(0, (x - a) / (b - a)));
	return t * t * (3 - 2 * t);
}

void wsm_pixman_render_pass_add_blur(struct wlr_render_pass *target,
	const struct wsm_render_blur_buffer_options *o) {
	uint64_t wait_point = o->wait_point;
	if (o->wait_timeline &&
		drmSyncobjTimelineWait(o->wait_timeline->drm_fd, &o->wait_timeline->handle, &wait_point, 1,
			INT64_MAX, DRM_SYNCOBJ_WAIT_FLAGS_WAIT_FOR_SUBMIT, NULL) < 0) {
		return;
	}
	struct wsm_pixman_render_pass *p = wsm_pixman_render_pass_from_render_pass(target);
	struct wsm_pixman_texture *texture = wsm_pixman_texture_from_texture(o->texture);
	struct level source = {0}, levels[WSM_BLUR_BUFFER_MAX_PASSES + 1] = {0}, result = {0};
	pixman_image_t *copy = NULL;
	struct wsm_pixman_texture *filtered = NULL;
	bool mapped = false;
	if (!allocate(&source, o->texture->width, o->texture->height)) {
		goto done;
	}
	if (texture->buffer) {
		void *data;
		uint32_t format;
		size_t stride;
		if (!wlr_buffer_begin_data_ptr_access(
				texture->buffer, WLR_BUFFER_DATA_PTR_ACCESS_READ, &data, &format, &stride)) {
			goto done;
		}
		mapped = true;
	}
	copy = pixman_image_create_bits(
		PIXMAN_a8r8g8b8, source.width, source.height, source.pixels, source.width * 4);
	if (!copy) {
		goto done;
	}
	pixman_image_composite32(
		PIXMAN_OP_SRC, texture->image, NULL, copy, 0, 0, 0, 0, 0, 0, source.width, source.height);
	if (mapped) {
		wlr_buffer_end_data_ptr_access(texture->buffer);
		mapped = false;
	}
	int width = o->dst_box.width + 2 * o->padding, height = o->dst_box.height + 2 * o->padding;
	int iterations = o->iterations > 0 ? o->iterations : 4;
	if (iterations > WSM_BLUR_BUFFER_MAX_PASSES) {
		iterations = WSM_BLUR_BUFFER_MAX_PASSES;
	}
	int min_size = o->dst_box.width < o->dst_box.height ? o->dst_box.width : o->dst_box.height;
	while (iterations > 1 && (min_size >> (iterations + 1)) < 1) {
		iterations--;
	}
	for (int i = 0; i <= iterations; i++) {
		width = width > 1 ? width / 2 : 1;
		height = height > 1 ? height / 2 : 1;
		if (!allocate(&levels[i], width, height)) {
			goto done;
		}
	}
	float matrix[9], identity[9];
	wlr_matrix_identity(matrix);
	wlr_matrix_identity(identity);
	struct wlr_fbox src = o->src_box;
	if (wlr_fbox_empty(&src)) {
		src = (struct wlr_fbox){.width = source.width, .height = source.height};
	}
	wlr_matrix_translate(matrix, src.x / source.width, src.y / source.height);
	wlr_matrix_scale(matrix, src.width / source.width, src.height / source.height);
	wlr_matrix_translate(matrix, .5f, .5f);
	wlr_matrix_transform(matrix, (o->transform & WL_OUTPUT_TRANSFORM_90)
									 ? wlr_output_transform_invert(o->transform)
									 : o->transform);
	wlr_matrix_translate(matrix, -.5f, -.5f);
	float offset = o->offset > 0 ? o->offset : 1.8f;
	filter(&source, &levels[0], false, offset, matrix);
	for (int i = 1; i <= iterations; i++) {
		filter(&levels[i - 1], &levels[i], false, offset, identity);
	}
	for (int i = iterations; i > 0; i--) {
		filter(&levels[i], &levels[i - 1], true, offset, identity);
	}
	if (!allocate(&result, o->dst_box.width, o->dst_box.height)) {
		goto done;
	}
	float radii[4] = {
		o->top_left_radius, o->top_right_radius, o->bottom_left_radius, o->bottom_right_radius};
	for (int y = 0; y < result.height; y++) {
		for (int x = 0; x < result.width; x++) {
			float color[4];
			sample(&levels[0],
				(x + .5f + o->padding) * levels[0].width / (result.width + 2 * o->padding),
				(y + .5f + o->padding) * levels[0].height / (result.height + 2 * o->padding),
				color);
			float luma = color[2] * .2126f + color[1] * .7152f + color[0] * .0722f;
			float saturation = o->saturation != 0 ? o->saturation : 1.35f;
			float brightness = o->brightness != 0 ? o->brightness : 1.06f;
			for (int c = 0; c < 3; c++) {
				color[c] = (luma + (color[c] - luma) * saturation) * brightness;
			}
			for (int c = 0; c < 3; c++) {
				color[c] = color[c] * (1 - o->tint[3]) + o->tint[2 - c] * color[3] * o->tint[3];
			}
			luma = color[2] * .2126f + color[1] * .7152f + color[0] * .0722f;
			float contrast = smooth(.5f, .95f, luma) - (1 - smooth(.05f, .5f, luma));
			float noise = sinf(x * 12.9898f + y * 78.233f) * 43758.5453f;
			noise = (noise - floorf(noise) - .5f) * o->noise_amount;
			for (int c = 0; c < 3; c++) {
				color[c] =
					fminf(color[3], fmaxf(0, color[c] - contrast * o->material_contrast * color[3] +
												 noise * color[3]));
			}
			result.pixels[y * result.width + x] = pack(color);
		}
	}
	filtered = wsm_pixman_texture_create(
		p->buffer->renderer, DRM_FORMAT_ARGB8888, result.width, result.height);
	if (!filtered) {
		goto done;
	}
	filtered->image = pixman_image_create_bits(
		PIXMAN_a8r8g8b8, result.width, result.height, result.pixels, result.width * 4);
	if (!filtered->image) {
		goto done;
	}
	wsm_render_pass_add_texture(target, &(struct wlr_render_texture_options){
		.texture = &filtered->base,
		.dst_box = o->dst_box,
		.alpha = o->alpha,
		.clip = o->clip,
		.filter_mode = WLR_SCALE_FILTER_NEAREST,
		.blend_mode = o->blend_mode,
	}, radii);
done:
	if (mapped) {
		wlr_buffer_end_data_ptr_access(texture->buffer);
	}
	if (copy) {
		pixman_image_unref(copy);
	}
	if (filtered) {
		wlr_texture_destroy(&filtered->base);
	}
	free(source.pixels);
	free(result.pixels);
	for (int i = 0; i <= WSM_BLUR_BUFFER_MAX_PASSES; i++) {
		free(levels[i].pixels);
	}
}

struct wlr_texture *wsm_pixman_render_pass_capture_blur(struct wlr_render_pass *target,
	struct wlr_texture *previous,
	const struct wlr_box *box, const pixman_region32_t *damage) {
	struct wsm_pixman_render_pass *p = wsm_pixman_render_pass_from_render_pass(target);
	struct wlr_box extent =
		box ? *box : (struct wlr_box){0, 0, p->buffer->base->width, p->buffer->base->height};
	struct wsm_pixman_texture *texture =
		previous ? wsm_pixman_texture_from_texture(previous)
				 : wsm_pixman_texture_create(
					   p->buffer->renderer, DRM_FORMAT_ARGB8888, extent.width, extent.height);
	if (!texture) {
		return NULL;
	}
	if (!texture->image) {
		texture->image = pixman_image_create_bits(
			PIXMAN_a8r8g8b8, texture->base.width, texture->base.height, NULL, 0);
		if (!texture->image) {
			wlr_texture_destroy(&texture->base);
			return NULL;
		}
	}
	pixman_region32_t region;
	pixman_region32_init_rect(&region, extent.x, extent.y, extent.width, extent.height);
	pixman_region32_intersect_rect(
		&region, &region, 0, 0, p->buffer->base->width, p->buffer->base->height);
	if (damage) {
		pixman_region32_intersect(&region, &region, damage);
	}
	int count;
	const pixman_box32_t *rects = pixman_region32_rectangles(&region, &count);
	for (int i = 0; i < count; i++) {
		const pixman_box32_t *r = &rects[i];
		pixman_image_composite32(PIXMAN_OP_SRC, p->buffer->image, NULL, texture->image, r->x1,
			r->y1, 0, 0, r->x1 - extent.x, r->y1 - extent.y, r->x2 - r->x1, r->y2 - r->y1);
	}
	pixman_region32_fini(&region);
	return &texture->base;
}
