#include "buffer/gles/buffer.h"
#include "renderer/gles/egl.h"
#include "util/wsm_log.h"

#include <stdlib.h>

static void handle_buffer_destroy(struct wlr_addon *addon) {
	struct wsm_gles2_buffer *buffer =
		wl_container_of(addon, buffer, addon);
	wsm_gles2_buffer_destroy(buffer);
}

static const struct wlr_addon_interface buffer_addon_impl = {
	.name = "wlr_gles2_buffer",
	.destroy = handle_buffer_destroy,
};

struct wsm_gles2_buffer *wsm_gles2_buffer_get_or_create(struct wsm_gles2_renderer *renderer,
		struct wlr_buffer *wlr_buffer) {
	struct wlr_addon *addon =
		wlr_addon_find(&wlr_buffer->addons, renderer, &buffer_addon_impl);
	if (addon) {
		struct wsm_gles2_buffer *buffer = wl_container_of(addon, buffer, addon);
		return buffer;
	}

	struct wsm_gles2_buffer *buffer = calloc(1, sizeof(*buffer));
	if (buffer == NULL) {
		wsm_log_errno(WSM_ERROR, "Allocation failed");
		return NULL;
	}
	buffer->base = wlr_buffer;
	buffer->renderer = renderer;

	struct wlr_dmabuf_attributes dmabuf = {0};
	if (!wlr_buffer_get_dmabuf(wlr_buffer, &dmabuf)) {
		goto error_buffer;
	}

	buffer->image = wsm_egl_create_image_from_dmabuf(renderer->egl,
		&dmabuf, &buffer->external_only);
	if (buffer->image == EGL_NO_IMAGE_KHR) {
		goto error_buffer;
	}

	wlr_addon_init(&buffer->addon, &wlr_buffer->addons, renderer,
		&buffer_addon_impl);

	wl_list_insert(&renderer->buffers, &buffer->link);

	wsm_log(WSM_DEBUG, "Created GL FBO for buffer %dx%d",
		wlr_buffer->width, wlr_buffer->height);

	return buffer;

error_buffer:
	free(buffer);
	return NULL;
}

void wsm_gles2_buffer_destroy(struct wsm_gles2_buffer *buffer) {
	wl_list_remove(&buffer->link);
	wlr_addon_finish(&buffer->addon);

	struct wsm_egl_context prev_ctx;
	wsm_egl_make_current(buffer->renderer->egl, &prev_ctx);

	wsm_gles2_push_debug(buffer->renderer);

	glDeleteFramebuffers(1, &buffer->fbo);
	glDeleteRenderbuffers(1, &buffer->rbo);
	glDeleteTextures(1, &buffer->tex);

	wsm_gles2_pop_debug(buffer->renderer);

	wsm_egl_destroy_image(buffer->renderer->egl, buffer->image);
	wsm_egl_restore_context(&prev_ctx);

	free(buffer);
}

GLuint wsm_gles2_buffer_get_fbo(struct wsm_gles2_buffer *buffer) {
	if (buffer->external_only) {
		wsm_log(WSM_ERROR, "DMA-BUF format is external-only");
		return 0;
	}

	if (buffer->fbo) {
		return buffer->fbo;
	}

	 wsm_gles2_push_debug(buffer->renderer);

	if (!buffer->rbo) {
		glGenRenderbuffers(1, &buffer->rbo);
		glBindRenderbuffer(GL_RENDERBUFFER, buffer->rbo);
		buffer->renderer->procs.glEGLImageTargetRenderbufferStorageOES(GL_RENDERBUFFER,
			buffer->image);
		glBindRenderbuffer(GL_RENDERBUFFER, 0);
	}

	glGenFramebuffers(1, &buffer->fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, buffer->fbo);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
		GL_RENDERBUFFER, buffer->rbo);
	GLenum fb_status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	if (fb_status != GL_FRAMEBUFFER_COMPLETE) {
		wsm_log(WSM_ERROR, "Failed to create FBO");
		glDeleteFramebuffers(1, &buffer->fbo);
		buffer->fbo = 0;
	}

	wsm_gles2_pop_debug(buffer->renderer);

	return buffer->fbo;
}
