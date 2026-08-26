#include "renderer/gles/renderer.h"
#include "renderer/gles/pixel_format.h"
#include "renderer/gles/egl.h"
#include "texture/gles/texture.h"
#include "buffer/gles/buffer.h"
#include "util/wsm_log.h"
#include "util/wsm_common.h"
#include "util/wsm_matrix.h"
#include "renderer/gles/render_timer.h"
#include "pass/gles/pass.h"
#include "common_vert_src.h"
#include "quad_frag_src.h"
#include "tex_rgba_frag_src.h"
#include "tex_rgbx_frag_src.h"
#include "tex_external_frag_src.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include <drm_fourcc.h>

#include <xf86drm.h>

#include <wayland-util.h>

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include <wlr/types/wlr_buffer.h>
#include <wlr/render/interface.h>
#include <wlr/util/log.h>

static void gles2_destroy(struct wlr_renderer *wlr_renderer) {
	struct wsm_gles2_renderer *renderer = wsm_gles2_renderer_from_renderer(wlr_renderer);

	wsm_egl_make_current(renderer->egl, NULL);

	struct wsm_gles2_texture *tex, *tex_tmp;
	wl_list_for_each_safe(tex, tex_tmp, &renderer->textures, link) {
		wsm_gles2_texture_destroy(tex);
	}

	struct wsm_gles2_buffer *buffer, *buffer_tmp;
	wl_list_for_each_safe(buffer, buffer_tmp, &renderer->buffers, link) {
		wsm_gles2_buffer_destroy(buffer);
	}

	wsm_gles2_push_debug(renderer);
	glDeleteProgram(renderer->shaders.quad.program);
	glDeleteProgram(renderer->shaders.tex_rgba.program);
	glDeleteProgram(renderer->shaders.tex_rgbx.program);
	glDeleteProgram(renderer->shaders.tex_ext.program);
	wsm_gles2_pop_debug(renderer);

	if (renderer->exts.KHR_debug) {
		glDisable(GL_DEBUG_OUTPUT_KHR);
		renderer->procs.glDebugMessageCallbackKHR(NULL, NULL);
	}

	wsm_egl_unset_current(renderer->egl);
	wsm_egl_destroy(renderer->egl);

	wlr_drm_format_set_finish(&renderer->shm_texture_formats);

	if (renderer->drm_fd >= 0) {
		close(renderer->drm_fd);
	}

	free(renderer);
}

static const struct wlr_drm_format_set *gles2_get_texture_formats(
		struct wlr_renderer *wlr_renderer, uint32_t buffer_caps) {
	struct wsm_gles2_renderer *renderer = wsm_gles2_renderer_from_renderer(wlr_renderer);
	if (buffer_caps & WLR_BUFFER_CAP_DMABUF) {
		return wsm_egl_get_dmabuf_texture_formats(renderer->egl);
	} else if (buffer_caps & WLR_BUFFER_CAP_DATA_PTR) {
		return &renderer->shm_texture_formats;
	} else {
		return NULL;
	}
}

static const struct wlr_drm_format_set *gles2_get_render_formats(
		struct wlr_renderer *wlr_renderer) {
	struct wsm_gles2_renderer *renderer = wsm_gles2_renderer_from_renderer(wlr_renderer);
	return wsm_egl_get_dmabuf_render_formats(renderer->egl);
}

static int gles2_get_drm_fd(struct wlr_renderer *wlr_renderer) {
	struct wsm_gles2_renderer *renderer =
		wsm_gles2_renderer_from_renderer(wlr_renderer);

	if (renderer->drm_fd < 0) {
		renderer->drm_fd = wsm_egl_dup_drm_fd(renderer->egl);
	}

	return renderer->drm_fd;
}

static struct wlr_texture *gles2_texture_from_buffer(struct wlr_renderer *wlr_renderer,
		struct wlr_buffer *buffer) {
	struct wsm_gles2_texture *texture =
		wsm_gles2_texture_from_buffer(wlr_renderer, buffer);

	return &texture->base;
}

