/**
 * @file        instance.h
 * @brief       Vulkan instance state and lifecycle helpers.
 * @details     Defines instance handles, optional validation callbacks, and renderer instance
 *              access.
 * @author      YaoBing Xiao
 * @date        2026-11-19
 * @version     v1.0
 * @par Copyright(c):
 * @par History:
 *      version: v1.0, YaoBing Xiao, 2026-11-19, initial version\n
 */

#ifndef RENDERER_VULKAN_INSTANCE_H
#define RENDERER_VULKAN_INSTANCE_H

#include <stdbool.h>
#include <vulkan/vulkan.h>

struct wlr_renderer;

/**
 * @brief Vulkan instance and optional debug messenger resources.
 */
struct wsm_vk_instance {
	VkInstance instance; /**< Vulkan instance handle. */
	VkDebugUtilsMessengerEXT messenger; /**< Optional Vulkan debug messenger handle. */

	struct {
		PFN_vkCreateDebugUtilsMessengerEXT createDebugUtilsMessengerEXT; /**< Creates a debug messenger. */
		PFN_vkDestroyDebugUtilsMessengerEXT destroyDebugUtilsMessengerEXT; /**< Destroys a debug messenger. */
	} api; /**< Loaded Vulkan extension entry points. */
};

/**
 * @brief Creates and initializes a Vulkan instance.
 * @param debug Whether to request validation layers and a debug messenger.
 * @return Vulkan instance state, or NULL on failure.
 */
struct wsm_vk_instance *wsm_vk_instance_create(bool debug);

/**
 * @brief Destroys a Vulkan instance and its debug messenger.
 * @param ini Instance to destroy.
 */
void wsm_vk_instance_destroy(struct wsm_vk_instance *ini);

/**
 * @brief Gets the instance backing a Vulkan renderer.
 * @param renderer Renderer known to be Vulkan-backed.
 * @return Borrowed Vulkan instance handle.
 */
VkInstance wsm_vk_renderer_get_instance(struct wlr_renderer *renderer);

#endif // RENDERER_VULKAN_INSTANCE_H
