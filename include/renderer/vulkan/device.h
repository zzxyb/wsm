/* SPDX-License-Identifier: MIT */

/**
 * @file        device.h
 * @brief       Vulkan device state and DRM device selection.
 * @details     Defines device capabilities, extension entry points, supported formats, and memory
 *              selection helpers.
 *
 * @par Upstream:
 *              Derived from wlroots; see LICENSE.wlroots in the project root.
 *
 * @author      YaoBing Xiao (wsm adaptations)
 * @date        2026-11-19
 * @version     v1.0
 * @par Copyright(c):
 *              Upstream copyright notices and MIT license: LICENSE.wlroots
 *              in the project root. See LICENSE for wsm copyright and license.
 * @par History:
 *      version: v1.0, YaoBing Xiao, 2026-11-19, initial version\n
 */

#ifndef RENDERER_VULKAN_DEVICE_H
#define RENDERER_VULKAN_DEVICE_H

#include <stdbool.h>
#include <stdint.h>
#include <vulkan/vulkan.h>
#include <wlr/render/drm_format_set.h>
#include "renderer/vulkan/instance.h"

struct wsm_vk_format_props;

/**
 * @brief Logical Vulkan device, supported formats, and extension entry points.
 */
struct wsm_vk_device {
	struct wsm_vk_instance *instance; /**< Vulkan instance containing this device. */

	VkPhysicalDevice phdev; /**< Selected Vulkan physical device. */
	VkDevice dev; /**< Vulkan logical device handle. */

	int drm_fd; /**< DRM node file descriptor associated with the device. */

	bool sync_file_import_export; /**< Whether Vulkan semaphore sync-file import and export are supported. */
	bool implicit_sync_interop; /**< Whether DMA-BUF implicit fences can interoperate with Vulkan. */
	bool sampler_ycbcr_conversion; /**< Whether YCbCr sampler conversion is supported. */

	uint32_t queue_family; /**< Queue family used for both rendering and transfers. */
	VkQueue queue; /**< Rendering and transfer command queue. */

	struct {
		PFN_vkGetMemoryFdPropertiesKHR vkGetMemoryFdPropertiesKHR; /**< Queries memory properties for an imported file descriptor. */
		PFN_vkWaitSemaphoresKHR vkWaitSemaphoresKHR; /**< Waits for timeline semaphore points. */
		PFN_vkGetSemaphoreCounterValueKHR vkGetSemaphoreCounterValueKHR; /**< Queries a timeline semaphore counter. */
		PFN_vkGetSemaphoreFdKHR vkGetSemaphoreFdKHR; /**< Exports a semaphore payload as a file descriptor. */
		PFN_vkImportSemaphoreFdKHR vkImportSemaphoreFdKHR; /**< Imports a file descriptor into a semaphore. */
		PFN_vkQueueSubmit2KHR vkQueueSubmit2KHR; /**< Submits commands and synchronization dependencies. */
		PFN_vkBindImageMemory2KHR vkBindImageMemory2KHR; /**< Binds memory to imported images. */
		PFN_vkCreateSamplerYcbcrConversionKHR vkCreateSamplerYcbcrConversionKHR; /**< Creates a YCbCr sampler conversion. */
		PFN_vkDestroySamplerYcbcrConversionKHR vkDestroySamplerYcbcrConversionKHR; /**< Destroys a YCbCr sampler conversion. */
		PFN_vkGetImageMemoryRequirements2KHR vkGetImageMemoryRequirements2KHR; /**< Queries image memory requirements, including plane requirements. */
	} api; /**< Loaded Vulkan extension entry points. */

	uint32_t format_prop_count; /**< Number of cached format property entries. */
	struct wsm_vk_format_props *format_props; /**< Cached device format properties. */
	struct wlr_drm_format_set dmabuf_render_formats; /**< DMA-BUF formats and modifiers supported for render targets. */
	struct wlr_drm_format_set dmabuf_texture_formats; /**< DMA-BUF formats and modifiers supported for textures. */
	struct wlr_drm_format_set shm_texture_formats; /**< Supported formats for shared-memory texture uploads. */

	float timestamp_period; /**< Nanoseconds per GPU timestamp tick. */
	uint32_t timestamp_valid_bits; /**< Number of valid timestamp bits in the selected queue family. */
};

/**
 * @brief Creates a logical Vulkan device and queries its capabilities.
 * @param ini Vulkan instance containing the physical device.
 * @param phdev Physical device to initialize.
 * @return Device state, or NULL on failure.
 */
struct wsm_vk_device *wsm_vk_device_create(struct wsm_vk_instance *ini,
	VkPhysicalDevice phdev);

/**
 * @brief Destroys a logical device and releases its format resources.
 * @param dev Device to destroy.
 */
void wsm_vk_device_destroy(struct wsm_vk_device *dev);

/**
 * @brief Finds the Vulkan physical device matching a DRM device.
 * @param ini Instance to search.
 * @param drm_fd DRM device file descriptor.
 * @return Matching physical device, or VK_NULL_HANDLE on failure.
 */
VkPhysicalDevice wsm_vk_find_drm_phdev(struct wsm_vk_instance *ini, int drm_fd);

/**
 * @brief Opens a DRM node for a Vulkan physical device.
 * @param phdev Physical device exposing DRM device properties.
 * @return Owned DRM file descriptor, or -1 on failure.
 */
int wsm_vk_open_phdev_drm_fd(VkPhysicalDevice phdev);

/**
 * @brief Finds a compatible Vulkan memory type.
 * @param device Device whose memory properties are searched.
 * @param flags Required memory property flags.
 * @param req_bits Bitmask of permitted memory types.
 * @return Memory type index, or -1 when no compatible type exists.
 */
int wsm_vk_find_mem_type(struct wsm_vk_device *device,
	VkMemoryPropertyFlags flags, uint32_t req_bits);

/**
 * @brief Gets the physical device backing a Vulkan renderer.
 * @param renderer Renderer known to be Vulkan-backed.
 * @return Borrowed Vulkan physical device handle.
 */
VkPhysicalDevice wsm_vk_renderer_get_physical_device(struct wlr_renderer *renderer);

/**
 * @brief Gets the logical device backing a Vulkan renderer.
 * @param renderer Renderer known to be Vulkan-backed.
 * @return Borrowed Vulkan logical device handle.
 */
VkDevice wsm_vk_renderer_get_device(struct wlr_renderer *renderer);

#endif // RENDERER_VULKAN_DEVICE_H
