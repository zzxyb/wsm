#include "buffer/pixman/buffer.h"
#include "util/wsm_log.h"
#include "util/wsm_common.h"
#include "renderer/pixman/renderer.h"
#include "renderer/pixman/pixel_format.h"

#include <stdlib.h>
#include <assert.h>

#include <wlr/interfaces/wlr_buffer.h>

static void readonly_data_buffer_destroy(struct wlr_buffer *wlr_buffer) {
	struct wsm_readonly_data_buffer *buffer =
		wsm_readonly_data_buffer_from_buffer(wlr_buffer);
	wlr_buffer_finish(wlr_buffer);
	free(buffer->saved_data);
	free(buffer);
}

static bool readonly_data_buffer_begin_data_ptr_access(struct wlr_buffer *wlr_buffer,
		uint32_t flags, void **data, uint32_t *format, size_t *stride) {
	struct wsm_readonly_data_buffer *buffer =
		wsm_readonly_data_buffer_from_buffer(wlr_buffer);
	if (buffer->data == NULL) {
		return false;
	}
	if (flags & WLR_BUFFER_DATA_PTR_ACCESS_WRITE) {
		return false;
	}
	*data = (void *)buffer->data;
	*format = buffer->format;
	*stride = buffer->stride;
	return true;
}

static void readonly_data_buffer_end_data_ptr_access(struct wlr_buffer *wlr_buffer) {
	W_UNUSED(wlr_buffer);
}

static const struct wlr_buffer_impl readonly_data_buffer_impl = {
	.destroy = readonly_data_buffer_destroy,
	.begin_data_ptr_access = readonly_data_buffer_begin_data_ptr_access,
	.end_data_ptr_access = readonly_data_buffer_end_data_ptr_access,
};

static void handle_destroy_buffer(struct wl_listener *listener, void *data) {
	struct wsm_pixman_buffer *buffer =
			wl_container_of(listener, buffer, buffer_destroy);
	wsm_pixman_buffer_destroy(buffer);
}

struct wsm_pixman_buffer *wsm_pixman_buffer_create(
		struct wsm_pixman_renderer *renderer, struct wlr_buffer *wlr_buffer) {
	struct wsm_pixman_buffer *buffer = calloc(1, sizeof(*buffer));
	if (buffer == NULL) {
		wsm_log_errno(WSM_ERROR, "Allocation wsm_pixman_buffer failed");
		return NULL;
	}
	buffer->base = wlr_buffer;
	buffer->renderer = renderer;

	void *data = NULL;
	uint32_t drm_format;
	size_t stride;
	if (!wlr_buffer_begin_data_ptr_access(wlr_buffer,
			WLR_BUFFER_DATA_PTR_ACCESS_READ | WLR_BUFFER_DATA_PTR_ACCESS_WRITE,
			&data, &drm_format, &stride)) {
		wsm_log(WSM_ERROR, "Failed to get buffer data");
		goto error_buffer;
	}
	wlr_buffer_end_data_ptr_access(wlr_buffer);

	pixman_format_code_t format = get_pixman_format_from_drm(drm_format);
	if (format == 0) {
		wsm_log(WSM_ERROR, "Unsupported pixman drm format 0x%"PRIX32,
						drm_format);
		goto error_buffer;
	}

	buffer->image = pixman_image_create_bits(format, wlr_buffer->width,
			wlr_buffer->height, data, stride);
	if (buffer->image == NULL) {
		wsm_log(WSM_ERROR, "Failed to allocate pixman image");
		goto error_buffer;
	}

	buffer->buffer_destroy.notify = handle_destroy_buffer;
	wl_signal_add(&wlr_buffer->events.destroy, &buffer->buffer_destroy);

	wl_list_insert(&renderer->buffers, &buffer->link);

	wsm_log(WSM_DEBUG, "Created pixman buffer %dx%d",
					wlr_buffer->width, wlr_buffer->height);

	return buffer;

error_buffer:
	free(buffer);
	return NULL;
}

