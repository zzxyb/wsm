#include <assert.h>
#include <stdlib.h>
#include "pass/gles/copy.h"
#include "pass/gles/pass.h"
#include "texture/gles/texture.h"
#include "util/wsm_log.h"

#include <unistd.h>
#include <drm_fourcc.h>

static uint32_t texture_format(struct wsm_gles2_texture *texture) {
	if (texture->buffer != NULL) {
		struct wlr_dmabuf_attributes dmabuf;
		if (wlr_buffer_get_dmabuf(texture->buffer->base, &dmabuf)) {
			return dmabuf.format;
		}
	}
	return texture->drm_format;
}

static bool render_pass_copy(struct wlr_render_pass *wlr_pass,
		const struct wsm_copy_texture_options *options) {
	if (!wlr_render_pass_is_wsm_gles2(wlr_pass)) {
		return false;
	}
	struct wsm_gles2_render_pass *pass = wlr_gles2_render_pass_from_render_pass(wlr_pass);
	struct wsm_gles2_renderer *renderer = pass->buffer->renderer;
	if (!renderer->procs.glCopyImageSubDataEXT ||
			options->texture->renderer != &renderer->base ||
			!wlr_texture_is_wsm_gles2(options->texture) ||
			!wlr_texture_is_wsm_gles2(options->dst_texture)) {
		return false;
	}
	struct wsm_gles2_texture *src = wsm_gles2_texture_from_texture(options->texture);
	struct wsm_gles2_texture *dst = wsm_gles2_texture_from_texture(options->dst_texture);
	if (src->target != GL_TEXTURE_2D || dst->target != GL_TEXTURE_2D ||
			src->tex == dst->tex || src->buffer == pass->buffer || dst->buffer == pass->buffer ||
			texture_format(src) == DRM_FORMAT_INVALID ||
			texture_format(src) != texture_format(dst)) {
		return false;
	}
	if (options->wait_timeline != NULL) {
		int fd = wlr_drm_syncobj_timeline_export_sync_file(
			options->wait_timeline, options->wait_point);
		if (fd < 0) {
			return false;
		}
		EGLSyncKHR sync = wsm_egl_create_sync(renderer->egl, fd);
		close(fd);
		if (sync == EGL_NO_SYNC_KHR) {
			return false;
		}
		bool ok = wsm_egl_wait_sync(renderer->egl, sync);
		wsm_egl_destroy_sync(renderer->egl, sync);
		if (!ok) {
			return false;
		}
	}

	wsm_gles2_push_debug(renderer);
	GLint binding, src_filter, dst_filter;
	glGetIntegerv(GL_TEXTURE_BINDING_2D, &binding);
	glBindTexture(GL_TEXTURE_2D, src->tex);
	glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, &src_filter);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glBindTexture(GL_TEXTURE_2D, dst->tex);
	glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, &dst_filter);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	renderer->procs.glCopyImageSubDataEXT(src->tex, src->target, 0,
		options->src_x, options->src_y, 0, dst->tex, dst->target, 0,
		options->dst_x, options->dst_y, 0, options->width, options->height, 1);
	bool ok = glGetError() == GL_NO_ERROR;
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, dst_filter);
	glBindTexture(GL_TEXTURE_2D, src->tex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, src_filter);
	glBindTexture(GL_TEXTURE_2D, binding);
	wsm_gles2_pop_debug(renderer);
	return ok;
}

static void copy_pass_destroy(struct wsm_render_pass_copy *pass) {
	free(wsm_render_pass_copy_gles_from_pass(pass));
}

static const struct wsm_copy_texture_impl copy_impl = {
	.copy = render_pass_copy,
	.destroy = copy_pass_destroy,
};

struct wsm_render_pass_copy *wsm_render_pass_copy_gles_create(struct wlr_renderer *renderer) {
	if (renderer == NULL || !wlr_renderer_is_wsm_gles2(renderer)) {
		return NULL;
	}
	if (wsm_gles2_renderer_from_renderer(renderer)->procs.glCopyImageSubDataEXT == NULL) {
		return NULL;
	}
	struct wsm_render_pass_copy_gles *pass = calloc(1, sizeof(*pass));
	if (pass == NULL) {
		return NULL;
	}
	wsm_render_pass_copy_init(&pass->base, &copy_impl, renderer);
	return &pass->base;
}

bool wsm_render_pass_copy_is_gles(const struct wsm_render_pass_copy *pass) {
	return pass != NULL && pass->impl == &copy_impl;
}

struct wsm_render_pass_copy_gles *wsm_render_pass_copy_gles_from_pass(
		struct wsm_render_pass_copy *pass) {
	assert(wsm_render_pass_copy_is_gles(pass));
	struct wsm_render_pass_copy_gles *gles_pass = wl_container_of(pass, gles_pass, base);
	return gles_pass;
}
