#include <assert.h>
#include <stdlib.h>
#include <unistd.h>
#include <drm_fourcc.h>
#include "pass/gles/blit.h"
#include "pass/gles/pass.h"
#include "texture/gles/texture.h"
#include "util/wsm_log.h"

/* Core ES3 and the NV/ANGLE extensions use the same framebuffer tokens. */
#ifndef GL_READ_FRAMEBUFFER
#define GL_READ_FRAMEBUFFER 0x8CA8
#define GL_DRAW_FRAMEBUFFER 0x8CA9
#define GL_READ_FRAMEBUFFER_BINDING 0x8CAA
#define GL_DRAW_FRAMEBUFFER_BINDING 0x8CA6
#endif

static uint32_t texture_format(struct wsm_gles2_texture *texture) {
	if (texture->buffer != NULL) {
		struct wlr_dmabuf_attributes dmabuf;
		if (wlr_buffer_get_dmabuf(texture->buffer->base, &dmabuf)) {
			return dmabuf.format;
		}
	}
	return texture->drm_format;
}

static bool render_pass_blit(struct wlr_render_pass *wlr_pass,
		const struct wsm_blit_texture_options *options) {
	if (!wlr_render_pass_is_wsm_gles2(wlr_pass)) {
		return false;
	}
	struct wsm_gles2_render_pass *pass = wlr_gles2_render_pass_from_render_pass(wlr_pass);
	struct wsm_gles2_renderer *renderer = pass->buffer->renderer;
	if (renderer->procs.glBlitFramebuffer == NULL ||
			options->texture->renderer != &renderer->base ||
			!wlr_texture_is_wsm_gles2(options->texture) ||
			!wlr_texture_is_wsm_gles2(options->dst_texture)) {
		return false;
	}
	struct wsm_gles2_texture *src = wsm_gles2_texture_from_texture(options->texture);
	struct wsm_gles2_texture *dst = wsm_gles2_texture_from_texture(options->dst_texture);
	if (src->target != GL_TEXTURE_2D || dst->target != GL_TEXTURE_2D ||
			src->tex == dst->tex || src->buffer == pass->buffer || dst->buffer == pass->buffer ||
			(src->buffer != NULL && src->buffer == dst->buffer)) {
		return false;
	}
	const struct wlr_box *s = &options->src_box, *d = &options->dst_box;
	if (renderer->exts.ANGLE_framebuffer_blit &&
			(options->filter_mode != WLR_SCALE_FILTER_NEAREST ||
			 s->width != d->width || s->height != d->height ||
			 texture_format(src) == DRM_FORMAT_INVALID ||
			 texture_format(src) != texture_format(dst))) {
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
	GLint read_fbo, draw_fbo;
	glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &read_fbo);
	glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &draw_fbo);
	GLboolean scissor = glIsEnabled(GL_SCISSOR_TEST);
	GLuint fbos[2] = {0};
	glGenFramebuffers(2, fbos);
	bool ok = false;
	if (fbos[0] == 0 || fbos[1] == 0) {
		goto out;
	}
	glBindFramebuffer(GL_READ_FRAMEBUFFER, fbos[0]);
	glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
		GL_TEXTURE_2D, src->tex, 0);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbos[1]);
	glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
		GL_TEXTURE_2D, dst->tex, 0);
	if (glCheckFramebufferStatus(GL_READ_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE ||
			glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
		goto out;
	}
	/* Ignore damage scissoring left by earlier draws in this pass. */
	glDisable(GL_SCISSOR_TEST);
	renderer->procs.glBlitFramebuffer(s->x, s->y, s->x + s->width, s->y + s->height,
		d->x, d->y, d->x + d->width, d->y + d->height, GL_COLOR_BUFFER_BIT,
		options->filter_mode == WLR_SCALE_FILTER_NEAREST ? GL_NEAREST : GL_LINEAR);
	ok = glGetError() == GL_NO_ERROR;
 out:
	glBindFramebuffer(GL_READ_FRAMEBUFFER, read_fbo);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, draw_fbo);
	if (scissor) {
		glEnable(GL_SCISSOR_TEST);
	}
	glDeleteFramebuffers(2, fbos);
	wsm_gles2_pop_debug(renderer);
	return ok;
}

static void blit_pass_destroy(struct wsm_render_pass_blit *pass) {
	free(wsm_render_pass_blit_gles_from_pass(pass));
}

static const struct wsm_blit_texture_impl blit_impl = {
	.blit = render_pass_blit,
	.destroy = blit_pass_destroy,
};

struct wsm_render_pass_blit *wsm_render_pass_blit_gles_create(struct wlr_renderer *renderer) {
	if (renderer == NULL || !wlr_renderer_is_wsm_gles2(renderer) ||
			wsm_gles2_renderer_from_renderer(renderer)->procs.glBlitFramebuffer == NULL) {
		return NULL;
	}
	struct wsm_render_pass_blit_gles *pass = calloc(1, sizeof(*pass));
	if (pass == NULL) {
		return NULL;
	}
	wsm_render_pass_blit_init(&pass->base, &blit_impl, renderer);
	return &pass->base;
}

bool wsm_render_pass_blit_is_gles(const struct wsm_render_pass_blit *pass) {
	return pass != NULL && pass->impl == &blit_impl;
}

struct wsm_render_pass_blit_gles *wsm_render_pass_blit_gles_from_pass(
		struct wsm_render_pass_blit *pass) {
	assert(wsm_render_pass_blit_is_gles(pass));
	struct wsm_render_pass_blit_gles *gles_pass = wl_container_of(pass, gles_pass, base);
	return gles_pass;
}