static struct wlr_render_pass *gles2_begin_buffer_pass(struct wlr_renderer *wlr_renderer,
		struct wlr_buffer *wlr_buffer, const struct wlr_buffer_pass_options *options) {
	struct wsm_gles2_renderer *renderer = wsm_gles2_renderer_from_renderer(wlr_renderer);

	struct wsm_egl_context prev_ctx = {0};
	if (!wsm_egl_make_current(renderer->egl, &prev_ctx)) {
		return NULL;
	}

	struct wsm_gles2_render_timer *timer = NULL;
	if (options->timer) {
		timer = wsm_gles2_render_timer_from_timer(options->timer);
		clock_gettime(CLOCK_MONOTONIC, &timer->cpu_start);
	}

	struct wsm_gles2_buffer *buffer = wsm_gles2_buffer_get_or_create(renderer, wlr_buffer);
	if (!buffer) {
		return NULL;
	}

	struct wsm_gles2_render_pass *pass = wsm_render_pass_begin_gles2_buffer_pass(buffer,
		&prev_ctx, timer, options->signal_timeline, options->signal_point);
	if (!pass) {
		return NULL;
	}
	return &pass->base;
}

static struct wlr_render_timer *gles2_render_timer_create(struct wlr_renderer *wlr_renderer) {
	struct wsm_gles2_render_timer *timer = wsm_gles2_render_timer_create(wlr_renderer);
	return &timer->base;
}

static const struct wlr_renderer_impl renderer_impl = {
	.destroy = gles2_destroy,
	.get_texture_formats = gles2_get_texture_formats,
	.get_render_formats = gles2_get_render_formats,
	.get_drm_fd = gles2_get_drm_fd,
	.texture_from_buffer = gles2_texture_from_buffer,
	.begin_buffer_pass = gles2_begin_buffer_pass,
	.render_timer_create = gles2_render_timer_create,
};

static GLuint compile_shader(struct wsm_gles2_renderer *renderer,
		GLenum type, const GLchar *src) {
	 wsm_gles2_push_debug(renderer);

	GLuint shader = glCreateShader(type);
	glShaderSource(shader, 1, &src, NULL);
	glCompileShader(shader);

	GLint ok;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
	if (ok == GL_FALSE) {
		wsm_log(WSM_ERROR, "Failed to compile shader");
		glDeleteShader(shader);
		shader = 0;
	}

	wsm_gles2_pop_debug(renderer);
	return shader;
}

static bool check_gl_ext(const char *exts, const char *ext) {
	size_t extlen = strlen(ext);
	const char *end = exts + strlen(exts);

	while (exts < end) {
		if (exts[0] == ' ') {
			exts++;
			continue;
		}
		size_t n = strcspn(exts, " ");
		if (n == extlen && strncmp(ext, exts, n) == 0) {
			return true;
		}
		exts += n;
	}
	return false;
}

static enum wlr_log_importance gles2_log_importance_to_wlr(GLenum type) {
	switch (type) {
	case GL_DEBUG_TYPE_ERROR_KHR:               return WLR_ERROR;
	case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR_KHR: return WLR_DEBUG;
	case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR_KHR:  return WLR_ERROR;
	case GL_DEBUG_TYPE_PORTABILITY_KHR:         return WLR_DEBUG;
	case GL_DEBUG_TYPE_PERFORMANCE_KHR:         return WLR_DEBUG;
	case GL_DEBUG_TYPE_OTHER_KHR:               return WLR_DEBUG;
	case GL_DEBUG_TYPE_MARKER_KHR:              return WLR_DEBUG;
	case GL_DEBUG_TYPE_PUSH_GROUP_KHR:          return WLR_DEBUG;
	case GL_DEBUG_TYPE_POP_GROUP_KHR:           return WLR_DEBUG;
	default:                                    return WLR_DEBUG;
	}
}

static void load_gl_proc(void *proc_ptr, const char *name) {
	void *proc = (void *)eglGetProcAddress(name);
	if (proc == NULL) {
		wsm_log(WSM_ERROR, "eglGetProcAddress(%s) failed", name);
		abort();
	}
	*(void **)proc_ptr = proc;
}

static void gles2_log(GLenum src, GLenum type, GLuint id, GLenum severity,
		GLsizei len, const GLchar *msg, const void *user) {
	_wlr_log(gles2_log_importance_to_wlr(type), "[GLES2] %s", msg);
}

struct wsm_gles2_renderer *wsm_gles2_renderer_create_with_drm_fd(int drm_fd) {
	struct wsm_egl *egl = wsm_egl_create_with_drm_fd(drm_fd);
	if (egl == NULL) {
		wlr_log(WLR_ERROR, "Could not initialize EGL");
		return NULL;
	}

	struct wsm_gles2_renderer *renderer = wsm_gles2_renderer_create(egl);
	if (!renderer) {
		wlr_log(WLR_ERROR, "Failed to create GLES2 renderer");
		wsm_egl_destroy(egl);
		return NULL;
	}

	return renderer;
}

struct wsm_gles2_renderer *wsm_gles2_renderer_create(struct wsm_egl *egl) {
	if (!wsm_egl_make_current(egl, NULL)) {
		return NULL;
	}

	const char *exts_str = (const char *)glGetString(GL_EXTENSIONS);
	if (exts_str == NULL) {
		wsm_log(WSM_ERROR, "Failed to get GL_EXTENSIONS");
		return NULL;
	}

	struct wsm_gles2_renderer *renderer = calloc(1, sizeof(*renderer));
	if (renderer == NULL) {
		return NULL;
	}
	wlr_renderer_init(&renderer->base, &renderer_impl, WLR_BUFFER_CAP_DMABUF);
	renderer->base.features.output_color_transform = false;

	wl_list_init(&renderer->buffers);
	wl_list_init(&renderer->textures);

	renderer->egl = egl;
	renderer->exts_str = exts_str;
	renderer->drm_fd = -1;

	wsm_log(WSM_INFO, "Creating GLES2 renderer");
	wsm_log(WSM_INFO, "Using %s", glGetString(GL_VERSION));
	wsm_log(WSM_INFO, "GL vendor: %s", glGetString(GL_VENDOR));
	wsm_log(WSM_INFO, "GL renderer: %s", glGetString(GL_RENDERER));
	wsm_log(WSM_INFO, "Supported GLES2 extensions: %s", exts_str);

	if (!renderer->egl->exts.EXT_image_dma_buf_import) {
		wsm_log(WSM_ERROR, "EGL_EXT_image_dma_buf_import not supported");
		free(renderer);
		return NULL;
	}
	if (!check_gl_ext(exts_str, "GL_EXT_texture_format_BGRA8888")) {
		wsm_log(WSM_ERROR, "BGRA8888 format not supported by GLES2");
		free(renderer);
		return NULL;
	}
	if (!check_gl_ext(exts_str, "GL_EXT_unpack_subimage")) {
		wsm_log(WSM_ERROR, "GL_EXT_unpack_subimage not supported");
		free(renderer);
		return NULL;
	}

	renderer->exts.EXT_read_format_bgra =
		check_gl_ext(exts_str, "GL_EXT_read_format_bgra");

	renderer->exts.EXT_texture_type_2_10_10_10_REV =
		check_gl_ext(exts_str, "GL_EXT_texture_type_2_10_10_10_REV");

	renderer->exts.OES_texture_half_float_linear =
		check_gl_ext(exts_str, "GL_OES_texture_half_float_linear");

	renderer->exts.EXT_texture_norm16 =
		check_gl_ext(exts_str, "GL_EXT_texture_norm16");

	if (check_gl_ext(exts_str, "GL_KHR_debug")) {
		renderer->exts.KHR_debug = true;
		load_gl_proc(&renderer->procs.glDebugMessageCallbackKHR,
			"glDebugMessageCallbackKHR");
		load_gl_proc(&renderer->procs.glDebugMessageControlKHR,
			"glDebugMessageControlKHR");
	}

	if (check_gl_ext(exts_str, "GL_OES_EGL_image_external")) {
		renderer->exts.OES_egl_image_external = true;
		load_gl_proc(&renderer->procs.glEGLImageTargetTexture2DOES,
			"glEGLImageTargetTexture2DOES");
	}

	if (check_gl_ext(exts_str, "GL_OES_EGL_image")) {
		renderer->exts.OES_egl_image = true;
		load_gl_proc(&renderer->procs.glEGLImageTargetRenderbufferStorageOES,
			"glEGLImageTargetRenderbufferStorageOES");
	}

	if (check_gl_ext(exts_str, "GL_KHR_robustness")) {
		GLint notif_strategy = 0;
		glGetIntegerv(GL_RESET_NOTIFICATION_STRATEGY_KHR, &notif_strategy);
		switch (notif_strategy) {
		case GL_LOSE_CONTEXT_ON_RESET_KHR:
			wsm_log(WSM_DEBUG, "GPU reset notifications are enabled");
			load_gl_proc(&renderer->procs.glGetGraphicsResetStatusKHR,
				"glGetGraphicsResetStatusKHR");
			break;
		case GL_NO_RESET_NOTIFICATION_KHR:
			wsm_log(WSM_DEBUG, "GPU reset notifications are disabled");
			break;
		}
	}

	if (check_gl_ext(exts_str, "GL_EXT_disjoint_timer_query")) {
		renderer->exts.EXT_disjoint_timer_query = true;
		load_gl_proc(&renderer->procs.glGenQueriesEXT, "glGenQueriesEXT");
		load_gl_proc(&renderer->procs.glDeleteQueriesEXT, "glDeleteQueriesEXT");
		load_gl_proc(&renderer->procs.glQueryCounterEXT, "glQueryCounterEXT");
		load_gl_proc(&renderer->procs.glGetQueryObjectivEXT, "glGetQueryObjectivEXT");
		load_gl_proc(&renderer->procs.glGetQueryObjectui64vEXT, "glGetQueryObjectui64vEXT");
		if (eglGetProcAddress("glGetInteger64vEXT")) {
			load_gl_proc(&renderer->procs.glGetInteger64vEXT, "glGetInteger64vEXT");
		} else {
			load_gl_proc(&renderer->procs.glGetInteger64vEXT, "glGetInteger64v");
		}
	}

	if (renderer->exts.KHR_debug) {
		glEnable(GL_DEBUG_OUTPUT_KHR);
		glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS_KHR);
		renderer->procs.glDebugMessageCallbackKHR(gles2_log, NULL);

		// Silence unwanted message types
		renderer->procs.glDebugMessageControlKHR(GL_DONT_CARE,
			GL_DEBUG_TYPE_POP_GROUP_KHR, GL_DONT_CARE, 0, NULL, GL_FALSE);
		renderer->procs.glDebugMessageControlKHR(GL_DONT_CARE,
			GL_DEBUG_TYPE_PUSH_GROUP_KHR, GL_DONT_CARE, 0, NULL, GL_FALSE);
	}

	wsm_gles2_push_debug(renderer);

	GLuint prog;
	renderer->shaders.quad.program = prog =
		wsm_gles2_link_program(renderer, common_vert_src, quad_frag_src);
	if (!renderer->shaders.quad.program) {
		goto error;
	}
	renderer->shaders.quad.proj = glGetUniformLocation(prog, "proj");
	renderer->shaders.quad.color = glGetUniformLocation(prog, "color");
	renderer->shaders.quad.pos_attrib = glGetAttribLocation(prog, "pos");

	renderer->shaders.tex_rgba.program = prog =
		wsm_gles2_link_program(renderer, common_vert_src, tex_rgba_frag_src);
	if (!renderer->shaders.tex_rgba.program) {
		goto error;
	}
	renderer->shaders.tex_rgba.proj = glGetUniformLocation(prog, "proj");
	renderer->shaders.tex_rgba.tex_proj = glGetUniformLocation(prog, "tex_proj");
	renderer->shaders.tex_rgba.tex = glGetUniformLocation(prog, "tex");
	renderer->shaders.tex_rgba.alpha = glGetUniformLocation(prog, "alpha");
	renderer->shaders.tex_rgba.pos_attrib = glGetAttribLocation(prog, "pos");

	renderer->shaders.tex_rgbx.program = prog =
		wsm_gles2_link_program(renderer, common_vert_src, tex_rgbx_frag_src);
	if (!renderer->shaders.tex_rgbx.program) {
		goto error;
	}
	renderer->shaders.tex_rgbx.proj = glGetUniformLocation(prog, "proj");
	renderer->shaders.tex_rgbx.tex_proj = glGetUniformLocation(prog, "tex_proj");
	renderer->shaders.tex_rgbx.tex = glGetUniformLocation(prog, "tex");
	renderer->shaders.tex_rgbx.alpha = glGetUniformLocation(prog, "alpha");
	renderer->shaders.tex_rgbx.pos_attrib = glGetAttribLocation(prog, "pos");

	if (renderer->exts.OES_egl_image_external) {
		renderer->shaders.tex_ext.program = prog =
			wsm_gles2_link_program(renderer, common_vert_src, tex_external_frag_src);
		if (!renderer->shaders.tex_ext.program) {
			goto error;
		}
		renderer->shaders.tex_ext.proj = glGetUniformLocation(prog, "proj");
		renderer->shaders.tex_ext.tex_proj = glGetUniformLocation(prog, "tex_proj");
		renderer->shaders.tex_ext.tex = glGetUniformLocation(prog, "tex");
		renderer->shaders.tex_ext.alpha = glGetUniformLocation(prog, "alpha");
		renderer->shaders.tex_ext.pos_attrib = glGetAttribLocation(prog, "pos");
	}

	wsm_gles2_pop_debug(renderer);

	wsm_egl_unset_current(renderer->egl);

	get_gles2_shm_formats(renderer, &renderer->shm_texture_formats);

	int drm_fd = wlr_renderer_get_drm_fd(&renderer->base);
	uint64_t cap_syncobj_timeline;
	if (drm_fd >= 0 && drmGetCap(drm_fd, DRM_CAP_SYNCOBJ_TIMELINE, &cap_syncobj_timeline) == 0) {
		renderer->base.features.timeline = egl->procs.eglDupNativeFenceFDANDROID &&
			egl->procs.eglWaitSyncKHR && cap_syncobj_timeline != 0;
	}

	return renderer;

