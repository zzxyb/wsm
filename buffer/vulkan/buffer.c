#include "buffer/vulkan/buffer.h"
#include "renderer/vulkan/dmabuf.h"
#include "texture/vulkan/texture.h"
#include "texture/vulkan/pixel_format.h"
#include "util/wsm_log.h"

#include <assert.h>
#include <inttypes.h>
#include <stdlib.h>
#include <drm_fourcc.h>
#include <fcntl.h>
#include <poll.h>

#include <wlr/types/wlr_buffer.h>

static void finish_render_buffer_out(struct wsm_vk_render_buffer_out *out,
		VkDevice dev) {
	vkDestroyFramebuffer(dev, out->framebuffer, NULL);
	vkDestroyImageView(dev, out->image_view, NULL);
}

static void handle_render_buffer_destroy(struct wlr_addon *addon) {
	struct wsm_vk_render_buffer *buffer = wl_container_of(addon, buffer, addon);
	wsm_vk_render_buffer_destroy(buffer);
}

static struct wlr_addon_interface render_buffer_addon_impl = {
	.name = "wsm_vk_render_buffer",
	.destroy = handle_render_buffer_destroy,
};

bool wsm_vk_setup_two_pass_framebuffer(struct wsm_vk_render_buffer *buffer,
		const struct wlr_dmabuf_attributes *dmabuf) {
	struct wsm_vk_renderer *renderer = buffer->renderer;
	VkResult res;
	VkDevice dev = renderer->dev->dev;

	const struct wsm_vk_format_props *fmt = vulkan_format_props_from_drm(
		renderer->dev, dmabuf->format);
	assert(fmt);

	VkImageViewCreateInfo view_info = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.image = buffer->image,
		.viewType = VK_IMAGE_VIEW_TYPE_2D,
		.format = fmt->format.vk,
		.components.r = VK_COMPONENT_SWIZZLE_IDENTITY,
		.components.g = VK_COMPONENT_SWIZZLE_IDENTITY,
		.components.b = VK_COMPONENT_SWIZZLE_IDENTITY,
		.components.a = VK_COMPONENT_SWIZZLE_IDENTITY,
		.subresourceRange = (VkImageSubresourceRange) {
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.baseMipLevel = 0,
			.levelCount = 1,
			.baseArrayLayer = 0,
			.layerCount = 1,
		},
	};

	res = vkCreateImageView(dev, &view_info, NULL, &buffer->two_pass.out.image_view);
	if (res != VK_SUCCESS) {
		wsm_vk_error("vkCreateImageView failed", res);
		goto error;
	}

	buffer->two_pass.render_setup = find_or_create_render_setup(
		renderer, &fmt->format, true, false);
	if (!buffer->two_pass.render_setup) {
		goto error;
	}

	// Set up an extra 16F buffer on which to do linear blending,
	// and afterwards to render onto the target
	VkImageCreateInfo img_info = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		.imageType = VK_IMAGE_TYPE_2D,
		.format = VK_FORMAT_R16G16B16A16_SFLOAT,
		.mipLevels = 1,
		.arrayLayers = 1,
		.samples = VK_SAMPLE_COUNT_1_BIT,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
		.tiling = VK_IMAGE_TILING_OPTIMAL,
		.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
		.extent = (VkExtent3D) { dmabuf->width, dmabuf->height, 1 },
		.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT,
	};

	res = vkCreateImage(dev, &img_info, NULL, &buffer->two_pass.blend_image);
	if (res != VK_SUCCESS) {
		wsm_vk_error("vkCreateImage failed", res);
		goto error;
	}

	VkMemoryRequirements mem_reqs;
	vkGetImageMemoryRequirements(dev, buffer->two_pass.blend_image, &mem_reqs);

	int mem_type_index = wsm_vk_find_mem_type(renderer->dev,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, mem_reqs.memoryTypeBits);
	if (mem_type_index == -1) {
		wsm_log(WSM_ERROR, "failed to find suitable vulkan memory type");
		goto error;
	}

	VkMemoryAllocateInfo mem_info = {
		.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
		.allocationSize = mem_reqs.size,
		.memoryTypeIndex = mem_type_index,
	};

	res = vkAllocateMemory(dev, &mem_info, NULL, &buffer->two_pass.blend_memory);
	if (res != VK_SUCCESS) {
		wsm_vk_error("vkAllocatorMemory failed", res);
		goto error;
	}

	res = vkBindImageMemory(dev, buffer->two_pass.blend_image, buffer->two_pass.blend_memory, 0);
	if (res != VK_SUCCESS) {
		wsm_vk_error("vkBindMemory failed", res);
		goto error;
	}

	VkImageViewCreateInfo blend_view_info = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.image = buffer->two_pass.blend_image,
		.viewType = VK_IMAGE_VIEW_TYPE_2D,
		.format = img_info.format,
		.components.r = VK_COMPONENT_SWIZZLE_IDENTITY,
		.components.g = VK_COMPONENT_SWIZZLE_IDENTITY,
		.components.b = VK_COMPONENT_SWIZZLE_IDENTITY,
		.components.a = VK_COMPONENT_SWIZZLE_IDENTITY,
		.subresourceRange = (VkImageSubresourceRange) {
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.baseMipLevel = 0,
			.levelCount = 1,
			.baseArrayLayer = 0,
			.layerCount = 1,
		},
	};

	res = vkCreateImageView(dev, &blend_view_info, NULL, &buffer->two_pass.blend_image_view);
	if (res != VK_SUCCESS) {
		wsm_vk_error("vkCreateImageView failed", res);
		goto error;
	}

	buffer->two_pass.blend_attachment_pool = vulkan_alloc_blend_ds(renderer,
		&buffer->two_pass.blend_descriptor_set);
	if (!buffer->two_pass.blend_attachment_pool) {
		wsm_log(WSM_ERROR, "failed to allocate descriptor");
		goto error;
	}

	VkDescriptorImageInfo ds_attach_info = {
		.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		.imageView = buffer->two_pass.blend_image_view,
		.sampler = VK_NULL_HANDLE,
	};
	VkWriteDescriptorSet ds_write = {
		.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		.descriptorCount = 1,
		.descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
		.dstSet = buffer->two_pass.blend_descriptor_set,
		.dstBinding = 0,
		.pImageInfo = &ds_attach_info,
	};
	vkUpdateDescriptorSets(dev, 1, &ds_write, 0, NULL);

	VkImageView attachments[] = {
		buffer->two_pass.blend_image_view,
		buffer->two_pass.out.image_view,
	};
	VkFramebufferCreateInfo fb_info = {
		.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
		.attachmentCount = sizeof(attachments) / sizeof(attachments[0]),
		.pAttachments = attachments,
		.flags = 0u,
		.width = dmabuf->width,
		.height = dmabuf->height,
		.layers = 1u,
		.renderPass = buffer->two_pass.render_setup->render_pass,
	};

	res = vkCreateFramebuffer(dev, &fb_info, NULL, &buffer->two_pass.out.framebuffer);
	if (res != VK_SUCCESS) {
		wsm_vk_error("vkCreateFramebuffer", res);
		goto error;
	}

	return true;

