#include <unistd.h>
#include "pass/gles/blit.h"
#include "pass/gles/pass.h"
#include "texture/gles/texture.h"
#include "util/wsm_log.h"

/* Core ES3 framebuffer tokens, absent from the GLES2 headers. */
#ifndef GL_READ_FRAMEBUFFER
#define GL_READ_FRAMEBUFFER 0x8CA8
#define GL_DRAW_FRAMEBUFFER 0x8CA9
#define GL_READ_FRAMEBUFFER_BINDING 0x8CAA
#define GL_DRAW_FRAMEBUFFER_BINDING 0x8CA6
#endif

/* Only existing render targets qualify as framebuffer-backed textures. */
static GLuint texture_framebuffer(struct wsm_gles2_texture *texture) {
	return texture->fbo ? texture->fbo : texture->buffer ? texture->buffer->fbo : 0;
}

static bool blit(struct wsm_gles2_renderer *renderer,
		const struct wsm_blit_texture_options *options) {
	struct wsm_gles2_texture *source = wsm_gles2_texture_from_texture(options->texture);
	struct wsm_gles2_texture *destination = wsm_gles2_texture_from_texture(options->dst_texture);
	GLuint src_fbo = texture_framebuffer(source);
	GLuint dst_fbo = texture_framebuffer(destination);
	if (src_fbo == 0 || !glIsFramebuffer(src_fbo)) {
		return false;
	}
	const struct wlr_box *src = &options->src_box, *dst = &options->dst_box;
	bool has_blit = renderer->procs.glBlitFramebuffer != NULL;
	if (dst_fbo != 0) {
		if (!has_blit || src_fbo == dst_fbo || !glIsFramebuffer(dst_fbo)) {
			return false;
		}
	} else {
		/* A plain texture has no draw framebuffer: only a direct pixel copy is possible. */
		if (destination->target != GL_TEXTURE_2D || destination->tex == 0 ||
				src->width <= 0 || src->height <= 0 ||
				src->width != dst->width || src->height != dst->height) {
			return false;
		}
	}

	wsm_gles2_push_debug(renderer);
	GLint read_fbo, draw_fbo, binding = 0;
	if (has_blit) {
		glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &read_fbo);
		glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &draw_fbo);
		glBindFramebuffer(GL_READ_FRAMEBUFFER, src_fbo);
	} else {
		glGetIntegerv(GL_FRAMEBUFFER_BINDING, &read_fbo);
		draw_fbo = read_fbo;
		glBindFramebuffer(GL_FRAMEBUFFER, src_fbo);
	}
	GLboolean scissor = glIsEnabled(GL_SCISSOR_TEST);
	bool ok = false;
	if (glCheckFramebufferStatus(has_blit ? GL_READ_FRAMEBUFFER : GL_FRAMEBUFFER) !=
			GL_FRAMEBUFFER_COMPLETE) {
		goto out;
	}
	if (dst_fbo == 0) {
		glGetIntegerv(GL_TEXTURE_BINDING_2D, &binding);
		glBindTexture(GL_TEXTURE_2D, destination->tex);
		glCopyTexSubImage2D(GL_TEXTURE_2D, 0, dst->x, dst->y,
			src->x, src->y, src->width, src->height);
		glBindTexture(GL_TEXTURE_2D, binding);
	} else {
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, dst_fbo);
		if (glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
			goto out;
		}
		/* The caller supplies copy rectangles independently of render-pass scissoring. */
		glDisable(GL_SCISSOR_TEST);
		renderer->procs.glBlitFramebuffer(src->x, src->y,
			src->x + src->width, src->y + src->height, dst->x, dst->y,
			dst->x + dst->width, dst->y + dst->height, GL_COLOR_BUFFER_BIT,
			options->filter_mode == WLR_SCALE_FILTER_NEAREST ? GL_NEAREST : GL_LINEAR);
	}
	ok = glGetError() == GL_NO_ERROR;

 out:
	if (has_blit) {
		glBindFramebuffer(GL_READ_FRAMEBUFFER, read_fbo);
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, draw_fbo);
	} else {
		glBindFramebuffer(GL_FRAMEBUFFER, read_fbo);
	}
	if (scissor) {
		glEnable(GL_SCISSOR_TEST);
	}
	wsm_gles2_pop_debug(renderer);
	return ok;
}

bool wsm_gles2_render_pass_blit(struct wlr_render_pass *wlr_pass,
		const struct wsm_blit_texture_options *options) {
	struct wsm_gles2_render_pass *pass = wlr_gles2_render_pass_from_render_pass(wlr_pass);
	struct wsm_gles2_renderer *renderer = pass->buffer->renderer;
	if (options->texture->renderer != &renderer->base ||
			!wlr_texture_is_wsm_gles2(options->texture) ||
			!wlr_texture_is_wsm_gles2(options->dst_texture)) {
		return false;
	}
	struct wsm_gles2_texture *src = wsm_gles2_texture_from_texture(options->texture);
	struct wsm_gles2_texture *dst = wsm_gles2_texture_from_texture(options->dst_texture);
	if ((src->tex != 0 && src->tex == dst->tex) || dst->buffer == pass->buffer ||
			(src->buffer != NULL && src->buffer == dst->buffer)) {
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

	return blit(renderer, options);
}