bool wlr_buffer_is_wsm_pixman(struct wsm_pixman_renderer *renderer,
		const struct wlr_buffer *wlr_buffer) {
	if (wlr_buffer == NULL) {
		return false;
	}

	struct wsm_pixman_buffer *buffer;
	wl_list_for_each(buffer, &renderer->buffers, link) {
		if (buffer->base == wlr_buffer) {
			return true;
		}
	}

	return false;
}

void wsm_pixman_buffer_destroy(struct wsm_pixman_buffer *buffer) {
	if (buffer == NULL) {
		return;
	}

	wl_list_remove(&buffer->link);
	wl_list_remove(&buffer->buffer_destroy.link);

	pixman_image_unref(buffer->image);

	free(buffer);
}

struct wsm_pixman_buffer *wsm_pixman_buffer_from_buffer(
		struct wsm_pixman_renderer *renderer, const struct wlr_buffer *wlr_buffer) {
	struct wsm_pixman_buffer *buffer;
	wl_list_for_each(buffer, &renderer->buffers, link) {
		if (buffer->base == wlr_buffer) {
			return buffer;
		}
	}

	return NULL;
}

bool wlr_buffer_is_wsm_readonly_data(const struct wlr_buffer *wlr_buffer) {
	return wlr_buffer != NULL && wlr_buffer->impl == &readonly_data_buffer_impl;
}

struct wsm_readonly_data_buffer *wsm_readonly_data_buffer_from_buffer(
		struct wlr_buffer *wlr_buffer) {
	assert(wlr_buffer->impl == &readonly_data_buffer_impl);
	struct wsm_readonly_data_buffer *buffer = wl_container_of(wlr_buffer, buffer, base);
	return buffer;
}

bool wsm_pixman_buffer_begin_data_ptr_access(struct wlr_buffer *buffer,
		pixman_image_t **image_ptr, uint32_t flags) {
	pixman_image_t *image = *image_ptr;

	void *data = NULL;
	uint32_t drm_format;
	size_t stride;
	if (!wlr_buffer_begin_data_ptr_access(buffer, flags,
			&data, &drm_format, &stride)) {
		return false;
	}

	if (data != pixman_image_get_data(image)) {
		pixman_format_code_t format = get_pixman_format_from_drm(drm_format);
		assert(format != 0);

		pixman_image_t *new_image = pixman_image_create_bits_no_clear(format,
			buffer->width, buffer->height, data, stride);
		if (new_image == NULL) {
			wlr_buffer_end_data_ptr_access(buffer);
			return false;
		}

		pixman_image_unref(image);
		image = new_image;
	}

	*image_ptr = image;
	return true;
}

struct wsm_readonly_data_buffer *wsm_readonly_data_buffer_create(uint32_t format,
		size_t stride, uint32_t width, uint32_t height, const void *data) {
	struct wsm_readonly_data_buffer *buffer = calloc(1, sizeof(*buffer));
	if (buffer == NULL) {
		wsm_log_errno(WSM_ERROR, "Allocation wsm_readonly_data_buffer failed");
		return NULL;
	}
	wlr_buffer_init(&buffer->base, &readonly_data_buffer_impl, width, height);

	buffer->data = data;
	buffer->format = format;
	buffer->stride = stride;

	return buffer;
}

bool wsm_readonly_data_buffer_drop(struct wsm_readonly_data_buffer *buffer) {
	bool ok = true;

	if (buffer->base.n_locks > 0) {
		size_t size = buffer->stride * buffer->base.height;
		buffer->saved_data = malloc(size);
		if (buffer->saved_data == NULL) {
			wsm_log_errno(WSM_ERROR, "Allocation saved_data failed");
			ok = false;
			buffer->data = NULL;
		} else {
			memcpy(buffer->saved_data, buffer->data, size);
			buffer->data = buffer->saved_data;
		}
	}

	wlr_buffer_drop(&buffer->base);
	return ok;
}
