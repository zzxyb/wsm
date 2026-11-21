#include "pass/vulkan/rect.h"
#include "pass/vulkan/pass.h"
#include "renderer/vulkan/renderer.h"
#include "buffer/vulkan/buffer.h"
#include "util/wsm_matrix.h"

#include <math.h>
#include <string.h>

void wsm_vk_render_pass_add_rect(struct wlr_render_pass *target,
		const struct wsm_render_rect_options *options) {
	struct wsm_vk_render_pass *pass = wsm_vk_render_pass_from_render_pass(target);
	const struct wlr_box *box = &options->box;
	pixman_region32_t clip;
	pixman_region32_init(&clip);
	pixman_region32_intersect_rect(&clip, options->clip, 0, 0,
		pass->render_buffer->wlr_buffer->width, pass->render_buffer->wlr_buffer->height);
	int count;
	pixman_box32_t *rects = pixman_region32_rectangles(&clip, &count);
	if (!count) {
		pixman_region32_fini(&clip);
		return;
	}

	struct wsm_vk_pipeline *pipe = setup_get_or_create_pipeline(pass->render_setup,
		&(struct wsm_vk_pipeline_key){.source = WLR_VK_SHADER_SOURCE_ROUNDED_RECT});
	struct wsm_vk_buffer_span span = vulkan_get_stage_span(pass->renderer,
		count * 4 * sizeof(float), 16);
	if (!pipe || !span.buffer) {
		pass->failed = true;
		pixman_region32_fini(&clip);
		return;
	}

	float *instances = (float *)((char *)span.buffer->cpu_mapping + span.offset);
	for (int i = 0; i < count; i++) {
		instances[4*i] = (float)(rects[i].x1 - box->x) / box->width;
		instances[4*i+1] = (float)(rects[i].y1 - box->y) / box->height;
		instances[4*i+2] = (float)(rects[i].x2 - rects[i].x1) / box->width;
		instances[4*i+3] = (float)(rects[i].y2 - rects[i].y1) / box->height;
		if (pass->two_pass) {
			rect_union_add(&pass->updated_region, &rects[i]);
		}
	}

	float identity[9], matrix[9];
	wlr_matrix_identity(identity);
	wlr_matrix_project_box(matrix, box, WL_OUTPUT_TRANSFORM_NORMAL, identity);
	wlr_matrix_multiply(matrix, pass->projection, matrix);
	struct wsm_vk_vert_pcr_data vert = {.uv_size = {1, 1}};
	memcpy(vert.proj_packed, matrix, sizeof(vert.proj_packed));
	struct {
		float color[4], radius[4], size[2], smoothness;
	} frag = {
		.radius = {options->radii[0], options->radii[1], options->radii[2], options->radii[3]},
		.size = {box->width, box->height},
		.smoothness = options->corner_smoothness > 0 ? fmaxf(1, options->corner_smoothness) : 4,
	};
	float a = options->color.a;
	frag.color[0] = a == 0 ? 0 : powf(options->color.r / a, 2.2f) * a;
	frag.color[1] = a == 0 ? 0 : powf(options->color.g / a, 2.2f) * a;
	frag.color[2] = a == 0 ? 0 : powf(options->color.b / a, 2.2f) * a;
	frag.color[3] = a;
	VkCommandBuffer cb = pass->command_buffer->vk;
	if (pass->bound_pipeline != pipe->vk) {
		vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe->vk);
		pass->bound_pipeline = pipe->vk;
	}
	vkCmdPushConstants(cb, pipe->layout->vk, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(vert), &vert);
	vkCmdPushConstants(cb, pipe->layout->vk, VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(vert), sizeof(frag), &frag);
	vkCmdBindVertexBuffers(cb, 0, 1, &span.buffer->buffer, &span.offset);
	vkCmdDraw(cb, 4, count, 0, 0);
	pixman_region32_fini(&clip);
}