error:
	// cleaning up everything is the caller's responsibility,
	// since it will need to do this anyway if framebuffer setup fails
	return false;
}

bool wsm_vk_setup_one_pass_framebuffer(struct wsm_vk_render_buffer *buffer,
		const struct wlr_dmabuf_attributes *dmabuf, bool srgb) {
	struct wsm_vk_renderer *renderer = buffer->renderer;
	VkResult res;
	VkDevice dev = renderer->dev->dev;

	const struct wsm_vk_format_props *fmt = vulkan_format_props_from_drm(
		renderer->dev, dmabuf->format);
	assert(fmt);

	VkFormat vk_fmt = srgb ? fmt->format.vk_srgb : fmt->format.vk;
	assert(vk_fmt != VK_FORMAT_UNDEFINED);

	struct wsm_vk_render_buffer_out *out = srgb ? &buffer->srgb.out : &buffer->linear.out;

	// Set up the srgb framebuffer by default; two-pass framebuffer and
	// blending image will be set up later if necessary
	VkImageViewCreateInfo view_info = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.image = buffer->image,
		.viewType = VK_IMAGE_VIEW_TYPE_2D,
		.format = vk_fmt,
		.components.r = VK_COMPONENT_SWIZZLE_IDENTITY,
		.components.g = VK_COMPONENT_SWIZZLE_IDENTITY,
		.components.b = VK_COMPONENT_SWIZZLE_IDENTITY,
		.components.a = VK_COMPONENT_SWIZZLE_IDENTITY,
		.subresourceRange = (VkImageSubresourceRange) {
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.baseMipLevel = 0,
			.levelCount = 1,
			.baseArrayLayer = 0,
			.layerCount = 1,
		},
	};

	res = vkCreateImageView(dev, &view_info, NULL, &out->image_view);
	if (res != VK_SUCCESS) {
		wsm_vk_error("vkCreateImageView failed", res);
		goto error;
	}

	struct wsm_vk_render_format_setup *render_setup =
		find_or_create_render_setup(renderer, &fmt->format, false, srgb);
	if (!render_setup) {
		goto error;
	}

	VkFramebufferCreateInfo fb_info = {
		.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
		.attachmentCount = 1,
		.pAttachments = &out->image_view,
		.flags = 0u,
		.width = dmabuf->width,
		.height = dmabuf->height,
		.layers = 1u,
		.renderPass = render_setup->render_pass,
	};

	res = vkCreateFramebuffer(dev, &fb_info, NULL, &out->framebuffer);
	if (res != VK_SUCCESS) {
		wsm_vk_error("vkCreateFramebuffer", res);
		goto error;
	}

	if (srgb) {
		buffer->srgb.render_setup = render_setup;
	} else {
		buffer->linear.render_setup = render_setup;
	}

	return true;

error:
	// cleaning up everything is the caller's responsibility,
	// since it will need to do this anyway if framebuffer setup fails
	return false;
}

struct wsm_vk_render_buffer *wsm_vk_render_buffer_create(
		struct wsm_vk_renderer *renderer, struct wlr_buffer *wlr_buffer) {
	struct wsm_vk_render_buffer *buffer = calloc(1, sizeof(*buffer));
	if (buffer == NULL) {
		wsm_log_errno(WSM_ERROR, "Allocation failed");
		return NULL;
	}

	buffer->wlr_buffer = wlr_buffer;
	buffer->renderer = renderer;
	wlr_addon_init(&buffer->addon, &wlr_buffer->addons, renderer,
		&render_buffer_addon_impl);
	wl_list_insert(&renderer->render_buffers, &buffer->link);

	struct wlr_dmabuf_attributes dmabuf = {0};
	if (!wlr_buffer_get_dmabuf(wlr_buffer, &dmabuf)) {
		goto error;
	}

	wsm_log(WSM_DEBUG, "vulkan create_render_buffer: %.4s, %dx%d",
		(const char*) &dmabuf.format, dmabuf.width, dmabuf.height);

	bool using_mutable_srgb = false;
	buffer->image = wsm_vk_import_dmabuf(renderer, &dmabuf,
		buffer->memories, &buffer->mem_count, true, &using_mutable_srgb);
	if (!buffer->image) {
		goto error;
	}

	const struct wsm_vk_format_props *fmt = vulkan_format_props_from_drm(
		renderer->dev, dmabuf.format);
	if (fmt == NULL) {
		wsm_log(WSM_ERROR, "Unsupported pixel format %"PRIx32 " (%.4s)",
			dmabuf.format, (const char*) &dmabuf.format);
		goto error;
	}

	if (using_mutable_srgb) {
		if (!wsm_vk_setup_one_pass_framebuffer(buffer, &dmabuf, true)) {
			goto error;
		}
	} else {
		// Set up the two-pass framebuffer & blending image
		if (!wsm_vk_setup_two_pass_framebuffer(buffer, &dmabuf)) {
			goto error;
		}
	}

	return buffer;

error:
	if (buffer) {
		wsm_vk_render_buffer_destroy(buffer);
	}

	/* DMA-BUF attributes are borrowed from wlr_buffer. */
	return NULL;
}

