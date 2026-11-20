#include "pass/vulkan/blit.h"
#include "pass/vulkan/copy.h"
#include "pass/vulkan/pass.h"
#include "buffer/vulkan/buffer.h"
#include "texture/vulkan/texture.h"
#include "texture/vulkan/pixel_format.h"

#include <wlr/render/drm_syncobj.h>

static VkFormatFeatureFlags format_features(struct wsm_vk_render_pass *pass,
		struct wsm_vk_texture *texture, bool target) {
	if (texture->dmabuf_imported || target) {
		struct wlr_dmabuf_attributes attrs;
		struct wlr_buffer *buffer = target ? pass->render_buffer->wlr_buffer : texture->buffer;
		if (!buffer || !wlr_buffer_get_dmabuf(buffer, &attrs)) {
			return 0;
		}
		const struct wsm_vk_format_props *props = vulkan_format_props_from_drm(pass->renderer->dev, attrs.format);
		if (!props) {
			return 0;
		}
		uint32_t count = target ? props->dmabuf.render_mod_count : props->dmabuf.texture_mod_count;
		const struct wsm_vk_format_modifier_props *mods = target ?
			props->dmabuf.render_mods : props->dmabuf.texture_mods;
		for (uint32_t i = 0; i < count; i++) {
			if (mods[i].props.drmFormatModifier == attrs.modifier) {
				return mods[i].props.drmFormatModifierTilingFeatures;
			}
		}
		return 0;
	}
	VkFormatProperties props;
	vkGetPhysicalDeviceFormatProperties(pass->renderer->dev->phdev, texture->format->vk, &props);
	return props.optimalTilingFeatures;
}

bool wsm_vk_render_pass_blit(struct wlr_render_pass *target,
		const struct wsm_blit_texture_options *options) {
	struct wsm_vk_render_pass *pass = wsm_vk_render_pass_from_render_pass(target);
	if (options->texture->renderer != &pass->renderer->wlr_renderer ||
			options->dst_texture->renderer != &pass->renderer->wlr_renderer ||
			!wlr_texture_is_wsm_vk(options->texture) || !wlr_texture_is_wsm_vk(options->dst_texture)) {
		return false;
	}
	struct wsm_vk_texture *src = wsm_vk_texture_from_texture(options->texture);
	struct wsm_vk_texture *dst = wsm_vk_texture_from_texture(options->dst_texture);
	if (!wsm_vk_transfer_validate(pass, src, dst, true)) {
		return false;
	}
	bool from_target = wsm_vk_transfer_uses_target(pass, src);
	if (from_target && pass->two_pass) {
		return false;
	}
	VkFormatFeatureFlags source_features = format_features(pass, src, from_target);
	VkFormatFeatureFlags destination_features = format_features(pass, dst, false);
	if (!(source_features & VK_FORMAT_FEATURE_BLIT_SRC_BIT) ||
			!(destination_features & VK_FORMAT_FEATURE_BLIT_DST_BIT) ||
			(options->filter_mode == WLR_SCALE_FILTER_BILINEAR &&
			 !(source_features & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT))) {
		return false;
	}
	if (!wsm_vk_render_pass_use_texture_write(pass, dst)) {
		pass->failed = true;
		return false;
	}
	if (from_target) {
		/* The texture may be another VkImage importing the same DMA-BUF.
		 * Read the acquired render-buffer image, avoiding a second ownership
		 * transfer for the same storage. Keep any explicit acquire wait. */
		if (options->wait_timeline) {
			struct wsm_vk_render_pass_texture *dep = wl_array_add(&pass->textures, sizeof(*dep));
			if (!dep) {
				pass->failed = true;
				return false;
			}
			*dep = (struct wsm_vk_render_pass_texture){
				.texture = src,
				.wait_timeline = wlr_drm_syncobj_timeline_ref(options->wait_timeline),
				.wait_point = options->wait_point,
			};
		}
	} else if (!wsm_vk_render_pass_use_texture(pass, src, options->wait_timeline, options->wait_point)) {
		pass->failed = true;
		return false;
	}
	wsm_vk_render_pass_suspend(pass);
	struct wsm_vk_texture source = *src;
	if (from_target) {
		source.image = pass->render_buffer->image;
	}
	wsm_vk_transfer_barriers(pass, &source, dst, true);
	const struct wlr_box *s = &options->src_box, *d = &options->dst_box;
	VkImageBlit blit = {
		.srcSubresource = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .layerCount = 1},
		.dstSubresource = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .layerCount = 1},
		.srcOffsets = {{s->x, s->y, 0}, {s->x + s->width, s->y + s->height, 1}},
		.dstOffsets = {{d->x, d->y, 0}, {d->x + d->width, d->y + d->height, 1}},
	};
	vkCmdBlitImage(pass->command_buffer->vk, source.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		dst->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit,
		options->filter_mode == WLR_SCALE_FILTER_NEAREST ? VK_FILTER_NEAREST : VK_FILTER_LINEAR);
	wsm_vk_transfer_barriers(pass, &source, dst, false);
	src->last_used_cb = pass->command_buffer;
	wsm_vk_render_pass_resume(pass);
	return true;
}
