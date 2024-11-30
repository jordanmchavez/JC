#pragma once

#include "JC/Render.h"
#include "JC/Render_Vk.h"

#include "JC/Array.h"
#include "JC/Err.h"
#include "JC/Log.h"
#include "JC/Mem.h"

#define Render_Debug

namespace JC {

//--------------------------------------------------------------------------------------------------

#define MakeVkErr(Fn, vkRes) \
	_MakeErr(0, SrcLoc::Here(), ErrCode { .ns = "vk", .code = (u64)vkRes }, "fn", #Fn, "desc", Vk::ResultStr(vkRes))

#define LogVkErr(Fn, vkRes) \
	LogErr(MakeVkErr(Fn, vkRes))

//--------------------------------------------------------------------------------------------------

struct RenderApiObj : RenderApi {
	struct Extension {
		const char* name   = 0;
		bool        present = false;
	};

	LogApi*                logApi          = 0;
	Mem*                   mem             = 0;
	TempMem*               tempMem         = 0;
	VkAllocationCallbacks* allocCallbacks  = 0;
	VkInstance             instance        = VK_NULL_HANDLE;

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
	/*
	Res<> AddLayer(const char* name, Array<const char*>* out) {
		for (const VkLayerProperties* props = layerProps.Begin(); props < layerProps.End(); props++) {
			if (!StrCmp(name, props->layerName)) {
				out->Add(name);
				return Ok();
			}
		}
		return MakeErr(Err_LayerNotFound, "name", name);;
	}

	//-------------------------------------------------------------------------------------------------

	Res<> AddInstanceExtension(const char* name, Array<const char*>* out) {
		for (const VkExtensionProperties* props = instExtProps.Begin(); props < instExtProps.End(); props++) {
			if (!StrCmp(name, props->extensionName)) {
				out->Add(name);
				return Ok();
			}
		}
		return MakeErr(Err_ExtensionNotFound, "name", name);
	}
	
	Res<> EnumeratePhysicalDevices() {
		VkResult vkRes = VK_SUCCESS;
		u32 n = 0;
		physicalDevices.Init(mem);
		do {
			if (vkRes = vkEnumeratePhysicalDevices(instance, &n, nullptr); vkRes== VK_SUCCESS) {
				vkRes = vkEnumeratePhysicalDevices(instance, &n, physicalDevices.Resize(n));
			}
		} while (vkRes == VK_INCOMPLETE);
		if (vkRes != VK_SUCCESS) {
			return MakeVkErr(vkEnumeratePhysicalDevices, vkRes);
		}

		"VK_KHR_swapchain";
		for (u32 i = 0; i < physicalDevices.len; i++) {
			VkPhysicalDevice pd = physicalDevices[i];

			Array<VkExtensionProperties> extProps;
			if (Err* err = EnumerateDeviceExtensions(tempMem, pd).To(extProps)) {
				LogErr(err);
			}

			vkGetPhysicalDeviceMemoryProperties(pd->vkPhysicalDevice, &pd->vkPhysicalDeviceMemoryProperties);

			n = 0;
			vkGetPhysicalDeviceQueueFamilyProperties(pd->vkPhysicalDevice, &n, nullptr);
			vkGetPhysicalDeviceQueueFamilyProperties(pd->vkPhysicalDevice, &n, pd->vkQueueFamilyProperties.Resize(n));

			pd->graphicsQueueFamily = InvalidQueueFamily;
			pd->presentQueueFamily  = InvalidQueueFamily;
			pd->computeQueueFamily  = InvalidQueueFamily;
			pd->transferQueueFamily = InvalidQueueFamily;
			for (u32 qf = 0; qf < pd->vkQueueFamilyProperties.len; qf++) {
				bool supportsPresent = false;
				#if defined JC_OS_WINDOWS
					vkGetPhysicalDeviceWin32PresentationSupportKHR(pd->vkPhysicalDevice, qf);
				#else	// JC_OS
					#error("Unsupported OS");
				#endif	// JC_OS

				VkQueueFamilyProperties* const q = &pd->vkQueueFamilyProperties[qf];
				if (
					(q->queueFlags & VK_QUEUE_GRAPHICS_BIT) && supportsPresent &&
					(
						(pd->graphicsQueueFamily == InvalidQueueFamily && pd->presentQueueFamily == InvalidQueueFamily) ||
						(pd->graphicsQueueFamily != pd->presentQueueFamily)
					)
				) {
					pd->graphicsQueueFamily = qf;
					pd->presentQueueFamily  = qf;

				}
				else if ((q->queueFlags & VK_QUEUE_GRAPHICS_BIT) && pd->graphicsQueueFamily == InvalidQueueFamily) { pd->graphicsQueueFamily = qf; }
				else if ((q->queueFlags & VK_QUEUE_COMPUTE_BIT)  && pd->computeQueueFamily  == InvalidQueueFamily) { pd->presentQueueFamily  = qf; }
				else if ((q->queueFlags & VK_QUEUE_TRANSFER_BIT) && pd->transferQueueFamily == InvalidQueueFamily) { pd->presentQueueFamily  = qf; }
				else if (supportsPresent                         && pd->presentQueueFamily  == InvalidQueueFamily) { pd->presentQueueFamily  = qf; }
			}
		}


		//typedef struct VkFormatProperties {
		//	VkFormatFeatureFlags    linearTilingFeatures;
		//	VkFormatFeatureFlags    optimalTilingFeatures;
		//	VkFormatFeatureFlags    bufferFeatures;
		//} VkFormatProperties;
		//typedef struct VkFormatProperties2 {
		//	VkStructureType       sType;
		//	void*                 pNext;
		//	VkFormatProperties    formatProperties;
		//} VkFormatProperties2;
		//vkGetPhysicalDeviceFormatProperties2(vkPhysicalDevices[i], VkFormat format, VkFormatProperties2* pFormatProperties);


		//typedef struct VkPhysicalDeviceMemoryProperties2 {
		//	VkStructureType                     sType;
		//	void*                               pNext;
		//	VkPhysicalDeviceMemoryProperties    memoryProperties;
		//} VkPhysicalDeviceMemoryProperties2;
		//vkGetPhysicalDeviceMemoryProperties2(vkPhysicalDevices[i], VkPhysicalDeviceMemoryProperties2* pMemoryProperties);

		//typedef struct VkSurfaceCapabilitiesKHR {
		//	uint32_t                         minImageCount;
		//	uint32_t                         maxImageCount;
		//	VkExtent2D                       currentExtent;
		//	VkExtent2D                       minImageExtent;
		//	VkExtent2D                       maxImageExtent;
		//	uint32_t                         maxImageArrayLayers;
		//	VkSurfaceTransformFlagsKHR       supportedTransforms;
		//	VkSurfaceTransformFlagBitsKHR    currentTransform;
		//	VkCompositeAlphaFlagsKHR         supportedCompositeAlpha;
		//	VkImageUsageFlags                supportedUsageFlags;
		//} VkSurfaceCapabilitiesKHR;
		//VkResult vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vkPhysicalDevices[i], VkSurfaceKHR surface, VkSurfaceCapabilitiesKHR* pSurfaceCapabilities);

		//typedef struct VkSurfaceFormatKHR {
		//	VkFormat           format;
		//	VkColorSpaceKHR    colorSpace;
		//} VkSurfaceFormatKHR;
		//VkResult vkGetPhysicalDeviceSurfaceFormatsKHR(vkPhysicalDevices[i], VkSurfaceKHR surface, uint32_t* pSurfaceFormatCount, VkSurfaceFormatKHR* pSurfaceFormats);

		//typedef enum VkPresentModeKHR {
		//	VK_PRESENT_MODE_IMMEDIATE_KHR = 0,
		//	VK_PRESENT_MODE_MAILBOX_KHR = 1,
		//	VK_PRESENT_MODE_FIFO_KHR = 2,
		//	VK_PRESENT_MODE_FIFO_RELAXED_KHR = 3,
		//	VK_PRESENT_MODE_SHARED_DEMAND_REFRESH_KHR = 1000111000,
		//	VK_PRESENT_MODE_SHARED_CONTINUOUS_REFRESH_KHR = 1000111001,
		//	VK_PRESENT_MODE_MAX_ENUM_KHR = 0x7FFFFFFF
		//} VkPresentModeKHR;
		//VkResult vkGetPhysicalDeviceSurfacePresentModesKHR(vkPhysicalDevices[i], VkSurfaceKHR surface, uint32_t* pPresentModeCount, VkPresentModeKHR* pPresentModes);

		//VkResult vkGetPhysicalDeviceSurfaceSupportKHR(vkPhysicalDevices[i], uint32_t queueFamilyIndex, VkSurfaceKHR surface, VkBool32* pSupported);

		//VkBool32 vkGetPhysicalDeviceWin32PresentationSupportKHR(vkPhysicalDevices[i], uint32_t queueFamilyIndex);

		}
		return Ok();
	}
	*/
	//-------------------------------------------------------------------------------------------------

	#define CheckVk(expr) { \
		if (vkRes = expr; vkRes != VK_SUCCESS) { \
			return MakeVkErr(#expr, vkRes); \
		} \
	}

	Res<> Init(LogApi* logApi_, Mem* mem_, TempMem* tempMem_) override {
		logApi  = logApi_;
		mem     = mem_;
		tempMem = tempMem_;

		if (Res<> r = Vk::LoadRootFns(); !r) { return r; }

		VkApplicationInfo appInfo = {
			.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO,
			.pNext              = nullptr,
			.pApplicationName   = "JC",
			.applicationVersion = 1,
			.pEngineName        = "JC",
			.engineVersion      = 1,
			.apiVersion         = VK_API_VERSION_1_3,
		};

		VkResult vkRes = VK_SUCCESS;
		u32 n = 0;
		Array<VkLayerProperties> layerProps(tempMem);
		CheckVk(vkEnumerateInstanceLayerProperties(&n, 0));
		CheckVk(vkEnumerateInstanceLayerProperties(&n, layerProps.Resize(n)));

		Array<const char*> requiredLayers(tempMem);
		#if defined Render_Debug
			requiredLayers.Add("VK_LAYER_KHRONOS_validation");
		#endif	// Render_Debug
		for (u64 i = 0; i < requiredLayers.len; i++) {
			bool found = false;
			for (u64 j = 0; j < layerProps.len; j++) {
				if (!StrCmp(requiredLayers[i], layerProps[j].layerName)) {
					found = true;
					break;
				}
			}
			if (!found) {
				return MakeErr(Err_LayerNotFound, "name", requiredLayers[i]);
			}
		}
	
		Array<VkExtensionProperties> instExtProps(tempMem);
		CheckVk(vkEnumerateInstanceExtensionProperties(0, &n, 0));
		CheckVk(vkEnumerateInstanceExtensionProperties(0, &n, instExtProps.Resize(n)));

		Array<const char*> requiredInstExts(tempMem);
		requiredInstExts.Add("VK_KHR_surface");
		#if defined Os_Windows
			requiredInstExts.Add("VK_KHR_win32_surface");
		#else	//  Os_
			#error("Unsupported OS")
		#endif	//	Os_

		for (u64 i = 0; i < instExtProps.len; i++) {
			
			Log("  {}: specVersion={}", instExtProps[i].extensionName, instExtProps[i].specVersion);
		}
		const bool haveDebugUtils = (bool)AddInstanceExtension("VK_EXT_debug_utils", &instExts);
		
		VkInstanceCreateInfo createInfo = {
			.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
			.pNext                   = nullptr,
			.flags                   = 0,
			.pApplicationInfo        = &appInfo,
			.enabledLayerCount       = (u32)layers.len,
			.ppEnabledLayerNames     = layers.data,
			.enabledExtensionCount   = (u32)instExts.len,
			.ppEnabledExtensionNames = instExts.data,
		};

		VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo;
		if (haveDebugUtils) {
			debugCreateInfo.sType           = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
			debugCreateInfo.pNext           = nullptr;
			debugCreateInfo.flags           = 0;
			debugCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
			debugCreateInfo.messageType     = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_DEVICE_ADDRESS_BINDING_BIT_EXT;
			debugCreateInfo.pfnUserCallback = RenderApiObj::DebugCallback;
			debugCreateInfo.pUserData       = this;
			createInfo.pNext = &debugCreateInfo;
		}

		VkResult vkRes = vkCreateInstance(&createInfo, allocCallbacks, &instance);
		if (vkRes != VK_SUCCESS) {
			return MakeVkErr(vkCreateInstance, vkRes);
		}

		EnumerateInstanceExtensions();
		if (Res<> r = CreateInstance(); !r) { return r; }
		Vk::LoadInstanceFns(instance);
		if (Res<> r = EnumeratePhysicalDevices(); !r) { return r; }
		//if (Res<> r = CreateDevice(); !r) { return r; }
		*/
		return Ok();
	}

	//----------------------------------------------------------------------------------------------

	void Shutdown() override {
	}
};

//--------------------------------------------------------------------------------------------------

static RenderApiObj renderApi;

RenderApi* RenderApi::Get() {
	return &renderApi;
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC