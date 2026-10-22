#include "node/wsm_scene_shadow.h"
#include "pass/gles/shadow_cache.h"
#include "pass/pixman/shadow_cache.h"
#include "renderer/gles/renderer.h"
#include "renderer/pixman/renderer.h"

#include <limits.h>
#include <math.h>

struct wsm_scene_shadow_cache *wsm_scene_shadow_cache_create_rounded(
		struct wlr_renderer *renderer, const struct wsm_scene_shadow_slices *margins,
		float corner_radius, float inner_offset, const float color[static 4]) {
	const float radii[4] = {
		corner_radius, corner_radius, corner_radius, corner_radius,
	};
	return wsm_scene_shadow_cache_create_rounded_corners(renderer, margins,
		corner_radius, radii, inner_offset, color);
}

struct wsm_scene_shadow_cache *wsm_scene_shadow_cache_create_rounded_corners(
		struct wlr_renderer *renderer, const struct wsm_scene_shadow_slices *margins,
		float corner_extent, const float radii[static 4], float inner_offset,
		const float color[static 4]) {
	if (renderer == NULL || margins == NULL || !isfinite(corner_extent) ||
			corner_extent < 0.f || corner_extent > (INT_MAX - 1) / 2 ||
			!isfinite(inner_offset) || inner_offset < 0.f ||
			margins->left < 0 || margins->right < 0 ||
			margins->top < 0 || margins->bottom < 0) {
		return NULL;
	}
	for (int i = 0; i < 4; i++) {
		if (!isfinite(color[i]) || color[i] < 0.f || color[i] > 1.f ||
				!isfinite(radii[i]) || radii[i] < 0.f || radii[i] > corner_extent) {
			return NULL;
		}
	}
	if (wlr_renderer_is_wsm_gles2(renderer)) {
		return wsm_gles2_shadow_cache_create(renderer, margins,
			corner_extent, radii, inner_offset, color);
	}
	if (wlr_renderer_is_wsm_pixman(renderer)) {
		return wsm_pixman_shadow_cache_create(renderer, margins,
			corner_extent, radii, inner_offset, color);
	}
	return NULL;
}
