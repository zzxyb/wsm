#include "pass/shadow.h"
#include "node/wsm_scene_shadow.h"

#include <math.h>
#include <wlr/render/pass.h>
#include <wlr/util/transform.h>

static int scale_length(int length, int offset, float scale) {
	return round((offset + length) * scale) - round(offset * scale);
}

static void scale_box(struct wlr_box *box, float scale) {
	box->width = scale_length(box->width, box->x, scale);
	box->height = scale_length(box->height, box->y, scale);
	box->x = round(box->x * scale);
	box->y = round(box->y * scale);
}

static void transform_output_box(struct wlr_box *box,
		const struct wsm_render_data *data) {
	enum wl_output_transform transform = wlr_output_transform_invert(data->transform);
	scale_box(box, data->scale);
	wlr_box_transform(box, box, transform,
		data->trans_width, data->trans_height);
}

static void render_shadow_part(struct wsm_scene_shadow *shadow,
		const struct wsm_render_data *data, const pixman_region32_t *clip,
		int x, int y, const struct wlr_fbox *src_box,
		const struct wlr_box *dst_box) {
	if (dst_box->width <= 0 || dst_box->height <= 0 ||
			src_box->width <= 0 || src_box->height <= 0) {
		return;
	}

	struct wlr_box output_box = {
		.x = x + dst_box->x,
		.y = y + dst_box->y,
		.width = dst_box->width,
		.height = dst_box->height,
	};
	transform_output_box(&output_box, data);

	wlr_render_pass_add_texture(data->render_pass,
		&(struct wlr_render_texture_options){
			.texture = shadow->cache->texture,
			.src_box = *src_box,
			.dst_box = output_box,
			.alpha = &shadow->opacity,
			.clip = clip,
			.blend_mode = WLR_RENDER_BLEND_MODE_PREMULTIPLIED,
			.filter_mode = WLR_SCALE_FILTER_BILINEAR,
		});
}

void wsm_render_pass_add_shadow(struct wsm_scene_shadow *shadow,
		const struct wsm_render_data *data, const pixman_region32_t *clip,
		int x, int y) {
	struct wsm_scene_shadow_cache *cache = shadow->cache;
	int src_x[3] = { 0, cache->slices.left, cache->width - cache->slices.right };
	int src_y[3] = { 0, cache->slices.top, cache->height - cache->slices.bottom };
	int src_w[3] = {
		cache->slices.left,
		cache->width - cache->slices.left - cache->slices.right,
		cache->slices.right,
	};
	int src_h[3] = {
		cache->slices.top,
		cache->height - cache->slices.top - cache->slices.bottom,
		cache->slices.bottom,
	};

	/* Keep the cached rectangle edge at the shadow's inner rectangle edge. */
	int dst_left = cache->slices.left;
	int dst_right = cache->slices.right;
	int dst_top = cache->slices.top;
	int dst_bottom = cache->slices.bottom;
	int dst_total_width = shadow->margins.left + shadow->width + shadow->margins.right;
	int dst_total_height = shadow->margins.top + shadow->height + shadow->margins.bottom;

	if (dst_left + dst_right > dst_total_width) {
		dst_left = shadow->margins.left;
		dst_right = shadow->margins.right;
	}
	if (dst_top + dst_bottom > dst_total_height) {
		dst_top = shadow->margins.top;
		dst_bottom = shadow->margins.bottom;
	}

	int dst_x[3] = { 0, dst_left, dst_total_width - dst_right };
	int dst_y[3] = { 0, dst_top, dst_total_height - dst_bottom };
	int dst_w[3] = { dst_left, dst_total_width - dst_left - dst_right, dst_right };
	int dst_h[3] = { dst_top, dst_total_height - dst_top - dst_bottom, dst_bottom };

	for (size_t row = 0; row < 3; row++) {
		for (size_t col = 0; col < 3; col++) {
			if (row == 1 && col == 1) {
				continue;
			}

			struct wlr_fbox src_box = {
				.x = src_x[col],
				.y = src_y[row],
				.width = src_w[col],
				.height = src_h[row],
			};
			struct wlr_box dst_box = {
				.x = dst_x[col],
				.y = dst_y[row],
				.width = dst_w[col],
				.height = dst_h[row],
			};
			render_shadow_part(shadow, data, clip,
				x, y, &src_box, &dst_box);
		}
	}
}
