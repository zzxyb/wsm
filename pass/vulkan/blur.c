#include "pass/vulkan/blur.h"
#include "pass/vulkan/pass.h"
#include "pass/texture.h"
#include "buffer/vulkan/buffer.h"
#include "texture/vulkan/pixel_format.h"
#include "texture/vulkan/texture.h"
#include "util/wsm_matrix.h"

#include <drm_fourcc.h>
#include <math.h>
#include <string.h>

static bool render_blur_quad(struct wsm_vk_command_buffer *cb,
		struct wsm_vk_texture *target, struct wsm_vk_texture *source,
		enum wsm_vk_shader_source shader, const void *frag, size_t frag_size,
		const struct wlr_fbox *uv, enum wl_output_transform transform) {
	struct wsm_vk_renderer *renderer = target->renderer;
	struct wsm_vk_pipeline *pipe = setup_get_or_create_pipeline(target->render_setup,
		&(struct wsm_vk_pipeline_key){
			.source = shader, .blend_mode = WLR_RENDER_BLEND_MODE_NONE,
			.layout = {
				.filter_mode = WLR_SCALE_FILTER_BILINEAR,
				.ycbcr = {
					.format = vulkan_format_is_ycbcr(source->format) ? source->format : NULL,
					.encoding = vulkan_format_is_ycbcr(source->format) ? WLR_COLOR_ENCODING_BT601 : 0,
					.range = vulkan_format_is_ycbcr(source->format) ? WLR_COLOR_RANGE_LIMITED : 0,
				},
			},
		});
	if (!pipe) {
		return false;
	}
	struct wsm_vk_texture_view *view = vulkan_texture_get_or_create_view(source, pipe->layout, false);
	if (!view) {
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
	wlr_matrix_project_box(matrix, &box, transform, identity);
	wlr_matrix_multiply(matrix, projection, matrix);
	struct wsm_vk_vert_pcr_data vert = {.uv_size = {1, 1}};
	memcpy(vert.proj_packed, matrix, sizeof(vert.proj_packed));
	if (uv) {
		vert.uv_off[0] = uv->x; vert.uv_off[1] = uv->y;
		vert.uv_size[0] = uv->width; vert.uv_size[1] = uv->height;
	}
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
	vkCmdBindDescriptorSets(cb->vk, VK_PIPELINE_BIND_POINT_GRAPHICS,
		pipe->layout->vk, 0, 1, &view->ds, 0, NULL);
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
	source->last_used_cb = cb;
	return true;
}

static bool ensure_target(struct wsm_vk_renderer *renderer,
		struct wsm_vk_texture **texture, int width, int height) {
	if (*texture && ((*texture)->wlr_texture.width != (uint32_t)width ||
			(*texture)->wlr_texture.height != (uint32_t)height)) {
		wlr_texture_destroy(&(*texture)->wlr_texture);
		*texture = NULL;
	}
	if (!*texture) {
		*texture = wsm_vk_texture_create_render_target(renderer, width, height);
	}
	return *texture != NULL;
}

void wsm_vk_blur_finish(struct wsm_vk_renderer *renderer) {
	for (int i = 0; i <= WSM_BLUR_BUFFER_MAX_PASSES; i++) {
		if (renderer->blur.levels[i]) {
			wlr_texture_destroy(&renderer->blur.levels[i]->wlr_texture);
		}
	}
	if (renderer->blur.filtered) {
		wlr_texture_destroy(&renderer->blur.filtered->wlr_texture);
	}
	memset(&renderer->blur, 0, sizeof(renderer->blur));
}

void wsm_vk_render_pass_add_blur(struct wlr_render_pass *target,
		const struct wsm_render_blur_buffer_options *options) {
	struct wsm_vk_render_pass *pass = wsm_vk_render_pass_from_render_pass(target);
	struct wsm_vk_texture *source = wsm_vk_texture_from_texture(options->texture);
	if (source->image == pass->render_buffer->image) {
		return;
	}
	pixman_region32_t clip;
	pixman_region32_init_rect(&clip, options->dst_box.x, options->dst_box.y,
		options->dst_box.width, options->dst_box.height);
	if (options->clip) {
		pixman_region32_intersect(&clip, &clip, options->clip);
	}
	if (pixman_region32_empty(&clip)) {
		pixman_region32_fini(&clip);
		return;
	}
	int iterations = options->iterations <= 0 ? 4 : options->iterations;
	if (iterations > WSM_BLUR_BUFFER_MAX_PASSES) {
		iterations = WSM_BLUR_BUFFER_MAX_PASSES;
	}
	int min_size = fmin(options->dst_box.width, options->dst_box.height);
	while (iterations > 1 && (min_size >> iterations) < 2) {
		iterations--;
	}
	struct wsm_vk_texture **levels = pass->renderer->blur.levels;
	int width = options->dst_box.width + 2 * options->padding;
	int height = options->dst_box.height + 2 * options->padding;
	for (int i = 0; i <= iterations; i++) {
		width = width > 1 ? width / 2 : 1;
		height = height > 1 ? height / 2 : 1;
		if (!ensure_target(pass->renderer, &levels[i], width, height)) {
			goto out;
		}
	}
	if (!ensure_target(pass->renderer, &pass->renderer->blur.filtered,
			options->dst_box.width, options->dst_box.height)) {
		goto out;
	}
	struct wsm_vk_texture *filtered = pass->renderer->blur.filtered;
	if (!wsm_vk_render_pass_use_texture(pass, source, options->wait_timeline, options->wait_point)) {
		pass->failed = true;
		goto out;
	}
	struct wlr_fbox uv = options->src_box;
	if (wlr_fbox_empty(&uv)) {
		uv = (struct wlr_fbox){0, 0, source->wlr_texture.width, source->wlr_texture.height};
	}
	uv.x /= source->wlr_texture.width;
	uv.y /= source->wlr_texture.height;
	uv.width /= source->wlr_texture.width;
	uv.height /= source->wlr_texture.height;
	float offset = options->offset == 0 ? 1.8f : options->offset;
	wsm_vk_render_pass_suspend(pass);
	bool ok = true;
	for (int i = 0; i <= iterations && ok; i++) {
		struct wsm_vk_texture *src = i == 0 ? source : levels[i - 1];
		struct {
			float halfpixel[2], offset;
			int32_t capture_encoding;
		} frag = {
			.halfpixel = {.5f / src->wlr_texture.width, .5f / src->wlr_texture.height},
			.offset = offset,
		};
		if (src->capture_transfer == WLR_COLOR_TRANSFER_FUNCTION_EXT_LINEAR) {
			frag.capture_encoding = 1;
		} else if (src->capture_transfer == WLR_COLOR_TRANSFER_FUNCTION_SRGB) {
			frag.capture_encoding = 2;
		}
		ok = render_blur_quad(pass->command_buffer, levels[i], src,
			WSM_VK_SHADER_SOURCE_BLUR_DOWN, &frag, sizeof(frag), i == 0 ? &uv : NULL,
			i == 0 ? options->transform : WL_OUTPUT_TRANSFORM_NORMAL);
	}
	for (int i = iterations; i > 0 && ok; i--) {
		struct {float halfpixel[2], offset;} frag = {
			.halfpixel = {.5f / levels[i]->wlr_texture.width, .5f / levels[i]->wlr_texture.height},
			.offset = offset,
		};
		ok = render_blur_quad(pass->command_buffer, levels[i - 1], levels[i],
			WSM_VK_SHADER_SOURCE_BLUR_UP, &frag, sizeof(frag), NULL, WL_OUTPUT_TRANSFORM_NORMAL);
	}
	struct {float tint[4], saturation, brightness, noise, contrast;} frag = {
		.saturation = options->saturation == 0 ? 1.35f : options->saturation,
		.brightness = options->brightness == 0 ? 1.06f : options->brightness,
		.noise = options->noise_amount, .contrast = options->material_contrast,
	};
	memcpy(frag.tint, options->tint, sizeof(frag.tint));
	float full_width = options->dst_box.width + 2 * options->padding;
	float full_height = options->dst_box.height + 2 * options->padding;
	uv = (struct wlr_fbox){
		.x = options->padding / full_width, .y = options->padding / full_height,
		.width = options->dst_box.width / full_width, .height = options->dst_box.height / full_height,
	};
	if (ok) {
		ok = render_blur_quad(pass->command_buffer, filtered, levels[0],
			WSM_VK_SHADER_SOURCE_BLUR_FINISH, &frag, sizeof(frag), &uv, WL_OUTPUT_TRANSFORM_NORMAL);
	}
	wsm_vk_render_pass_resume(pass);
	if (ok) {
		float radii[4] = {options->top_left_radius, options->top_right_radius,
			options->bottom_left_radius, options->bottom_right_radius};
		wsm_render_pass_add_texture(target, &(struct wlr_render_texture_options){
			.texture = &filtered->wlr_texture, .dst_box = options->dst_box,
			.alpha = options->alpha, .clip = &clip,
			.filter_mode = WLR_SCALE_FILTER_NEAREST, .blend_mode = options->blend_mode,
		}, radii);
	} else {
		pass->failed = true;
	}
out:
	pixman_region32_fini(&clip);
}

struct wlr_texture *wsm_vk_render_pass_capture_blur(struct wlr_render_pass *target,
		struct wlr_texture *previous, const struct wlr_box *box, const pixman_region32_t *damage) {
	struct wsm_vk_render_pass *pass = wsm_vk_render_pass_from_render_pass(target);
	int width = pass->render_buffer->wlr_buffer->width;
	int height = pass->render_buffer->wlr_buffer->height;
	struct wlr_box extent = box ? *box : (struct wlr_box){0, 0, width, height};
	if (extent.width <= 0 || extent.height <= 0 || (previous &&
			(previous->width != (uint32_t)extent.width || previous->height != (uint32_t)extent.height))) {
		return NULL;
	}
	static const struct wsm_vk_format blend_format = {
		.drm = DRM_FORMAT_ABGR16161616F, .vk = VK_FORMAT_R16G16B16A16_SFLOAT,
	};
	const struct wsm_vk_format *format = pass->two_pass ?
		&blend_format : pass->render_setup->render_format;
	enum wlr_color_transfer_function transfer = !pass->two_pass && pass->render_setup->use_srgb ?
		WLR_COLOR_TRANSFER_FUNCTION_SRGB : WLR_COLOR_TRANSFER_FUNCTION_EXT_LINEAR;
	struct wsm_vk_texture *old = previous ? wsm_vk_texture_from_texture(previous) : NULL;
	if (old && !old->capture_transfer) {
		return NULL;
	}
	struct wsm_vk_texture *texture = old;
	if (!texture || texture->format->vk != format->vk ||
			texture->format->drm != format->drm || texture->capture_transfer != transfer) {
		texture = wsm_vk_texture_create_render_target_format(pass->renderer,
			extent.width, extent.height, format);
		if (!texture) {
			return NULL;
		}
		texture->capture_transfer = transfer;
	}
	pixman_region32_t region;
	pixman_region32_init_rect(&region, extent.x, extent.y, extent.width, extent.height);
	pixman_region32_intersect_rect(&region, &region, 0, 0, width, height);
	if (damage && (!old || old == texture)) {
		pixman_region32_intersect(&region, &region, damage);
	}
	wsm_vk_render_pass_suspend(pass);
	VkCommandBuffer cb = pass->command_buffer->vk;
	VkImage image = pass->two_pass ? pass->render_buffer->two_pass.blend_image : pass->render_buffer->image;
	VkImageLayout layout = pass->two_pass ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_GENERAL;
	wsm_vk_change_layout(cb, image, layout, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
		VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_READ_BIT);
	wsm_vk_change_layout(cb, texture->image,
		texture->transitioned ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_UNDEFINED,
		texture->transitioned ? VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT : VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
		texture->transitioned ? VK_ACCESS_SHADER_READ_BIT : 0,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT);
	int count;
	pixman_box32_t *rects = pixman_region32_rectangles(&region, &count);
	for (int i = 0; i < count; i++) {
		VkImageCopy copy = {
			.srcSubresource = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .layerCount = 1},
			.dstSubresource = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .layerCount = 1},
			.srcOffset = {rects[i].x1, rects[i].y1, 0},
			.dstOffset = {rects[i].x1 - extent.x, rects[i].y1 - extent.y, 0},
			.extent = {rects[i].x2 - rects[i].x1, rects[i].y2 - rects[i].y1, 1},
		};
		vkCmdCopyImage(cb, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			texture->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
	}
	wsm_vk_change_layout(cb, texture->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT);
	texture->transitioned = true;
	texture->last_used_cb = pass->command_buffer;
	wsm_vk_change_layout(cb, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_READ_BIT, layout,
		VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT);
	wsm_vk_render_pass_resume(pass);
	pixman_region32_fini(&region);
	if (old && old != texture) {
		wlr_texture_destroy(&old->wlr_texture);
	}
	return &texture->wlr_texture;
}
