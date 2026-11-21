#include "pass/vulkan/texture.h"
#include "pass/vulkan/pass.h"
#include "renderer/vulkan/renderer.h"
#include "texture/vulkan/texture.h"
#include "texture/vulkan/pixel_format.h"
#include "util/wsm_matrix.h"

#include <assert.h>
#include <string.h>

#include <wlr/render/color.h>
#include <wlr/render/drm_syncobj.h>
#include <wlr/util/transform.h>

void wsm_vk_render_pass_add_texture(struct wlr_render_pass *wlr_pass,
		const struct wlr_render_texture_options *options, const float radii[static 4]) {
	struct wsm_vk_render_pass *pass = wsm_vk_render_pass_from_render_pass(wlr_pass);
	struct wsm_vk_renderer *renderer = pass->renderer;
	VkCommandBuffer cb = pass->command_buffer->vk;

	struct wsm_vk_texture *texture = wsm_vk_texture_from_texture(options->texture);
	assert(texture->renderer == renderer);


	struct wlr_fbox src_box;
	wlr_render_texture_options_get_src_box(options, &src_box);
	struct wlr_box dst_box;
	wlr_render_texture_options_get_dst_box(options, &dst_box);
	float alpha = wlr_render_texture_options_get_alpha(options);

	float proj[9], matrix[9];
	wlr_matrix_identity(proj);
	wlr_matrix_project_box(matrix, &dst_box, options->transform, proj);
	wlr_matrix_multiply(matrix, pass->projection, matrix);

	int clip_rects_len;
	const pixman_box32_t *clip_rects =
		pixman_region32_rectangles(options->clip, &clip_rects_len);

	const VkDeviceSize instance_size = 4 * sizeof(float);
	struct wsm_vk_buffer_span span = vulkan_get_stage_span(renderer,
		clip_rects_len * instance_size, 16);
	if (!span.buffer) {
		pass->failed = true;
		return;
	}

	struct wsm_vk_vert_pcr_data vert_pcr_data = {
		.uv_off = {
			src_box.x / options->texture->width,
			src_box.y / options->texture->height,
		},
		.uv_size = {
			src_box.width / options->texture->width,
			src_box.height / options->texture->height,
		},
	};
	memcpy(vert_pcr_data.proj_packed, matrix, sizeof(vert_pcr_data.proj_packed));

	enum wlr_color_transfer_function tf = options->transfer_function;
	if (tf == 0) {
		tf = texture->capture_transfer ? texture->capture_transfer : WLR_COLOR_TRANSFER_FUNCTION_GAMMA22;
	}

	bool srgb_image_view = false;
	enum wsm_vk_texture_transform tex_transform = 0;
	switch (tf) {
	case WLR_COLOR_TRANSFER_FUNCTION_SRGB:
		if (texture->using_mutable_srgb) {
			tex_transform = WLR_VK_TEXTURE_TRANSFORM_IDENTITY;
			srgb_image_view = true;
		} else {
			tex_transform = WLR_VK_TEXTURE_TRANSFORM_SRGB;
		}
		break;
	case WLR_COLOR_TRANSFER_FUNCTION_EXT_LINEAR:
		tex_transform = WLR_VK_TEXTURE_TRANSFORM_IDENTITY;
		break;
	case WLR_COLOR_TRANSFER_FUNCTION_ST2084_PQ:
		tex_transform = WLR_VK_TEXTURE_TRANSFORM_ST2084_PQ;
		break;
	case WLR_COLOR_TRANSFER_FUNCTION_GAMMA22:
		tex_transform = WLR_VK_TEXTURE_TRANSFORM_GAMMA22;
		break;
	case WLR_COLOR_TRANSFER_FUNCTION_BT1886:
		tex_transform = WLR_VK_TEXTURE_TRANSFORM_BT1886;
		break;
	}

	enum wlr_color_encoding color_encoding = options->color_encoding;
	bool is_ycbcr = vulkan_format_is_ycbcr(texture->format);
	if (is_ycbcr && color_encoding == WLR_COLOR_ENCODING_NONE) {
		color_encoding = WLR_COLOR_ENCODING_BT601;
	}

	enum wlr_color_range color_range = options->color_range;
	if (is_ycbcr && color_range == WLR_COLOR_RANGE_NONE) {
		color_range = WLR_COLOR_RANGE_LIMITED;
	}

	bool rounded = radii[0] != 0 || radii[1] != 0 || radii[2] != 0 || radii[3] != 0;
	struct wsm_vk_pipeline *pipe = setup_get_or_create_pipeline(
		pass->render_setup,
		&(struct wsm_vk_pipeline_key) {
			.source = WSM_VK_SHADER_SOURCE_ROUNDED_TEXTURE,
			.layout = {
				.ycbcr = {
					.format = is_ycbcr ? texture->format : NULL,
					.encoding = color_encoding,
					.range = color_range,
				},
				.filter_mode = options->filter_mode,
			},
			.texture_transform = tex_transform,
			/* Rounded coverage introduces alpha even for opaque sources. */
			.blend_mode = rounded ? WLR_RENDER_BLEND_MODE_PREMULTIPLIED :
				!texture->has_alpha && alpha == 1.0 ? WLR_RENDER_BLEND_MODE_NONE : options->blend_mode,
		});
	if (!pipe) {
		pass->failed = true;
		return;
	}

	struct wsm_vk_texture_view *view =
		vulkan_texture_get_or_create_view(texture, pipe->layout, srgb_image_view);
	if (!view) {
		pass->failed = true;
		return;
	}

	float color_matrix[9];
	if (options->primaries != NULL) {
		struct wlr_color_primaries srgb;
		wlr_color_primaries_from_named(&srgb, WLR_COLOR_NAMED_PRIMARIES_SRGB);

		wlr_color_primaries_transform_absolute_colorimetric(options->primaries,
			&srgb, color_matrix);
	} else {
		wlr_matrix_identity(color_matrix);
	}

	float luminance_multiplier = 1;
	if (options->luminance_multiplier != NULL) {
		luminance_multiplier = *options->luminance_multiplier;
	}

	struct wsm_vk_frag_round_texture_pcr_data frag_pcr_data = {
		.alpha = alpha,
		.luminance_multiplier = luminance_multiplier,
	};
	memcpy(frag_pcr_data.matrix, color_matrix, sizeof(color_matrix));
	memcpy(frag_pcr_data.radii, radii, sizeof(frag_pcr_data.radii));
	frag_pcr_data.box[0] = dst_box.x;
	frag_pcr_data.box[1] = dst_box.y;
	frag_pcr_data.box[2] = dst_box.width;
	frag_pcr_data.box[3] = dst_box.height;

	if (pass->bound_pipeline != pipe->vk) {
		vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe->vk);
		pass->bound_pipeline = pipe->vk;
	}

	vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS,
		pipe->layout->vk, 0, 1, &view->ds, 0, NULL);

	vkCmdPushConstants(cb, pipe->layout->vk,
		VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(vert_pcr_data), &vert_pcr_data);
	vkCmdPushConstants(cb, pipe->layout->vk,
		VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(vert_pcr_data),
		sizeof(frag_pcr_data), &frag_pcr_data);

	float *instance_data = (float *)((char *)span.buffer->cpu_mapping + span.offset);
	for (int i = 0; i < clip_rects_len; i++) {
		const pixman_box32_t *rect = &clip_rects[i];
		if (pass->two_pass) {
			rect_union_add(&pass->updated_region, rect);
		}

		struct wlr_fbox norm = {
			.x = (double)(rect->x1 - dst_box.x) / dst_box.width,
			.y = (double)(rect->y1 - dst_box.y) / dst_box.height,
			.width = (double)(rect->x2 - rect->x1) / dst_box.width,
			.height = (double)(rect->y2 - rect->y1) / dst_box.height,
		};

		if (options->transform != WL_OUTPUT_TRANSFORM_NORMAL) {
			wlr_fbox_transform(&norm, &norm, options->transform, 1.0, 1.0);
		}

		instance_data[i * 4 + 0] = (float)norm.x;
		instance_data[i * 4 + 1] = (float)norm.y;
		instance_data[i * 4 + 2] = (float)norm.width;
		instance_data[i * 4 + 3] = (float)norm.height;
	}

	VkDeviceSize vb_offset = span.offset;
	vkCmdBindVertexBuffers(cb, 0, 1, &span.buffer->buffer, &vb_offset);
	vkCmdDraw(cb, 4, clip_rects_len, 0, 0);

	if (!wsm_vk_render_pass_use_texture(pass, texture, options->wait_timeline, options->wait_point)) {
		pass->failed = true;
	}
}