void wsm_vk_render_buffer_destroy(struct wsm_vk_render_buffer *buffer) {
	wl_list_remove(&buffer->link);
	wlr_addon_finish(&buffer->addon);

	VkDevice dev = buffer->renderer->dev->dev;

	// TODO: asynchronously wait for the command buffers using this render
	// buffer to complete (just like we do for textures)
	VkResult res = vkQueueWaitIdle(buffer->renderer->dev->queue);
	if (res != VK_SUCCESS) {
		wsm_vk_error("vkQueueWaitIdle", res);
	}

	finish_render_buffer_out(&buffer->linear.out, dev);
	finish_render_buffer_out(&buffer->srgb.out, dev);

	finish_render_buffer_out(&buffer->two_pass.out, dev);
	vkDestroyImage(dev, buffer->two_pass.blend_image, NULL);
	vkFreeMemory(dev, buffer->two_pass.blend_memory, NULL);
	vkDestroyImageView(dev, buffer->two_pass.blend_image_view, NULL);
	if (buffer->two_pass.blend_attachment_pool) {
		vulkan_free_ds(buffer->renderer, buffer->two_pass.blend_attachment_pool,
				buffer->two_pass.blend_descriptor_set);
	}

	vkDestroyImage(dev, buffer->image, NULL);
	for (size_t i = 0u; i < buffer->mem_count; ++i) {
		vkFreeMemory(dev, buffer->memories[i], NULL);
	}

	free(buffer);
}

struct wsm_vk_render_buffer *wsm_get_vk_render_buffer(
		struct wsm_vk_renderer *renderer, struct wlr_buffer *wlr_buffer) {
	struct wlr_addon *addon =
		wlr_addon_find(&wlr_buffer->addons, renderer, &render_buffer_addon_impl);
	if (addon == NULL) {
		return NULL;
	}

	struct wsm_vk_render_buffer *buffer = wl_container_of(addon, buffer, addon);
	return buffer;
}

bool wsm_vk_sync_render_buffer_acquire(struct wsm_vk_render_buffer *render_buffer,
		int sync_file_fds[static WLR_DMABUF_MAX_PLANES]) {
	return wsm_buffer_export_sync_file(render_buffer->renderer, render_buffer->wlr_buffer,
		DMA_BUF_SYNC_WRITE, sync_file_fds);
}


bool wsm_buffer_export_sync_file(struct wsm_vk_renderer *renderer, struct wlr_buffer *buffer,
		uint32_t flags, int sync_file_fds[static WLR_DMABUF_MAX_PLANES]) {
	struct wlr_dmabuf_attributes dmabuf = {0};
	if (!wlr_buffer_get_dmabuf(buffer, &dmabuf)) {
		wsm_log(WSM_ERROR, "wlr_buffer_get_dmabuf() failed");
		return false;
	}

	if (!renderer->dev->implicit_sync_interop) {
		// We have no choice but to block here sadly
		for (int i = 0; i < dmabuf.n_planes; i++) {
			struct pollfd pollfd = {
				.fd = dmabuf.fd[i],
				.events = (flags & DMA_BUF_SYNC_WRITE) ? POLLOUT : POLLIN,
			};
			int timeout_ms = 1000;
			int ret = poll(&pollfd, 1, timeout_ms);
			if (ret < 0) {
				wsm_log_errno(WSM_ERROR, "Failed to wait for DMA-BUF fence");
				return false;
			} else if (ret == 0) {
				wsm_log(WSM_ERROR, "Timed out while waiting for DMA-BUF fence");
				return false;
			}
		}

		return true;
	}

	for (int i = 0; i < dmabuf.n_planes; i++) {
		int sync_file_fd = dmabuf_export_sync_file(dmabuf.fd[i], flags);
		if (sync_file_fd < 0) {
			wsm_log(WSM_ERROR, "Failed to extract DMA-BUF fence");
			return false;
		}

		sync_file_fds[i] = sync_file_fd;
	}

	return true;
}
