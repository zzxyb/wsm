#include "renderer/vulkan/instance.h"
#include "renderer/vulkan/renderer.h"
#include "util/wsm_log.h"

#include <assert.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include <vulkan/vulkan.h>

#include <wlr/version.h>

static VKAPI_ATTR VkBool32 debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
		VkDebugUtilsMessageTypeFlagsEXT type,
		const VkDebugUtilsMessengerCallbackDataEXT *debug_data,
		void *data) {
	// we ignore some of the non-helpful warnings
	static const char *const ignored[] = {
		// notifies us that shader output is not consumed since
		// we use the shared vertex buffer with uv output
		"UNASSIGNED-CoreValidation-Shader-OutputNotConsumed",
	};

	if (debug_data->pMessageIdName) {
		for (unsigned i = 0; i < sizeof(ignored) / sizeof(ignored[0]); ++i) {
			if (strcmp(debug_data->pMessageIdName, ignored[i]) == 0) {
				return false;
			}
		}
	}

	enum wsm_log_importance importance;
	switch (severity) {
	case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
	case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
		importance = WSM_ERROR;
		break;
	default:
	case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
		importance = WSM_INFO;
		break;
	}

	wsm_log(importance, "%s (%s)", debug_data->pMessage,
		debug_data->pMessageIdName);
	if (debug_data->queueLabelCount > 0) {
		const char *name = debug_data->pQueueLabels[0].pLabelName;
		if (name) {
			wsm_log(importance, "    last label '%s'", name);
		}
	}

	for (unsigned i = 0; i < debug_data->objectCount; ++i) {
		if (debug_data->pObjects[i].pObjectName) {
			wsm_log(importance, "    involving '%s'", debug_data->pMessage);
		}
	}

	return false;
}

struct wsm_vk_instance *wsm_vk_instance_create(bool debug) {

	uint32_t avail_extc = 0;
	VkResult res;
	res = vkEnumerateInstanceExtensionProperties(NULL, &avail_extc, NULL);
	if ((res != VK_SUCCESS) || (avail_extc == 0)) {
		wsm_vk_error("Could not enumerate instance extensions (1)", res);
		return NULL;
	}

	VkExtensionProperties *avail_ext_props = calloc(avail_extc, sizeof(*avail_ext_props));
	if (!avail_ext_props) {
		wsm_log_errno(WSM_ERROR, "Failed to allocate Vulkan extension properties");
		return NULL;
	}
	res = vkEnumerateInstanceExtensionProperties(NULL, &avail_extc,
		avail_ext_props);
	if (res != VK_SUCCESS) {
		wsm_vk_error("Could not enumerate instance extensions (2)", res);
		free(avail_ext_props);
		return NULL;
	}

	for (size_t j = 0; j < avail_extc; ++j) {
		wsm_log(WSM_DEBUG, "Vulkan instance extension %s v%"PRIu32,
			avail_ext_props[j].extensionName, avail_ext_props[j].specVersion);
	}

	struct wsm_vk_instance *ini = calloc(1, sizeof(*ini));
	if (!ini) {
		wsm_log_errno(WSM_ERROR, "allocation failed");
		free(avail_ext_props);
		return NULL;
	}

	size_t extensions_len = 0;
	const char *extensions[8] = {0};
	extensions[extensions_len++] = VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME;
	extensions[extensions_len++] = VK_KHR_EXTERNAL_SEMAPHORE_CAPABILITIES_EXTENSION_NAME;
	extensions[extensions_len++] = VK_KHR_EXTERNAL_MEMORY_CAPABILITIES_EXTENSION_NAME;

	for (size_t i = 0; i < extensions_len; i++) {
		if (!wsm_vk_check_extension(avail_ext_props, avail_extc, extensions[i])) {
			wsm_log(WSM_ERROR, "vulkan: required instance extension %s not found",
				extensions[i]);
			goto error;
		}
	}

	bool debug_utils_found = false;
	if (debug && wsm_vk_check_extension(avail_ext_props, avail_extc,
			VK_EXT_DEBUG_UTILS_EXTENSION_NAME)) {
		debug_utils_found = true;
		extensions[extensions_len++] = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
	}

	assert(extensions_len <= sizeof(extensions) / sizeof(extensions[0]));

	VkApplicationInfo application_info = {
		.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
		.pEngineName = "wsm",
		.engineVersion = WLR_VERSION_NUM,
		.apiVersion = VK_API_VERSION_1_0,
	};

	VkInstanceCreateInfo instance_info = {
		.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
		.pApplicationInfo = &application_info,
		.enabledExtensionCount = extensions_len,
		.ppEnabledExtensionNames = extensions,
		.enabledLayerCount = 0,
		.ppEnabledLayerNames = NULL,
	};

	VkDebugUtilsMessageSeverityFlagsEXT severity =
		// VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	VkDebugUtilsMessageTypeFlagsEXT types =
		// VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;

	VkDebugUtilsMessengerCreateInfoEXT debug_info = {
		.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
		.messageSeverity = severity,
		.messageType = types,
		.pfnUserCallback = &debug_callback,
		.pUserData = ini,
	};

	if (debug_utils_found) {
		// already adding the debug utils messenger extension to
		// instance creation gives us additional information during
		// instance creation and destruction, can be useful for debugging
		// layers/extensions not being found.
		instance_info.pNext = &debug_info;
	}

	res = vkCreateInstance(&instance_info, NULL, &ini->instance);
	if (res != VK_SUCCESS) {
		wsm_vk_error("Could not create instance", res);
		goto error;
	}

	if (debug_utils_found) {
		ini->api.createDebugUtilsMessengerEXT =
			(PFN_vkCreateDebugUtilsMessengerEXT) vkGetInstanceProcAddr(
				ini->instance, "vkCreateDebugUtilsMessengerEXT");
		ini->api.destroyDebugUtilsMessengerEXT =
			(PFN_vkDestroyDebugUtilsMessengerEXT) vkGetInstanceProcAddr(
				ini->instance, "vkDestroyDebugUtilsMessengerEXT");

		if (ini->api.createDebugUtilsMessengerEXT) {
			ini->api.createDebugUtilsMessengerEXT(ini->instance,
				&debug_info, NULL, &ini->messenger);
		} else {
			wsm_log(WSM_ERROR, "vkCreateDebugUtilsMessengerEXT not found");
		}
	}

	free(avail_ext_props);
	return ini;

error:
	free(avail_ext_props);
	wsm_vk_instance_destroy(ini);
	return NULL;
}

void wsm_vk_instance_destroy(struct wsm_vk_instance *ini) {
	if (!ini) {
		return;
	}

	if (ini->messenger && ini->api.destroyDebugUtilsMessengerEXT) {
		ini->api.destroyDebugUtilsMessengerEXT(ini->instance,
			ini->messenger, NULL);
	}

	if (ini->instance) {
		vkDestroyInstance(ini->instance, NULL);
	}

	free(ini);
}

VkInstance wsm_vk_renderer_get_instance(struct wlr_renderer *renderer) {
	struct wsm_vk_renderer *vk_renderer = wsm_vk_renderer_from_renderer(renderer);
	return vk_renderer->dev->instance->instance;
}
