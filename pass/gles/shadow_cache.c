#include "pass/gles/shadow_cache.h"
#include "renderer/gles/egl.h"
#include "renderer/gles/renderer.h"
#include "texture/gles/texture.h"
#include "util/wsm_log.h"

#include <GLES2/gl2.h>
#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <wlr/render/wlr_texture.h>

struct wsm_scene_shadow_cache *wsm_gles2_shadow_cache_create(
		struct wlr_renderer *wlr_renderer,
		const struct wsm_scene_shadow_slices *margins,
		float corner_extent, const float radii[static 4], float inner_offset,
		const float color[static 4]) {
	int radius = (int)ceilf(corner_extent);
	int64_t width64 = (int64_t)margins->left + radius * 2 + 1 + margins->right;
	int64_t height64 = (int64_t)margins->top + radius * 2 + 1 + margins->bottom;
	if (width64 <= 0 || height64 <= 0 || width64 > INT_MAX / 4 ||
			height64 > INT_MAX ||
			(uint64_t)(width64 * height64) > SIZE_MAX / sizeof(uint32_t)) {
		return NULL;
	}
	int width = width64, height = height64;
	struct wsm_gles2_renderer *renderer = wsm_gles2_renderer_from_renderer(wlr_renderer);
	struct wsm_egl_context prev_context = {0};
	if (!wsm_egl_make_current(renderer->egl, &prev_context)) {
		return NULL;
	}

	GLint old_fbo, old_viewport[4], old_program, old_texture, old_active_texture;
	GLfloat old_clear_color[4];
	GLboolean old_blend = glIsEnabled(GL_BLEND);
	glGetIntegerv(GL_FRAMEBUFFER_BINDING, &old_fbo);
	glGetIntegerv(GL_VIEWPORT, old_viewport);
	glGetIntegerv(GL_CURRENT_PROGRAM, &old_program);
	glGetIntegerv(GL_ACTIVE_TEXTURE, &old_active_texture);
	glGetIntegerv(GL_TEXTURE_BINDING_2D, &old_texture);
	glGetFloatv(GL_COLOR_CLEAR_VALUE, old_clear_color);

	struct wsm_scene_shadow_cache *cache = NULL;
	struct wsm_gles2_texture *texture = NULL;
	GLuint fbo = 0;
	wsm_gles2_push_debug(renderer);
	texture = wsm_gles2_texture_create_render_target(wlr_renderer, width, height);
	if (texture == NULL) {
		goto done;
	}
	glGenFramebuffers(1, &fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
		GL_TEXTURE_2D, texture->tex, 0);
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
		goto done;
	}
	glViewport(0, 0, width, height);
	glDisable(GL_BLEND);
	glClearColor(0.f, 0.f, 0.f, 0.f);
	glClear(GL_COLOR_BUFFER_BIT);

	glUseProgram(renderer->shaders.shadow_cache.program);
	glUniform2f(renderer->shaders.shadow_cache.size, width, height);
	glUniform4f(renderer->shaders.shadow_cache.slices, margins->left, margins->right,
		margins->top, margins->bottom);
	glUniform4fv(renderer->shaders.shadow_cache.radius, 1, radii);
	glUniform1f(renderer->shaders.shadow_cache.smoothness, 4.f);
	glUniform1f(renderer->shaders.shadow_cache.inset, 13.f);
	glUniform1f(renderer->shaders.shadow_cache.spread, 42.f);
	glUniform1f(renderer->shaders.shadow_cache.inner_offset, inner_offset);
	glUniform4fv(renderer->shaders.shadow_cache.color, 1, color);
	const GLfloat vertices[] = {
		0.f, 0.f, 1.f, 0.f, 0.f, 1.f,
		1.f, 0.f, 1.f, 1.f, 0.f, 1.f,
	};
	glEnableVertexAttribArray(renderer->shaders.shadow_cache.pos);
	glVertexAttribPointer(renderer->shaders.shadow_cache.pos, 2, GL_FLOAT, GL_FALSE, 0, vertices);
	glDrawArrays(GL_TRIANGLES, 0, 6);
	glDisableVertexAttribArray(renderer->shaders.shadow_cache.pos);

	glBindFramebuffer(GL_FRAMEBUFFER, old_fbo);
	glDeleteFramebuffers(1, &fbo);
	fbo = 0;
	struct wsm_scene_shadow_slices slices = {
		.left = margins->left + radius,
		.right = margins->right + radius,
		.top = margins->top + radius,
		.bottom = margins->bottom + radius,
	};
	cache = wsm_scene_shadow_cache_create_from_texture(&texture->base, &slices);
	if (cache != NULL) {
		texture = NULL;
	}

done:
	glBindFramebuffer(GL_FRAMEBUFFER, old_fbo);
	glViewport(old_viewport[0], old_viewport[1], old_viewport[2], old_viewport[3]);
	glUseProgram(old_program);
	if (old_blend) {
		glEnable(GL_BLEND);
	} else {
		glDisable(GL_BLEND);
	}
	glClearColor(old_clear_color[0], old_clear_color[1],
		old_clear_color[2], old_clear_color[3]);
	glBindTexture(GL_TEXTURE_2D, old_texture);
	glActiveTexture(old_active_texture);
	if (fbo != 0) {
		glDeleteFramebuffers(1, &fbo);
	}
	if (texture != NULL) {
		wlr_texture_destroy(&texture->base);
	}
	wsm_gles2_pop_debug(renderer);
	wsm_egl_restore_context(&prev_context);
	return cache;
}
