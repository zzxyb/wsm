#include "pass/gles/round_border.h"
#include "pass/gles/pass.h"
#include "renderer/gles/renderer.h"
#include "util/wsm_log.h"

#include <GLES2/gl2.h>
#include <math.h>

void wsm_gles2_render_pass_add_round_border(struct wlr_render_pass *target,
		const struct wsm_render_round_border_options *options) {
	const struct wlr_box *box = &options->box;
	if (box->width <= 0 || box->height <= 0) {
		return;
	}
	struct wsm_gles2_render_pass *pass = wlr_gles2_render_pass_from_render_pass(target);
	struct wsm_gles2_renderer *renderer = pass->buffer->renderer;
	pixman_region32_t region;
	pixman_region32_init_rect(&region, box->x, box->y, box->width, box->height);
	if (options->clip != NULL) {
		pixman_region32_intersect(&region, &region, options->clip);
	}
	int count;
	const pixman_box32_t *rects = pixman_region32_rectangles(&region, &count);
	if (count == 0) {
		pixman_region32_fini(&region);
		return;
	}
	wsm_gles2_push_debug(renderer);
	glEnable(GL_BLEND);
	glUseProgram(renderer->shaders.round_border.program);
	wsm_gles_set_proj_matrix(renderer->shaders.round_border.proj, pass->projection_matrix, box);
	glUniform4f(renderer->shaders.round_border.color, options->color.r, options->color.g,
		options->color.b, options->color.a);
	glUniform4fv(renderer->shaders.round_border.radius, 1, options->radii);
	glUniform2f(renderer->shaders.round_border.size, box->width, box->height);
	glUniform1f(renderer->shaders.round_border.border_width, options->border_width);
	glUniform1f(renderer->shaders.round_border.smoothness,
		options->corner_smoothness > 0 ? fmaxf(1.f, options->corner_smoothness) : 4.f);
	glEnableVertexAttribArray(renderer->shaders.round_border.pos);
	for (int i = 0; i < count; i++) {
		const pixman_box32_t *rect = &rects[i];
		float x1 = (float)(rect->x1 - box->x) / box->width;
		float y1 = (float)(rect->y1 - box->y) / box->height;
		float x2 = (float)(rect->x2 - box->x) / box->width;
		float y2 = (float)(rect->y2 - box->y) / box->height;
		const GLfloat vertices[] = {
			x1, y1, x2, y1, x1, y2,
			x2, y1, x2, y2, x1, y2,
		};
		glVertexAttribPointer(renderer->shaders.round_border.pos, 2, GL_FLOAT, GL_FALSE, 0, vertices);
		glDrawArrays(GL_TRIANGLES, 0, 6);
	}
	glDisableVertexAttribArray(renderer->shaders.round_border.pos);
	wsm_gles2_pop_debug(renderer);
	pixman_region32_fini(&region);
}