error:
	glDeleteProgram(renderer->shaders.quad.program);
	glDeleteProgram(renderer->shaders.tex_rgba.program);
	glDeleteProgram(renderer->shaders.tex_rgbx.program);
	glDeleteProgram(renderer->shaders.tex_ext.program);

	wsm_gles2_pop_debug(renderer);

	if (renderer->exts.KHR_debug) {
		glDisable(GL_DEBUG_OUTPUT_KHR);
		renderer->procs.glDebugMessageCallbackKHR(NULL, NULL);
	}

	wsm_egl_unset_current(renderer->egl);

	free(renderer);
	return NULL;
}

bool wlr_renderer_is_wsm_gles2(const struct wlr_renderer *wlr_renderer) {
	return wlr_renderer != NULL && wlr_renderer->impl == &renderer_impl;
}

struct wsm_gles2_renderer *wsm_gles2_renderer_from_renderer(
		struct wlr_renderer *wlr_renderer) {
	assert(wlr_renderer->impl == &renderer_impl);

	struct wsm_gles2_renderer *renderer = wl_container_of(wlr_renderer, renderer, base);
	return renderer;
}

void wsm_gles2_push_debug_(struct wsm_gles2_renderer *renderer,
		const char *file, const char *func) {
	if (!renderer->procs.glPushDebugGroupKHR) {
		return;
	}

	char *str = format_str("%s:%s", file, func);
	if (str == NULL) {
		return;
	}

	renderer->procs.glPushDebugGroupKHR(GL_DEBUG_SOURCE_APPLICATION_KHR, 1, -1, str);
	free(str);
}

void wsm_gles2_pop_debug(struct wsm_gles2_renderer *renderer) {
	if (renderer->procs.glPopDebugGroupKHR) {
		renderer->procs.glPopDebugGroupKHR();
	}
}

GLuint wsm_gles2_link_program(struct wsm_gles2_renderer *renderer,
		const GLchar *vert_src, const GLchar *frag_src) {
	 wsm_gles2_push_debug(renderer);

	GLuint vert = compile_shader(renderer, GL_VERTEX_SHADER, vert_src);
	if (!vert) {
		goto error;
	}

	GLuint frag = compile_shader(renderer, GL_FRAGMENT_SHADER, frag_src);
	if (!frag) {
		glDeleteShader(vert);
		goto error;
	}

	GLuint prog = glCreateProgram();
	glAttachShader(prog, vert);
	glAttachShader(prog, frag);
	glLinkProgram(prog);

	glDetachShader(prog, vert);
	glDetachShader(prog, frag);
	glDeleteShader(vert);
	glDeleteShader(frag);

	GLint ok;
	glGetProgramiv(prog, GL_LINK_STATUS, &ok);
	if (ok == GL_FALSE) {
		wsm_log(WSM_ERROR, "Failed to link shader");
		glDeleteProgram(prog);
		goto error;
	}

	wsm_gles2_pop_debug(renderer);
	return prog;

error:
	wsm_gles2_pop_debug(renderer);
	return 0;
}

void wsm_gles_set_proj_matrix(GLint loc, float proj[9], const struct wlr_box *box) {
	float gl_matrix[9];
	wlr_matrix_identity(gl_matrix);
	wlr_matrix_translate(gl_matrix, box->x, box->y);
	wlr_matrix_scale(gl_matrix, box->width, box->height);
	wlr_matrix_multiply(gl_matrix, proj, gl_matrix);
	glUniformMatrix3fv(loc, 1, GL_FALSE, gl_matrix);
}
