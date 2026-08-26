#include "texture/pixman/texture.h"
#include "util/wsm_log.h"
#include "renderer/pixman/renderer.h"
#include "renderer/pixman/pixel_format.h"

#include <stdlib.h>
#include <assert.h>

#include <wlr/render/interface.h>

static bool pixman_texture_read_pixels(struct wlr_texture *texture,
		const struct wlr_texture_read_pixels_options *options) {
	struct wsm_pixman_texture *pixman_texture =
		wsm_pixman_texture_from_texture(texture);

	struct wlr_box src;
	wlr_texture_read_pixels_options_get_src_box(options, texture, &src);

	pixman_format_code_t fmt = get_pixman_format_from_drm(options->format);
	if (fmt == 0) {
		wsm_log(WSM_ERROR, "Cannot read pixels: unsupported pixel format");
		return false;
	}

	void *p = wlr_texture_read_pixel_options_get_data(options);

	pixman_image_t *dst = pixman_image_create_bits_no_clear(fmt,
			src.width, src.height, p, options->stride);

	pixman_image_composite32(PIXMAN_OP_SRC, pixman_texture->image, NULL, dst,
			src.x, src.y, 0, 0, 0, 0, src.width, src.height);

	pixman_image_unref(dst);

	return true;
}

static uint32_t pixman_texture_preferred_read_format(struct wlr_texture *texture) {
	struct wsm_pixman_texture *pixman_texture =
		wsm_pixman_texture_from_texture(texture);

	pixman_format_code_t pixman_format =
		pixman_image_get_format(pixman_texture->image);
	return get_drm_format_from_pixman(pixman_format);
}

static void pixman_texture_destroy(struct wlr_texture *texture) {
	struct wsm_pixman_texture *pixman_texture =
		wsm_pixman_texture_from_texture(texture);

	wsm_pixman_texture_destroy(pixman_texture);
}

static const struct wlr_texture_impl texture_impl = {
	.read_pixels = pixman_texture_read_pixels,
	.preferred_read_format = pixman_texture_preferred_read_format,
	.destroy = pixman_texture_destroy,
};

struct wsm_pixman_texture *wsm_pixman_texture_create(
		struct wsm_pixman_renderer *renderer, uint32_t drm_format,
	uint32_t width, uint32_t height) {
	struct wsm_pixman_texture *texture = calloc(1, sizeof(*texture));
	if (texture == NULL) {
		wsm_log_errno(WSM_ERROR, "Allocation wsm_pixman_texture failed");
		return NULL;
	}

	wlr_texture_init(&texture->base, &renderer->base,
		&texture_impl, width, height);

	texture->format_info = drm_get_pixel_format_info(drm_format);
	if (texture->format_info == NULL) {
		wsm_log(WSM_ERROR, "Unsupported drm format 0x%"PRIX32, drm_format);
		free(texture);
		return NULL;
	}

	texture->format = get_pixman_format_from_drm(drm_format);
	if (texture->format == 0) {
		wsm_log(WSM_ERROR, "Unsupported pixman drm format 0x%"PRIX32,
				drm_format);
		free(texture);
		return NULL;
	}

	wl_list_insert(&renderer->textures, &texture->link);

	return texture;
}

void wsm_pixman_texture_destroy(struct wsm_pixman_texture *texture) {
	if (texture == NULL) {
		return;
	}

	wl_list_remove(&texture->link);
	pixman_image_unref(texture->image);
	wlr_buffer_unlock(texture->buffer);
	free(texture->user_data);
	free(texture);
}

struct wsm_pixman_texture *wsm_pixman_texture_from_buffer(
		struct wsm_pixman_renderer *pixman_renderer,struct wlr_buffer *buffer) {
	void *data = NULL;
	uint32_t drm_format;
	size_t stride;
	if (!wlr_buffer_begin_data_ptr_access(buffer, WLR_BUFFER_DATA_PTR_ACCESS_READ,
			&data, &drm_format, &stride)) {
		return NULL;
	}
	wlr_buffer_end_data_ptr_access(buffer);

	struct wsm_pixman_texture *texture =
		wsm_pixman_texture_create(pixman_renderer,
		drm_format, buffer->width, buffer->height);
	if (texture == NULL) {
		return NULL;
	}

	texture->image = pixman_image_create_bits_no_clear(texture->format,
		buffer->width, buffer->height, data, stride);
	if (texture->image == NULL) {
		wsm_log(WSM_ERROR, "Failed to create pixman image");
		wl_list_remove(&texture->link);
		free(texture);
		return NULL;
	}

	texture->buffer = wlr_buffer_lock(buffer);

	return texture;
}

bool wlr_texture_is_wsm_pixman(const struct wlr_texture *texture) {
	return texture != NULL && texture->impl == &texture_impl;
}

struct wsm_pixman_texture *wsm_pixman_texture_from_texture(
		struct wlr_texture *texture) {
	assert(texture->impl == &texture_impl);

	struct wsm_pixman_texture *pixman_texture =
		wl_container_of(texture, pixman_texture, base);
	return pixman_texture;
}
