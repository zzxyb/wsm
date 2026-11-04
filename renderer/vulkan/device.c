#include "renderer/vulkan/device.h"
#include "renderer/vulkan/dmabuf.h"
#include "renderer/vulkan/renderer.h"
#include "util/wsm_log.h"
#include "texture/vulkan/pixel_format.h"

#if defined(__FreeBSD__)
#undef _POSIX_C_SOURCE
#endif
#include <assert.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <xf86drm.h>
#include <xf86drmMode.h>

#include <vulkan/vulkan.h>

#if defined(__linux__)
#include <sys/sysmacros.h>
#endif

static void log_phdev(const VkPhysicalDeviceProperties *props) {
	uint32_t vv_major = VK_VERSION_MAJOR(props->apiVersion);
	uint32_t vv_minor = VK_VERSION_MINOR(props->apiVersion);
	uint32_t vv_patch = VK_VERSION_PATCH(props->apiVersion);

	uint32_t dv_major = VK_VERSION_MAJOR(props->driverVersion);
	uint32_t dv_minor = VK_VERSION_MINOR(props->driverVersion);
	uint32_t dv_patch = VK_VERSION_PATCH(props->driverVersion);

	const char *dev_type = "unknown";
	switch (props->deviceType) {
	case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
		dev_type = "integrated";
		break;
	case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
		dev_type = "discrete";
		break;
	case VK_PHYSICAL_DEVICE_TYPE_CPU:
		dev_type = "cpu";
		break;
	case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
		dev_type = "vgpu";
		break;
	default:
		break;
	}

	wsm_log(WSM_INFO, "Vulkan device: '%s'", props->deviceName);
	wsm_log(WSM_INFO, "  Device type: '%s'", dev_type);
	wsm_log(WSM_INFO, "  Supported API version: %u.%u.%u", vv_major, vv_minor, vv_patch);
	wsm_log(WSM_INFO, "  Driver version: %u.%u.%u", dv_major, dv_minor, dv_patch);
}

struct wlr_vk_phdev_info {
	VkPhysicalDeviceType type;
	char name[VK_MAX_PHYSICAL_DEVICE_NAME_SIZE];

	bool has_drm, has_render;
	dev_t primary_devid, render_devid;
};

VkPhysicalDevice wsm_vk_find_drm_phdev(struct wsm_vk_instance *ini, int drm_fd) {
	VkPhysicalDevice *phdevs = NULL;
	struct wlr_vk_phdev_info *devices = NULL;
	VkResult res;
	uint32_t num_phdevs;

	res = vkEnumeratePhysicalDevices(ini->instance, &num_phdevs, NULL);
	if (res != VK_SUCCESS) {
		wsm_vk_error("Could not retrieve physical devices", res);
		free(devices);
		free(phdevs);
		return VK_NULL_HANDLE;
	}

	if (num_phdevs == 0) {
		free(devices);
		free(phdevs);
		return VK_NULL_HANDLE;
	}
	phdevs = calloc(num_phdevs, sizeof(*phdevs));
	devices = calloc(num_phdevs, sizeof(*devices));
	if (!phdevs || !devices) {
		wsm_log_errno(WSM_ERROR, "Failed to allocate Vulkan physical devices");
		free(devices);
		free(phdevs);
		return VK_NULL_HANDLE;
	}
	res = vkEnumeratePhysicalDevices(ini->instance, &num_phdevs, phdevs);
	if (res != VK_SUCCESS) {
		wsm_vk_error("Could not retrieve physical devices", res);
		free(devices);
		free(phdevs);
		return VK_NULL_HANDLE;
	}

	for (uint32_t i = 0; i < num_phdevs; ++i) {
		VkPhysicalDevice phdev = phdevs[i];

		VkPhysicalDeviceProperties phdev_props;
		vkGetPhysicalDeviceProperties(phdev, &phdev_props);
		log_phdev(&phdev_props);

		// check for extensions
		uint32_t avail_extc = 0;
		res = vkEnumerateDeviceExtensionProperties(phdev, NULL,
			&avail_extc, NULL);
		if ((res != VK_SUCCESS) || (avail_extc == 0)) {
			wsm_vk_error("  Could not enumerate device extensions", res);
			continue;
		}

		VkExtensionProperties *avail_ext_props = calloc(avail_extc, sizeof(*avail_ext_props));
		if (!avail_ext_props) {
			wsm_log_errno(WSM_ERROR, "Failed to allocate Vulkan device extensions");
			continue;
		}
		res = vkEnumerateDeviceExtensionProperties(phdev, NULL,
			&avail_extc, avail_ext_props);
		if (res != VK_SUCCESS) {
			wsm_vk_error("  Could not enumerate device extensions", res);
			free(avail_ext_props);
			continue;
		}

		bool has_drm_props = wsm_vk_check_extension(avail_ext_props, avail_extc,
			VK_EXT_PHYSICAL_DEVICE_DRM_EXTENSION_NAME);
		bool has_driver_props = wsm_vk_check_extension(avail_ext_props, avail_extc,
			VK_KHR_DRIVER_PROPERTIES_EXTENSION_NAME);
		free(avail_ext_props);

		VkPhysicalDeviceProperties2 props = {
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
		};

		VkPhysicalDeviceDrmPropertiesEXT drm_props = {
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRM_PROPERTIES_EXT,
		};
		if (has_drm_props) {
			drm_props.pNext = props.pNext;
			props.pNext = &drm_props;
		}

		VkPhysicalDeviceDriverPropertiesKHR driver_props = {
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES,
		};
		if (has_driver_props) {
			driver_props.pNext = props.pNext;
			props.pNext = &driver_props;
		}

		vkGetPhysicalDeviceProperties2(phdev, &props);

		if (has_driver_props) {
			wsm_log(WSM_INFO, "  Driver name: %s (%s)", driver_props.driverName, driver_props.driverInfo);
		}

		struct wlr_vk_phdev_info info = {
			.type = phdev_props.deviceType,
			.has_drm = has_drm_props,
		};
		memcpy(info.name, phdev_props.deviceName, sizeof(phdev_props.deviceName));
		if (has_drm_props) {
			info.has_render = drm_props.hasRender;
			info.primary_devid = makedev(drm_props.primaryMajor, drm_props.primaryMinor);
			info.render_devid = makedev(drm_props.renderMajor, drm_props.renderMinor);
		}
		devices[i] = info;
	}

	// Find a Vulkan device matching the DRM device's dev_t
	struct stat drm_stat = {0};
	if (drm_fd >= 0 && fstat(drm_fd, &drm_stat) != 0) {
		wsm_log_errno(WSM_ERROR, "fstat failed");
		free(devices);
		free(phdevs);
		return VK_NULL_HANDLE;
	}

	for (size_t i = 0; i < num_phdevs; ++i) {
		VkPhysicalDevice phdev = phdevs[i];
		const struct wlr_vk_phdev_info *info = &devices[i];

		bool found;
		if (drm_fd >= 0) {
			if (!info->has_drm) {
				wsm_log(WSM_DEBUG, "  Ignoring physical device \"%s\": "
					"VK_EXT_physical_device_drm not supported", info->name);
				continue;
			}

			found = info->primary_devid == drm_stat.st_rdev || info->render_devid == drm_stat.st_rdev;
		} else {
			found = info->type == VK_PHYSICAL_DEVICE_TYPE_CPU;
		}

		if (found) {
			wsm_log(WSM_INFO, "Found matching Vulkan physical device: %s", info->name);
			free(devices);
			free(phdevs);
			return phdev;
		}
	}

	if (drm_fd < 0) {
		free(devices);
		free(phdevs);
		return VK_NULL_HANDLE;
	}

	// If the DRM device we're interested in uses the platform bus, is
	// KMS-capable, and has no render node associated, search for a sibling
	// render-capable platform bus device. The platform bus is a good enough
	// hint that these two devices should be compatible.
	drmDevice *drm_dev = NULL;
	if (drmGetDevice2(drm_fd, 0, &drm_dev) != 0) {
		wsm_log(WSM_ERROR, "drmGetDevice2() failed");
		free(devices);
		free(phdevs);
		return VK_NULL_HANDLE;
	}
	bool is_platform_lone_kms = drm_dev->bustype == DRM_BUS_PLATFORM &&
		!(drm_dev->available_nodes & (1 << DRM_NODE_RENDER)) && drmIsKMS(drm_fd);
	drmFreeDevice(&drm_dev);
	if (!is_platform_lone_kms) {
		free(devices);
		free(phdevs);
		return VK_NULL_HANDLE;
	}

	for (size_t i = 0; i < num_phdevs; ++i) {
		VkPhysicalDevice phdev = phdevs[i];
		const struct wlr_vk_phdev_info *info = &devices[i];

		if (!info->has_render) {
			continue;
		}

		drmDevice *drm_dev = NULL;
		if (drmGetDeviceFromDevId(info->render_devid, 0, &drm_dev) != 0) {
			wsm_log(WSM_ERROR, "drmGetDeviceFromDevId() failed");
			free(devices);
			free(phdevs);
			return VK_NULL_HANDLE;
		}

		if (drm_dev->bustype == DRM_BUS_PLATFORM) {
			wsm_log(WSM_INFO, "Found Vulkan physical device on platform bus: %s", info->name);
			drmFreeDevice(&drm_dev);
			free(devices);
			free(phdevs);
			return phdev;
		}

		drmFreeDevice(&drm_dev);
	}

	free(devices);
	free(phdevs);
	return VK_NULL_HANDLE;
}

int wsm_vk_open_phdev_drm_fd(VkPhysicalDevice phdev) {
	// vulkan_find_drm_phdev() already checks that VK_EXT_physical_device_drm
	// is supported
	VkPhysicalDeviceDrmPropertiesEXT drm_props = {
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRM_PROPERTIES_EXT,
	};
	VkPhysicalDeviceProperties2 props = {
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
		.pNext = &drm_props,
	};
	vkGetPhysicalDeviceProperties2(phdev, &props);

	dev_t devid;
	if (drm_props.hasRender) {
		devid = makedev(drm_props.renderMajor, drm_props.renderMinor);
	} else if (drm_props.hasPrimary) {
		devid = makedev(drm_props.primaryMajor, drm_props.primaryMinor);
	} else {
		wsm_log(WSM_INFO, "Physical device is missing both render and primary nodes");
		return -1;
	}

	drmDevice *device = NULL;
	if (drmGetDeviceFromDevId(devid, 0, &device) != 0) {
		wsm_log_errno(WSM_ERROR, "drmGetDeviceFromDevId failed");
		return -1;
	}

	const char *name = NULL;
	if (device->available_nodes & (1 << DRM_NODE_RENDER)) {
		name = device->nodes[DRM_NODE_RENDER];
	} else {
		assert(device->available_nodes & (1 << DRM_NODE_PRIMARY));
		name = device->nodes[DRM_NODE_PRIMARY];
		wsm_log(WSM_DEBUG, "DRM device %s has no render node, "
			"falling back to primary node", name);
	}

	int drm_fd = open(name, O_RDWR | O_NONBLOCK | O_CLOEXEC);
	if (drm_fd < 0) {
		wsm_log_errno(WSM_ERROR, "Failed to open DRM node %s", name);
	}
	drmFreeDevice(&device);
	return drm_fd;
}

static void load_device_proc(struct wsm_vk_device *dev, const char *name,
		void *proc_ptr) {
	void *proc = (void *)vkGetDeviceProcAddr(dev->dev, name);
	if (proc == NULL) {
		abort();
	}
	*(void **)proc_ptr = proc;
}

struct wsm_vk_device *wsm_vk_device_create(struct wsm_vk_instance *ini,
		VkPhysicalDevice phdev) {
	VkResult res;

	uint32_t avail_extc = 0;
	res = vkEnumerateDeviceExtensionProperties(phdev, NULL,
		&avail_extc, NULL);
	if (res != VK_SUCCESS || avail_extc == 0) {
		wsm_vk_error("Could not enumerate device extensions (1)", res);
		return NULL;
	}

	VkExtensionProperties *avail_ext_props = calloc(avail_extc, sizeof(*avail_ext_props));
	if (!avail_ext_props) {
		wsm_log_errno(WSM_ERROR, "Failed to allocate Vulkan device extensions");
		return NULL;
	}
	res = vkEnumerateDeviceExtensionProperties(phdev, NULL,
		&avail_extc, avail_ext_props);
	if (res != VK_SUCCESS) {
		wsm_vk_error("Could not enumerate device extensions (2)", res);
		free(avail_ext_props);
		return NULL;
	}

	for (size_t j = 0; j < avail_extc; ++j) {
		wsm_log(WSM_DEBUG, "Vulkan device extension %s v%"PRIu32,
			avail_ext_props[j].extensionName, avail_ext_props[j].specVersion);
	}

	struct wsm_vk_device *dev = calloc(1, sizeof(*dev));
	if (!dev) {
		wsm_log_errno(WSM_ERROR, "allocation failed");
		free(avail_ext_props);
		return NULL;
	}

	dev->phdev = phdev;
	dev->instance = ini;
	dev->drm_fd = -1;

	// For dmabuf import we require at least the external_memory_fd,
	// external_memory_dma_buf, queue_family_foreign,
	// image_drm_format_modifier, and image_format_list extensions.
	// The size is set to a large number to allow for other conditional
	// extensions before the device is created
	const char *extensions[32] = {0};
	size_t extensions_len = 0;
	extensions[extensions_len++] = VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME;
	extensions[extensions_len++] = VK_KHR_IMAGE_FORMAT_LIST_EXTENSION_NAME; // or vulkan 1.2
	extensions[extensions_len++] = VK_EXT_EXTERNAL_MEMORY_DMA_BUF_EXTENSION_NAME;
	extensions[extensions_len++] = VK_EXT_QUEUE_FAMILY_FOREIGN_EXTENSION_NAME;
	extensions[extensions_len++] = VK_EXT_IMAGE_DRM_FORMAT_MODIFIER_EXTENSION_NAME;
	extensions[extensions_len++] = VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME; // or vulkan 1.2
	extensions[extensions_len++] = VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME; // or vulkan 1.3
	extensions[extensions_len++] = VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME; // or vulkan 1.1
	extensions[extensions_len++] = VK_KHR_BIND_MEMORY_2_EXTENSION_NAME; // or vulkan 1.1
	extensions[extensions_len++] = VK_KHR_SAMPLER_YCBCR_CONVERSION_EXTENSION_NAME; // or vulkan 1.1
	extensions[extensions_len++] = VK_KHR_EXTERNAL_SEMAPHORE_EXTENSION_NAME; // or vulkan 1.1
	extensions[extensions_len++] = VK_KHR_MAINTENANCE_1_EXTENSION_NAME; // or vulkan 1.1
	extensions[extensions_len++] = VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME; // or vulkan 1.1

	for (size_t i = 0; i < extensions_len; i++) {
		if (!wsm_vk_check_extension(avail_ext_props, avail_extc, extensions[i])) {
			wsm_log(WSM_ERROR, "vulkan: required device extension %s not found",
				extensions[i]);
			goto error;
		}
	}

	{
		uint32_t qfam_count;
		vkGetPhysicalDeviceQueueFamilyProperties(phdev, &qfam_count, NULL);
		VkQueueFamilyProperties *queue_props = calloc(qfam_count, sizeof(*queue_props));
		if (qfam_count == 0 || !queue_props) {
			free(queue_props);
			wsm_log(WSM_ERROR, "Failed to allocate Vulkan queue families");
			goto error;
		}
		vkGetPhysicalDeviceQueueFamilyProperties(phdev, &qfam_count,
			queue_props);

		bool graphics_found = false;
		for (unsigned i = 0u; i < qfam_count; ++i) {
			graphics_found = queue_props[i].queueFlags & VK_QUEUE_GRAPHICS_BIT;
			if (graphics_found) {
				dev->queue_family = i;
				dev->timestamp_valid_bits = queue_props[i].timestampValidBits;
				break;
			}
		}
		free(queue_props);
		if (!graphics_found) {
			wsm_log(WSM_ERROR, "No Vulkan graphics queue family available");
			goto error;
		}

		VkPhysicalDeviceProperties phdev_props;
		vkGetPhysicalDeviceProperties(phdev, &phdev_props);
		dev->timestamp_period = phdev_props.limits.timestampPeriod;
	}

	bool exportable_semaphore = false, importable_semaphore = false;
	bool has_external_semaphore_fd =
		wsm_vk_check_extension(avail_ext_props, avail_extc, VK_KHR_EXTERNAL_SEMAPHORE_FD_EXTENSION_NAME);
	if (has_external_semaphore_fd) {
		const VkPhysicalDeviceExternalSemaphoreInfo ext_semaphore_info = {
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_SEMAPHORE_INFO,
			.handleType = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT,
		};
		VkExternalSemaphoreProperties ext_semaphore_props = {
			.sType = VK_STRUCTURE_TYPE_EXTERNAL_SEMAPHORE_PROPERTIES,
		};
		vkGetPhysicalDeviceExternalSemaphoreProperties(phdev,
			&ext_semaphore_info, &ext_semaphore_props);
		exportable_semaphore = ext_semaphore_props.externalSemaphoreFeatures &
			VK_EXTERNAL_SEMAPHORE_FEATURE_EXPORTABLE_BIT;
		importable_semaphore = ext_semaphore_props.externalSemaphoreFeatures &
			VK_EXTERNAL_SEMAPHORE_FEATURE_IMPORTABLE_BIT;
		extensions[extensions_len++] = VK_KHR_EXTERNAL_SEMAPHORE_FD_EXTENSION_NAME;
	}
	if (!exportable_semaphore) {
		wsm_log(WSM_DEBUG, "VkSemaphore is not exportable to a sync_file");
	}
	if (!importable_semaphore) {
		wsm_log(WSM_DEBUG, "VkSemaphore is not importable from a sync_file");
	}

	bool dmabuf_sync_file_import_export = dmabuf_check_sync_file_import_export();
	if (!dmabuf_sync_file_import_export) {
		wsm_log(WSM_DEBUG, "DMA-BUF sync_file import/export not supported");
	}

	dev->sync_file_import_export = exportable_semaphore && importable_semaphore;
	dev->implicit_sync_interop =
		exportable_semaphore && importable_semaphore && dmabuf_sync_file_import_export;
	if (dev->implicit_sync_interop) {
		wsm_log(WSM_DEBUG, "Implicit sync interop supported");
	} else {
		wsm_log(WSM_INFO, "Implicit sync interop not supported, "
			"falling back to blocking");
	}

	VkPhysicalDeviceSamplerYcbcrConversionFeatures phdev_sampler_ycbcr_features = {
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SAMPLER_YCBCR_CONVERSION_FEATURES,
	};
	VkPhysicalDeviceFeatures2 phdev_features = {
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
		.pNext = &phdev_sampler_ycbcr_features,
	};
	vkGetPhysicalDeviceFeatures2(phdev, &phdev_features);

	dev->sampler_ycbcr_conversion = phdev_sampler_ycbcr_features.samplerYcbcrConversion;
	wsm_log(WSM_DEBUG, "Sampler YCbCr conversion %s",
		dev->sampler_ycbcr_conversion ? "supported" : "not supported");

	const float prio = 1.f;
	VkDeviceQueueCreateInfo qinfo = {
		.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
		.queueFamilyIndex = dev->queue_family,
		.queueCount = 1,
		.pQueuePriorities = &prio,
	};

	VkDeviceQueueGlobalPriorityCreateInfoEXT global_priority;
	bool has_global_priority = wsm_vk_check_extension(avail_ext_props, avail_extc,
		VK_EXT_GLOBAL_PRIORITY_EXTENSION_NAME);
	if (has_global_priority) {
		// If global priorities are supported, request a high-priority context
		global_priority = (VkDeviceQueueGlobalPriorityCreateInfoEXT){
			.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_GLOBAL_PRIORITY_CREATE_INFO_EXT,
			.globalPriority = VK_QUEUE_GLOBAL_PRIORITY_HIGH_EXT,
		};
		qinfo.pNext = &global_priority;
		extensions[extensions_len++] = VK_EXT_GLOBAL_PRIORITY_EXTENSION_NAME;
		wsm_log(WSM_DEBUG, "Requesting a high-priority device queue");
	} else {
		wsm_log(WSM_DEBUG, "Global priorities are not supported, "
			"falling back to regular queue priority");
	}

	VkPhysicalDeviceSamplerYcbcrConversionFeatures sampler_ycbcr_features = {
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SAMPLER_YCBCR_CONVERSION_FEATURES,
		.samplerYcbcrConversion = dev->sampler_ycbcr_conversion,
	};
	VkPhysicalDeviceSynchronization2FeaturesKHR sync2_features = {
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES_KHR,
		.pNext = &sampler_ycbcr_features,
		.synchronization2 = VK_TRUE,
	};
	VkPhysicalDeviceTimelineSemaphoreFeaturesKHR timeline_features = {
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES_KHR,
		.pNext = &sync2_features,
		.timelineSemaphore = VK_TRUE,
	};
	VkDeviceCreateInfo dev_info = {
		.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
		.pNext = &timeline_features,
		.queueCreateInfoCount = 1u,
		.pQueueCreateInfos = &qinfo,
		.enabledExtensionCount = extensions_len,
		.ppEnabledExtensionNames = extensions,
	};

	assert(extensions_len <= sizeof(extensions) / sizeof(extensions[0]));

	res = vkCreateDevice(phdev, &dev_info, NULL, &dev->dev);

	if (has_global_priority && (res == VK_ERROR_NOT_PERMITTED_EXT ||
			res == VK_ERROR_INITIALIZATION_FAILED)) {
		// Try to recover from the driver denying a global priority queue
		wsm_log(WSM_DEBUG, "Failed to obtain a high-priority device queue, "
			"falling back to regular queue priority");
		qinfo.pNext = NULL;
		res = vkCreateDevice(phdev, &dev_info, NULL, &dev->dev);
	}

	if (res != VK_SUCCESS) {
		wsm_vk_error("Failed to create vulkan device", res);
		goto error;
	}

	vkGetDeviceQueue(dev->dev, dev->queue_family, 0, &dev->queue);

	load_device_proc(dev, "vkGetMemoryFdPropertiesKHR",
		&dev->api.vkGetMemoryFdPropertiesKHR);
	load_device_proc(dev, "vkWaitSemaphoresKHR", &dev->api.vkWaitSemaphoresKHR);
	load_device_proc(dev, "vkGetSemaphoreCounterValueKHR",
		&dev->api.vkGetSemaphoreCounterValueKHR);
	load_device_proc(dev, "vkQueueSubmit2KHR", &dev->api.vkQueueSubmit2KHR);
	load_device_proc(dev, "vkBindImageMemory2KHR", &dev->api.vkBindImageMemory2KHR);
	load_device_proc(dev, "vkCreateSamplerYcbcrConversionKHR", &dev->api.vkCreateSamplerYcbcrConversionKHR);
	load_device_proc(dev, "vkDestroySamplerYcbcrConversionKHR", &dev->api.vkDestroySamplerYcbcrConversionKHR);
	load_device_proc(dev, "vkGetImageMemoryRequirements2KHR", &dev->api.vkGetImageMemoryRequirements2KHR);

	if (has_external_semaphore_fd) {
		load_device_proc(dev, "vkGetSemaphoreFdKHR", &dev->api.vkGetSemaphoreFdKHR);
		load_device_proc(dev, "vkImportSemaphoreFdKHR", &dev->api.vkImportSemaphoreFdKHR);
	}

	size_t max_fmts;
	const struct wsm_vk_format *fmts = vulkan_get_format_list(&max_fmts);
	dev->format_props = calloc(max_fmts, sizeof(*dev->format_props));
	if (!dev->format_props) {
		wsm_log_errno(WSM_ERROR, "allocation failed");
		free(avail_ext_props);
		goto error;
	}

	wsm_log(WSM_DEBUG, "Supported Vulkan formats:");
	for (unsigned i = 0u; i < max_fmts; ++i) {
		vulkan_format_props_query(dev, &fmts[i]);
	}

	free(avail_ext_props);
	return dev;

error:
	free(avail_ext_props);
	wsm_vk_device_destroy(dev);
	return NULL;
}

void wsm_vk_device_destroy(struct wsm_vk_device *dev) {
	if (!dev) {
		return;
	}

	if (dev->dev) {
		vkDestroyDevice(dev->dev, NULL);
	}

	if (dev->drm_fd >= 0) {
		close(dev->drm_fd);
	}

	wlr_drm_format_set_finish(&dev->dmabuf_render_formats);
	wlr_drm_format_set_finish(&dev->dmabuf_texture_formats);
	wlr_drm_format_set_finish(&dev->shm_texture_formats);

	for (unsigned i = 0u; i < dev->format_prop_count; ++i) {
		vulkan_format_props_finish(&dev->format_props[i]);
	}

	free(dev->format_props);
	free(dev);
}

int wsm_vk_find_mem_type(struct wsm_vk_device *dev,
		VkMemoryPropertyFlags flags, uint32_t req_bits) {
	VkPhysicalDeviceMemoryProperties props;
	vkGetPhysicalDeviceMemoryProperties(dev->phdev, &props);

	for (unsigned i = 0u; i < props.memoryTypeCount; ++i) {
		if (req_bits & (1 << i)) {
			if ((props.memoryTypes[i].propertyFlags & flags) == flags) {
				return i;
			}
		}
	}

	return -1;
}

VkPhysicalDevice wsm_vk_renderer_get_physical_device(struct wlr_renderer *renderer) {
	struct wsm_vk_renderer *vk_renderer = wsm_vk_renderer_from_renderer(renderer);
	return vk_renderer->dev->phdev;
}

VkDevice wsm_vk_renderer_get_device(struct wlr_renderer *renderer) {
	struct wsm_vk_renderer *vk_renderer = wsm_vk_renderer_from_renderer(renderer);
	return vk_renderer->dev->dev;
}
