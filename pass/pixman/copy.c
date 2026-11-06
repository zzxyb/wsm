#include "pass/pixman/copy.h"
#include "pass/pixman/pass.h"
#include "texture/pixman/texture.h"

static pixman_image_t *image_view(pixman_image_t *image) {
	return pixman_image_create_bits_no_clear(pixman_image_get_format(image),
		pixman_image_get_width(image), pixman_image_get_height(image),
		pixman_image_get_data(image), pixman_image_get_stride(image));
}

bool wsm_pixman_render_pass_copy(struct wlr_render_pass *wlr_pass,
		const struct wsm_copy_texture_options *options) {
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
			(src->buffer != NULL && src->buffer == dst->buffer) || src->format != dst->format) {
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
		pixman_image_composite32(PIXMAN_OP_SRC, src_image, NULL, dst_image,
			options->src_x, options->src_y, 0, 0, options->dst_x, options->dst_y,
			options->width, options->height);
		ok = true;
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
