#include <assert.h>
#include <stdlib.h>
#include "pass/pixman/blit.h"
#include "pass/pixman/pass.h"
#include "texture/pixman/texture.h"

static pixman_image_t *image_view(pixman_image_t *image) {
	return pixman_image_create_bits_no_clear(pixman_image_get_format(image),
		pixman_image_get_width(image), pixman_image_get_height(image),
		pixman_image_get_data(image), pixman_image_get_stride(image));
}

static bool render_pass_blit(struct wlr_render_pass *wlr_pass,
		const struct wsm_blit_texture_options *options) {
	if (!wlr_render_pass_is_wsm_pixman(wlr_pass)) {
		return false;
	}
	struct wsm_pixman_render_pass *pass = wsm_pixman_render_pass_from_render_pass(wlr_pass);
	if (options->wait_timeline != NULL ||
			options->texture->renderer != &pass->buffer->renderer->base ||
			!wlr_texture_is_wsm_pixman(options->texture) ||
			!wlr_texture_is_wsm_pixman(options->dst_texture)) {
		return false;
	}
	struct wsm_pixman_texture *src = wsm_pixman_texture_from_texture(options->texture);
	struct wsm_pixman_texture *dst = wsm_pixman_texture_from_texture(options->dst_texture);
	if (src->buffer == pass->buffer->base || dst->buffer == pass->buffer->base ||
			(src->buffer != NULL && src->buffer == dst->buffer)) {
		return false;
	}
	if (src->buffer != NULL && !wsm_renderer_begin_pixman_data_ptr_access(
			src->buffer, &src->image, WLR_BUFFER_DATA_PTR_ACCESS_READ)) {
		return false;
	}
	bool ok = false;
	if (dst->buffer != NULL && !wsm_renderer_begin_pixman_data_ptr_access(
			dst->buffer, &dst->image, WLR_BUFFER_DATA_PTR_ACCESS_WRITE)) {
		goto out_src;
	}
	pixman_image_t *src_image = image_view(src->image);
	pixman_image_t *dst_image = image_view(dst->image);
	if (src_image != NULL && dst_image != NULL &&
			pixman_image_get_data(src_image) != pixman_image_get_data(dst_image)) {
		const struct wlr_box *s = &options->src_box, *d = &options->dst_box;
		int dx = d->width < 0 ? d->x + d->width : d->x;
		int dy = d->height < 0 ? d->y + d->height : d->y;
		double sx = (double)s->width / d->width;
		double sy = (double)s->height / d->height;
		/* Map local destination pixel centers into source texture coordinates. */
		pixman_f_transform_t mapping = { .m = {
			{sx, 0, s->x + (dx - d->x) * sx},
			{0, sy, s->y + (dy - d->y) * sy},
			{0, 0, 1},
		} };
		pixman_transform_t transform;
		if (pixman_transform_from_pixman_f_transform(&transform, &mapping) &&
				pixman_image_set_transform(src_image, &transform) &&
				pixman_image_set_filter(src_image,
					options->filter_mode == WLR_SCALE_FILTER_NEAREST ?
					PIXMAN_FILTER_NEAREST : PIXMAN_FILTER_BILINEAR, NULL, 0)) {
			pixman_image_set_repeat(src_image, PIXMAN_REPEAT_PAD);
			pixman_image_composite32(PIXMAN_OP_SRC, src_image, NULL, dst_image,
				0, 0, 0, 0, dx, dy, abs(d->width), abs(d->height));
			ok = true;
		}
	}
	if (src_image != NULL) {
		pixman_image_unref(src_image);
	}
	if (dst_image != NULL) {
		pixman_image_unref(dst_image);
	}
	if (dst->buffer != NULL) {
		wlr_buffer_end_data_ptr_access(dst->buffer);
	}
 out_src:
	if (src->buffer != NULL) {
		wlr_buffer_end_data_ptr_access(src->buffer);
	}
	return ok;
}

static void blit_pass_destroy(struct wsm_render_pass_blit *pass) {
	free(wsm_render_pass_blit_pixman_from_pass(pass));
}

static const struct wsm_blit_texture_impl blit_impl = {
	.blit = render_pass_blit,
	.destroy = blit_pass_destroy,
};

struct wsm_render_pass_blit *wsm_render_pass_blit_pixman_create(struct wlr_renderer *renderer) {
	if (renderer == NULL || !wlr_renderer_is_wsm_pixman(renderer)) {
		return NULL;
	}
	struct wsm_render_pass_blit_pixman *pass = calloc(1, sizeof(*pass));
	if (pass == NULL) {
		return NULL;
	}
	wsm_render_pass_blit_init(&pass->base, &blit_impl, renderer);
	return &pass->base;
}

bool wsm_render_pass_blit_is_pixman(const struct wsm_render_pass_blit *pass) {
	return pass != NULL && pass->impl == &blit_impl;
}

struct wsm_render_pass_blit_pixman *wsm_render_pass_blit_pixman_from_pass(
		struct wsm_render_pass_blit *pass) {
	assert(wsm_render_pass_blit_is_pixman(pass));
	struct wsm_render_pass_blit_pixman *pixman_pass = wl_container_of(pass, pixman_pass, base);
	return pixman_pass;
}
