#include "renderer/vulkan/render_timer.h"
#include "renderer/vulkan/renderer.h"
#include "util/wsm_log.h"

#include <stdlib.h>
#include <assert.h>

static int vulkan_render_timer_get_duration_ns(
		struct wlr_render_timer *wlr_timer) {
	struct wsm_vk_render_timer *timer =
			wl_container_of(wlr_timer, timer, base);
	struct wsm_vk_renderer *renderer = timer->renderer;
	
	// Layout: [ timestamp1, avail1, timestamp2, avail2 ]
	uint64_t data[4] = {0};
	VkResult res = vkGetQueryPoolResults(renderer->dev->dev,
			timer->query_pool, 0, 2, sizeof(data), data,
			2 * sizeof(uint64_t),
			VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT);
	if (res == VK_NOT_READY || data[1] == 0 || data[3] == 0) {
		wsm_log(WSM_ERROR, "Failed to get render duration: "
						   "timestamp query results not yet ready");
		return -1;
	}
	if (res != VK_SUCCESS) {
		wsm_vk_error("vkGetQueryPoolResults", res);
		return -1;
	}
	
	uint64_t ticks = data[2] - data[0];
	return (int)(ticks * renderer->dev->timestamp_period);
}

static void vulkan_render_timer_destroy(
		struct wlr_render_timer *wlr_timer) {
	struct wsm_vk_render_timer *timer =
			wl_container_of(wlr_timer, timer, base);
	vkDestroyQueryPool(timer->renderer->dev->dev, timer->query_pool, NULL);
	free(timer);
}

static const struct wlr_render_timer_impl render_timer_impl = {
		.get_duration_ns = vulkan_render_timer_get_duration_ns,
		.destroy = vulkan_render_timer_destroy,
};

struct wsm_vk_render_timer *wsm_vk_render_timer_create(
		struct wlr_renderer *wlr_renderer) {
	struct wsm_vk_renderer *renderer = wsm_vk_renderer_from_renderer(wlr_renderer);
	if (renderer->dev->timestamp_valid_bits == 0) {
		wsm_log(WSM_ERROR, "Failed to create render timer: "
						   "timestamp queries not supported by queue family");
		return NULL;
	}
	
	struct wsm_vk_render_timer *timer = calloc(1, sizeof(*timer));
	if (!timer) {
		wsm_log_errno(WSM_ERROR, "Allocation failed");
		return NULL;
	}
	
	VkQueryPoolCreateInfo pool_info = {
			.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
			.queryType = VK_QUERY_TYPE_TIMESTAMP,
			.queryCount = 2,
	};
	VkResult res = vkCreateQueryPool(renderer->dev->dev, &pool_info,
			NULL, &timer->query_pool);
	if (res != VK_SUCCESS) {
		wsm_vk_error("vkCreateQueryPool", res);
		free(timer);
		return NULL;
	}
	
	timer->base.impl = &render_timer_impl;
	timer->renderer = renderer;
	return timer;
}

bool wlr_render_timer_is_wsm_vk(const struct wlr_render_timer *wlr_timer) {
	return wlr_timer != NULL && wlr_timer->impl == &render_timer_impl;
}

struct wsm_vk_render_timer *wlr_vk_render_timer_from_timer(
		struct wlr_render_timer *wlr_timer) {
	assert(wlr_timer->impl == &render_timer_impl);
	struct wsm_vk_render_timer *timer = wl_container_of(wlr_timer, timer, base);
	return timer;
}
