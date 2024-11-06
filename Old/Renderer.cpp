/*
typedef enum VkMemoryPropertyFlagBits {
	VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT = 0x00000001,
	VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT = 0x00000002,
	VK_MEMORY_PROPERTY_HOST_COHERENT_BIT = 0x00000004,
	VK_MEMORY_PROPERTY_HOST_CACHED_BIT = 0x00000008,
	VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT = 0x00000010,
} VkMemoryPropertyFlagBits;


VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT = render targets, static textures, static vtx/idx buffers
VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT = streaming vtx/idx buffers, uniform buffers
VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT = pci memory: uniform buffers, dynamic vtx/idx buffers, staging buffers
VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT

staging = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
static  = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
*/


#include "J/Renderer.h"

#include "J/Array.h"
#include "J/ArrayPrint.h"
#include "J/Log.h"
#include "J/Allocator.h"
#include "J/Vulkan.h"

namespace J {

//-------------------------------------------------------------------------------------------------

struct Renderer_Vulkan : Renderer {
	virtual void Init() override;

	VkDevice CreateDevice();

	static VkBool32 VKAPI_PTR DebugUtilsMessengerCallback(VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT type, VkDebugUtilsMessengerCallbackDataEXT const* data, void* userData);


};

//-------------------------------------------------------------------------------------------------

void Renderer_Vulkan::Init() {
	Vulkan_LoadLoadFunctions();
}

//-------------------------------------------------------------------------------------------------

VkDevice Renderer_Vulkan::CreateDevice() {
	VkApplicationInfo applicationInfo = {
		.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
		.pNext = nullptr,
		.pApplicationName = "",
		.applicationVersion = 0,
		.pEngineName = "",
		.engineVersion = 0,
		.apiVersion = VK_API_VERSION_1_3,
	};

	char const* enabledExtensions[] = {
		VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
		VK_KHR_SURFACE_EXTENSION_NAME,
		#if defined J_OS_Windows
			VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
		#else
			#error("Unsupported OS")
		#endif
	};

	VkInstanceCreateInfo instanceCreateInfo = {
		.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.pApplicationInfo = &applicationInfo,
		.enabledLayerCount = 0,
		.ppEnabledLayerNames = nullptr,
		.enabledExtensionCount = J_ArrayCount(enabledExtensions),
		.ppEnabledExtensionNames = enabledExtensions,
	};

	#if defined J_DEBUG_RENDER
		char const* enabledLayerNames[] = { "VK_LAYER_KHRONOS_validation" };
		instanceCreateInfo.enabledLayerCount = J_ArrayCount(enabledLayerNames);
		instanceCreateInfo.ppEnabledLayerNames = enabledLayerNames;

		VkValidationFeatureEnableEXT validationFeatureEnables[] = {
			VK_VALIDATION_FEATURE_ENABLE_BEST_PRACTICES_EXT,
			VK_VALIDATION_FEATURE_ENABLE_DEBUG_PRINTF_EXT,
			VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT,
		};
		VkValidationFeaturesEXT validationFeatures = {
			.sType = VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT,
			.pNext = nullptr,
			.enabledValidationFeatureCount = J_ArrayCount(validationFeatureEnables),
			.pEnabledValidationFeatures = validationFeatureEnables,
			.disabledValidationFeatureCount = 0,
			.pDisabledValidationFeatures = nullptr, 
		};
		instanceCreateInfo.pNext = &validationFeatures;

		VkDebugUtilsMessengerCreateInfoEXT debugUtilsMessagengerCreateInfo = {
			.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
			.pNext = nullptr,
			.flags = 0,
			.messageSeverity = (
				VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
				VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
				VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
				VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT
			),
			.messageType = (
				VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
				VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
				VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT
			),
			.pfnUserCallback = DebugUtilsMessengerCallback,
			.pUserData = this,
		};
		instanceCreateInfo.pNext = &debugUtilsMessagengerCreateInfo;
	#endif // J_DEBUG_RENDER
}

//-------------------------------------------------------------------------------------------------

VkBool32 VKAPI_PTR Renderer_Vulkan::DebugUtilsMessengerCallback(
	VkDebugUtilsMessageSeverityFlagBitsEXT severity,
	VkDebugUtilsMessageTypeFlagsEXT type,
	VkDebugUtilsMessengerCallbackDataEXT const* data,
	void* userData
) {
	Renderer_Vulkan* renderer = (Renderer_Vulkan*)userData;

	ArrayT<char> msgBuf = { .allocator = Mem::tempAllocator };

	Array::Printf(msgBuf, "[");
	if (type & VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT) {
		Array::Printf(msgBuf, "G");
	}
	if (type & VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT) {
		Array::Printf(msgBuf, "V");
	}
    if (type & VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT) {
		Array::Printf(msgBuf, "P");
	}
	Array::Printf(msgBuf, "] %s", data->pMessage);

	for (u32 i = 0; i < data->queueLabelCount; i++) {
		Array::Printf(msgBuf, 
			"\n\t[Que] %s { %.2f, %.2f, %.2f, %.2f }\n",
			data->pQueueLabels[i].pLabelName,
			data->pQueueLabels[i].color[0],
			data->pQueueLabels[i].color[1],
			data->pQueueLabels[i].color[2],
			data->pQueueLabels[i].color[3]
		);
	}

	for (u32 i = 0; i < data->cmdBufLabelCount; i++) {
		Array::Printf(msgBuf, 
			"\n\t[Cmd] %s { %.2f, %.2f, %.2f, %.2f }",
			data->pCmdBufLabels[i].pLabelName,
			data->pCmdBufLabels[i].color[0],
			data->pCmdBufLabels[i].color[1],
			data->pCmdBufLabels[i].color[2],
			data->pCmdBufLabels[i].color[3]
		);
	}

	for (u32 i = 0; i < data->objectCount; i++) {
		Array::Printf(msgBuf, 
			"\n\t[Obj] %s %s",
			VulkanObjectTypeString(data->pObjects[i].objectType),
			data->pObjects[i].pObjectName
		);
	}

	if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT) {
		VkDebugf(msgBuf.data);
	} else if (severity & (VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)) {
		VkLogf(msgBuf.data);
	} else if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
		VkErrorf(msgBuf.data);
	}

	return VK_FALSE;
}

//-------------------------------------------------------------------------------------------------

}	// namespace J