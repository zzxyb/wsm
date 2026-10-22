#include "pass/pixman/shadow_cache.h"

#include <drm_fourcc.h>
#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>

static float smoothstep(float start, float end, float value) {
	if (end <= start) {
		return value >= end ? 1.f : 0.f;
	}
	float t = fmaxf(0.f, fminf(1.f, (value - start) / (end - start)));
	return t * t * (3.f - 2.f * t);
}

static float rounded_rect_distance(float x, float y, float half_size,
		float radius) {
	float qx = fabsf(x) - half_size + radius;
	float qy = fabsf(y) - half_size + radius;
	float ox = fmaxf(qx, 0.f);
	float oy = fmaxf(qy, 0.f);
	float ox2 = ox * ox, oy2 = oy * oy;
	float sum4 = ox2 * ox2 + oy2 * oy2;
	float norm4 = sqrtf(sqrtf(sum4));
	/* Match the continuous radial distance used by the GLES cache shader. */
	float euclidean = hypotf(ox, oy);
	float radial_scale = (euclidean + .001f) / (norm4 + .001f);
	float corner = euclidean - radius * radial_scale;
	return corner + fminf(fmaxf(qx, qy), 0.f);
}

struct wsm_scene_shadow_cache *wsm_pixman_shadow_cache_create(
		struct wlr_renderer *renderer, const struct wsm_scene_shadow_slices *margins,
		float corner_extent, const float radii[static 4], float inner_offset,
		const float color[static 4]) {
	if (renderer == NULL || margins == NULL ||
			!isfinite(corner_extent) || corner_extent < 0.f ||
			!isfinite(inner_offset) || inner_offset < 0.f ||
			margins->left < 0 || margins->right < 0 ||
			margins->top < 0 || margins->bottom < 0 ||
			corner_extent > (INT_MAX - 1) / 2) {
		return NULL;
	}
	for (int i = 0; i < 4; i++) {
		if (!isfinite(color[i]) || color[i] < 0.f || color[i] > 1.f) {
			return NULL;
		}
	}

	int radius = (int)ceilf(corner_extent);
	int content = radius * 2 + 1;
	int64_t width64 = (int64_t)margins->left + content + margins->right;
	int64_t height64 = (int64_t)margins->top + content + margins->bottom;
	if (width64 > INT_MAX / 4 || height64 > INT_MAX ||
			(uint64_t)(width64 * height64) > SIZE_MAX / sizeof(uint32_t)) {
		return NULL;
	}
	int width = width64, height = height64;
	uint32_t *pixels = calloc((size_t)width * height, sizeof(*pixels));
	if (pixels == NULL) {
		return NULL;
	}

	float extent = fminf(fminf(margins->left, margins->right),
		fminf(margins->top, margins->bottom));
	float fade_end = fmaxf(extent - 2.f, 0.f);
	float fade_start = fmaxf(fade_end - 42.f * 1.25f, 0.f);
	float center_x = margins->left + content * .5f;
	float center_y = margins->top + content * .5f;
	for (int y = 0; y < height; y++) {
		for (int x = 0; x < width; x++) {
			float px = x + .5f, py = y + .5f;
			int corner = (px >= center_x) + 2 * (py >= center_y);
			float dist = rounded_rect_distance(px - center_x,
				py - center_y, content * .5f, radii[corner]);
			float d = fmaxf(dist, 0.f);
			float alpha = expf(-.5f * d * d / (13.f * 13.f)) * .42f +
				expf(-.5f * d * d / (42.f * 42.f)) * .26f;
			alpha *= smoothstep(-inner_offset, 1.f - inner_offset, dist);
			alpha *= 1.f - smoothstep(fade_start, fade_end, d);
			float edge = fminf(fminf(px, width - px),
				fminf(py, height - py));
			alpha *= smoothstep(0.f, 8.f, edge) * color[3];
			alpha = fmaxf(0.f, fminf(1.f, alpha));
			uint32_t a = lroundf(alpha * 255.f);
			uint32_t r = lroundf(color[0] * alpha * 255.f);
			uint32_t g = lroundf(color[1] * alpha * 255.f);
			uint32_t b = lroundf(color[2] * alpha * 255.f);
			pixels[(size_t)y * width + x] = (a << 24) | (r << 16) |
				(g << 8) | b;
		}
	}

	struct wsm_scene_shadow_slices slices = {
		.left = margins->left + radius,
		.right = margins->right + radius,
		.top = margins->top + radius,
		.bottom = margins->bottom + radius,
	};
	struct wsm_scene_shadow_cache *cache = wsm_scene_shadow_cache_create(
		renderer, DRM_FORMAT_ARGB8888, width * 4, width, height, pixels, &slices);
	free(pixels);
	return cache;
}
