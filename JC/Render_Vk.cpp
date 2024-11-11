#pragma once

#include "JC/Render.h"
#include "JC/Render_Vk.h"
#include "JC/Render_VkStr.h"

#include "JC/Allocator.h"
#include "JC/Log.h"

namespace JC {

//--------------------------------------------------------------------------------------------------

VkBool32 VKAPI_PTR VkDebugUtilsCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT      severity,
    VkDebugUtilsMessageTypeFlagsEXT             types,
    const VkDebugUtilsMessengerCallbackDataEXT* data,
    void*                                       userData
);

struct RenderApiImpl : RenderApi {
	LogApi*                logApi                = nullptr;
	TempAllocator*         ta                    = nullptr;
	VkInstance             vkInstance            = VK_NULL_HANDLE;
	VkAllocationCallbacks* vkAllocationCallbacks = nullptr;

	Err* MakeVkErr(VkResult r) {
		return JC_ERR(ta, ErrCode { .ns = "vk", .code = (u64)r }, "desc", RenderVk::VkResultStr(r));
	}

	#define JC_LOG_VK_ERR(r) \
		JC_LOG_ERR(MakeVkErr(r))

	VkBool32 VkDebugUtilsCallback(VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT, const VkDebugUtilsMessengerCallbackDataEXT* data) {
		logApi->Log(
			__FILE__,
			__LINE__,
			(severity & (VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)) ? LogCategory::Error : LogCategory::Info,
			"[{}] {}: {}",
			data->messageIdNumber,
			data->pMessageIdName,
			data->pMessage ? data->pMessage : ""
		);
		return VK_FALSE;
	}

	void LogLayers() {
		u32 vkLayerPropertiesLen = 0;
		if (VkResult r = vkEnumerateInstanceLayerProperties(&vkLayerPropertiesLen, nullptr); r != VK_SUCCESS) {
			JC_LOG_VK_ERR(r);
			return;
		}

		VkLayerProperties* const vkLayerProperties = (VkLayerProperties*)ta->Alloc(vkLayerPropertiesLen * sizeof(vkLayerProperties[0]));
		if (VkResult r = vkEnumerateInstanceLayerProperties(&vkLayerPropertiesLen, vkLayerProperties); r != VK_SUCCESS) {
			JC_LOG_VK_ERR(r);
			return;
		}

		JC_LOG("{} layers:", vkLayerPropertiesLen);
		for (u32 i = 0; i < vkLayerPropertiesLen; i++) {
			const VkLayerProperties* const l = &vkLayerProperties[i];
			JC_LOG(
				"    {}: specVersion={}, implementationVersion={}, description={}",
				l->layerName,
				l->specVersion,
				l->implementationVersion,
				l->description
			);

			u32 vkExtensionPropertiesLen = 0;
			if (VkResult r = vkEnumerateInstanceExtensionProperties(l->layerName, &vkExtensionPropertiesLen, nullptr); r != VK_SUCCESS) {
				JC_LOG_VK_ERR(r);
				continue;
			}

			VkExtensionProperties* vkExtensionProperties = nullptr;
			vkExtensionProperties = (VkExtensionProperties*)ta->Alloc(vkExtensionPropertiesLen * sizeof(VkExtensionProperties));
			if (VkResult r = vkEnumerateInstanceExtensionProperties(l->layerName, &vkExtensionPropertiesLen, vkExtensionProperties); r != VK_SUCCESS) {
				JC_LOG_VK_ERR(r);
				continue;
			}

			JC_LOG("    {} extensions:", vkExtensionPropertiesLen);
			for (u32 j = 0; j < vkExtensionPropertiesLen; j++) {
				const VkExtensionProperties* const e = &vkExtensionProperties[j];
				JC_LOG("        {}: specVersion={}", e->extensionName, e->specVersion);
			}
		}

		u32 vkExtensionPropertiesLen = 0;
		if (VkResult r = vkEnumerateInstanceExtensionProperties(nullptr, &vkExtensionPropertiesLen, nullptr); r != VK_SUCCESS) {
			JC_LOG_VK_ERR(r);
			return;
		}

		VkExtensionProperties* vkExtensionProperties = nullptr;
		vkExtensionProperties = (VkExtensionProperties*)ta->Alloc(vkExtensionPropertiesLen * sizeof(VkExtensionProperties));
		if (VkResult r = vkEnumerateInstanceExtensionProperties(nullptr, &vkExtensionPropertiesLen, vkExtensionProperties); r != VK_SUCCESS) {
			JC_LOG_VK_ERR(r);
			return;
		}

		JC_LOG("{} Global extensions:", vkExtensionPropertiesLen);
		for (u32 j = 0; j < vkExtensionPropertiesLen; j++) {
			const VkExtensionProperties* const e = &vkExtensionProperties[j];
			JC_LOG("    {}: specVersion={}", e->extensionName, e->specVersion);
		}
	}

	Res<> Init(LogApi* inLogApi, TempAllocator* inTa) override {
		logApi = inLogApi;
		ta     = inTa;

		if (Res<> r = RenderVk::LoadRootFns(ta); !r) {
			return r;
		}

		LogLayers();

		VkDebugUtilsMessengerCreateInfoEXT vkDebugUtilsMessengerCreateInfoEXT = {
			.sType           = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
			.pNext           = nullptr,
			.flags           = 0,
			.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
			.messageType     = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_DEVICE_ADDRESS_BINDING_BIT_EXT,
			.pfnUserCallback = JC::VkDebugUtilsCallback,
			.pUserData       = this,
		};

		VkApplicationInfo vkApplicationInfo = {
			.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO,
			.pNext              = nullptr,
			.pApplicationName   = "JC",
			.applicationVersion = 1,
			.pEngineName        = "JC",
			.engineVersion      = 1,
			.apiVersion         = VK_API_VERSION_1_3,
		};

		constexpr const char* DeviceLayers[] = {
			"VK_LAYER_KHRONOS_validation",
		};
		constexpr u32 DeviceLayersLen = JC_LEN(DeviceLayers);

		constexpr const char* DeviceExtensions[] = {
			VK_KHR_SURFACE_EXTENSION_NAME,
			#if defined JC_OS_WINDOWS
				VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
			#else	// JC_OS
				#error("unsupported OS")
			#endif // JC_OS
			VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
		};
		constexpr u32 DeviceExtensionsLen = JC_LEN(DeviceExtensions);

		VkInstanceCreateInfo vkInstanceCreateInfo = {
			.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
			.pNext                   = &vkDebugUtilsMessengerCreateInfoEXT,
			.flags                   = 0,
			.pApplicationInfo        = &vkApplicationInfo,
			.enabledLayerCount       = DeviceLayersLen,
			.ppEnabledLayerNames     = DeviceLayers,
			.enabledExtensionCount   = DeviceExtensionsLen,
			.ppEnabledExtensionNames = DeviceExtensions,
		};

		/*
		VkAllocationCallbacks vkAllocationCallbacks = {
			.pUserData             = this,
			.pfnAllocation         = ,
			.pfnReallocation       = ,
			.pfnFree               = ,
			.pfnInternalAllocation = ,
			.pfnInternalFree       = ,
		};
		*/

		if (VkResult r = vkCreateInstance(&vkInstanceCreateInfo, vkAllocationCallbacks, &vkInstance); r != VK_SUCCESS) {
			return MakeVkErr(r);
		}

		RenderVk::LoadInstanceFns(vkInstance);

		return Ok();
	}

	void Shutdown() override {
		vkDestroyInstance(vkInstance, vkAllocationCallbacks);
	}
};

RenderApiImpl renderApiImpl;

RenderApi* RenderApi::Get() {
	return &renderApiImpl;
}

VkBool32 VKAPI_PTR VkDebugUtilsCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT      severity,
    VkDebugUtilsMessageTypeFlagsEXT             types,
    const VkDebugUtilsMessengerCallbackDataEXT* data,
    void*                                       userData
) {
	return ((RenderApiImpl*)userData)->VkDebugUtilsCallback(severity, types, data);
}
//--------------------------------------------------------------------------------------------------

}	// namespace JC