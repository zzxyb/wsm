#ifndef BUFFER_VULKAN_BUFFER_H
#define BUFFER_VULKAN_BUFFER_H

#include "renderer/vulkan/renderer.h"

void destroy_render_buffer(struct wlr_vk_render_buffer *buffer);
struct wlr_vk_render_buffer *create_render_buffer(
	struct wlr_vk_renderer *renderer, struct wlr_buffer *buffer);
struct wlr_vk_render_buffer *get_render_buffer(
	struct wlr_vk_renderer *renderer, struct wlr_buffer *buffer);

#endif
