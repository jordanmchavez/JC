#pragma once

#include "JC/Render.h"
#include "JC/Render_Vk.h"

#include "JC/Allocator.h"
#include "JC/Array.h"
#include "JC/Log.h"

namespace JC {

//--------------------------------------------------------------------------------------------------

VkBool32 VKAPI_PTR VkDebugUtilsCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT      severity,
    VkDebugUtilsMessageTypeFlagsEXT             types,
    const VkDebugUtilsMessengerCallbackDataEXT* data,
    void*                                       userData
);

struct Seg {
	u64 offset = 0;
	u64 len    = 0;
};

template <class T>
struct Buf {
	T*  data;
	u64 len;
};

struct Extension {
	u64 name;
	u32 specVer;
};

struct Layer {
	u64            name;
	u32            specVer;
	u32            implVer;
	u64            desc;
	Buf<Extension> exts;
};

struct Layer {
	VkLayerProperties vkLayerProperties;
	Seg               extensions;
};

struct PhysicalDevice {
	VkPhysicalDevice           vkPhysicalDevice = VK_NULL_HANDLE;
	Seg                        extensions;
	VkPhysicalDeviceProperties vkPhysicalDeviceProperties;
};

struct RenderApiImpl : RenderApi {
	LogApi*                      logApi = nullptr;
	Allocator*                   allocator = nullptr;
	TempAllocator*               tempAllocator = nullptr;

	Array<VkExtensionProperties> vkExtensionProperties;
	Seg                          globalInstanceExtensions;
	Array<Layer>                 layers;
	bool                         haveVkExtDebugUtils = false;
	VkAllocationCallbacks*       vkAllocationCallbacks = nullptr;
	VkInstance                   vkInstance = VK_NULL_HANDLE;
	VkDebugUtilsMessengerEXT     vkDebugUtilsMessengerEXT = VK_NULL_HANDLE;
	Array<PhysicalDevice>        physicalDevices;

	//----------------------------------------------------------------------------------------------

	#define JC_MAKE_VK_ERR(r) \
		Err::Make(tempAllocator, nullptr, __FILE__, __LINE__, ErrCode { .ns = "vk", .code = (u64)r }, "desc", RenderVk::ResultStr(r))

	#define JC_LOG_VK_ERR(r) \
		JC_LOG_ERR(JC_MAKE_VK_ERR(r))

	//----------------------------------------------------------------------------------------------

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

	//----------------------------------------------------------------------------------------------

	Seg EnumerateInstanceExtensions(const char* layerName) {
		VkResult r = VK_SUCCESS;
		const u64 offset = vkExtensionProperties.len;
		u32 n = 0;
		do {
			if (r = vkEnumerateInstanceExtensionProperties(layerName, &n, nullptr); r == VK_SUCCESS) {
				r = vkEnumerateInstanceExtensionProperties(layerName, &n, vkExtensionProperties.Resize(offset + n));
			}
		} while (r == VK_INCOMPLETE);
		if (r) {
			JC_LOG_VK_ERR(r);
			vkExtensionProperties.Resize(offset);
			return Seg {};
		}
		vkExtensionProperties.Resize(n);
		return Seg { .offset = offset, .len = n };
	}
	//----------------------------------------------------------------------------------------------

	void EnumerateLayers() {
		VkResult r = VK_SUCCESS;
		u32 n = 0;
		VkLayerProperties* vkLayerProperties = nullptr;
		do {
			const u32 oldN = n;
			if (r = vkEnumerateInstanceLayerProperties(&n, nullptr); r == VK_SUCCESS) {
				vkLayerProperties = (VkLayerProperties*)tempAllocator->Realloc(vkLayerProperties, oldN * sizeof(VkLayerProperties), n * sizeof(VkLayerProperties));
				r = vkEnumerateInstanceLayerProperties(&n, vkLayerProperties);
			}
		} while (r == VK_INCOMPLETE);
		if (r) {
			JC_LOG_VK_ERR(r);
			return;
		}
		layers.Resize(n);
		JC_LOG("{} layers", layers.len);
		for (u32 i = 0; i < layers.len; i++) {
			const VkLayerProperties* const l = &vkLayerProperties[i];
			layers[i].vkLayerProperties = *l;
			layers[i].extensions = EnumerateInstanceExtensions(l->layerName);
			JC_LOG("    {}: specVersion={}, implementationVersion={}, description={}", l->layerName, l->specVersion, l->implementationVersion, l->description);
			for (u64 j = 0; j < layers[i].extensions.len; j++) {
				const VkExtensionProperties* e = &vkExtensionProperties[layers[i].extensions.offset + j];
				JC_LOG("        {}: specVersion={}", e->extensionName, e->specVersion);
			}
		}
	}

	//----------------------------------------------------------------------------------------------

	Res<> AddInstanceLayer(s8 name, Array<const char*>* out) {
		for (u64 i = 0; i < layers.len; i++) {
			if (name == layers[i].vkLayerProperties.layerName) {
				out->Add(layers[i].vkLayerProperties.layerName);
				return Ok();
			}
		}
		return JC_ERR(tempAllocator, Err_LayerNotFound, "name", name);
	}

