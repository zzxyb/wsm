#include "pass/gles/blur.h"
#include "blur_quad_vert_src.h"
#include "round_tex_vert_src.h"
#include "blur_down_rgba_frag_src.h"
#include "blur_down_external_frag_src.h"
#include "blur_up_frag_src.h"
#include "blur_round_frag_src.h"

#include "renderer/gles/egl.h"
#include <unistd.h>
#include <wlr/render/drm_syncobj.h>
#include <wlr/render/gles2.h>
#include <wlr/util/log.h>
#include <wlr/util/transform.h>

#include "util/wsm_matrix.h"
#include "util/wsm_log.h"

#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define DEFAULT_CORNER_SMOOTHNESS 4.0f
#define DEFAULT_ITERATIONS 4
#define DEFAULT_OFFSET 1.8f

static float clamp_corner_smoothness(float smoothness) {
	if (smoothness <= 0.f) {
		return DEFAULT_CORNER_SMOOTHNESS;
	}
	return smoothness < 1.f ? 1.f : smoothness;
}

static int clamp_iterations(int iterations, int width, int height) {
	if (iterations <= 0) {
		iterations = DEFAULT_ITERATIONS;
	}
	if (iterations > WSM_BLUR_BUFFER_MAX_PASSES) {
		iterations = WSM_BLUR_BUFFER_MAX_PASSES;
	}

	int min_size = width < height ? width : height;
	while (iterations > 1 && (min_size >> iterations) < 2) {
		iterations--;
	}
	return iterations;
}

static float option_or_default(float value, float fallback) {
	return value == 0.f ? fallback : value;
}

static void render_unit_quad(GLint attrib) {
	const GLfloat verts[] = {
		0.f,
		0.f,
		1.f,
		0.f,
		0.f,
		1.f,
		1.f,
		0.f,
		1.f,
		1.f,
		0.f,
		1.f,
	};
	glEnableVertexAttribArray(attrib);
	glVertexAttribPointer(attrib, 2, GL_FLOAT, GL_FALSE, 0, verts);
	glDrawArrays(GL_TRIANGLES, 0, 6);
	glDisableVertexAttribArray(attrib);
}

static void render_region(const struct wlr_box *box, const pixman_region32_t *clip, GLint attrib) {
	pixman_region32_t region;
	pixman_region32_init_rect(&region, box->x, box->y, box->width, box->height);
	if (clip) {
		pixman_region32_intersect(&region, &region, clip);
	}

	int rects_len;
	const pixman_box32_t *rects = pixman_region32_rectangles(&region, &rects_len);
	if (rects_len == 0) {
		pixman_region32_fini(&region);
		return;
	}

	glEnableVertexAttribArray(attrib);
	for (int i = 0; i < rects_len; i++) {
		const pixman_box32_t *rect = &rects[i];
		const GLfloat verts[] = {
			(GLfloat)(rect->x1 - box->x) / box->width,
			(GLfloat)(rect->y1 - box->y) / box->height,
			(GLfloat)(rect->x2 - box->x) / box->width,
			(GLfloat)(rect->y1 - box->y) / box->height,
			(GLfloat)(rect->x1 - box->x) / box->width,
			(GLfloat)(rect->y2 - box->y) / box->height,
			(GLfloat)(rect->x2 - box->x) / box->width,
			(GLfloat)(rect->y1 - box->y) / box->height,
			(GLfloat)(rect->x2 - box->x) / box->width,
			(GLfloat)(rect->y2 - box->y) / box->height,
			(GLfloat)(rect->x1 - box->x) / box->width,
			(GLfloat)(rect->y2 - box->y) / box->height,
		};
		glVertexAttribPointer(attrib, 2, GL_FLOAT, GL_FALSE, 0, verts);
		glDrawArrays(GL_TRIANGLES, 0, 6);
	}
	glDisableVertexAttribArray(attrib);
	pixman_region32_fini(&region);
}

static void set_tex_matrix(GLint loc, enum wl_output_transform trans, const struct wlr_fbox *box) {
	float tex_matrix[9];
	wlr_matrix_identity(tex_matrix);
	wlr_matrix_translate(tex_matrix, box->x, box->y);
	wlr_matrix_scale(tex_matrix, box->width, box->height);
	wlr_matrix_translate(tex_matrix, .5, .5);
	if (trans & WL_OUTPUT_TRANSFORM_90) {
		wlr_matrix_transform(tex_matrix, wlr_output_transform_invert(trans));
	} else {
		wlr_matrix_transform(tex_matrix, trans);
	}
	wlr_matrix_translate(tex_matrix, -.5, -.5);
	glUniformMatrix3fv(loc, 1, GL_FALSE, tex_matrix);
}

static void identity_tex_matrix(GLint loc) {
	float tex_matrix[9];
	wlr_matrix_identity(tex_matrix);
	glUniformMatrix3fv(loc, 1, GL_FALSE, tex_matrix);
}

static bool wait_texture_sync(
	struct wsm_gles2_renderer *renderer, const struct wsm_render_blur_buffer_options *options) {
	if (options->wait_timeline == NULL) {
		return true;
	}

	int sync_file_fd =
		wlr_drm_syncobj_timeline_export_sync_file(options->wait_timeline, options->wait_point);
	if (sync_file_fd < 0) {
		return false;
	}

	EGLSyncKHR sync = wsm_egl_create_sync(renderer->egl, sync_file_fd);
	close(sync_file_fd);
	if (sync == EGL_NO_SYNC_KHR) {
		return false;
	}

	bool ok = wsm_egl_wait_sync(renderer->egl, sync);
	wsm_egl_destroy_sync(renderer->egl, sync);
	return ok;
}

static bool ensure_level(struct wsm_gles_blur_texture *level, int width, int height) {
	if (level->tex != 0 && level->width == width && level->height == height) {
		return true;
	}

	if (level->tex != 0) {
		glDeleteFramebuffers(1, &level->fbo);
		glDeleteTextures(1, &level->tex);
		*level = (struct wsm_gles_blur_texture){0};
	}

	glGenTextures(1, &level->tex);
	glBindTexture(GL_TEXTURE_2D, level->tex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);

	glGenFramebuffers(1, &level->fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, level->fbo);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, level->tex, 0);
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
		wlr_log(WLR_ERROR, "Failed to create blur FBO");
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glDeleteFramebuffers(1, &level->fbo);
		glDeleteTextures(1, &level->tex);
		*level = (struct wsm_gles_blur_texture){0};
		return false;
	}

	level->width = width;
	level->height = height;
	return true;
}

static void destroy_level(struct wsm_gles_blur_texture *level) {
	if (level->tex == 0) {
		return;
	}
	glDeleteFramebuffers(1, &level->fbo);
	glDeleteTextures(1, &level->tex);
	*level = (struct wsm_gles_blur_texture){0};
}

static void destroy_shader(GLuint program) {
	if (program != 0) {
		glDeleteProgram(program);
	}
}

static void blur_pass_destroy(struct wsm_render_blur_buffer_pass *pass) {
	struct wsm_gles_render_blur_buffer_pass *gles2_pass =
		wsm_gles_render_blur_buffer_pass_from_pass(pass);
	struct wsm_egl_context previous;
	wsm_egl_make_current(gles2_pass->renderer->egl, &previous);
	destroy_shader(gles2_pass->down.rgba.program);
	destroy_shader(gles2_pass->down.ext.program);
	destroy_shader(gles2_pass->up.program);
	destroy_shader(gles2_pass->round.program);
	for (size_t i = 0; i < sizeof(gles2_pass->levels) / sizeof(gles2_pass->levels[0]); i++) {
		destroy_level(&gles2_pass->levels[i]);
	}
	wsm_egl_restore_context(&previous);
	free(gles2_pass);
}

static void get_src_box(
	const struct wsm_render_blur_buffer_options *options, struct wlr_fbox *box) {
	if (wlr_fbox_empty(&options->src_box)) {
		*box =
			(struct wlr_fbox){.width = options->texture->width, .height = options->texture->height};
		return;
	}
	*box = options->src_box;
}

static float get_alpha(const struct wsm_render_blur_buffer_options *options) {
	return options->alpha == NULL ? 1.f : *options->alpha;
}

static void setup_blending(enum wlr_render_blend_mode mode) {
	switch (mode) {
	case WLR_RENDER_BLEND_MODE_PREMULTIPLIED:
		glEnable(GL_BLEND);
		break;
	case WLR_RENDER_BLEND_MODE_NONE:
		glDisable(GL_BLEND);
		break;
	}
}

static void render_down(struct wsm_gles_blur_down_shader *shader, GLenum texture_target,
	GLuint texture, int src_width, int src_height, struct wsm_gles_blur_texture *dst, float offset,
	enum wl_output_transform transform, const struct wlr_fbox *src_box) {
	glBindFramebuffer(GL_FRAMEBUFFER, dst->fbo);
	glViewport(0, 0, dst->width, dst->height);
	glUseProgram(shader->program);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(texture_target, texture);
	glUniform1i(shader->tex, 0);
	glUniform2f(shader->halfpixel, 0.5f / src_width, 0.5f / src_height);
	glUniform1f(shader->offset, offset);
	if (src_box != NULL) {
		set_tex_matrix(shader->tex_proj, transform, src_box);
	} else {
		identity_tex_matrix(shader->tex_proj);
	}
	render_unit_quad(shader->pos);
}

static void render_up(struct wsm_gles_blur_up_shader *shader, struct wsm_gles_blur_texture *src,
	struct wsm_gles_blur_texture *dst, float offset) {
	glBindFramebuffer(GL_FRAMEBUFFER, dst->fbo);
	glViewport(0, 0, dst->width, dst->height);
	glUseProgram(shader->program);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, src->tex);
	glUniform1i(shader->tex, 0);
	glUniform2f(shader->halfpixel, 0.5f / src->width, 0.5f / src->height);
	glUniform1f(shader->offset, offset);
	render_unit_quad(shader->pos);
}

static void render_final(struct wsm_gles2_render_pass *target,
	struct wsm_gles_blur_round_shader *shader, struct wsm_gles_blur_texture *src,
	const struct wsm_render_blur_buffer_options *options, const pixman_region32_t *region) {
	float width = options->dst_box.width + 2 * options->padding;
	float height = options->dst_box.height + 2 * options->padding;
	struct wlr_fbox src_box = {.x = options->padding / width,
		.y = options->padding / height,
		.width = options->dst_box.width / width,
		.height = options->dst_box.height / height};
	glUseProgram(shader->program);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, src->tex);
	glUniform1i(shader->tex, 0);
	glUniform1f(shader->alpha, get_alpha(options));
	float projection[9];
	wlr_matrix_identity(projection);
	wlr_matrix_translate(projection, options->dst_box.x, options->dst_box.y);
	wlr_matrix_scale(projection, options->dst_box.width, options->dst_box.height);
	wlr_matrix_multiply(projection, target->projection_matrix, projection);
	glUniformMatrix3fv(shader->proj, 1, GL_FALSE, projection);
	set_tex_matrix(shader->tex_proj, WL_OUTPUT_TRANSFORM_NORMAL, &src_box);
	glUniform4f(shader->radius, options->top_left_radius, options->top_right_radius,
		options->bottom_left_radius, options->bottom_right_radius);
	glUniform2f(shader->size, (float)options->dst_box.width, (float)options->dst_box.height);
	glUniform1f(shader->corner_smoothness, clamp_corner_smoothness(options->corner_smoothness));
	glUniform1f(shader->saturation, option_or_default(options->saturation, 1.35f));
	glUniform1f(shader->brightness, option_or_default(options->brightness, 1.06f));
	glUniform1f(shader->noise_amount, options->noise_amount);
	glUniform1f(shader->material_contrast, options->material_contrast);
	glUniform4fv(shader->tint, 1, options->tint);
	render_region(&options->dst_box, region, shader->pos);
}

static void blur_pass_render(struct wlr_render_pass *target,
	struct wsm_render_blur_buffer_pass *pass,
	const struct wsm_render_blur_buffer_options *options) {
	struct wsm_gles2_render_pass *gles_target = wlr_gles2_render_pass_from_render_pass(target);
	struct wsm_gles2_renderer *renderer = gles_target->buffer->renderer;
	struct wsm_gles_render_blur_buffer_pass *blur_pass =
		wsm_gles_render_blur_buffer_pass_from_pass(pass);
	assert(wlr_texture_is_wsm_gles2(options->texture));

	if (options->dst_box.width <= 0 || options->dst_box.height <= 0) {
		return;
	}

	struct wsm_gles2_texture texture = *wsm_gles2_texture_from_texture(options->texture);
	struct wsm_gles_blur_down_shader *down_shader = NULL;
	switch (texture.target) {
	case GL_TEXTURE_2D:
		down_shader = &blur_pass->down.rgba;
		break;
	case GL_TEXTURE_EXTERNAL_OES:
		assert(renderer->exts.OES_egl_image_external);
		down_shader = &blur_pass->down.ext;
		break;
	default:
		abort();
	}

	pixman_region32_t region;
	pixman_region32_init_rect(&region, options->dst_box.x, options->dst_box.y,
		options->dst_box.width, options->dst_box.height);
	if (options->clip) {
		pixman_region32_intersect(&region, &region, options->clip);
	}
	if (pixman_region32_empty(&region)) {
		pixman_region32_fini(&region);
		return;
	}

	wsm_gles2_push_debug(renderer);
	GLint target_fbo = 0;
	glGetIntegerv(GL_FRAMEBUFFER_BINDING, &target_fbo);
	if (!wait_texture_sync(renderer, options)) {
		goto out;
	}

	int iterations =
		clamp_iterations(options->iterations, options->dst_box.width, options->dst_box.height);
	float offset = option_or_default(options->offset, DEFAULT_OFFSET);
	int width = options->dst_box.width + 2 * options->padding;
	int height = options->dst_box.height + 2 * options->padding;
	for (int i = 0; i <= iterations; i++) {
		width = fmax(1, i == 0 ? (options->dst_box.width + 2 * options->padding) / 2 : width / 2);
		height =
			fmax(1, i == 0 ? (options->dst_box.height + 2 * options->padding) / 2 : height / 2);
		if (!ensure_level(&blur_pass->levels[i], width, height)) {
			goto out;
		}
	}

	setup_blending(WLR_RENDER_BLEND_MODE_NONE);
	struct wlr_fbox src_box;
	get_src_box(options, &src_box);
	src_box.x /= options->texture->width;
	src_box.y /= options->texture->height;
	src_box.width /= options->texture->width;
	src_box.height /= options->texture->height;

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(texture.target, texture.tex);
	glTexParameteri(texture.target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(texture.target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	render_down(down_shader, texture.target, texture.tex, options->texture->width,
		options->texture->height, &blur_pass->levels[0], offset, options->transform, &src_box);

	for (int i = 1; i <= iterations; i++) {
		render_down(&blur_pass->down.rgba, GL_TEXTURE_2D, blur_pass->levels[i - 1].tex,
			blur_pass->levels[i - 1].width, blur_pass->levels[i - 1].height, &blur_pass->levels[i],
			offset, WL_OUTPUT_TRANSFORM_NORMAL, NULL);
	}
	for (int i = iterations; i > 0; i--) {
		render_up(&blur_pass->up, &blur_pass->levels[i], &blur_pass->levels[i - 1], offset);
	}

	glBindFramebuffer(GL_FRAMEBUFFER, target_fbo);
	glViewport(0, 0, gles_target->buffer->base->width, gles_target->buffer->base->height);
	glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
	setup_blending(options->blend_mode);
	render_final(gles_target, &blur_pass->round, &blur_pass->levels[0], options, &region);

out:
	glViewport(0, 0, gles_target->buffer->base->width, gles_target->buffer->base->height);
	glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_BLEND);
	glBindTexture(GL_TEXTURE_2D, 0);
	glBindFramebuffer(GL_FRAMEBUFFER, target_fbo);
	wsm_gles2_pop_debug(renderer);
	pixman_region32_fini(&region);
}

#ifndef GL_READ_FRAMEBUFFER
#define GL_READ_FRAMEBUFFER 0x8CA8
#define GL_DRAW_FRAMEBUFFER 0x8CA9
#endif

typedef void(GL_APIENTRYP blit_framebuffer_func_t)(
	GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLbitfield, GLenum);

/* Same blit path and GLES2 fallback as wlroots' framebuffer blur node. */
static blit_framebuffer_func_t get_blit_framebuffer(void) {
	const char *version = (const char *)glGetString(GL_VERSION);
	const char *extensions = (const char *)glGetString(GL_EXTENSIONS);
	if (strstr(version, "OpenGL ES 3")) {
		return (blit_framebuffer_func_t)eglGetProcAddress("glBlitFramebuffer");
	}
	if (strstr(extensions, "GL_ANGLE_framebuffer_blit")) {
		return (blit_framebuffer_func_t)eglGetProcAddress("glBlitFramebufferANGLE");
	}
	if (strstr(extensions, "GL_NV_framebuffer_blit")) {
		return (blit_framebuffer_func_t)eglGetProcAddress("glBlitFramebufferNV");
	}
	return NULL;
}

static struct wlr_texture *capture(struct wlr_render_pass *target, struct wlr_texture *previous,
	const struct wlr_box *box, const pixman_region32_t *damage) {
	struct wsm_gles2_render_pass *p = wlr_gles2_render_pass_from_render_pass(target);
	struct wlr_box extent =
		box ? *box : (struct wlr_box){0, 0, p->buffer->base->width, p->buffer->base->height};
	GLint target_fbo;
	glGetIntegerv(GL_FRAMEBUFFER_BINDING, &target_fbo);
	struct wsm_gles2_texture *texture =
		previous ? wsm_gles2_texture_from_texture(previous)
				 : wsm_gles2_texture_create_render_target(
					   &p->buffer->renderer->base, extent.width, extent.height);
	if (!texture) {
		return NULL;
	}
	pixman_region32_t region;
	pixman_region32_init_rect(&region, extent.x, extent.y, extent.width, extent.height);
	pixman_region32_intersect_rect(
		&region, &region, 0, 0, p->buffer->base->width, p->buffer->base->height);
	if (damage) {
		pixman_region32_intersect(&region, &region, damage);
	}
	blit_framebuffer_func_t blit = get_blit_framebuffer();
	if (blit && !texture->fbo) {
		glGenFramebuffers(1, &texture->fbo);
		glBindFramebuffer(GL_FRAMEBUFFER, texture->fbo);
		glFramebufferTexture2D(
			GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture->tex, 0);
		if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
			glDeleteFramebuffers(1, &texture->fbo);
			texture->fbo = 0;
			blit = NULL;
		}
	}
	int count;
	const pixman_box32_t *rects = pixman_region32_rectangles(&region, &count);
	if (blit) {
		glBindFramebuffer(GL_READ_FRAMEBUFFER, target_fbo);
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, texture->fbo);
		for (int i = 0; i < count; i++) {
			const pixman_box32_t *r = &rects[i];
			blit(r->x1, r->y1, r->x2, r->y2, r->x1 - extent.x, r->y1 - extent.y, r->x2 - extent.x,
				r->y2 - extent.y, GL_COLOR_BUFFER_BIT, GL_NEAREST);
		}
	} else {
		glBindFramebuffer(GL_FRAMEBUFFER, target_fbo);
		glBindTexture(GL_TEXTURE_2D, texture->tex);
		for (int i = 0; i < count; i++) {
			const pixman_box32_t *r = &rects[i];
			glCopyTexSubImage2D(GL_TEXTURE_2D, 0, r->x1 - extent.x, r->y1 - extent.y, r->x1, r->y1,
				r->x2 - r->x1, r->y2 - r->y1);
		}
	}
	glBindFramebuffer(GL_FRAMEBUFFER, target_fbo);
	glBindTexture(GL_TEXTURE_2D, 0);
	pixman_region32_fini(&region);
	return &texture->base;
}

static const struct wsm_render_blur_buffer_pass_impl render_pass_impl = {
	.capture = capture,
	.destroy = blur_pass_destroy,
	.render = blur_pass_render,
};

static bool init_down_shader(struct wsm_gles2_renderer *renderer,
	struct wsm_gles_blur_down_shader *shader, const GLchar *frag_src) {
	GLuint prog;
	shader->program = prog = wsm_gles2_link_program(renderer, blur_quad_vert_src, frag_src);
	if (!shader->program) {
		return false;
	}
	shader->tex = glGetUniformLocation(prog, "tex");
	shader->tex_proj = glGetUniformLocation(prog, "tex_proj");
	shader->halfpixel = glGetUniformLocation(prog, "halfpixel");
	shader->offset = glGetUniformLocation(prog, "offset");
	shader->pos = glGetAttribLocation(prog, "pos");
	return shader->tex >= 0 && shader->tex_proj >= 0 && shader->halfpixel >= 0 &&
		   shader->offset >= 0 && shader->pos >= 0;
}

static bool init_up_shader(
	struct wsm_gles2_renderer *renderer, struct wsm_gles_blur_up_shader *shader) {
	GLuint prog;
	shader->program = prog = wsm_gles2_link_program(renderer, blur_quad_vert_src, blur_up_frag_src);
	if (!shader->program) {
		return false;
	}
	shader->tex = glGetUniformLocation(prog, "tex");
	shader->halfpixel = glGetUniformLocation(prog, "halfpixel");
	shader->offset = glGetUniformLocation(prog, "offset");
	shader->pos = glGetAttribLocation(prog, "pos");
	return shader->tex >= 0 && shader->halfpixel >= 0 && shader->offset >= 0 && shader->pos >= 0;
}

static bool init_round_shader(
	struct wsm_gles2_renderer *renderer, struct wsm_gles_blur_round_shader *shader) {
	GLuint prog;
	shader->program = prog =
		wsm_gles2_link_program(renderer, round_tex_vert_src, blur_round_frag_src);
	if (!shader->program) {
		return false;
	}
	shader->proj = glGetUniformLocation(prog, "proj");
	shader->tex_proj = glGetUniformLocation(prog, "tex_proj");
	shader->tex = glGetUniformLocation(prog, "tex");
	shader->alpha = glGetUniformLocation(prog, "alpha");
	shader->pos = glGetAttribLocation(prog, "pos");
	shader->radius = glGetUniformLocation(prog, "radius");
	shader->size = glGetUniformLocation(prog, "size");
	shader->corner_smoothness = glGetUniformLocation(prog, "corner_smoothness");
	shader->saturation = glGetUniformLocation(prog, "saturation");
	shader->brightness = glGetUniformLocation(prog, "brightness");
	shader->noise_amount = glGetUniformLocation(prog, "noise_amount");
	shader->material_contrast = glGetUniformLocation(prog, "material_contrast");
	shader->tint = glGetUniformLocation(prog, "tint");
	return shader->proj >= 0 && shader->tex_proj >= 0 && shader->tex >= 0 && shader->alpha >= 0 &&
		   shader->pos >= 0 && shader->radius >= 0 && shader->size >= 0 &&
		   shader->corner_smoothness >= 0 && shader->saturation >= 0 && shader->brightness >= 0 &&
		   shader->noise_amount >= 0 && shader->material_contrast >= 0 && shader->tint >= 0;
}

struct wsm_render_blur_buffer_pass *wsm_gles_render_blur_buffer_pass_create(
	struct wlr_renderer *wlr_renderer) {
	struct wsm_gles_render_blur_buffer_pass *pass = calloc(1, sizeof(*pass));
	if (pass == NULL) {
		wlr_log_errno(WLR_ERROR, "failed to allocate wsm_gles_render_blur_buffer_pass");
		return NULL;
	}

	struct wsm_gles2_renderer *renderer = wsm_gles2_renderer_from_renderer(wlr_renderer);
	struct wsm_egl_context previous;
	if (!wsm_egl_make_current(renderer->egl, &previous)) {
		free(pass);
		return NULL;
	}

	pass->renderer = renderer;
	wsm_render_blur_buffer_pass_init(&pass->base, &render_pass_impl);
	wsm_gles2_push_debug(renderer);
	if (!init_down_shader(renderer, &pass->down.rgba, blur_down_rgba_frag_src) ||
		!init_up_shader(renderer, &pass->up) || !init_round_shader(renderer, &pass->round)) {
		wlr_log(WLR_ERROR, "blur buffer shader lookup failed");
		goto error;
	}

	if (renderer->exts.OES_egl_image_external &&
		!init_down_shader(renderer, &pass->down.ext, blur_down_external_frag_src)) {
		wlr_log(WLR_ERROR, "blur buffer external shader lookup failed");
		goto error;
	}

	wsm_gles2_pop_debug(renderer);
	wsm_egl_restore_context(&previous);
	pass->renderer = renderer;
	return &pass->base;

error:
	blur_pass_destroy(&pass->base);
	wsm_gles2_pop_debug(renderer);
	wsm_egl_restore_context(&previous);
	return NULL;
}

bool wsm_render_blur_buffer_pass_is_gles2(const struct wsm_render_blur_buffer_pass *pass) {
	return pass->impl == &render_pass_impl;
}

struct wsm_gles_render_blur_buffer_pass *wsm_gles_render_blur_buffer_pass_from_pass(
	struct wsm_render_blur_buffer_pass *pass) {
	assert(pass->impl == &render_pass_impl);
	struct wsm_gles_render_blur_buffer_pass *gles2_pass = wl_container_of(pass, gles2_pass, base);
	return gles2_pass;
}
