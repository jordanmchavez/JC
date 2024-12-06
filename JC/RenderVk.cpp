#pragma once

#include "JC/Render.h"

#include "JC/Array.h"
#include "JC/Fmt.h"
#include "JC/Log.h"
#include "JC/Mem.h"
#include "JC/RenderVk.h"

#define Render_Debug

#include <stdio.h>	
#include <math.h>

namespace JC {

//--------------------------------------------------------------------------------------------------

#define MakeVkErr(Fn, vkRes) \
	MakeErr(ErrCode { .ns = "vk", .code = (u64)vkRes }, "fn", #Fn, "desc", Vk::ResultStr(vkRes))

#define CheckVk(expr) { \
	if (vkRes = expr; vkRes != VK_SUCCESS) { \
		return MakeVkErr(#expr, vkRes); \
	} \
}

//--------------------------------------------------------------------------------------------------

Array<u8> ReadFile(Mem* mem, const char* path) {
	FILE* f = fopen(path, "rb");
	Assert(f);
	fseek(f, 0, SEEK_END);
	u64 len = ftell(f);
	fseek(f, 0, SEEK_SET);
	Array<u8> a(mem);
	a.Resize(len);
	Assert(fread(a.data, 1, len, f) == len);
	fclose(f);
	return a;
}

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
		u32                              grahicsQueueFamily               = VK_QUEUE_FAMILY_IGNORED;
		u32                              score                            = 0;
	};

	Log*                     log                         = 0;
	Mem*                     mem                         = 0;
	TempMem*                 tempMem                     = 0;
	VkAllocationCallbacks*   vkAllocationCallbacks       = 0;
	VkInstance               vkInstance                  = VK_NULL_HANDLE;
	VkDebugUtilsMessengerEXT vkDebugUtilsMessenger       = VK_NULL_HANDLE;
	VkSurfaceKHR             vkSurface                   = VK_NULL_HANDLE;
	Array<PhysicalDevice>    physicalDevices             = {};
	VkDevice                 vkDevice                    = VK_NULL_HANDLE;
	VkSwapchainKHR           vkSwapchain                 = VK_NULL_HANDLE;
	VkQueue                  vkQueue                     = VK_NULL_HANDLE;
	VkCommandPool            vkCommandPool               = VK_NULL_HANDLE;
	VkCommandBuffer          vkSetupCommandBuffer        = VK_NULL_HANDLE;
	VkCommandBuffer          vkRenderCommandBuffer       = VK_NULL_HANDLE;
	VkExtent2D               vkSwapchainExtent           = {};
	Array<VkImage>           vkSwapchainImages           = {};
	VkFence                  vkFence                     = VK_NULL_HANDLE;
	Array<VkImageView>       vkSwapchainImageViews       = {};
	VkImage                  vkDepthImage                = VK_NULL_HANDLE;
	VkDeviceMemory           vkDepthImageMemory          = VK_NULL_HANDLE;
	VkImageView              vkDepthImageView            = VK_NULL_HANDLE;
	VkRenderPass             vkRenderPass                = VK_NULL_HANDLE;
	Array<VkFramebuffer>     vkFramebuffers              = {};
	VkBuffer                 vkVertexBuffer              = VK_NULL_HANDLE;
	VkDeviceMemory           vkVertexBufferMemory        = VK_NULL_HANDLE;
	VkShaderModule           vkVertexShaderModule        = VK_NULL_HANDLE;
	VkShaderModule           vkFragmentShaderModule      = VK_NULL_HANDLE;
	VkPipelineLayout         vkPipelineLayout            = VK_NULL_HANDLE;
	VkPipeline               vkPipeline                  = VK_NULL_HANDLE;
	VkBuffer                 vkShaderUniformBuffer       = VK_NULL_HANDLE;
	VkDeviceMemory           vkShaderUniformBufferMemory = VK_NULL_HANDLE;
	VkDescriptorSetLayout    vkDescriptorSetLayout       = VK_NULL_HANDLE;
	VkDescriptorPool         vkDescriptorPool            = VK_NULL_HANDLE;
	VkDescriptorSet          vkDescriptorSet             = VK_NULL_HANDLE;
	float                    cameraZ                     = 10.0f;
	float                    cameraZDir                  = -1.0f;
	
	//-------------------------------------------------------------------------------------------------

	s8 SizeStr(u64 size) {
		if (size > 1024 * 1024 * 1024) {
			return Fmt(tempMem, "{.1}gb", (double)size / (1024.0 * 1024.0 * 1024.0));
		}
		if (size > 1024 * 1024) {
			return Fmt(tempMem, "{.1}mb", (double)size / (1024.0 * 1024.0));
		}
		if (size > 1024) {
			return Fmt(tempMem, "{.1}kb", (double)size / 1024.0);
		}
		return Fmt(tempMem, "{}", size);
	}

	//-------------------------------------------------------------------------------------------------

	static VkBool32 VKAPI_CALL DebugCallback(
		VkDebugUtilsMessageSeverityFlagBitsEXT      severity,
		VkDebugUtilsMessageTypeFlagsEXT             flags,
		const VkDebugUtilsMessengerCallbackDataEXT* data,
		void*                                       userData
	) {
		flags;
		Log* const log = (Log*)userData;
		if (severity & (VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)) {
			log->Error("[{}] {}: {}", data->messageIdNumber, data->pMessageIdName, data->pMessage ? data->pMessage : "");
		} else {
			log->Print("[{}] {}: {}", data->messageIdNumber, data->pMessageIdName, data->pMessage ? data->pMessage : "");
		}
		return VK_FALSE;
	}

	//-------------------------------------------------------------------------------------------------