static bool render_pass_use_texture_access(struct wsm_vk_render_pass *pass,
		struct wsm_vk_texture *texture, struct wlr_drm_syncobj_timeline *timeline, uint64_t point, bool write) {
	struct wsm_vk_renderer *renderer = pass->renderer;
	if (texture->dmabuf_imported && !texture->owned) {
		// Store this texture in the list of textures that need to be
		// acquired before rendering and released after rendering.
		// We don't do it here immediately since barriers inside
		// a renderpass are suboptimal (would require additional renderpass
		// dependency and potentially multiple barriers) and it's
		// better to issue one barrier for all used textures anyways.
		texture->owned = true;
		assert(texture->foreign_link.prev == NULL);
		assert(texture->foreign_link.next == NULL);
		wl_list_insert(&renderer->foreign_textures, &texture->foreign_link);
	}

	texture->last_used_cb = pass->command_buffer;
	if (texture->dmabuf_imported || timeline) {
		struct wsm_vk_render_pass_texture *existing;
		wl_array_for_each(existing, &pass->textures) {
			if (existing->texture == texture && existing->wait_timeline == timeline &&
					(!timeline || existing->wait_point == point)) {
				existing->write |= write;
				return true;
			}
		}
		struct wsm_vk_render_pass_texture *dep = wl_array_add(&pass->textures, sizeof(*dep));
		if (!dep) {
			return false;
		}
		*dep = (struct wsm_vk_render_pass_texture){
			.texture = texture, .wait_timeline = timeline ? wlr_drm_syncobj_timeline_ref(timeline) : NULL,
			.wait_point = point, .write = write,
		};
	}
	return true;
}

bool wsm_vk_render_pass_use_texture(struct wsm_vk_render_pass *pass,
		struct wsm_vk_texture *texture, struct wlr_drm_syncobj_timeline *timeline, uint64_t point) {
	return render_pass_use_texture_access(pass, texture, timeline, point, false);
}

bool wsm_vk_render_pass_use_texture_write(struct wsm_vk_render_pass *pass,
		struct wsm_vk_texture *texture) {
	return render_pass_use_texture_access(pass, texture, NULL, 0, true);
}
