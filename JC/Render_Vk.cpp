#pragma once

#include "JC/Render.h"

#include "JC/Array.h"
#include "JC/Err.h"
#include "JC/Log.h"
#include "JC/Mem.h"
#include "JC/Vk.h"

#define Render_Debug

namespace JC {

//--------------------------------------------------------------------------------------------------

#define MakeVkErr(Fn, vkRes) \
	_MakeErr(0, SrcLoc::Here(), ErrCode { .ns = "vk", .code = (u64)vkRes }, "fn", #Fn, "desc", Vk::ResultStr(vkRes))

#define LogVkErr(Fn, vkRes) \
	LogErr(MakeVkErr(Fn, vkRes))

//--------------------------------------------------------------------------------------------------

struct RenderApiObj : RenderApi {
	struct QueueFamily {
		VkQueueFamilyProperties vkQueueFamilyProperties = {};
		bool                    supportsPresent         = false;
	};

	struct PhysicalDevice {
		VkPhysicalDevice                 vkPhysicalDevice                 = VK_NULL_HANDLE;
		VkPhysicalDeviceProperties       vkPhysicalDeviceProperties       = {};
		VkPhysicalDeviceFeatures         vkPhysicalDeviceFeatures         = {};
		VkPhysicalDeviceMemoryProperties vkPhysicalDeviceMemoryProperties = {};
		VkSurfaceCapabilitiesKHR         vkSurfaceCapabilities            = {};
		Array<VkExtensionProperties>     vkExtensionProperties            = {};
		Array<VkSurfaceFormatKHR>        vkSurfaceFormats                 = {};
		Array<VkPresentModeKHR>          vkPresentModes                   = {};
		Array<QueueFamily>               queueFamilies                    = {};
	};

	LogApi*                  logApi                = 0;
	Mem*                     mem                   = 0;
	TempMem*                 tempMem               = 0;
	VkAllocationCallbacks*   vkAllocationCallbacks = 0;
	VkInstance               vkInstance            = VK_NULL_HANDLE;
	VkDebugUtilsMessengerEXT vkDebugUtilsMessenger = VK_NULL_HANDLE;
	VkSurfaceKHR             vkSurface             = VK_NULL_HANDLE;
	Array<PhysicalDevice>    physicalDevices       = {};
	VkDevice                 vkDevice              = VK_NULL_HANDLE;

	//-------------------------------------------------------------------------------------------------

	static VkBool32 VKAPI_CALL DebugCallback(
		VkDebugUtilsMessageSeverityFlagBitsEXT      severity,
		VkDebugUtilsMessageTypeFlagsEXT             flags,
		const VkDebugUtilsMessengerCallbackDataEXT* data,
		void*                                       userData
	) {
		flags;
		LogApi* logApi = ((RenderApiObj*)userData)->logApi;
		logApi->Print(
			SrcLoc::Here(),
			(severity & (VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)) ? LogCategory::Error : LogCategory::Info,
			"[{}] {}: {}",
			data->messageIdNumber,
			data->pMessageIdName,
			data->pMessage ? data->pMessage : ""
		);
		return VK_FALSE;
	}

	//-------------------------------------------------------------------------------------------------

	#define CheckVk(expr) { \
		if (vkRes = expr; vkRes != VK_SUCCESS) { \
			return MakeVkErr(#expr, vkRes); \
		} \
	}

	Res<> Init(LogApi* logApi_, Mem* mem_, TempMem* tempMem_, const OsWindowData* osWindowData) override {
		logApi  = logApi_;
		mem     = mem_;
		tempMem = tempMem_;

		VkResult vkRes = VK_SUCCESS;
		u32 n = 0;

		if (Res<> r = Vk::LoadRootFns(); !r) { return r; }

		Array<VkLayerProperties> layers(tempMem);
		CheckVk(vkEnumerateInstanceLayerProperties(&n, 0));
		CheckVk(vkEnumerateInstanceLayerProperties(&n, layers.Resize(n)));

		Log("{} layers:", layers.len);
		for (u64 i = 0; i < layers.len; i++) {
			Log(
				"  {}: implementationVersion={}, specVerion={}, description={}",
				layers[i].layerName,
				Vk::VersionStr(layers[i].implementationVersion),
				Vk::VersionStr(layers[i].specVersion),
				layers[i].description
			);
		}

		constexpr const char* requiredLayers[] = {
			#if defined Render_Debug
				"VK_LAYER_KHRONOS_validation",
			#endif	// Render_Debug
		};
		constexpr u32 requiredLayersLen = LenOf(requiredLayers);
		for (u32 i = 0; i < requiredLayersLen; i++) {
			bool found = false;
			for (u64 j = 0; j < layers.len; j++) {
				if (!StrCmp(requiredLayers[i], layers[j].layerName)) {
					Log("Found required layer '{}'", requiredLayers[i]);
					found = true;
					break;
				}
			}
			if (!found) {
				return MakeErr(Err_LayerNotFound, "name", requiredLayers[i]);
			}
		}
	
		Array<VkExtensionProperties> instExts(tempMem);
		CheckVk(vkEnumerateInstanceExtensionProperties(0, &n, 0));
		CheckVk(vkEnumerateInstanceExtensionProperties(0, &n, instExts.Resize(n)));

		Log("{} instance extensions:", instExts.len);
		for (u64 i = 0; i < instExts.len; i++) {
			Log("  {}: specVersion={}", instExts[i].extensionName, Vk::VersionStr(instExts[i].specVersion));
		}

		constexpr const char* requiredInstExts[] = {
			"VK_KHR_surface",
			#if defined Os_Windows
				"VK_KHR_win32_surface",
			#else	//  Os_
				#error("Unsupported OS")
			#endif	//	Os_
			#if defined Render_Debug
				"VK_EXT_debug_utils"
			#endif	// Render_Debug
		};
		constexpr u32 requiredInstExtsLen = LenOf(requiredInstExts);
		for (u32 i = 0; i < requiredInstExtsLen; i++) {
			bool found = false;
			for (u64 j = 0; j < instExts.len; j++) {
				if (!StrCmp(requiredInstExts[i], instExts[j].extensionName)) {
					Log("Found required instance extension '{}'", requiredInstExts[i]);
					found = true;
					break;
				}
			}
			if (!found) {
				return MakeErr(Err_LayerNotFound, "name", requiredInstExts[i]);
			}
		}
		
		VkApplicationInfo vkApplicationInfo = {
			.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO,
			.pNext              = 0,
			.pApplicationName   = "JC",
			.applicationVersion = 1,
			.pEngineName        = "JC",
			.engineVersion      = 1,
			.apiVersion         = VK_API_VERSION_1_3,
		};
		VkInstanceCreateInfo vkInstanceCreateInfo = {
			.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
			.pNext                   = 0,
			.flags                   = 0,
			.pApplicationInfo        = &vkApplicationInfo,
			.enabledLayerCount       = requiredLayersLen,
			.ppEnabledLayerNames     = requiredLayers,
			.enabledExtensionCount   = requiredInstExtsLen,
			.ppEnabledExtensionNames = requiredInstExts,
		};

		#if defined Render_Debug
			VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo = {
				.sType           = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
				.pNext           = nullptr,
				.flags           = 0,
				.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
				.messageType     = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_DEVICE_ADDRESS_BINDING_BIT_EXT,
				.pfnUserCallback = RenderApiObj::DebugCallback,
				.pUserData       = this,
			};
			vkInstanceCreateInfo.pNext = &debugCreateInfo;
		#endif	// Render_Debug

		Log("Creating instance");
		CheckVk(vkCreateInstance(&vkInstanceCreateInfo, vkAllocationCallbacks, &vkInstance));
		Vk::LoadInstanceFns(vkInstance);

		#if defined Render_Debug
			Log("Creating debug messenger");
			CheckVk(vkCreateDebugUtilsMessengerEXT(vkInstance, &debugCreateInfo, vkAllocationCallbacks, &vkDebugUtilsMessenger));
		#endif	// Render_Debug

		#if defined Os_Windows
			Log("Creating win32 surface");
			VkWin32SurfaceCreateInfoKHR win32SurfaceCreateInfo = {
				.sType     = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
				.pNext     = 0,
				.flags     = 0,
				.hinstance = (HINSTANCE)osWindowData->hinstance,
				.hwnd      = (HWND)osWindowData->hwnd,
			};
			CheckVk(vkCreateWin32SurfaceKHR(vkInstance, &win32SurfaceCreateInfo, vkAllocationCallbacks, &vkSurface));
		#else	// Os_
			#error("Unsupported OS");
		#endif	// OS_

		Array<VkPhysicalDevice> vkPhysicalDevices(tempMem);
		CheckVk(vkEnumeratePhysicalDevices(vkInstance, &n, nullptr));
		CheckVk(vkEnumeratePhysicalDevices(vkInstance, &n, vkPhysicalDevices.Resize(n)));

		physicalDevices.Init(mem);
		physicalDevices.Resize(n);
		for (u64 i = 0; i < physicalDevices.len; i++) {
			PhysicalDevice* const pd = &physicalDevices[i];

			pd->vkPhysicalDevice = vkPhysicalDevices[i];

			vkGetPhysicalDeviceProperties      (pd->vkPhysicalDevice, &pd->vkPhysicalDeviceProperties);
			vkGetPhysicalDeviceFeatures        (pd->vkPhysicalDevice, &pd->vkPhysicalDeviceFeatures);
			vkGetPhysicalDeviceMemoryProperties(pd->vkPhysicalDevice, &pd->vkPhysicalDeviceMemoryProperties);

			CheckVk(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(pd->vkPhysicalDevice, vkSurface, &pd->vkSurfaceCapabilities));

			Array<VkQueueFamilyProperties> vkQueueFamilyProperties(tempMem);
			vkGetPhysicalDeviceQueueFamilyProperties(pd->vkPhysicalDevice, &n, nullptr);
			vkGetPhysicalDeviceQueueFamilyProperties(pd->vkPhysicalDevice, &n, vkQueueFamilyProperties.Resize(n));

			pd->vkExtensionProperties.Init(mem);
			CheckVk(vkEnumerateDeviceExtensionProperties(pd->vkPhysicalDevice, 0, &n, 0));
			CheckVk(vkEnumerateDeviceExtensionProperties(pd->vkPhysicalDevice, 0, &n, pd->vkExtensionProperties.Resize(n)));

			pd->vkSurfaceFormats.Init(mem);
			CheckVk(vkGetPhysicalDeviceSurfaceFormatsKHR(pd->vkPhysicalDevice, vkSurface, &n, 0));
			CheckVk(vkGetPhysicalDeviceSurfaceFormatsKHR(pd->vkPhysicalDevice, vkSurface, &n, pd->vkSurfaceFormats.Resize(n)));

			pd->vkPresentModes.Init(mem);
			CheckVk(vkGetPhysicalDeviceSurfacePresentModesKHR(pd->vkPhysicalDevice, vkSurface, &n, 0));
			CheckVk(vkGetPhysicalDeviceSurfacePresentModesKHR(pd->vkPhysicalDevice, vkSurface, &n, pd->vkPresentModes.Resize(n)));

			pd->queueFamilies.Init(mem);
			pd->queueFamilies.Resize(vkQueueFamilyProperties.len);
			for (u64 j = 0; j < pd->queueFamilies.len; j++) {
				VkBool32 supportsPresent = VK_FALSE;
				CheckVk(vkGetPhysicalDeviceSurfaceSupportKHR(pd->vkPhysicalDevice, (u32)j, vkSurface, &supportsPresent));
				pd->queueFamilies[j].vkQueueFamilyProperties = vkQueueFamilyProperties[j];
				pd->queueFamilies[j].supportsPresent         = (supportsPresent == VK_TRUE);
			}

			//vkGetPhysicalDeviceFormatProperties2(pd->vkPhysicalDevice, 
			//vkGetPhysicalDeviceWin32PresentationSupportKHR;
		}

		Log("{} physical devices:", physicalDevices.len);
		for (u64 i = 0; i < physicalDevices.len; i++) {
			const PhysicalDevice* pd = &physicalDevices[i];
			Log(
				"  {}: apiVersion={}, driverVersion={}, vendorID={}, deviceId={}, deviceType={}",
				pd->vkPhysicalDeviceProperties.deviceName,
				Vk::VersionStr(pd->vkPhysicalDeviceProperties.apiVersion),
				Vk::VersionStr(pd->vkPhysicalDeviceProperties.driverVersion),
				pd->vkPhysicalDeviceProperties.vendorID,
				pd->vkPhysicalDeviceProperties.deviceID,
				Vk::PhysicalDeviceTypeStr(pd->vkPhysicalDeviceProperties.deviceType)
			);
			pd->
		VkPhysicalDeviceFeatures         vkPhysicalDeviceFeatures         = {};
		VkPhysicalDeviceMemoryProperties vkPhysicalDeviceMemoryProperties = {};
		VkSurfaceCapabilitiesKHR         vkSurfaceCapabilities            = {};
		Array<VkExtensionProperties>     vkExtensionProperties            = {};
		Array<VkSurfaceFormatKHR>        vkSurfaceFormats                 = {};
		Array<VkPresentModeKHR>          vkPresentModes                   = {};
		Array<QueueFamily>               queueFamilies                    = {};

		//VkPhysicalDeviceFeatures         vkPhysicalDeviceFeatures         = {};
		//VkPhysicalDeviceMemoryProperties vkPhysicalDeviceMemoryProperties = {};
		//VkSurfaceCapabilitiesKHR         vkSurfaceCapabilities            = {};
		//Array<VkExtensionProperties>     vkExtensionProperties            = {};
		//Array<VkSurfaceFormatKHR>        vkSurfaceFormats                 = {};
		//Array<VkPresentModeKHR>          vkPresentModes                   = {};
		//Array<QueueFamily>               queueFamilies                    = {};

		}







		/*

		float queuePriority = 1.0f;
		VkDeviceQueueCreateInfo queueCreateInfo = {
			.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
			.pNext            = 0,
			.flags            = 0,
			.queueFamilyIndex = selectedQueueFamily,
			.queueCount       = 1,
			.pQueuePriorities = &queuePriority,
		};

		constexpr const char* requiredDeviceExts[] = {
			"VK_KHR_swapchain",
		};
		constexpr u64 requiredDeviceExtsLen = LenOf(requiredDeviceExts);
		for (u32 i = 0; i < requiredDeviceExtsLen; i++) {
			bool found = false;
			for (u64 j = 0; j < deviceExts.len; j++) {
				if (!StrCmp(requiredDeviceExts[i], deviceExts[j].extensionName)) {
					found = true;
					break;
				}
			}
			if (!found) {
				return MakeErr(Err_LayerNotFound, "name", requiredDeviceExts[i]);
			}
		}

		VkPhysicalDeviceFeatures physicalDeviceFeatures = {};
		physicalDeviceFeatures.shaderClipDistance = VK_TRUE;

		VkDeviceCreateInfo deviceCreateInfo = {
			.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
			.pNext                   = 0,
			.flags                   = 0,
			.queueCreateInfoCount    = 1,
			.pQueueCreateInfos       = &queueCreateInfo,
			.enabledLayerCount       = (u32)requiredLayersLen,
			.ppEnabledLayerNames     = requiredLayers,
			.enabledExtensionCount   = (u32)requiredDeviceExtsLen,
			.ppEnabledExtensionNames = requiredDeviceExts,
			.pEnabledFeatures        = &physicalDeviceFeatures,
		};
		CheckVk(vkCreateDevice(selectedPhysicalDevice, &deviceCreateInfo, vkAllocationCallbacks, &device));
		Vk::LoadDeviceFns(device);

		Array<VkSurfaceFormatKHR> surfaceFormats(tempMem);
		VkFormat surfaceFormat = (surfaceFormats.len == 1 && surfaceFormats[0].format == VK_FORMAT_UNDEFINED)
			? VK_FORMAT_B8G8R8_UNORM
			: surfaceFormats[0].format;
		VkColorSpaceKHR surfaceColorSpace = surfaceFormats[0].colorSpace;


		u32 desiredImageCount = 2;
		if (surfaceCaps.minImageCount > desiredImageCount) {
			desiredImageCount = surfaceCaps.minImageCount;
		}
		if (surfaceCaps.maxImageCount && surfaceCaps.maxImageCount < desiredImageCount) {
			desiredImageCount = surfaceCaps.maxImageCount;
		}
		VkExtent2D surfaceExtent = surfaceCaps.currentExtent;
		if (surfaceExtent.width == -1) {
			surfaceExtent.width  = osWindowData->width;
			surfaceExtent.height = osWindowData->height;
		}
		VkSurfaceTransformFlagBitsKHR transformFlags = surfaceCaps.currentTransform;
		if (surfaceCaps.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR) {
			transformFlags = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
		}

		Array<VkPresentModeKHR> presentModes(tempMem);
		VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;
		for (u64 i = 0; i < presentModes.len; i++) {
			if (presentModes[i] == VK_PRESENT_MODE_MAILBOX_KHR) {
				presentMode = VK_PRESENT_MODE_MAILBOX_KHR;
				break;
			}
		}
		*/
		return Ok();
	}

	//----------------------------------------------------------------------------------------------

	void Shutdown() override {
		if (vkDevice != VK_NULL_HANDLE) {
			vkDestroyDevice(vkDevice , vkAllocationCallbacks);
			vkDevice  = VK_NULL_HANDLE;
		}
		if (vkSurface != VK_NULL_HANDLE) {
			vkDestroySurfaceKHR(vkInstance, vkSurface, vkAllocationCallbacks);
			vkSurface = VK_NULL_HANDLE;
		}
		if (vkDebugUtilsMessenger != VK_NULL_HANDLE) {
			vkDestroyDebugUtilsMessengerEXT(vkInstance, vkDebugUtilsMessenger, vkAllocationCallbacks);
			vkDebugUtilsMessenger = VK_NULL_HANDLE;
		}
		if (vkInstance != VK_NULL_HANDLE) {
			vkDestroyInstance(vkInstance, vkAllocationCallbacks);
			vkInstance = VK_NULL_HANDLE;
		}
	}
};

//--------------------------------------------------------------------------------------------------

static RenderApiObj renderApi;

RenderApi* RenderApi::Get() {
	return &renderApi;
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC