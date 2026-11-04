#ifndef PASS_VULKAN_PASS_H
#define PASS_VULKAN_PASS_H

#include <stdbool.h>
#include <stdint.h>
#include <vulkan/vulkan.h>
#include <wayland-util.h>
#include <wlr/render/interface.h>
#include "renderer/vulkan/rect_union.h"

struct wlr_vk_renderer;
struct wlr_vk_texture;
struct wlr_vk_render_buffer;
struct wlr_vk_render_buffer_out;
struct wlr_vk_render_format_setup;
struct wlr_vk_command_buffer;
struct wlr_vk_render_timer;
struct wlr_color_transform;
struct wlr_drm_syncobj_timeline;

struct wlr_vk_render_pass_texture {
	struct wlr_vk_texture *texture;

	struct wlr_drm_syncobj_timeline *wait_timeline;
	uint64_t wait_point;
};

struct wlr_vk_render_pass {
	struct wlr_render_pass base;
	struct wlr_vk_renderer *renderer;
	struct wlr_vk_render_buffer *render_buffer;
	struct wlr_vk_render_buffer_out *render_buffer_out;
	struct wlr_vk_render_format_setup *render_setup;
	struct wlr_vk_command_buffer *command_buffer;
	struct rect_union updated_region;
	VkPipeline bound_pipeline;
	float projection[9];
	bool failed;
	bool two_pass; // rendering via intermediate blending buffer
	struct wlr_color_transform *color_transform;

	struct wlr_drm_syncobj_timeline *signal_timeline;
	uint64_t signal_point;

	struct wlr_vk_render_timer *timer;

	struct wl_array textures; // struct wlr_vk_render_pass_texture
};

struct wlr_vk_render_pass *vulkan_begin_render_pass(struct wlr_vk_renderer *renderer,
	struct wlr_vk_render_buffer *buffer, const struct wlr_buffer_pass_options *options);

#endif // PASS_VULKAN_PASS_H
