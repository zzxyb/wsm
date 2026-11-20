#include "pass/vulkan/copy.h"
#include "pass/vulkan/pass.h"
#include "texture/vulkan/texture.h"
#include "buffer/vulkan/buffer.h"
#include "texture/vulkan/pixel_format.h"

#include <sys/stat.h>
#include <stdlib.h>

struct buffer_identity {
	struct wlr_addon addon;
	dev_t devices[WLR_DMABUF_MAX_PLANES];
	ino_t inodes[WLR_DMABUF_MAX_PLANES];
	int n_planes;
	bool valid;
};

static void buffer_identity_destroy(struct wlr_addon *addon) {
	struct buffer_identity *identity = wl_container_of(addon, identity, addon);
	wlr_addon_finish(addon);
	free(identity);
}

static const struct wlr_addon_interface buffer_identity_impl = {
	.name = "wsm_vk_buffer_identity",
	.destroy = buffer_identity_destroy,
};

static const struct buffer_identity *get_buffer_identity(struct wlr_buffer *buffer) {
	struct wlr_addon *addon = wlr_addon_find(&buffer->addons,
		&buffer_identity_impl, &buffer_identity_impl);
	if (addon) {
		struct buffer_identity *identity = wl_container_of(addon, identity, addon);
		return identity;
	}
	struct buffer_identity *identity = calloc(1, sizeof(*identity));
	if (!identity) {
		return NULL;
	}
	wlr_addon_init(&identity->addon, &buffer->addons,
		&buffer_identity_impl, &buffer_identity_impl);
	struct wlr_dmabuf_attributes attrs;
	if (!wlr_buffer_get_dmabuf(buffer, &attrs) || attrs.n_planes <= 0 ||
			attrs.n_planes > WLR_DMABUF_MAX_PLANES) {
		return identity;
	}
	identity->n_planes = attrs.n_planes;
	/* Backing storage is fixed for a wlr_buffer's lifetime. Resolve plane
	 * identities once, rather than issuing fstat for every damage rectangle. */
	for (int i = 0; i < attrs.n_planes; i++) {
		struct stat stat;
		if (fstat(attrs.fd[i], &stat) < 0) {
			return identity;
		}
		identity->devices[i] = stat.st_dev;
		identity->inodes[i] = stat.st_ino;
	}
	identity->valid = true;
	return identity;
}

static bool buffers_alias(struct wlr_buffer *a, struct wlr_buffer *b) {
	if (!a || !b) {
		return false;
	}
	if (a == b) {
		return true;
	}
	const struct buffer_identity *aa = get_buffer_identity(a);
	const struct buffer_identity *bb = get_buffer_identity(b);
	if (!aa || !bb || !aa->valid || !bb->valid) {
		return true;
	}
	for (int i = 0; i < aa->n_planes; i++) {
		for (int j = 0; j < bb->n_planes; j++) {
			if (aa->devices[i] == bb->devices[j] && aa->inodes[i] == bb->inodes[j]) {
				return true;
			}
		}
	}
	return false;
}

bool wsm_vk_transfer_uses_target(struct wsm_vk_render_pass *pass, struct wsm_vk_texture *src) {
	return src->image == pass->render_buffer->image ||
		buffers_alias(src->buffer, pass->render_buffer->wlr_buffer);
}

bool wsm_vk_transfer_validate(struct wsm_vk_render_pass *pass,
		struct wsm_vk_texture *src, struct wsm_vk_texture *dst, bool allow_target_source) {
	return !pass->failed && src->renderer == pass->renderer && dst->renderer == pass->renderer &&
		src->image != dst->image && !buffers_alias(src->buffer, dst->buffer) &&
		!wsm_vk_transfer_uses_target(pass, dst) &&
		(allow_target_source || !wsm_vk_transfer_uses_target(pass, src)) &&
		(src->usage & VK_IMAGE_USAGE_TRANSFER_SRC_BIT) &&
		(dst->usage & VK_IMAGE_USAGE_TRANSFER_DST_BIT) &&
		!vulkan_format_is_ycbcr(src->format) && !vulkan_format_is_ycbcr(dst->format);
}

void wsm_vk_transfer_barriers(struct wsm_vk_render_pass *pass,
		struct wsm_vk_texture *src, struct wsm_vk_texture *dst, bool before) {
	VkCommandBuffer cb = pass->command_buffer->vk;
	VkImageLayout src_layout = wsm_vk_transfer_uses_target(pass, src) ?
		VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	if (before) {
		wsm_vk_change_layout(cb, src->image, src_layout,
			VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
			VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_READ_BIT);
		wsm_vk_change_layout(cb, dst->image,
			(dst->dmabuf_imported || dst->transitioned) ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_UNDEFINED,
			VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT);
	} else {
		wsm_vk_change_layout(cb, src->image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_READ_BIT, src_layout,
			VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT);
		wsm_vk_change_layout(cb, dst->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT);
		dst->transitioned = true;
		src->last_used_cb = pass->command_buffer;
		dst->last_used_cb = pass->command_buffer;
	}
}

bool wsm_vk_render_pass_copy(struct wlr_render_pass *target,
		const struct wsm_copy_texture_options *options) {
	struct wsm_vk_render_pass *pass = wsm_vk_render_pass_from_render_pass(target);
	if (options->texture->renderer != &pass->renderer->wlr_renderer ||
			options->dst_texture->renderer != &pass->renderer->wlr_renderer) {
		return false;
	}
	struct wsm_vk_texture *src = wsm_vk_texture_from_texture(options->texture);
	struct wsm_vk_texture *dst = wsm_vk_texture_from_texture(options->dst_texture);
	if (!wsm_vk_transfer_validate(pass, src, dst, false) ||
			src->format->drm != dst->format->drm || src->format->vk != dst->format->vk) {
		return false;
	}
	if (!wsm_vk_render_pass_use_texture(pass, src, options->wait_timeline, options->wait_point) ||
			!wsm_vk_render_pass_use_texture_write(pass, dst)) {
		pass->failed = true;
		return false;
	}
	wsm_vk_render_pass_suspend(pass);
	wsm_vk_transfer_barriers(pass, src, dst, true);
	VkImageCopy copy = {
		.srcSubresource = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .layerCount = 1},
		.dstSubresource = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .layerCount = 1},
		.srcOffset = {options->src_x, options->src_y, 0},
		.dstOffset = {options->dst_x, options->dst_y, 0},
		.extent = {options->width, options->height, 1},
	};
	vkCmdCopyImage(pass->command_buffer->vk, src->image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		dst->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
	wsm_vk_transfer_barriers(pass, src, dst, false);
	wsm_vk_render_pass_resume(pass);
	return true;
}
