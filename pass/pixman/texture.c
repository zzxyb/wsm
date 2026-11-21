#include "pass/pixman/texture.h"
#include "pass/pixman/pass.h"
#include "pass/texture.h"
#include "texture/pixman/texture.h"
#include <assert.h>

#include <math.h>
#include <stdint.h>

void wsm_pixman_render_pass_add_texture(struct wlr_render_pass *wlr_pass,
		const struct wlr_render_texture_options *options, const float *radii) {
	struct wsm_pixman_render_pass *pass = wsm_pixman_render_pass_from_render_pass(wlr_pass);
	struct wsm_pixman_texture *texture = wsm_pixman_texture_from_texture(options->texture);
	struct wsm_pixman_buffer *buffer = pass->buffer;

	if (texture->buffer != NULL && !wsm_renderer_begin_pixman_data_ptr_access(texture->buffer,
			&texture->image, WLR_BUFFER_DATA_PTR_ACCESS_READ)) {
		return;
	}

	struct wlr_fbox src_fbox;
	wlr_render_texture_options_get_src_box(options, &src_fbox);
	struct wlr_box src_box = {
		.x = roundf(src_fbox.x),
		.y = roundf(src_fbox.y),
		.width = roundf(src_fbox.width),
		.height = roundf(src_fbox.height),
	};

	struct wlr_box dst_box;
	wlr_render_texture_options_get_dst_box(options, &dst_box);

	// Rotate the source size into destination coordinates
	struct wlr_box src_box_transformed;
	wlr_box_transform(&src_box_transformed, &src_box, options->transform,
		buffer->base->width, buffer->base->height);

	bool transformed = options->transform != WL_OUTPUT_TRANSFORM_NORMAL ||
		src_box_transformed.width != dst_box.width ||
		src_box_transformed.height != dst_box.height;
	if (transformed) {
		// Cosinus/sinus values are exact integers for enum wl_output_transform entries
		int tr_cos = 1, tr_sin = 0, tr_x = 0, tr_y = 0;
		switch (options->transform) {
		case WL_OUTPUT_TRANSFORM_NORMAL:
		case WL_OUTPUT_TRANSFORM_FLIPPED:
			break;
		case WL_OUTPUT_TRANSFORM_90:
		case WL_OUTPUT_TRANSFORM_FLIPPED_90:
			tr_cos = 0;
			tr_sin = 1;
			tr_y = src_box.width;
			break;
		case WL_OUTPUT_TRANSFORM_180:
		case WL_OUTPUT_TRANSFORM_FLIPPED_180:
			tr_cos = -1;
			tr_sin = 0;
			tr_x = src_box.width;
			tr_y = src_box.height;
			break;
		case WL_OUTPUT_TRANSFORM_270:
		case WL_OUTPUT_TRANSFORM_FLIPPED_270:
			tr_cos = 0;
			tr_sin = -1;
			tr_x = src_box.height;
			break;
		}

		// Pixman transforms are generally the opposite of what you expect because they
		// apply to the coordinate system rather than the image.  The comments here
		// refer to what happens to the image, so all the code between
		// pixman_transform_init_identity() and pixman_image_set_transform() is probably
		// best read backwards.  Also this means translations are in the opposite
		// direction, imagine them as moving the origin around rather than moving the
		// image.
		//
		// Beware that this doesn't work quite the same as wp_viewporter: We apply crop
		// before transform and scale, whereas it defines crop in post-transform-scale
		// coordinates.  But this only applies to internal wlroots code - the viewporter
		// extension code makes sure that to clients everything works as it should.

		struct pixman_transform transform;
		pixman_transform_init_identity(&transform);

		// Apply scaling to get to the dst_box size.  Because the scaling is applied last
		// it depends on the whether the rotation swapped width and height, which is why
		// we use src_box_transformed instead of src_box.
		pixman_transform_scale(&transform, NULL,
			pixman_double_to_fixed(src_box_transformed.width / (double)dst_box.width),
			pixman_double_to_fixed(src_box_transformed.height / (double)dst_box.height));

		// pixman rotates about the origin which again leaves everything outside of the
		// viewport.  Translate the result so that its new top-left corner is back at the
		// origin.
		pixman_transform_translate(&transform, NULL,
			-pixman_int_to_fixed(tr_x), -pixman_int_to_fixed(tr_y));

		// Apply the rotation
		pixman_transform_rotate(&transform, NULL,
			pixman_int_to_fixed(tr_cos), pixman_int_to_fixed(tr_sin));

		// Apply flip before rotation
		if (options->transform >= WL_OUTPUT_TRANSFORM_FLIPPED) {
			// The flip leaves everything left of the Y axis which is outside the
			// viewport. So translate everything back into the viewport.
			pixman_transform_translate(&transform, NULL,
				-pixman_int_to_fixed(src_box.width), pixman_int_to_fixed(0));
			// Flip by applying a scale of -1 to the X axis
			pixman_transform_scale(&transform, NULL,
				pixman_int_to_fixed(-1), pixman_int_to_fixed(1));
		}

		// Apply the translation for source crop so the origin is now at the top-left of
		// the region we're actually using.  Do this last so all the other transforms
		// apply on top of this.
		pixman_transform_translate(&transform, NULL,
			pixman_int_to_fixed(src_box.x), pixman_int_to_fixed(src_box.y));

		pixman_image_set_transform(texture->image, &transform);

		switch (options->filter_mode) {
		case WLR_SCALE_FILTER_BILINEAR:
			pixman_image_set_repeat(texture->image, PIXMAN_REPEAT_PAD);
			pixman_image_set_filter(texture->image, PIXMAN_FILTER_BILINEAR, NULL, 0);
			break;
		case WLR_SCALE_FILTER_NEAREST:
			pixman_image_set_filter(texture->image, PIXMAN_FILTER_NEAREST, NULL, 0);
			break;
		}

	} else {
		pixman_image_set_transform(texture->image, NULL);
	}

	assert(options->clip);
	pixman_region32_t clip;
	pixman_region32_init(&clip);
	pixman_region32_intersect_rect(&clip, options->clip, 0, 0,
		buffer->base->width, buffer->base->height);
	float alpha = wlr_render_texture_options_get_alpha(options);
	int count;
	const pixman_box32_t *rects = pixman_region32_rectangles(&clip, &count);
	int mask_width = 0, mask_height = 0;
	for (int i = 0; i < count; i++) {
		int width = rects[i].x2 - rects[i].x1;
		int height = rects[i].y2 - rects[i].y1;
		mask_width = width > mask_width ? width : mask_width;
		mask_height = height > mask_height ? height : mask_height;
	}
	pixman_image_t *mask = count > 0 ?
		pixman_image_create_bits(PIXMAN_a8, mask_width, mask_height, NULL, 0) : NULL;
	for (int i = 0; mask && i < count; i++) {
		const pixman_box32_t *rect = &rects[i];
		int width = rect->x2 - rect->x1, height = rect->y2 - rect->y1;
		int dx = rect->x1 - dst_box.x, dy = rect->y1 - dst_box.y;
		uint8_t *pixels = (uint8_t *)pixman_image_get_data(mask);
		int stride = pixman_image_get_stride(mask);
		for (int y = 0; y < height; y++) {
			for (int x = 0; x < width; x++) {
				pixels[(size_t)y * stride + x] = roundf(255 * alpha *
					wsm_render_texture_corner_coverage(dst_box.width, dst_box.height,
						radii, dx + x + .5f, dy + y + .5f));
			}
		}
		// A transformed source already includes the source-box origin.
		pixman_image_composite32(PIXMAN_OP_OVER, texture->image, mask, buffer->image,
			dx + (transformed ? 0 : src_box.x), dy + (transformed ? 0 : src_box.y),
			0, 0, rect->x1, rect->y1, width, height);
	}
	if (mask) {
		pixman_image_unref(mask);
	}
	pixman_region32_fini(&clip);
	pixman_image_set_transform(texture->image, NULL);
	if (texture->buffer != NULL) {
		wlr_buffer_end_data_ptr_access(texture->buffer);
	}
}
