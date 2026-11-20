#include "pass/vulkan/shadow.h"
#include "texture/vulkan/texture.h"
#include "util/wsm_matrix.h"

#include <limits.h>
#include <math.h>
#include <string.h>

static bool render_shadow(struct wsm_vk_command_buffer *cb,
		struct wsm_vk_texture *target, const void *frag, size_t frag_size) {
	struct wsm_vk_renderer *renderer = target->renderer;
	struct wsm_vk_pipeline *pipe = setup_get_or_create_pipeline(target->render_setup,
		&(struct wsm_vk_pipeline_key){
			.source = WSM_VK_SHADER_SOURCE_SHADOW,
			.blend_mode = WLR_RENDER_BLEND_MODE_NONE,
			.layout.filter_mode = WLR_SCALE_FILTER_BILINEAR,
		});
	if (!pipe) {
		return false;
	}

	struct wsm_vk_buffer_span span = vulkan_get_stage_span(renderer, 4 * sizeof(float), 16);
	if (!span.buffer) {
		return false;
	}

	float instance[4] = {0, 0, 1, 1};
	memcpy((char *)span.buffer->cpu_mapping + span.offset, instance, sizeof(instance));
	int width = target->wlr_texture.width, height = target->wlr_texture.height;
	float projection[9], matrix[9], identity[9];
	matrix_projection(projection, width, height, WL_OUTPUT_TRANSFORM_FLIPPED_180);
	wlr_matrix_identity(identity);
	struct wlr_box box = {0, 0, width, height};
	wlr_matrix_project_box(matrix, &box, WL_OUTPUT_TRANSFORM_NORMAL, identity);
	wlr_matrix_multiply(matrix, projection, matrix);
	struct wsm_vk_vert_pcr_data vert = {.uv_size = {1, 1}};
	memcpy(vert.proj_packed, matrix, sizeof(vert.proj_packed));
	wsm_vk_change_layout(cb->vk, target->image,
		target->transitioned ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_UNDEFINED,
		VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
		VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
	VkRect2D rect = {.extent = {width, height}};
	VkRenderPassBeginInfo begin = {
		.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
		.renderPass = target->render_setup->render_pass,
		.framebuffer = target->framebuffer, .renderArea = rect,
	};
	vkCmdBeginRenderPass(cb->vk, &begin, VK_SUBPASS_CONTENTS_INLINE);
	vkCmdSetViewport(cb->vk, 0, 1, &(VkViewport){.width = width, .height = height, .maxDepth = 1});
	vkCmdSetScissor(cb->vk, 0, 1, &rect);
	vkCmdBindPipeline(cb->vk, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe->vk);
	vkCmdPushConstants(cb->vk, pipe->layout->vk, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(vert), &vert);
	vkCmdPushConstants(cb->vk, pipe->layout->vk, VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(vert), frag_size, frag);
	vkCmdBindVertexBuffers(cb->vk, 0, 1, &span.buffer->buffer, &span.offset);
	vkCmdDraw(cb->vk, 4, 1, 0, 0);
	vkCmdEndRenderPass(cb->vk);
	wsm_vk_change_layout(cb->vk, target->image, VK_IMAGE_LAYOUT_GENERAL,
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT);
	target->transitioned = true;
	target->last_used_cb = cb;
	return true;
}

struct wsm_scene_shadow_cache *wsm_vk_shadow_cache_create(
		struct wlr_renderer *base, const struct wsm_scene_shadow_slices *margins,
		float corner_extent, const float radii[static 4], float inner_offset,
		const float color[static 4]) {
	int radius = ceilf(corner_extent);
	int64_t width = (int64_t)margins->left + 2 * radius + 1 + margins->right;
	int64_t height = (int64_t)margins->top + 2 * radius + 1 + margins->bottom;
	if (width <= 0 || height <= 0 || width > INT_MAX / 4 || height > INT_MAX) {
		return NULL;
	}

	struct wsm_vk_renderer *renderer = wsm_vk_renderer_from_renderer(base);
	struct wsm_vk_texture *texture = wsm_vk_texture_create_render_target(renderer, width, height);
	if (!texture) {
		return NULL;
	}

	struct {
		float color[4], slices[4], radii[4], size[2];
		float smoothness, inset, spread, inner_offset;
	} frag = {
		.slices = {margins->left, margins->right, margins->top, margins->bottom},
		.size = {width, height}, .smoothness = 4, .inset = 13, .spread = 42,
		.inner_offset = inner_offset,
	};
	memcpy(frag.color, color, sizeof(frag.color));
	memcpy(frag.radii, radii, sizeof(frag.radii));
	if (!vulkan_record_stage_cb(renderer) || !render_shadow(renderer->stage.cb,
			texture, &frag, sizeof(frag))) {
		wlr_texture_destroy(&texture->wlr_texture);
		return NULL;
	}

	struct wsm_scene_shadow_slices slices = {
		.left = margins->left + radius, .right = margins->right + radius,
		.top = margins->top + radius, .bottom = margins->bottom + radius,
	};
	struct wsm_scene_shadow_cache *cache = wsm_scene_shadow_cache_create_from_texture(
		&texture->wlr_texture, &slices);
	if (!cache) {
		wlr_texture_destroy(&texture->wlr_texture);
	}

	return cache;
}