	//----------------------------------------------------------------------------------------------

	Res<> AddInstanceExtension(s8 name, Span<const char*> layerNames, Array<const char*>* out) {
		for (u64 i = 0; i < globalInstanceExtensions.len; i++) {
			const VkExtensionProperties* const e = &vkExtensionProperties[globalInstanceExtensions.offset + i];
			if (name == e->extensionName) {
				JC_LOG("Found requested instance extension {} in global instance extensions", name);
				out->Add(e->extensionName);
				return Ok();
			}
		}
		for (u64 i = 0; i < layerNames.len; i++) {
			for (u64 j = 0; j < layers.len; j++) {
				if (layerNames[i] == layers[j].vkLayerProperties.layerName) {
					for (u64 k = 0; k < layers[j].extensions.len; k++) {
						const VkExtensionProperties* const e = &vkExtensionProperties[layers[j].extensions.offset + k];
						if (name == e->extensionName) {
							JC_LOG("Found requested instance extension {} in layer {} extensions", name, layerNames[i]);
							out->Add(e->extensionName);
							return Ok();
						}
					}
				}
			}
		}
		return JC_ERR(tempAllocator, Err_ExtensionNotFound, "name", name);
	}

	//----------------------------------------------------------------------------------------------

	Res<> Init(Allocator* inAllocator, LogApi* inLogApi, TempAllocator* inTempAllocator) override {
		allocator     = inAllocator;
		logApi        = inLogApi;
		tempAllocator = inTempAllocator;

		vkExtensionProperties.Init(allocator);
		layers.Init(allocator);
		physicalDevices.Init(allocator);

		if (Res<> r = RenderVk::LoadRootFns(tempAllocator); !r) {
			return r;
		}

		EnumerateLayers();

		globalInstanceExtensions = EnumerateInstanceExtensions(nullptr);
		JC_LOG("{} global instance extensions", globalInstanceExtensions.len);
		for (u64 i = 0; i < globalInstanceExtensions.len; i++) {
			const VkExtensionProperties* const e = &vkExtensionProperties[globalInstanceExtensions.offset + i];
			JC_LOG("    {}: specVersion={}", e->extensionName, e->specVersion);
		}

		VkApplicationInfo vkApplicationInfo = {
			.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO,
			.pNext              = nullptr,
			.pApplicationName   = "JC",
			.applicationVersion = 1,
			.pEngineName        = "JC",
			.engineVersion      = 1,
			.apiVersion         = VK_API_VERSION_1_3,
		};

		Array<const char*> layerNames;
		layerNames.Init(tempAllocator);
		if (Res<> r = AddInstanceLayer("VK_LAYER_KHRONOS_validation", &layerNames); !r) {
			JC_LOG("Couldn't find optional instance extension 'VK_LAYER_KHRONOS_validation'");
		}

		Array<const char*> instanceExtensionNames;
		instanceExtensionNames.Init(tempAllocator);
		if (Res<> r = AddInstanceExtension(VK_KHR_SURFACE_EXTENSION_NAME, layerNames, &instanceExtensionNames); !r) {
			return r;
		}
		#if defined JC_OS_WINDOWS
			constexpr const char* platformSurfaceExtension = VK_KHR_WIN32_SURFACE_EXTENSION_NAME;
		#else	// JC_OS
			#error("unsupported OS")
		#endif // JC_OS
		if (Res<> r = AddInstanceExtension(platformSurfaceExtension, layerNames, &instanceExtensionNames); !r) {
			return r;
		}

		haveVkExtDebugUtils = (bool)AddInstanceExtension(VK_EXT_DEBUG_UTILS_EXTENSION_NAME, layerNames, &instanceExtensionNames);

		VkInstanceCreateInfo vkInstanceCreateInfo = {
			.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
			.pNext                   = nullptr,
			.flags                   = 0,
			.pApplicationInfo        = &vkApplicationInfo,
			.enabledLayerCount       = (u32)layerNames.len,
			.ppEnabledLayerNames     = layerNames.data,
			.enabledExtensionCount   = (u32)instanceExtensionNames.len,
			.ppEnabledExtensionNames = instanceExtensionNames.data,
		};

		VkDebugUtilsMessengerCreateInfoEXT vkDebugUtilsMessengerCreateInfoEXT;
		if (haveVkExtDebugUtils) {
			vkDebugUtilsMessengerCreateInfoEXT = {
				.sType           = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
				.pNext           = nullptr,
				.flags           = 0,
				.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
				.messageType     = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_DEVICE_ADDRESS_BINDING_BIT_EXT,
				.pfnUserCallback = JC::VkDebugUtilsCallback,
				.pUserData       = this,
			};
			vkInstanceCreateInfo.pNext = &vkDebugUtilsMessengerCreateInfoEXT;
		}

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

		if (VkResult r = vkCreateInstance(&vkInstanceCreateInfo, vkAllocationCallbacks, &vkInstance)) {
			return JC_MAKE_VK_ERR(r);
		}

		RenderVk::LoadInstanceFns(vkInstance);

		if (haveVkExtDebugUtils) {
			if (VkResult r = vkCreateDebugUtilsMessengerEXT(vkInstance, &vkDebugUtilsMessengerCreateInfoEXT, vkAllocationCallbacks, &vkDebugUtilsMessengerEXT)) {
				JC_LOG_VK_ERR(r);
			}
		}

		VkResult r = VK_SUCCESS;
		u32 n = 0;
		VkPhysicalDevice* vkPhysicalDevices = nullptr;
		do {
			const u32 oldN = n;
			if (r = vkEnumeratePhysicalDevices(vkInstance, &n, nullptr); r == VK_SUCCESS) {
				vkPhysicalDevices = (VkPhysicalDevice*)tempAllocator->Realloc(vkPhysicalDevices, oldN * sizeof(VkPhysicalDevice), n * sizeof(VkPhysicalDevice));
				r = vkEnumeratePhysicalDevices(vkInstance, &n, vkPhysicalDevices);
			}
		} while (r == VK_INCOMPLETE);
		if (r) {
			return JC_MAKE_VK_ERR(r);
		}
		physicalDevices.Resize(n);

		JC_LOG("{} physical devices", physicalDevices.len);
		for (u32 i = 0; i < physicalDevices.len; i++) {
			physicalDevices[i].vkPhysicalDevice = vkPhysicalDevices[i];
			vkGetPhysicalDeviceProperties(vkPhysicalDevices[i], &physicalDevices[i].vkPhysicalDeviceProperties);
			JC_LOG(
				"    [{}] {}: apiVersion={}, vendorId={}, deviceId={}, deviceType={}",
				i,
				physicalDevices[i].vkPhysicalDeviceProperties.deviceName,
				physicalDevices[i].vkPhysicalDeviceProperties.apiVersion,
				physicalDevices[i].vkPhysicalDeviceProperties.vendorID,
				physicalDevices[i].vkPhysicalDeviceProperties.deviceID,
				RenderVk::PhysicalDeviceTypeStr(physicalDevices[i].vkPhysicalDeviceProperties.deviceType)
			);

			//VkPhysicalDeviceFeatures2 vkPhysicalDeviceFeatures2;
			//vkGetPhysicalDeviceFeatures2(vkPhysicalDevices[i], &vkPhysicalDeviceFeatures2);

			const u64 offset = vkExtensionProperties.len;
			n = 0;
			do {
				if (r = vkEnumerateDeviceExtensionProperties(physicalDevices[i].vkPhysicalDevice, nullptr, &n, nullptr); r == VK_SUCCESS) {
					r = vkEnumerateDeviceExtensionProperties(physicalDevices[i].vkPhysicalDevice, vkExtensionProperties.
				}
			} while (r == VK_INCOMPLETE);
			//void vkGetPhysicalDeviceFormatProperties2(vkPhysicalDevices[i], VkFormat format, VkFormatProperties2* pFormatProperties);
			//void vkGetPhysicalDeviceMemoryProperties2(vkPhysicalDevices[i], VkPhysicalDeviceMemoryProperties2* pMemoryProperties);
			//void vkGetPhysicalDeviceQueueFamilyProperties2KHR(vkPhysicalDevices[i], uint32_t* pQueueFamilyPropertyCount, VkQueueFamilyProperties2* pQueueFamilyProperties);
			//VkResult vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vkPhysicalDevices[i], VkSurfaceKHR surface, VkSurfaceCapabilitiesKHR* pSurfaceCapabilities);
			//VkResult vkGetPhysicalDeviceSurfaceFormatsKHR(vkPhysicalDevices[i], VkSurfaceKHR surface, uint32_t* pSurfaceFormatCount, VkSurfaceFormatKHR* pSurfaceFormats);
			//VkResult vkGetPhysicalDeviceSurfacePresentModesKHR(vkPhysicalDevices[i], VkSurfaceKHR surface, uint32_t* pPresentModeCount, VkPresentModeKHR* pPresentModes);
			//VkResult vkGetPhysicalDeviceSurfaceSupportKHR(vkPhysicalDevices[i], uint32_t queueFamilyIndex, VkSurfaceKHR surface, VkBool32* pSupported);
			//VkBool32 vkGetPhysicalDeviceWin32PresentationSupportKHR(vkPhysicalDevices[i], uint32_t queueFamilyIndex);


			if (props->deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
			}
		}
		*/
		return Ok();
	}

	void Shutdown() override {
		if (vkDebugUtilsMessengerEXT != VK_NULL_HANDLE) {
			vkDestroyDebugUtilsMessengerEXT(vkInstance, vkDebugUtilsMessengerEXT, vkAllocationCallbacks);
		}
		if (vkInstance != VK_NULL_HANDLE) {
			vkDestroyInstance(vkInstance, vkAllocationCallbacks);
		}
		RenderVk::FreeFns();
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