	Res<> Init(const RenderApiInit* init) override {
		log     = init->log;
		mem     = init->mem;
		tempMem = init->tempMem;

		VkResult vkRes = VK_SUCCESS;
		u32 n = 0;

		Logf("Initializing vulkan renderer");

		if (Res<> r = Vk::LoadRootFns(); !r) { return r; }

		// TODO: we can remove 
		Array<VkLayerProperties> layers(tempMem);
		CheckVk(vkEnumerateInstanceLayerProperties(&n, 0));
		CheckVk(vkEnumerateInstanceLayerProperties(&n, layers.Resize(n)));

		Logf("{} layers:", layers.len);
		for (u64 i = 0; i < layers.len; i++) {
			Logf(
				"  {}: implementationVersion={}, specVersion={}, description={}",
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
					Logf("Found required layer '{}'", requiredLayers[i]);
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

		Logf("{} instance extensions:", instExts.len);
		for (u64 i = 0; i < instExts.len; i++) {
			Logf("  {}: specVersion={}", instExts[i].extensionName, Vk::VersionStr(instExts[i].specVersion));
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
					Logf("Found required instance extension '{}'", requiredInstExts[i]);
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
		CheckVk(vkCreateInstance(&vkInstanceCreateInfo, vkAllocationCallbacks, &vkInstance));
		Vk::LoadInstanceFns(vkInstance);
		Logf("Created instance");

		#if defined Render_Debug
			CheckVk(vkCreateDebugUtilsMessengerEXT(vkInstance, &debugCreateInfo, vkAllocationCallbacks, &vkDebugUtilsMessenger));
			Logf("Created debug messenger");
		#endif	// Render_Debug

		#if defined Os_Windows
			VkWin32SurfaceCreateInfoKHR win32SurfaceCreateInfo = {
				.sType     = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
				.pNext     = 0,
				.flags     = 0,
				.hinstance = (HINSTANCE)init->osInstanceHandle,
				.hwnd      = (HWND)init->osWindowHandle,
			};
			CheckVk(vkCreateWin32SurfaceKHR(vkInstance, &win32SurfaceCreateInfo, vkAllocationCallbacks, &vkSurface));
			Logf("Created win32 surface");
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
			constexpr VkFormat vkDesiredSurfaceFormats[] = {
				VK_FORMAT_B8G8R8A8_UNORM,
				VK_FORMAT_R8G8B8A8_UNORM,
				VK_FORMAT_B8G8R8_UNORM,
				VK_FORMAT_R8G8B8_UNORM 
			};
			for (u64 j = 0; j < pd->vkSurfaceFormats.len; j++) {
				
			}

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
		// TODO: separate queue families

		constexpr const char* requiredDeviceExts[] = {
			"VK_KHR_swapchain",
		};
		constexpr u64 requiredDeviceExtsLen = LenOf(requiredDeviceExts);

		Logf("{} physical devices:", physicalDevices.len);
		for (u64 i = 0; i < physicalDevices.len; i++) {
			PhysicalDevice* pd = &physicalDevices[i];
			Logf(
				"{}: apiVersion={}, driverVersion={}, vendorID={}, deviceId={}, deviceType={}",
				pd->vkPhysicalDeviceProperties.deviceName,
				Vk::VersionStr(pd->vkPhysicalDeviceProperties.apiVersion),
				Vk::VersionStr(pd->vkPhysicalDeviceProperties.driverVersion),
				pd->vkPhysicalDeviceProperties.vendorID,
				pd->vkPhysicalDeviceProperties.deviceID,
				Vk::PhysicalDeviceTypeStr(pd->vkPhysicalDeviceProperties.deviceType)
			);
			Logf("  {} memory types:", pd->vkPhysicalDeviceMemoryProperties.memoryTypeCount);
			for (u64 j = 0; j < pd->vkPhysicalDeviceMemoryProperties.memoryTypeCount; j++) {
				const VkMemoryType mt = pd->vkPhysicalDeviceMemoryProperties.memoryTypes[j];
				Logf("    [{}] heapIndex={}, flags={}", j, mt.heapIndex, Vk::MemoryPropertyFlagsStr(mt.propertyFlags));
			}
			Logf("  {} memory heaps:", pd->vkPhysicalDeviceMemoryProperties.memoryHeapCount);
			for (u64 j = 0; j < pd->vkPhysicalDeviceMemoryProperties.memoryHeapCount; j++) {
				const VkMemoryHeap mh = pd->vkPhysicalDeviceMemoryProperties.memoryHeaps[j];
				Logf("    [{}] size={}, flags={}", j, SizeStr(mh.size), Vk::MemoryHeapFlagsStr(mh.flags));
			}
			Logf("  {} device extensions: ", pd->vkExtensionProperties.len);
			for (u64 j = 0; j < pd->vkExtensionProperties.len; j++) {
				Logf("    {}: specVersion={}", pd->vkExtensionProperties[j].extensionName, Vk::VersionStr(pd->vkExtensionProperties[j].specVersion));
			}
			Logf("  {} surface formats:", pd->vkSurfaceFormats.len);
			for (u64 j = 0; j < pd->vkSurfaceFormats.len; j++) {
				Logf("    {}, {}", Vk::FormatStr(pd->vkSurfaceFormats[j].format), Vk::ColorSpaceStr(pd->vkSurfaceFormats[j].colorSpace));
			}
			Logf("  {} present modes:", pd->vkPresentModes.len);
			for (u64 j = 0; j < pd->vkPresentModes.len; j++) {
				Logf("    {}", Vk::PresentModeStr(pd->vkPresentModes[j]));
			}
			Logf("  {} queue families:", pd->queueFamilies.len);
			for (u32 j = 0; j < pd->queueFamilies.len; j++) {
				const VkQueueFamilyProperties* props = &pd->queueFamilies[j].vkQueueFamilyProperties;
				Logf("    [{}] count={}, flags={}, supportsPresent={}", j, props->queueCount, Vk::QueueFlagsStr(props->queueFlags), pd->queueFamilies[j].supportsPresent);

				const VkQueueFlags flags = pd->queueFamilies[j].vkQueueFamilyProperties.queueFlags;
				if (pd->grahicsQueueFamily == VK_QUEUE_FAMILY_IGNORED && (flags & VK_QUEUE_GRAPHICS_BIT) && pd->queueFamilies[j].supportsPresent) {
					pd->grahicsQueueFamily = j;
				}
			}

			pd->score = 0;
			switch (pd->vkPhysicalDeviceProperties.deviceType) {
				case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:   pd->score += 1000; break;
				case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: pd->score +=  100; break;
				case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:    pd->score +=   10; break;
				case VK_PHYSICAL_DEVICE_TYPE_CPU:            pd->score +=    1; break;
				case VK_PHYSICAL_DEVICE_TYPE_OTHER:          pd->score +=    1; break;
			};
			for (u64 j = 0; j < requiredDeviceExtsLen; j++) {
				bool found = false;
				for (u64 k = 0; k < pd->vkExtensionProperties.len; k++) {
					if (!StrCmp(requiredDeviceExts[j], pd->vkExtensionProperties[k].extensionName)) {
						found = true;
						break;
					}
				}
				if (!found) {
					Logf("Rejecting device '{}': doesn't support extension '{}'", pd->vkPhysicalDeviceProperties.deviceName, requiredDeviceExts[j]);
					pd->score = 0;
				}
			}
			if (pd->vkSurfaceFormats.len == 0) {
				Logf("Rejecting device '{}': no surface formats", pd->vkPhysicalDeviceProperties.deviceName);
				pd->score = 0;
			}
			if (pd->vkPresentModes.len == 0) {
				Logf("Rejecting device '{}': no present modes", pd->vkPhysicalDeviceProperties.deviceName);
				pd->score = 0;
			}
			if (pd->grahicsQueueFamily == VK_QUEUE_FAMILY_IGNORED) {
				Logf("Rejecting device '{}': no graphics queue supporting present", pd->vkPhysicalDeviceProperties.deviceName);
				pd->score = 0;
			}
		}

		u32 bestScore = 0;
		const PhysicalDevice* physicalDevice = 0;
		for (u64 i = 0; i < physicalDevices.len; i++) {
			const PhysicalDevice* pd = &physicalDevices[i];
			Logf("{}: score {}", pd->vkPhysicalDeviceProperties.deviceName, pd->score);
			if (pd->score > bestScore) {
				physicalDevice = pd;
				bestScore = pd->score;
			}
		}
		if (bestScore == 0) {
			return MakeErr(Err_NoSuitableDevice);
		}
		Logf("Selected physical device '{}' with score={}", physicalDevice->vkPhysicalDeviceProperties.deviceName, physicalDevice->score);

		float queuePriority = 1.0f;
		VkDeviceQueueCreateInfo vkDeviceQueueCreateInfo = {
			.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
			.pNext            = 0,
			.flags            = 0,
			.queueFamilyIndex = physicalDevice->grahicsQueueFamily,
			.queueCount       = 1,
			.pQueuePriorities = &queuePriority,
		};
		VkPhysicalDeviceFeatures vkPhysicalDeviceFeatures = {};
		vkPhysicalDeviceFeatures.shaderClipDistance = VK_TRUE;
		VkDeviceCreateInfo vkDeviceCreateInfo = {
			.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
			.pNext                   = 0,
			.flags                   = 0,
			.queueCreateInfoCount    = 1,
			.pQueueCreateInfos       = &vkDeviceQueueCreateInfo,
			.enabledLayerCount       = (u32)requiredLayersLen,
			.ppEnabledLayerNames     = requiredLayers,
			.enabledExtensionCount   = (u32)requiredDeviceExtsLen,
			.ppEnabledExtensionNames = requiredDeviceExts,
			.pEnabledFeatures        = &vkPhysicalDeviceFeatures,
		};
		CheckVk(vkCreateDevice(physicalDevice->vkPhysicalDevice, &vkDeviceCreateInfo, vkAllocationCallbacks, &vkDevice));
		Logf("Created device");

		Vk::LoadDeviceFns(vkDevice);

		VkFormat vkSwapchainFormat = (physicalDevice->vkSurfaceFormats.len == 1 && physicalDevice->vkSurfaceFormats[0].format == VK_FORMAT_UNDEFINED)
			? VK_FORMAT_B8G8R8_UNORM
			: physicalDevice->vkSurfaceFormats[0].format;
		VkColorSpaceKHR vkSwapchainColorSpace = physicalDevice->vkSurfaceFormats[0].colorSpace;
		Logf("Selected swapchainForma=t{}, swapchainColorSpace={}", Vk::FormatStr(vkSwapchainFormat), Vk::ColorSpaceStr(vkSwapchainColorSpace));


		u32 swapchainImageCount = 2;
		if (physicalDevice->vkSurfaceCapabilities.minImageCount > swapchainImageCount) {
			swapchainImageCount = physicalDevice->vkSurfaceCapabilities.minImageCount;
		}
		if (physicalDevice->vkSurfaceCapabilities.maxImageCount && physicalDevice->vkSurfaceCapabilities.maxImageCount < swapchainImageCount) {
			swapchainImageCount = physicalDevice->vkSurfaceCapabilities.maxImageCount;
		}
		Logf("Selected swapchainImageCount={}", swapchainImageCount);

		vkSwapchainExtent = physicalDevice->vkSurfaceCapabilities.currentExtent;
		if (vkSwapchainExtent.width == -1) {
			vkSwapchainExtent.width  = init->width;
			vkSwapchainExtent.height = init->height;
		}
		Logf("Selected vkSwapchainExtent=({}, {})", vkSwapchainExtent.width, vkSwapchainExtent.height);

		VkSurfaceTransformFlagBitsKHR swapchainTransform = physicalDevice->vkSurfaceCapabilities.currentTransform;
		if (physicalDevice->vkSurfaceCapabilities.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR) {
			swapchainTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
		}

		VkPresentModeKHR swapchainPresentMode = VK_PRESENT_MODE_FIFO_KHR;
		for (u64 i = 0; i < physicalDevice->vkPresentModes.len; i++) {
			if (physicalDevice->vkPresentModes[i] == VK_PRESENT_MODE_MAILBOX_KHR) {
				swapchainPresentMode = VK_PRESENT_MODE_MAILBOX_KHR;
			}
		}
		Logf("Selected swapchainPresentMode={}", Vk::PresentModeStr(swapchainPresentMode));

		VkSwapchainCreateInfoKHR vkSwapchainCreateInfoKHR = {
			.sType                 = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
			.pNext                 = 0,
			.flags                 = 0,
			.surface               = vkSurface,
			.minImageCount         = swapchainImageCount,
			.imageFormat           = vkSwapchainFormat,
			.imageColorSpace       = vkSwapchainColorSpace,
			.imageExtent           = vkSwapchainExtent,
			.imageArrayLayers      = 1,
			.imageUsage            = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
			.imageSharingMode      = VK_SHARING_MODE_EXCLUSIVE,
			.queueFamilyIndexCount = 0,
			.pQueueFamilyIndices   = 0,
			.preTransform          = swapchainTransform,
			.compositeAlpha        = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
			.presentMode           = swapchainPresentMode,
			.clipped               = VK_TRUE,
			.oldSwapchain          = 0,
		};
		CheckVk(vkCreateSwapchainKHR(vkDevice, &vkSwapchainCreateInfoKHR, vkAllocationCallbacks, &vkSwapchain));
		Logf("Created swapchain");

		vkGetDeviceQueue(vkDevice, physicalDevice->grahicsQueueFamily, 0, &vkQueue);

		VkCommandPoolCreateInfo vkCommandPoolCreateInfo = {
			.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
			.pNext            = 0,
			.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
			.queueFamilyIndex = physicalDevice->grahicsQueueFamily,
		};
		CheckVk(vkCreateCommandPool(vkDevice, &vkCommandPoolCreateInfo, vkAllocationCallbacks, &vkCommandPool));
		Logf("Created command pool");

		VkCommandBufferAllocateInfo vkCommandBufferAllocateInfo = {
			.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
			.pNext              = 0,
			.commandPool        = vkCommandPool,
			.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
			.commandBufferCount = 1,
		};
		CheckVk(vkAllocateCommandBuffers(vkDevice, &vkCommandBufferAllocateInfo, &vkSetupCommandBuffer));
		CheckVk(vkAllocateCommandBuffers(vkDevice, &vkCommandBufferAllocateInfo, &vkRenderCommandBuffer));
		Logf("Created command buffers");

		vkSwapchainImages.Init(mem);
		CheckVk(vkGetSwapchainImagesKHR(vkDevice, vkSwapchain, &n, 0));
		CheckVk(vkGetSwapchainImagesKHR(vkDevice, vkSwapchain, &n, vkSwapchainImages.Resize(n)));
		Logf("Got swapchain images");

		VkFenceCreateInfo vkFenceCreateInfo = {
			.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
			.pNext = 0,
			.flags = 0,
		};
		CheckVk(vkCreateFence(vkDevice, &vkFenceCreateInfo, vkAllocationCallbacks, &vkFence));
		Logf("Created fence");

		bool* const transitioned = (bool*)tempMem->Alloc(swapchainImageCount * sizeof(bool));
		MemSet(transitioned, 0, sizeof(transitioned));
		i32 imagesRemaining = (i32)swapchainImageCount;
		while (imagesRemaining > 0) {
			VkSemaphoreCreateInfo vkSemaphoreCreateInfo = {
				.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
				.pNext = 0,
				.flags = 0,
			};
			VkSemaphore vkSemaphore = VK_NULL_HANDLE;
			CheckVk(vkCreateSemaphore(vkDevice, &vkSemaphoreCreateInfo, vkAllocationCallbacks, &vkSemaphore));

			u32 imageIndex = 0;
			CheckVk(vkAcquireNextImageKHR(vkDevice, vkSwapchain, UINT64_MAX, vkSemaphore, 0, &imageIndex));

			if (!transitioned[imageIndex]) {
				VkCommandBufferBeginInfo vkCommandBufferBeginInfo = {
					.sType            = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
					.pNext            = 0,
					.flags            = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
					.pInheritanceInfo = 0,
				};
				CheckVk(vkBeginCommandBuffer(vkSetupCommandBuffer, &vkCommandBufferBeginInfo));

				VkImageMemoryBarrier vkImageMemoryBarrier = {
					.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
					.pNext               = 0,
					.srcAccessMask       = 0,
					.dstAccessMask       = VK_ACCESS_MEMORY_READ_BIT,
					.oldLayout           = VK_IMAGE_LAYOUT_UNDEFINED,
					.newLayout           = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
					.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
					.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
					.image               = vkSwapchainImages[imageIndex],
					.subresourceRange    = {
						.aspectMask      = VK_IMAGE_ASPECT_COLOR_BIT,
						.baseMipLevel    = 0,
						.levelCount      = 1,
						.baseArrayLayer  = 0,
						.layerCount      = 1,
					},
				};
				vkCmdPipelineBarrier(
					vkSetupCommandBuffer,
					VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
					VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
					0,
					0, 0,
					0, 0,
					1, &vkImageMemoryBarrier
				);

				CheckVk(vkEndCommandBuffer(vkSetupCommandBuffer));

				VkPipelineStageFlags vkWaitDstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
				VkSubmitInfo vkSubmitInfo = {
					.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO,
					.pNext                = 0,
					.waitSemaphoreCount   = 1,
					.pWaitSemaphores      = &vkSemaphore,
					.pWaitDstStageMask    = &vkWaitDstStageMask,
					.commandBufferCount   = 1,
					.pCommandBuffers      = &vkSetupCommandBuffer,
					.signalSemaphoreCount = 0,
					.pSignalSemaphores    = 0,
				};
				CheckVk(vkQueueSubmit(vkQueue, 1, &vkSubmitInfo, vkFence));

				CheckVk(vkWaitForFences(vkDevice, 1, &vkFence, VK_TRUE, UINT64_MAX));
				CheckVk(vkResetFences(vkDevice, 1, &vkFence));
				vkDestroySemaphore(vkDevice, vkSemaphore, vkAllocationCallbacks);
				CheckVk(vkResetCommandBuffer(vkSetupCommandBuffer, 0));

				transitioned[imageIndex] = true;
				imagesRemaining--;
				Logf("Transitioned swap chain image {}", imageIndex);
			}

			VkPresentInfoKHR vkPresentInfo = {
				.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
				.pNext              = 0,
				.waitSemaphoreCount = 0,
				.pWaitSemaphores    = 0,
				.swapchainCount     = 1,
				.pSwapchains        = &vkSwapchain,
				.pImageIndices      = &imageIndex,
				.pResults           = 0,
			};
			CheckVk(vkQueuePresentKHR(vkQueue, &vkPresentInfo));
		}
		Logf("Transitioned all swap chain images");

		vkSwapchainImageViews.Init(mem);
		vkSwapchainImageViews.Resize(vkSwapchainImages.len);
		for (u64 i = 0; i < vkSwapchainImages.len; i++) {
			VkImageViewCreateInfo vkImageViewCreateInfo = {
				.sType              = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
				.pNext              = 0,
				.flags              = 0,
				.image              = vkSwapchainImages[i],
				.viewType           = VK_IMAGE_VIEW_TYPE_2D,
				.format             = vkSwapchainFormat,
				.components         = {
					.r              = VK_COMPONENT_SWIZZLE_R,
					.g              = VK_COMPONENT_SWIZZLE_G,
					.b              = VK_COMPONENT_SWIZZLE_B,
					.a              = VK_COMPONENT_SWIZZLE_A,
				},
				.subresourceRange   = {
					.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
					.baseMipLevel   = 0,
					.levelCount     = 1,
					.baseArrayLayer = 0,
					.layerCount     = 1,
				},
			};
			CheckVk(vkCreateImageView(vkDevice, &vkImageViewCreateInfo, vkAllocationCallbacks, &vkSwapchainImageViews[i]));
		}
		Logf("Got swapchain image views");

		VkImageCreateInfo vkImageCreateInfo = {
			.sType                 = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
			.pNext                 = 0,
			.flags                 = 0,
			.imageType             = VK_IMAGE_TYPE_2D,
			.format                = VK_FORMAT_D16_UNORM,
			.extent                = {
				.width             = vkSwapchainExtent.width,
				.height            = vkSwapchainExtent.height,
				.depth             = 1,
			},
			.mipLevels             = 1,
			.arrayLayers           = 1,
			.samples               = VK_SAMPLE_COUNT_1_BIT,
			.tiling                = VK_IMAGE_TILING_OPTIMAL,
			.usage                 = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
			.sharingMode           = VK_SHARING_MODE_EXCLUSIVE,
			.queueFamilyIndexCount = 0,
			.pQueueFamilyIndices   = 0,
			.initialLayout         = VK_IMAGE_LAYOUT_UNDEFINED,
		};
		CheckVk(vkCreateImage(vkDevice, &vkImageCreateInfo, vkAllocationCallbacks, &vkDepthImage));
		Logf("Created depth image");

		VkMemoryRequirements vkMemoryRequirements = {};
		vkGetImageMemoryRequirements(vkDevice, vkDepthImage, &vkMemoryRequirements);
		VkMemoryAllocateInfo vkMemoryAllocateInfo = {
			.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
			.pNext           = 0,
			.allocationSize  = vkMemoryRequirements.size,
			.memoryTypeIndex = VK_MAX_MEMORY_TYPES,
		};
		for (u32 i = 0; i < VK_MAX_MEMORY_TYPES; i++) {
			if (
				(vkMemoryRequirements.memoryTypeBits & (1 << i)) &&
				(physicalDevice->vkPhysicalDeviceMemoryProperties.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
			) {
				vkMemoryAllocateInfo.memoryTypeIndex = i;
				break;
			}
		}
		CheckVk(vkAllocateMemory(vkDevice, &vkMemoryAllocateInfo, vkAllocationCallbacks, &vkDepthImageMemory));
		CheckVk(vkBindImageMemory(vkDevice, vkDepthImage, vkDepthImageMemory, 0));
		Logf("Allocate depth image memory: size={}, memoryTypeIndex={}", vkMemoryAllocateInfo.allocationSize, vkMemoryAllocateInfo.memoryTypeIndex);

		{
			VkCommandBufferBeginInfo vkCommandBufferBeginInfo = {
				.sType            = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
				.pNext            = 0,
				.flags            = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
				.pInheritanceInfo = 0,
			};
			CheckVk(vkBeginCommandBuffer(vkSetupCommandBuffer, &vkCommandBufferBeginInfo));

			VkImageMemoryBarrier vkImageMemoryBarrier = {
				.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
				.pNext               = 0,
				.srcAccessMask       = 0,
				.dstAccessMask       = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
				.oldLayout           = VK_IMAGE_LAYOUT_UNDEFINED,
				.newLayout           = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
				.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
				.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
				.image               = vkDepthImage,
				.subresourceRange    = {
					.aspectMask      = VK_IMAGE_ASPECT_DEPTH_BIT,
					.baseMipLevel    = 0,
					.levelCount      = 1,
					.baseArrayLayer  = 0,
					.layerCount      = 1,
				},
			};
			vkCmdPipelineBarrier(
				vkSetupCommandBuffer,
				VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
				VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
				0,
				0, 0,
				0, 0,
				1, &vkImageMemoryBarrier
			);

			CheckVk(vkEndCommandBuffer(vkSetupCommandBuffer));

			//VkPipelineStageFlags vkWaitDstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			VkSubmitInfo vkSubmitInfo = {
				.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO,
				.pNext                = 0,
				.waitSemaphoreCount   = 0,
				.pWaitSemaphores      = 0,
				.pWaitDstStageMask    = 0,//&vkWaitDstStageMask,
				.commandBufferCount   = 1,
				.pCommandBuffers      = &vkSetupCommandBuffer,
				.signalSemaphoreCount = 0,
				.pSignalSemaphores    = 0,
			};
			CheckVk(vkQueueSubmit(vkQueue, 1, &vkSubmitInfo, vkFence));

			CheckVk(vkWaitForFences(vkDevice, 1, &vkFence, VK_TRUE, UINT64_MAX));
			CheckVk(vkResetFences(vkDevice, 1, &vkFence));
			CheckVk(vkResetCommandBuffer(vkSetupCommandBuffer, 0));
		}
		Logf("Transitioned depth image to VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL");

		VkImageViewCreateInfo vkImageViewCreateInfo = {
			.sType              = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			.pNext              = 0,
			.flags              = 0,
			.image              = vkDepthImage,
			.viewType           = VK_IMAGE_VIEW_TYPE_2D,
			.format             = VK_FORMAT_D16_UNORM,
			.components         = {
				.r              = VK_COMPONENT_SWIZZLE_IDENTITY,
				.g              = VK_COMPONENT_SWIZZLE_IDENTITY,
				.b              = VK_COMPONENT_SWIZZLE_IDENTITY,
				.a              = VK_COMPONENT_SWIZZLE_IDENTITY,
			},
			.subresourceRange   = {
				.aspectMask     = VK_IMAGE_ASPECT_DEPTH_BIT,
				.baseMipLevel   = 0,
				.levelCount     = 1,
				.baseArrayLayer = 0,
				.layerCount     = 1,
			},
		};
		CheckVk(vkCreateImageView(vkDevice, &vkImageViewCreateInfo, vkAllocationCallbacks, &vkDepthImageView));
		Logf("Created depth image view");

		VkAttachmentDescription vkAttachmentDescriptions[2] = {
			{
				.flags          = 0,
				.format         = vkSwapchainFormat,
				.samples        = VK_SAMPLE_COUNT_1_BIT,
				.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR,
				.storeOp        = VK_ATTACHMENT_STORE_OP_STORE,
				.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
				.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
				.initialLayout  = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
				.finalLayout    = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			},
			{
				.flags          = 0,
				.format         = VK_FORMAT_D16_UNORM,
				.samples        = VK_SAMPLE_COUNT_1_BIT,
				.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR,
				.storeOp        = VK_ATTACHMENT_STORE_OP_STORE,
				.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
				.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
				.initialLayout  = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
				.finalLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
			},
		};
		VkAttachmentReference vkColorAttachmentReference = {
			.attachment = 0,
			.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		};
		VkAttachmentReference vkDepthlAttachmentReference = {
			.attachment = 1,
			.layout     = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
		};
		VkSubpassDescription vkSubpassDescription = {
			.flags                   = 0,
			.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS,
			.inputAttachmentCount    = 0,
			.pInputAttachments       = 0,
			.colorAttachmentCount    = 1,
			.pColorAttachments       = &vkColorAttachmentReference,
			.pResolveAttachments     = 0,
			.pDepthStencilAttachment = &vkDepthlAttachmentReference,
			.preserveAttachmentCount = 0,
			.pPreserveAttachments    = 0,
		};
		VkRenderPassCreateInfo vkRenderPassCreateInfo = {
			.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
			.pNext           = 0,
			.flags           = 0,
			.attachmentCount = 2,
			.pAttachments    = vkAttachmentDescriptions,
			.subpassCount    = 1,
			.pSubpasses      = &vkSubpassDescription,
			.dependencyCount = 0,
			.pDependencies   = 0,

		};
		CheckVk(vkCreateRenderPass(vkDevice, &vkRenderPassCreateInfo, vkAllocationCallbacks, &vkRenderPass));
		Logf("Created render pass");

		vkFramebuffers.Init(mem);
		vkFramebuffers.Resize(vkSwapchainImageViews.len);
		for (u64 i = 0; i < vkSwapchainImageViews.len; i++) {
			VkImageView vkImageViewAttachments[] = { vkSwapchainImageViews[i], vkDepthImageView };
			VkFramebufferCreateInfo vkFramebufferCreateInfo = {
				.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
				.pNext           = 0,
				.flags           = 0,
				.renderPass      = vkRenderPass,
				.attachmentCount = 2,
				.pAttachments    = vkImageViewAttachments,
				.width           = vkSwapchainExtent.width,
				.height          = vkSwapchainExtent.height,
				.layers          = 1,
			};
			CheckVk(vkCreateFramebuffer(vkDevice, &vkFramebufferCreateInfo, vkAllocationCallbacks, &vkFramebuffers[i]));
		}
		Logf("Created framebuffers");

		struct Vertex {
			float x, y, z, w;
		};

		VkBufferCreateInfo vkBufferCreateInfo = {
			.sType                 = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
			.pNext                 = 0,
			.flags                 = 0,
			.size                  = sizeof(Vertex) * 3,
			.usage                 = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
			.sharingMode           = VK_SHARING_MODE_EXCLUSIVE,
			.queueFamilyIndexCount = 0,
			.pQueueFamilyIndices   = 0,
		};
		CheckVk(vkCreateBuffer(vkDevice, &vkBufferCreateInfo, vkAllocationCallbacks, &vkVertexBuffer));
		Logf("Created vertex buffer");

		vkGetBufferMemoryRequirements(vkDevice, vkVertexBuffer, &vkMemoryRequirements);
		vkMemoryAllocateInfo = {
			.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
			.pNext           = 0,
			.allocationSize  = vkMemoryRequirements.size,
			.memoryTypeIndex = VK_MAX_MEMORY_TYPES,
		};
		for (u32 i = 0; i < VK_MAX_MEMORY_TYPES; i++) {
			if (
				(vkMemoryRequirements.memoryTypeBits & (1 << i)) &&
				(physicalDevice->vkPhysicalDeviceMemoryProperties.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
			) {
				vkMemoryAllocateInfo.memoryTypeIndex = i;
				break;
			}
		}
		CheckVk(vkAllocateMemory(vkDevice, &vkMemoryAllocateInfo, vkAllocationCallbacks, &vkVertexBufferMemory));
		Logf("Allocated vertex buffer memory: size={}, memoryTypeIndex={}", vkMemoryAllocateInfo.allocationSize, vkMemoryAllocateInfo.memoryTypeIndex);

		void* ptr;
		CheckVk(vkMapMemory(vkDevice, vkVertexBufferMemory, 0, VK_WHOLE_SIZE, 0, &ptr));
		Vertex* verts = (Vertex*)ptr;
		verts[0] = { -1.0f, -1.0f, 0.0f, 1.0f };
		verts[1] = {  1.0f, -1.0f, 0.0f, 1.0f };
		verts[2] = {  0.0f,  1.0f, 0.0f, 1.0f };
		vkUnmapMemory(vkDevice, vkVertexBufferMemory);
		Logf("Loaded vertices");

		CheckVk(vkBindBufferMemory(vkDevice, vkVertexBuffer, vkVertexBufferMemory, 0));

		Array<u8> vertShaderSpv = ReadFile(tempMem, "Shaders/vert.spv");
		VkShaderModuleCreateInfo vkShaderModuleCreateInfo = {
			.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
			.pNext    = 0,
			.flags    = 0,
			.codeSize = vertShaderSpv.len,
			.pCode    = (const u32*)vertShaderSpv.data,
		};
		CheckVk(vkCreateShaderModule(vkDevice, &vkShaderModuleCreateInfo, vkAllocationCallbacks, &vkVertexShaderModule));
		Logf("Created vertex shader module");

		Array<u8> fragShaderSpv = ReadFile(tempMem, "Shaders/frag.spv");
		vkShaderModuleCreateInfo = {
			.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
			.pNext    = 0,
			.flags    = 0,
			.codeSize = fragShaderSpv.len,
			.pCode    = (const u32*)fragShaderSpv.data,
		};
		CheckVk(vkCreateShaderModule(vkDevice, &vkShaderModuleCreateInfo, vkAllocationCallbacks, &vkFragmentShaderModule));
		Logf("Created fragment shader module");

		vkBufferCreateInfo = {
			.sType                 = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
			.pNext                 = 0,
			.flags                 = 0,
			.size                  = sizeof(float) * 16 * 3,
			.usage                 = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			.sharingMode           = VK_SHARING_MODE_EXCLUSIVE,
			.queueFamilyIndexCount = 0,
			.pQueueFamilyIndices   = 0,
		};
		CheckVk(vkCreateBuffer(vkDevice, &vkBufferCreateInfo, vkAllocationCallbacks, &vkShaderUniformBuffer));
		Logf("Created uniform buffer");

		vkGetBufferMemoryRequirements(vkDevice, vkShaderUniformBuffer, &vkMemoryRequirements);
		vkMemoryAllocateInfo = {
			.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
			.pNext           = 0,
			.allocationSize  = vkMemoryRequirements.size,
			.memoryTypeIndex = VK_MAX_MEMORY_TYPES,
		};
		for (u32 i = 0; i < VK_MAX_MEMORY_TYPES; i++) {
			if (
				(vkMemoryRequirements.memoryTypeBits & (1 << i)) &&
				(physicalDevice->vkPhysicalDeviceMemoryProperties.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
			) {
				vkMemoryAllocateInfo.memoryTypeIndex = i;
				break;
			}
		}
		CheckVk(vkAllocateMemory(vkDevice, &vkMemoryAllocateInfo, vkAllocationCallbacks, &vkShaderUniformBufferMemory));
		CheckVk(vkBindBufferMemory(vkDevice, vkShaderUniformBuffer, vkShaderUniformBufferMemory, 0));
		Logf("Allocated uniform buffer memory: size={}, memoryTypeIndex={}", vkMemoryAllocateInfo.allocationSize, vkMemoryAllocateInfo.memoryTypeIndex);

		VkDescriptorSetLayoutBinding vkDescriptorSetLayoutBinding = {
			.binding            = 0,
			.descriptorType     = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			.descriptorCount    = 1,
			.stageFlags         = VK_SHADER_STAGE_VERTEX_BIT,
			.pImmutableSamplers = 0,
		};
		VkDescriptorSetLayoutCreateInfo vkDescriptorSetLayoutCreateInfo = {
			.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
			.pNext        = 0,
			.flags        = 0,
			.bindingCount = 1,
			.pBindings    = &vkDescriptorSetLayoutBinding,
		};
		CheckVk(vkCreateDescriptorSetLayout(vkDevice, &vkDescriptorSetLayoutCreateInfo, vkAllocationCallbacks, &vkDescriptorSetLayout));
		Logf("Created descriptor set layout");

		VkDescriptorPoolSize vkDescriptorPoolSize = {
			.type            = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			.descriptorCount = 1
		};
		VkDescriptorPoolCreateInfo vkDescriptorPoolCreateInfo = {
			.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
			.pNext         = 0,
			.flags         = 0,
			.maxSets       = 1,
			.poolSizeCount = 1,
			.pPoolSizes    = &vkDescriptorPoolSize,
		};
		CheckVk(vkCreateDescriptorPool(vkDevice, &vkDescriptorPoolCreateInfo, vkAllocationCallbacks, &vkDescriptorPool));
		Logf("Created descriptor pool");

		VkDescriptorSetAllocateInfo vkDescriptorSetAllocateInfo = {
			.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
			.pNext              = 0,
			.descriptorPool     = vkDescriptorPool,
			.descriptorSetCount = 1,
			.pSetLayouts        = &vkDescriptorSetLayout,
		};
		CheckVk(vkAllocateDescriptorSets(vkDevice, &vkDescriptorSetAllocateInfo, &vkDescriptorSet));
		Logf("Created descriptor set");

		VkDescriptorBufferInfo vkDescriptorBufferInfo = {
			.buffer = vkShaderUniformBuffer,
			.offset = 0,
			.range  = VK_WHOLE_SIZE,
		};
		VkWriteDescriptorSet vkWriteDescriptorSet = {
			.sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.pNext            = 0,
			.dstSet           = vkDescriptorSet,
			.dstBinding       = 0,
			.dstArrayElement  = 0,
			.descriptorCount  = 1,
			.descriptorType   = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			.pImageInfo       = 0,
			.pBufferInfo      = &vkDescriptorBufferInfo,
			.pTexelBufferView = 0,
		};
		vkUpdateDescriptorSets(vkDevice, 1, &vkWriteDescriptorSet, 0, 0);

		VkPipelineLayoutCreateInfo vkPipelineLayoutCreateInfo = {
			.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
			.pNext                  = 0,
			.flags                  = 0,
			.setLayoutCount         = 1,
			.pSetLayouts            = &vkDescriptorSetLayout,
			.pushConstantRangeCount = 0,
			.pPushConstantRanges    = 0,
		};
		CheckVk(vkCreatePipelineLayout(vkDevice, &vkPipelineLayoutCreateInfo, vkAllocationCallbacks, &vkPipelineLayout));
		Logf("Created pipeline layout");

		VkPipelineShaderStageCreateInfo vkPipelineShaderStageCreateInfos[2] = {
			{
				.sType               = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
				.pNext               = 0,
				.flags               = 0,
				.stage               = VK_SHADER_STAGE_VERTEX_BIT,
				.module              = vkVertexShaderModule,
				.pName               = "main",
				.pSpecializationInfo = 0,
			},
			{
				.sType               = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
				.pNext               = 0,
				.flags               = 0,
				.stage               = VK_SHADER_STAGE_FRAGMENT_BIT,
				.module              = vkFragmentShaderModule,
				.pName               = "main",
				.pSpecializationInfo = 0,
			},
		};
		VkVertexInputBindingDescription vkVertexInputBindingDescription = {
			.binding   = 0,
			.stride    = sizeof(Vertex),
			.inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
		};
		VkVertexInputAttributeDescription vkVertexInputAttributeDescription = {
			.location = 0,
			.binding  = 0,
			.format   = VK_FORMAT_R32G32B32A32_SFLOAT,
			.offset   = 0,
		};
		VkPipelineVertexInputStateCreateInfo vkPipelineVertexInputStateCreateInfo = {
			.sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
			.pNext                           = 0,
			.flags                           = 0,
			.vertexBindingDescriptionCount   = 1,
			.pVertexBindingDescriptions      = &vkVertexInputBindingDescription,
			.vertexAttributeDescriptionCount = 1,
			.pVertexAttributeDescriptions    = &vkVertexInputAttributeDescription,
		};
		VkPipelineInputAssemblyStateCreateInfo vkPipelineInputAssemblyStateCreateInfo = {
			.sType                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
			.pNext                  = 0,
			.flags                  = 0,
			.topology               = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
			.primitiveRestartEnable = VK_FALSE,
		};
		VkViewport vkViewport = {
			.x        = 0,
			.y        = 0,
			.width    = (float)vkSwapchainExtent.width,
			.height   = (float)vkSwapchainExtent.height,
			.minDepth = 0,
			.maxDepth = 1,
		};
		VkRect2D vkScissorRect = {
			.offset = {
				.x = 0,
				.y = 0,
			},
			.extent = {
				.width  = vkSwapchainExtent.width,
				.height = vkSwapchainExtent.height,
			},
		};
		VkPipelineViewportStateCreateInfo vkPipelineViewportStateCreateInfo = {
			.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
			.pNext         = 0,
			.flags         = 0,
			.viewportCount = 1,
			.pViewports    = &vkViewport,
			.scissorCount  = 1,
			.pScissors     = &vkScissorRect,
		};
		VkPipelineRasterizationStateCreateInfo vkPipelineRasterizationStateCreateInfo = {
			.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
			.pNext                   = 0,
			.flags                   = 0,
			.depthClampEnable        = VK_FALSE,
			.rasterizerDiscardEnable = VK_FALSE,
			.polygonMode             = VK_POLYGON_MODE_FILL,
			.cullMode                = VK_CULL_MODE_NONE,
			.frontFace               = VK_FRONT_FACE_COUNTER_CLOCKWISE,
			.depthBiasEnable         = VK_FALSE,
			.depthBiasConstantFactor = 0.0,
			.depthBiasClamp          = 0.0,
			.depthBiasSlopeFactor    = 0.0,
			.lineWidth               = 1.0,
		};
		VkPipelineMultisampleStateCreateInfo vkPipelineMultisampleStateCreateInfo = {
			.sType                 = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
			.pNext                 = 0,
			.flags                 = 0,
			.rasterizationSamples  = VK_SAMPLE_COUNT_1_BIT,
			.sampleShadingEnable   = VK_FALSE,
			.minSampleShading      = 0.0f,
			.pSampleMask           = 0,
			.alphaToCoverageEnable = VK_FALSE,
			.alphaToOneEnable      = VK_FALSE,
		};
		VkStencilOpState vkStencilOpState = {
			.failOp      = VK_STENCIL_OP_KEEP,
			.passOp      = VK_STENCIL_OP_KEEP,
			.depthFailOp = VK_STENCIL_OP_KEEP,
			.compareOp   = VK_COMPARE_OP_ALWAYS,
			.compareMask = 0,
			.writeMask   = 0,
			.reference   = 0,
		};
		VkPipelineDepthStencilStateCreateInfo vkPipelineDepthStencilStateCreateInfo = {
			.sType                 = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
			.pNext                 = 0,
			.flags                 = 0,
			.depthTestEnable       = VK_TRUE,
			.depthWriteEnable      = VK_TRUE,
			.depthCompareOp        = VK_COMPARE_OP_LESS_OR_EQUAL,
			.depthBoundsTestEnable = VK_FALSE,
			.stencilTestEnable     = VK_FALSE,
			.front                 = vkStencilOpState,
			.back                  = vkStencilOpState,
			.minDepthBounds        = 0.0f,
			.maxDepthBounds        = 0.0f,
		};
		VkPipelineColorBlendAttachmentState vkPipelineColorBlendAttachmentState = {
			.blendEnable         = VK_FALSE,
			.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_COLOR,
			.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR,
			.colorBlendOp        = VK_BLEND_OP_ADD,
			.srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
			.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
			.alphaBlendOp        = VK_BLEND_OP_ADD,
			.colorWriteMask      = 0xf,
		};
		VkPipelineColorBlendStateCreateInfo vkPipelineColorBlendStateCreateInfo = {
			.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
			.pNext           = 0,
			.flags           = 0,
			.logicOpEnable   = VK_FALSE,
			.logicOp         = VK_LOGIC_OP_CLEAR,
			.attachmentCount = 1,
			.pAttachments    = &vkPipelineColorBlendAttachmentState,
			.blendConstants  = { 0.0f, 0.0f, 0.0f, 0.0f },
		};
		VkDynamicState vkDynamicStates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
		VkPipelineDynamicStateCreateInfo vkPipelineDynamicStateCreateInfo = {
			.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
			.pNext             = 0,
			.flags             = 0,
			.dynamicStateCount = 2,
			.pDynamicStates    = vkDynamicStates,
		};
		VkGraphicsPipelineCreateInfo vkGraphicsPipelineCreateInfo = {
			.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
			.pNext               = 0,
			.flags               = 0,
			.stageCount          = 2,
			.pStages             = vkPipelineShaderStageCreateInfos,
			.pVertexInputState   = &vkPipelineVertexInputStateCreateInfo,
			.pInputAssemblyState = &vkPipelineInputAssemblyStateCreateInfo,
			.pTessellationState  = 0,
			.pViewportState      = &vkPipelineViewportStateCreateInfo,
			.pRasterizationState = &vkPipelineRasterizationStateCreateInfo,
			.pMultisampleState   = &vkPipelineMultisampleStateCreateInfo,
			.pDepthStencilState  = &vkPipelineDepthStencilStateCreateInfo,
			.pColorBlendState    = &vkPipelineColorBlendStateCreateInfo,
			.pDynamicState       = &vkPipelineDynamicStateCreateInfo,
			.layout              = vkPipelineLayout,
			.renderPass          = vkRenderPass,
			.subpass             = 0,
			.basePipelineHandle  = 0,
			.basePipelineIndex   = 0,
		};
		CheckVk(vkCreateGraphicsPipelines(vkDevice, 0, 1, &vkGraphicsPipelineCreateInfo, vkAllocationCallbacks, &vkPipeline));
		Logf("Created graphics pipeline");


		return Ok();
	}

	//----------------------------------------------------------------------------------------------

	#define DestroyVkHandle(handle, DestroyFnInvocation) \
		if (handle != VK_NULL_HANDLE) { \
			DestroyFnInvocation; \
			handle = VK_NULL_HANDLE; \
		}

	void Shutdown() override {
		// Don't need to free because wasn't created with free bit
		//DestroyVkHandle(vkDescriptorSet, vkFreeDescriptorSets(vkDevice, vkDescriptorPool, 1, &vkDescriptorSet));
		DestroyVkHandle(vkDescriptorPool, vkDestroyDescriptorPool(vkDevice, vkDescriptorPool, vkAllocationCallbacks));
		DestroyVkHandle(vkDescriptorSetLayout, vkDestroyDescriptorSetLayout(vkDevice, vkDescriptorSetLayout, vkAllocationCallbacks));
		DestroyVkHandle(vkShaderUniformBufferMemory, vkFreeMemory(vkDevice, vkShaderUniformBufferMemory, vkAllocationCallbacks));
		DestroyVkHandle(vkShaderUniformBuffer, vkDestroyBuffer(vkDevice, vkShaderUniformBuffer, vkAllocationCallbacks));
		DestroyVkHandle(vkPipeline, vkDestroyPipeline(vkDevice, vkPipeline, vkAllocationCallbacks));
		DestroyVkHandle(vkPipelineLayout, vkDestroyPipelineLayout(vkDevice, vkPipelineLayout, vkAllocationCallbacks));
		DestroyVkHandle(vkFragmentShaderModule, vkDestroyShaderModule(vkDevice, vkFragmentShaderModule, vkAllocationCallbacks));
		DestroyVkHandle(vkVertexShaderModule, vkDestroyShaderModule(vkDevice, vkVertexShaderModule, vkAllocationCallbacks));
		DestroyVkHandle(vkVertexBufferMemory, vkFreeMemory(vkDevice, vkVertexBufferMemory, vkAllocationCallbacks));
		DestroyVkHandle(vkVertexBuffer, vkDestroyBuffer(vkDevice, vkVertexBuffer, vkAllocationCallbacks));
		for (u64 i = 0; i < vkFramebuffers.len; i++) {
			vkDestroyFramebuffer(vkDevice, vkFramebuffers[i], vkAllocationCallbacks);
		}
		vkFramebuffers.Free();
		DestroyVkHandle(vkRenderPass, vkDestroyRenderPass(vkDevice, vkRenderPass, vkAllocationCallbacks));
		DestroyVkHandle(vkDepthImageView, vkDestroyImageView(vkDevice, vkDepthImageView, vkAllocationCallbacks));
		DestroyVkHandle(vkDepthImageMemory, vkFreeMemory(vkDevice, vkDepthImageMemory, vkAllocationCallbacks));
		DestroyVkHandle(vkDepthImage, vkDestroyImage(vkDevice, vkDepthImage, vkAllocationCallbacks));
		for (u64 i = 0; i < vkSwapchainImageViews.len; i++) {
			vkDestroyImageView(vkDevice, vkSwapchainImageViews[i], vkAllocationCallbacks);
		}
		vkSwapchainImageViews.Free();
		vkSwapchainImages.Free();
		DestroyVkHandle(vkFence,               vkDestroyFence(vkDevice, vkFence, vkAllocationCallbacks));
		DestroyVkHandle(vkRenderCommandBuffer, vkFreeCommandBuffers(vkDevice, vkCommandPool, 1, &vkRenderCommandBuffer));
		DestroyVkHandle(vkSetupCommandBuffer,  vkFreeCommandBuffers(vkDevice, vkCommandPool, 1, &vkSetupCommandBuffer));
		DestroyVkHandle(vkCommandPool,         vkDestroyCommandPool(vkDevice, vkCommandPool, vkAllocationCallbacks));
		DestroyVkHandle(vkSwapchain,           vkDestroySwapchainKHR(vkDevice, vkSwapchain, vkAllocationCallbacks));
		DestroyVkHandle(vkDevice,              vkDestroyDevice(vkDevice , vkAllocationCallbacks));
		physicalDevices.Free();
		DestroyVkHandle(vkSurface,             vkDestroySurfaceKHR(vkInstance, vkSurface, vkAllocationCallbacks));
		DestroyVkHandle(vkDebugUtilsMessenger, vkDestroyDebugUtilsMessengerEXT(vkInstance, vkDebugUtilsMessenger, vkAllocationCallbacks));
		DestroyVkHandle(vkInstance,            vkDestroyInstance(vkInstance, vkAllocationCallbacks));
	}

	//----------------------------------------------------------------------------------------------

	Res<> Frame() override {
		VkResult vkRes = VK_SUCCESS;

		VkSemaphoreCreateInfo vkSemaphoreCreateInfo = {
			.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
			.pNext = 0,
			.flags = 0,
		};
		VkSemaphore vkPresentCompleteSemaphore = VK_NULL_HANDLE;
		CheckVk(vkCreateSemaphore(vkDevice, &vkSemaphoreCreateInfo, vkAllocationCallbacks, &vkPresentCompleteSemaphore));
		VkSemaphore vkRenderCompleteSemaphore = VK_NULL_HANDLE;
		CheckVk(vkCreateSemaphore(vkDevice, &vkSemaphoreCreateInfo, vkAllocationCallbacks, &vkRenderCompleteSemaphore));

		u32 imageIndex = 0;
		CheckVk(vkAcquireNextImageKHR(vkDevice, vkSwapchain, UINT64_MAX, vkPresentCompleteSemaphore, 0, &imageIndex));

		constexpr float Pi = 3.14159265359f;
		constexpr float RadsPerDeg = Pi / 180.0f;
		float fov = 45.0f;
		float nearZ = 0.1f;
		float farZ = 1000.0f;
		float aspect = (float)vkSwapchainExtent.width / (float)vkSwapchainExtent.height;
		float t = 1.0f / tanf(fov * RadsPerDeg * 0.5f);
		float nf = nearZ - farZ;
		float projMatrix[16] = {
			t / aspect, 0.0f, 0.0f,                 0.0f,
			0.0f,       t,    0.0f,                 0.0f,
			0.0f,       0.0f, (-nearZ - farZ) / nf, (2 * nearZ * farZ) / nf,
			0.0f,       0.0f, 1.0f,                 0.0f,
		};
		float viewMatrix[16] = {
			1.0f, 0.0f, 0.0f, 0.0f,
			0.0f, 1.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			0.0f, 0.0f, 0.0f, 1.0f,
		};
		float modelMatrix[16] = {
			1.0f, 0.0f, 0.0f, 0.0f,
			0.0f, 1.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			0.0f, 0.0f, 0.0f, 1.0f,
		};
		if (cameraZ <= 1) {
			cameraZ = 1;
			cameraZDir = 1;
		} else if (cameraZ >= 10) {
			cameraZ = 10;
			cameraZDir = -1;
		}
		cameraZ += cameraZDir * 0.01f;
		viewMatrix[11] = cameraZ;

		void* ptr = 0;
		CheckVk(vkMapMemory(vkDevice, vkShaderUniformBufferMemory, 0, VK_WHOLE_SIZE, 0, &ptr));
		MemCpy(ptr, projMatrix, sizeof(projMatrix));
		ptr = (u8*)ptr + sizeof(projMatrix);
		MemCpy(ptr, viewMatrix, sizeof(viewMatrix));
		ptr = (u8*)ptr + sizeof(viewMatrix);
		MemCpy(ptr, modelMatrix, sizeof(modelMatrix));
		VkMappedMemoryRange vkMappedMemoryRange = {
			.sType  = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
			.pNext  = 0,
			.memory = vkShaderUniformBufferMemory,
			.offset = 0,
			.size   = VK_WHOLE_SIZE,
		};
		CheckVk(vkFlushMappedMemoryRanges(vkDevice, 1, &vkMappedMemoryRange));
		vkUnmapMemory(vkDevice, vkShaderUniformBufferMemory);


		VkMemoryBarrier vkMemoryBarrier = {
			.sType         = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
			.pNext         = 0,
			.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT,
			.dstAccessMask = VK_ACCESS_UNIFORM_READ_BIT,
		};
		vkCmdPipelineBarrier(
			vkRenderCommandBuffer,
			VK_PIPELINE_STAGE_HOST_BIT,
			VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
			0,
			1, &vkMemoryBarrier,
			0, 0,
			0, 0
		);

		VkCommandBufferBeginInfo vkCommandBufferBeginInfo = {
			.sType            = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
			.pNext            = 0,
			.flags            = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
			.pInheritanceInfo = 0,
		};
		CheckVk(vkBeginCommandBuffer(vkRenderCommandBuffer, &vkCommandBufferBeginInfo));

		VkImageMemoryBarrier vkImageMemoryBarrier = {
			.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
			.pNext               = 0,
			.srcAccessMask       = VK_ACCESS_MEMORY_READ_BIT,
			.dstAccessMask       = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT  | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			.oldLayout           = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
			.newLayout           = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.image               = vkSwapchainImages[imageIndex],
			.subresourceRange    = {
				.aspectMask      = VK_IMAGE_ASPECT_COLOR_BIT,
				.baseMipLevel    = 0,
				.levelCount      = 1,
				.baseArrayLayer  = 0,
				.layerCount      = 1,
			},
		};
		vkCmdPipelineBarrier(
			vkRenderCommandBuffer,
			VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
			VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
			0,
			0, 0,
			0, 0,
			1, &vkImageMemoryBarrier
		);

		VkClearValue vkClearValues[2] = { { 1.0f, 1.0f, 1.0f, 1.0f }, { 1.0f, 1.0f } };
		VkRenderPassBeginInfo vkRenderPassBeginInfo = {
			.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
			.pNext           = 0,
			.renderPass      = vkRenderPass,
			.framebuffer     = vkFramebuffers[imageIndex],
			.renderArea      = { 0, 0, vkSwapchainExtent.width, vkSwapchainExtent.height },
			.clearValueCount = 2,
			.pClearValues    = vkClearValues,
		};
		vkCmdBeginRenderPass(vkRenderCommandBuffer, &vkRenderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

		vkCmdBindPipeline(vkRenderCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vkPipeline);

		VkViewport vkViewport = { 0.0f, 0.0f, (float)vkSwapchainExtent.width, (float)vkSwapchainExtent.height, 0.0f, 1.0f };
		vkCmdSetViewport(vkRenderCommandBuffer, 0, 1, &vkViewport);

		VkRect2D vkScissorRect = { 0, 0, vkSwapchainExtent.width, vkSwapchainExtent.height };
		vkCmdSetScissor(vkRenderCommandBuffer, 0, 1, &vkScissorRect);

		VkDeviceSize offset = 0;
		vkCmdBindVertexBuffers(vkRenderCommandBuffer, 0, 1, &vkVertexBuffer, &offset);

		vkCmdBindDescriptorSets(
			vkRenderCommandBuffer,
			VK_PIPELINE_BIND_POINT_GRAPHICS,
			vkPipelineLayout,
			0, 1,
			&vkDescriptorSet,
			0, 0
		);

		vkCmdDraw(vkRenderCommandBuffer, 3, 1, 0, 0);

		vkCmdEndRenderPass(vkRenderCommandBuffer);

		vkImageMemoryBarrier = {
			.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
			.pNext               = 0,
			.srcAccessMask       = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			.dstAccessMask       = VK_ACCESS_MEMORY_READ_BIT,
			.oldLayout           = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			.newLayout           = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
			.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.image               = vkSwapchainImages[imageIndex],
			.subresourceRange    = {
				.aspectMask      = VK_IMAGE_ASPECT_COLOR_BIT,
				.baseMipLevel    = 0,
				.levelCount      = 1,
				.baseArrayLayer  = 0,
				.layerCount      = 1,
			},
		};
		vkCmdPipelineBarrier(
			vkRenderCommandBuffer,
			VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
			VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
			0,
			0, 0,
			0, 0,
			1, &vkImageMemoryBarrier
		);

		CheckVk(vkEndCommandBuffer(vkRenderCommandBuffer));

		VkFence vkRenderFence = VK_NULL_HANDLE;
		VkFenceCreateInfo vkFenceCreateInfo = {
			.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
			.pNext = 0,
			.flags = 0,
		};
		CheckVk(vkCreateFence(vkDevice, &vkFenceCreateInfo, vkAllocationCallbacks, &vkRenderFence));

		VkPipelineStageFlags vkWaitDstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
		VkSubmitInfo vkSubmitInfo = {
			.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO,
			.pNext                = 0,
			.waitSemaphoreCount   = 1,
			.pWaitSemaphores      = &vkPresentCompleteSemaphore,
			.pWaitDstStageMask    = &vkWaitDstStageMask,
			.commandBufferCount   = 1,
			.pCommandBuffers      = &vkRenderCommandBuffer,
			.signalSemaphoreCount = 1,
			.pSignalSemaphores    = &vkRenderCompleteSemaphore,
		};
		CheckVk(vkQueueSubmit(vkQueue, 1, &vkSubmitInfo, vkRenderFence));

		CheckVk(vkWaitForFences(vkDevice, 1, &vkRenderFence, VK_TRUE, UINT64_MAX));
		vkDestroyFence(vkDevice, vkRenderFence, vkAllocationCallbacks);

		VkPresentInfoKHR vkPresentInfo = {
			.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
			.pNext              = 0,
			.waitSemaphoreCount = 0,
			.pWaitSemaphores    = 0,
			.swapchainCount     = 1,
			.pSwapchains        = &vkSwapchain,
			.pImageIndices      = &imageIndex,
			.pResults           = 0,
		};
		CheckVk(vkQueuePresentKHR(vkQueue, &vkPresentInfo));

		vkDestroySemaphore(vkDevice, vkPresentCompleteSemaphore, vkAllocationCallbacks);
		vkDestroySemaphore(vkDevice, vkRenderCompleteSemaphore, vkAllocationCallbacks);

		return Ok();
	}
};

//--------------------------------------------------------------------------------------------------

static RenderApiObj renderApi;

RenderApi* RenderApi::Get() {
	return &renderApi;
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC