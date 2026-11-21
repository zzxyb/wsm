#include "pass/gles/rect.h"
#include "pass/gles/pass.h"
#include "renderer/gles/renderer.h"

#include <GLES2/gl2.h>

#include <math.h>
#include <string.h>

void wsm_gles2_render_pass_add_rect(struct wlr_render_pass *target,
		const struct wsm_render_rect_options *options) {
	const struct wlr_box *box = &options->box;
	if (box->width <= 0 || box->height <= 0) {
		return;
	}
	struct wsm_gles2_render_pass *pass = wlr_gles2_render_pass_from_render_pass(target);
	struct wsm_gles2_renderer *renderer = pass->buffer->renderer;
	int count;
	const pixman_box32_t *rects = pixman_region32_rectangles(options->clip, &count);
	wsm_gles2_push_debug(renderer);
	glEnable(GL_BLEND);
	glUseProgram(renderer->shaders.rect.program);
	wsm_gles_set_proj_matrix(renderer->shaders.rect.proj, pass->projection_matrix, box);
	glUniform4f(renderer->shaders.rect.color, options->color.r, options->color.g,
		options->color.b, options->color.a);
	glUniform4fv(renderer->shaders.rect.radius, 1, options->radii);
	glUniform2f(renderer->shaders.rect.size, box->width, box->height);
	glUniform1f(renderer->shaders.rect.smoothness,
		options->corner_smoothness > 0 ? fmaxf(1.f, options->corner_smoothness) : 4.f);
	glEnableVertexAttribArray(renderer->shaders.rect.pos);
	/* Damage can split the at-most-four corners into more rectangles. */
	enum { MAX_QUADS = 64 };
	for (int i = 0; i < count;) {
		int batch = count - i < MAX_QUADS ? count - i : MAX_QUADS;
		GLfloat vertices[MAX_QUADS][12];
		for (int j = 0; j < batch; j++, i++) {
			const pixman_box32_t *rect = &rects[i];
			float x1 = (float)(rect->x1 - box->x) / box->width;
			float y1 = (float)(rect->y1 - box->y) / box->height;
			float x2 = (float)(rect->x2 - box->x) / box->width;
			float y2 = (float)(rect->y2 - box->y) / box->height;
			const GLfloat quad[] = {
				x1, y1, x2, y1, x1, y2,
				x2, y1, x2, y2, x1, y2,
			};
			memcpy(vertices[j], quad, sizeof(quad));
		}
		glVertexAttribPointer(renderer->shaders.rect.pos, 2, GL_FLOAT, GL_FALSE, 0, vertices);
		glDrawArrays(GL_TRIANGLES, 0, batch * 6);
	}
	glDisableVertexAttribArray(renderer->shaders.rect.pos);
	wsm_gles2_pop_debug(renderer);
}
