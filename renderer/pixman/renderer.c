#include "renderer/pixman/renderer.h"
#include "util/wsm_log.h"
#include "texture/pixman/texture.h"
#include "buffer/pixman/buffer.h"
#include "pass/pixman/pass.h"
#include "renderer/pixman/pixel_format.h"

#include <stdlib.h>
#include <drm_fourcc.h>
#include <assert.h>

#include <wlr/types/wlr_buffer.h>
#include <wlr/render/wlr_texture.h>
#include <wlr/render/interface.h>

static const struct wlr_drm_format_set *pixman_get_texture_formats(
		struct wlr_renderer *renderer, uint32_t buffer_caps) {
	struct wsm_pixman_renderer *pixman_renderer =
		wsm_pixman_renderer_from_renderer(renderer);
	if (buffer_caps & WLR_BUFFER_CAP_DATA_PTR) {
		return &pixman_renderer->drm_formats;
	} else {
		return NULL;
	}
}

static const struct wlr_drm_format_set *pixman_get_render_formats(
		struct wlr_renderer *renderer) {
	struct wsm_pixman_renderer *pixman_renderer =
		wsm_pixman_renderer_from_renderer(renderer);
	return &pixman_renderer->drm_formats;
}

static void pixman_destroy(struct wlr_renderer *renderer) {
	struct wsm_pixman_renderer *pixman_renderer =
		wsm_pixman_renderer_from_renderer(renderer);

	struct wsm_pixman_buffer *buffer, *buffer_tmp;
	wl_list_for_each_safe(buffer, buffer_tmp, &pixman_renderer->buffers, link) {
		wsm_pixman_buffer_destroy(buffer);
	}

	struct wsm_pixman_texture *tex, *tex_tmp;
	wl_list_for_each_safe(tex, tex_tmp, &pixman_renderer->textures, link) {
		wlr_texture_destroy(&tex->base);
	}

	wlr_drm_format_set_finish(&pixman_renderer->drm_formats);

	free(renderer);
}

static struct wlr_texture *pixman_texture_from_buffer(
		struct wlr_renderer *renderer, struct wlr_buffer *buffer) {
	struct wsm_pixman_renderer *pixman_renderer =
		wsm_pixman_renderer_from_renderer(renderer);
	struct wsm_pixman_texture *texture =
		wsm_pixman_texture_from_buffer(pixman_renderer, buffer);

	return &texture->base ;
}

static struct wlr_render_pass *pixman_begin_buffer_pass(struct wlr_renderer *wlr_renderer,
		struct wlr_buffer *wlr_buffer, const struct wlr_buffer_pass_options *options) {
	struct wsm_pixman_renderer *renderer =
		wsm_pixman_renderer_from_renderer(wlr_renderer);

	struct wsm_pixman_buffer *buffer =
		wsm_pixman_buffer_from_buffer(renderer, wlr_buffer);
	if (buffer == NULL) {
		buffer = wsm_pixman_buffer_create(renderer, wlr_buffer);
	}
	if (buffer == NULL) {
		return NULL;
	}

	struct wsm_pixman_render_pass *pass = wsm_render_pass_begin_pixman_render_pass(buffer);
	if (pass == NULL) {
		return NULL;
	}
	return &pass->base;
}

static const struct wlr_renderer_impl renderer_impl = {
	.get_texture_formats = pixman_get_texture_formats,
	.get_render_formats = pixman_get_render_formats,
	.destroy = pixman_destroy,
	.get_drm_fd = NULL,
	.texture_from_buffer = pixman_texture_from_buffer,
	.begin_buffer_pass = pixman_begin_buffer_pass,
	.render_timer_create = NULL,
};

struct wsm_pixman_renderer *wsm_pixman_renderer_create(void) {
	struct wsm_pixman_renderer *renderer = calloc(1, sizeof(*renderer));
	if (renderer == NULL) {
		wsm_log_errno(WSM_ERROR, "Allocation wsm_pixman_renderer failed");
		return NULL;
	}

	wsm_log(WSM_INFO, "Creating pixman renderer");
	wlr_renderer_init(&renderer->base, &renderer_impl, WLR_BUFFER_CAP_DATA_PTR);
	renderer->base.features.output_color_transform = false;
	wl_list_init(&renderer->buffers);
	wl_list_init(&renderer->textures);

	size_t len = 0;
	const uint32_t *drm_formats = get_pixman_drm_formats(&len);

	for (size_t i = 0; i < len; ++i) {
		wlr_drm_format_set_add(&renderer->drm_formats, drm_formats[i],
			DRM_FORMAT_MOD_INVALID);
		wlr_drm_format_set_add(&renderer->drm_formats, drm_formats[i],
			DRM_FORMAT_MOD_LINEAR);
	}

	return renderer;
}

bool wlr_renderer_is_wsm_pixman(const struct wlr_renderer *renderer) {
	return renderer != NULL && renderer->impl == &renderer_impl;
}

struct wsm_pixman_renderer *wsm_pixman_renderer_from_renderer(
		struct wlr_renderer *renderer) {
	assert(renderer->impl == &renderer_impl);
	struct wsm_pixman_renderer *pixman_renderer =
		wl_container_of(renderer, pixman_renderer, base);

	return pixman_renderer;
}

pixman_image_t *wsm_pixman_renderer_get_buffer_image(
		struct wlr_renderer *wlr_renderer, struct wlr_buffer *wlr_buffer) {
	struct wsm_pixman_renderer *renderer = wsm_pixman_renderer_from_renderer(wlr_renderer);
	struct wsm_pixman_buffer *buffer = wsm_pixman_buffer_from_buffer(renderer, wlr_buffer);
	if (!buffer) {
		buffer = wsm_pixman_buffer_create(renderer, wlr_buffer);
	}
	if (!buffer) {
		return NULL;
	}
	return buffer->image;
}

bool wsm_renderer_begin_pixman_data_ptr_access(struct wlr_buffer *wlr_buffer, pixman_image_t **image_ptr,
		uint32_t flags) {
	pixman_image_t *image = *image_ptr;

	void *data = NULL;
	uint32_t drm_format;
	size_t stride;
	if (!wlr_buffer_begin_data_ptr_access(wlr_buffer, flags,
			&data, &drm_format, &stride)) {
		return false;
	}

	// If the data pointer has changed, re-create the Pixman image. This can
	// happen if it's a client buffer and the wl_shm_pool has been resized.
	if (data != pixman_image_get_data(image)) {
		pixman_format_code_t format = get_pixman_format_from_drm(drm_format);
		assert(format != 0);

		pixman_image_t *new_image = pixman_image_create_bits_no_clear(format,
			wlr_buffer->width, wlr_buffer->height, data, stride);
		if (new_image == NULL) {
			wlr_buffer_end_data_ptr_access(wlr_buffer);
			return false;
		}

		pixman_image_unref(image);
		image = new_image;
	}

	*image_ptr = image;
	return true;
}

