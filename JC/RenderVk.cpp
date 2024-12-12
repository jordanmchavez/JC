#pragma once

#include "JC/Render.h"

#include "JC/Array.h"
#include "JC/Bit.h"
#include "JC/File.h"
#include "JC/Fmt.h"
#include "JC/Log.h"
#include "JC/Mem.h"
#include "JC/RenderVk.h"
#include "JC/Window.h"

#define Render_Debug

namespace JC {

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
		VkPhysicalDeviceVulkan12Features vkPhysicalDeviceVulkan12Features = {};
		VkPhysicalDeviceVulkan13Features vkPhysicalDeviceVulkan13Features = {};
		VkPhysicalDeviceMemoryProperties vkPhysicalDeviceMemoryProperties = {};
		VkSurfaceCapabilitiesKHR         vkSurfaceCapabilities            = {};
		Array<VkExtensionProperties>     vkExtensionProperties            = {};
		Array<VkSurfaceFormatKHR>        vkSurfaceFormats                 = {};
		VkFormat                         vkSurfaceFormat                  = VK_FORMAT_UNDEFINED;
		VkColorSpaceKHR                  vkSurfaceColorSpace              = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
		Array<VkPresentModeKHR>          vkPresentModes                   = {};
		Array<QueueFamily>               queueFamilies                    = {};
		u32                              queueFamily                      = VK_QUEUE_FAMILY_IGNORED;
		u32                              score                            = 0;
	};

	struct Vec2 { float x, y; };
	struct Vec3 { float x, y, z; };
	struct Vec4 { float x, y, z, w; };

	struct ComputePushConstants {
		Vec3 bottomColor;
		Vec3 topColor;
	};

	struct MeshPushConstants {
		u32 bufferIndex   = 0;
		u32 textureIndex = 0;
	};

	struct Vertex {
		Vec3 position;
		Vec2 texCoords;
	};

	static constexpr u32 MaxFrames = 2;
	static constexpr u32 MaxDescriptorSets = 16;
	static constexpr u32 MaxDescriptors    = 64 * 1024;

	FileApi*                 fileApi                      = 0;
	Log*                     log                          = 0;
	Mem*                     mem                          = 0;
	TempMem*                 tempMem                      = 0;
	VkAllocationCallbacks*   vkAllocationCallbacks        = 0;
	VkInstance               vkInstance                   = VK_NULL_HANDLE;
	VkDebugUtilsMessengerEXT vkDebugUtilsMessenger        = VK_NULL_HANDLE;
	VkSurfaceKHR             vkSurface                    = VK_NULL_HANDLE;
	Array<PhysicalDevice>    physicalDevices              = {};
	PhysicalDevice*          physicalDevice               = 0;
	VkDevice                 vkDevice                     = VK_NULL_HANDLE;
	VkQueue                  vkQueue                      = VK_NULL_HANDLE;
	VkSwapchainKHR           vkSwapchain                  = VK_NULL_HANDLE;
	VkExtent2D               vkSwapchainExtent2D          = {};
	Array<VkImage>           vkSwapchainImages            = {};
	Array<VkImageView>       vkSwapchainImageViews        = {};
	VkCommandPool            vkCommandPool                = VK_NULL_HANDLE;
	Array<VkCommandBuffer>   vkCommandBuffers             = {};
	Array<VkSemaphore>       vkAcquireImageSemaphores     = {};
	Array<VkSemaphore>       vkRenderSemaphores           = {};
	Array<VkFence>           vkRenderFences               = {};
	VkImage                  vkDrawImage                  = VK_NULL_HANDLE;
	VkImageView              vkDrawImageView              = VK_NULL_HANDLE;
	VkDeviceMemory           vkDrawImageDeviceMemory      = VK_NULL_HANDLE;
	VkExtent2D               vkDrawImageExtent2D          = {};
	VkDescriptorPool         vkDescriptorPool             = VK_NULL_HANDLE;
	VkDescriptorSetLayout    vkComputeDescriptorSetLayout = VK_NULL_HANDLE;
	VkDescriptorSetLayout    vkMeshDescriptorSetLayout    = VK_NULL_HANDLE;
	VkSampler                vkSampler                    = VK_NULL_HANDLE;


	VkDescriptorSet          vkComputeDescriptorSet       = VK_NULL_HANDLE;
	VkDescriptorSet          vkMeshDescriptorSet          = VK_NULL_HANDLE;
	VkShaderModule           vkComputeShaderModule        = VK_NULL_HANDLE;
	VkShaderModule           vkMeshVertShaderModule       = VK_NULL_HANDLE;
	VkShaderModule           vkMeshFragShaderModule       = VK_NULL_HANDLE;
	VkPipelineLayout         vkComputePipelineLayout      = VK_NULL_HANDLE;
	VkPipeline               vkComputePipeline            = VK_NULL_HANDLE;
	VkPipelineLayout         vkGraphicsPipelineLayout     = VK_NULL_HANDLE;
	VkPipeline               vkGraphicsPipeline           = VK_NULL_HANDLE;
	u64                      frameNumber                  = 0;
	
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

	Res<> CreateInstance() {
		u32 instanceVersion = 0;
		CheckVk(vkEnumerateInstanceVersion(&instanceVersion));
		if (instanceVersion < VK_API_VERSION_1_3) {
			return MakeErr(Err_Version, "ver", instanceVersion);
		}

		u32 n = 0;
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

		constexpr const char* RequiredLayers[] = {
			#if defined Render_Debug
				"VK_LAYER_KHRONOS_validation",
			#endif	// Render_Debug
		};
		for (u32 i = 0; i < LenOf(RequiredLayers); i++) {
			bool found = false;
			for (u64 j = 0; j < layers.len; j++) {
				if (!StrCmp(RequiredLayers[i], layers[j].layerName)) {
					Logf("Found required layer '{}'", RequiredLayers[i]);
					found = true;
					break;
				}
			}
			if (!found) {
				return MakeErr(Err_NoLayer, "name", RequiredLayers[i]);
			}
		}
	
		Array<VkExtensionProperties> instExts(tempMem);
		CheckVk(vkEnumerateInstanceExtensionProperties(0, &n, 0));
		CheckVk(vkEnumerateInstanceExtensionProperties(0, &n, instExts.Resize(n)));

		Logf("{} instance extensions:", instExts.len);
		for (u64 i = 0; i < instExts.len; i++) {
			Logf("  {}: specVersion={}", instExts[i].extensionName, Vk::VersionStr(instExts[i].specVersion));
		}

		constexpr const char* RequiredInstExts[] = {
			"VK_KHR_surface",
			#if defined Platform_Windows
				"VK_KHR_win32_surface",
			#endif	//	Platform_
			#if defined Render_Debug
				"VK_EXT_debug_utils"
			#endif	// Render_Debug
		};
		for (u32 i = 0; i < LenOf(RequiredInstExts); i++) {
			bool found = false;
			for (u64 j = 0; j < instExts.len; j++) {
				if (!StrCmp(RequiredInstExts[i], instExts[j].extensionName)) {
					Logf("Found required instance extension '{}'", RequiredInstExts[i]);
					found = true;
					break;
				}
			}
			if (!found) {
				return MakeErr(Err_NoLayer, "name", RequiredInstExts[i]);
			}
		}
		
		const VkApplicationInfo vkApplicationInfo = {
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
			.enabledLayerCount       = LenOf(RequiredLayers),
			.ppEnabledLayerNames     = RequiredLayers,
			.enabledExtensionCount   = LenOf(RequiredInstExts),
			.ppEnabledExtensionNames = RequiredInstExts,
		};
		#if defined Render_Debug
			const VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo = {
				.sType           = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
				.pNext           = nullptr,
				.flags           = 0,
				.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
				.messageType     = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_DEVICE_ADDRESS_BINDING_BIT_EXT,
				.pfnUserCallback = RenderApiObj::DebugCallback,
				.pUserData       = log,
			};
			vkInstanceCreateInfo.pNext = &debugCreateInfo;
		#endif	// Render_Debug

		CheckVk(vkCreateInstance(&vkInstanceCreateInfo, vkAllocationCallbacks, &vkInstance));
		Vk::LoadInstanceFns(vkInstance);

		#if defined Render_Debug
			CheckVk(vkCreateDebugUtilsMessengerEXT(vkInstance, &debugCreateInfo, vkAllocationCallbacks, &vkDebugUtilsMessenger));
		#endif	// Render_Debug

		return Ok();
	}

	//-------------------------------------------------------------------------------------------------

	Res<> CreateSurface(WindowPlatformData* windowPlatformData) {
		#if defined Platform_Windows
			VkWin32SurfaceCreateInfoKHR win32SurfaceCreateInfo = {
				.sType     = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
				.pNext     = 0,
				.flags     = 0,
				.hinstance = (HINSTANCE)windowPlatformData->hinstance,
				.hwnd      = (HWND)windowPlatformData->hwnd,
			};
			CheckVk(vkCreateWin32SurfaceKHR(vkInstance, &win32SurfaceCreateInfo, vkAllocationCallbacks, &vkSurface));
		#endif	// Platform_

		return Ok();
	}

	//-------------------------------------------------------------------------------------------------

	Res<> CreateDevice() {
		constexpr const char* RequiredDeviceExts[] = {
			"VK_KHR_swapchain",
		};

		u32 n = 0;
		Array<VkPhysicalDevice> vkPhysicalDevices(tempMem);
		CheckVk(vkEnumeratePhysicalDevices(vkInstance, &n, nullptr));
		CheckVk(vkEnumeratePhysicalDevices(vkInstance, &n, vkPhysicalDevices.Resize(n)));

		Logf("{} physical devices:", physicalDevices.len);
		physicalDevices.Init(mem);
		physicalDevices.Resize(n);
		u32 bestScore = 0;
		for (u64 i = 0; i < physicalDevices.len; i++) {
			PhysicalDevice* const pd = &physicalDevices[i];

			pd->vkPhysicalDevice = vkPhysicalDevices[i];
			pd->score = 0;

			vkGetPhysicalDeviceProperties(pd->vkPhysicalDevice, &pd->vkPhysicalDeviceProperties);
			Logf(
				"{}: apiVersion={}, driverVersion={}, vendorID={}, deviceId={}, deviceType={}",
				pd->vkPhysicalDeviceProperties.deviceName,
				Vk::VersionStr(pd->vkPhysicalDeviceProperties.apiVersion),
				Vk::VersionStr(pd->vkPhysicalDeviceProperties.driverVersion),
				pd->vkPhysicalDeviceProperties.vendorID,
				pd->vkPhysicalDeviceProperties.deviceID,
				Vk::PhysicalDeviceTypeStr(pd->vkPhysicalDeviceProperties.deviceType)
			);
			switch (pd->vkPhysicalDeviceProperties.deviceType) {
				case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:   pd->score += 1000; break;
				case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: pd->score +=  100; break;
				case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:    pd->score +=   10; break;
				case VK_PHYSICAL_DEVICE_TYPE_CPU:            pd->score +=    1; break;
				case VK_PHYSICAL_DEVICE_TYPE_OTHER:          pd->score +=    1; break;
			};
			if (pd->vkPhysicalDeviceProperties.apiVersion < VK_API_VERSION_1_3) {
				Logf("  Rejecting device: need Vulkan 1.3: apiVersion={}", Vk::VersionStr(pd->vkPhysicalDeviceProperties.apiVersion));
				pd->score = 0;
			}

			VkPhysicalDeviceFeatures2 vkPhysicalDeviceFeatures2 = {};
			vkPhysicalDeviceFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
			vkPhysicalDeviceFeatures2.pNext = &pd->vkPhysicalDeviceVulkan12Features;
			pd->vkPhysicalDeviceVulkan12Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
			pd->vkPhysicalDeviceVulkan12Features.pNext = &pd->vkPhysicalDeviceVulkan13Features;
			pd->vkPhysicalDeviceVulkan13Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
			pd->vkPhysicalDeviceVulkan13Features.pNext = 0;
			vkGetPhysicalDeviceFeatures2(pd->vkPhysicalDevice, &vkPhysicalDeviceFeatures2);
			pd->vkPhysicalDeviceFeatures = vkPhysicalDeviceFeatures2.features;
			if (!pd->vkPhysicalDeviceVulkan12Features.bufferDeviceAddress) {
				Logf("  Rejecting device: doesn't support bufferDeviceAddress");
				pd->score = 0;
			}
			if (!pd->vkPhysicalDeviceVulkan12Features.descriptorIndexing) {
				Logf("  Rejecting device: doesn't support descriptorIndexing");
				pd->score = 0;
			}
			if (!pd->vkPhysicalDeviceVulkan13Features.dynamicRendering) {
				Logf("  Rejecting device: doesn't support dynamicRendering");
				pd->score = 0;
			}
			if (!pd->vkPhysicalDeviceVulkan13Features.synchronization2) {
				Logf("  Rejecting device: doesn't support synchronization2");
				pd->score = 0;
			}

			vkGetPhysicalDeviceMemoryProperties(pd->vkPhysicalDevice, &pd->vkPhysicalDeviceMemoryProperties);
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

			CheckVk(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(pd->vkPhysicalDevice, vkSurface, &pd->vkSurfaceCapabilities));

			Array<VkQueueFamilyProperties> vkQueueFamilyProperties(tempMem);
			vkGetPhysicalDeviceQueueFamilyProperties(pd->vkPhysicalDevice, &n, nullptr);
			vkGetPhysicalDeviceQueueFamilyProperties(pd->vkPhysicalDevice, &n, vkQueueFamilyProperties.Resize(n));

			pd->vkExtensionProperties.Init(mem);
			CheckVk(vkEnumerateDeviceExtensionProperties(pd->vkPhysicalDevice, 0, &n, 0));
			CheckVk(vkEnumerateDeviceExtensionProperties(pd->vkPhysicalDevice, 0, &n, pd->vkExtensionProperties.Resize(n)));

			Array<char> extensionsStr(tempMem);
			for (u64 j = 0; j < pd->vkExtensionProperties.len; j++) {
				Fmt(&extensionsStr, "{}(specVersion={}), ", pd->vkExtensionProperties[j].extensionName, Vk::VersionStr(pd->vkExtensionProperties[j].specVersion));
			}
			if (extensionsStr.len >= 2) {
				extensionsStr.len -= 2;
			}
			Logf("  {} device extensions: {}",  pd->vkExtensionProperties.len, s8(extensionsStr.data, extensionsStr.len));
			for (u64 j = 0; j < LenOf(RequiredDeviceExts); j++) {
				bool found = false;
				for (u64 k = 0; k < pd->vkExtensionProperties.len; k++) {
					if (!StrCmp(RequiredDeviceExts[j], pd->vkExtensionProperties[k].extensionName)) {
						found = true;
						break;
					}
				}
				if (!found) {
					Logf("Rejecting device: doesn't support extension '{}'", RequiredDeviceExts[j]);
					pd->score = 0;
				}
			}

			pd->vkSurfaceFormats.Init(mem);
			CheckVk(vkGetPhysicalDeviceSurfaceFormatsKHR(pd->vkPhysicalDevice, vkSurface, &n, 0));
			CheckVk(vkGetPhysicalDeviceSurfaceFormatsKHR(pd->vkPhysicalDevice, vkSurface, &n, pd->vkSurfaceFormats.Resize(n)));
			for (u64 j = 0; j < pd->vkSurfaceFormats.len; j++) {
				if (
					(
						pd->vkSurfaceFormats[j].format == VK_FORMAT_B8G8R8A8_UNORM ||
						pd->vkSurfaceFormats[j].format == VK_FORMAT_R8G8B8A8_UNORM ||
						pd->vkSurfaceFormats[j].format == VK_FORMAT_B8G8R8_UNORM ||
						pd->vkSurfaceFormats[j].format == VK_FORMAT_R8G8B8_UNORM
					) &&
					pd->vkSurfaceFormats[j].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR
				) {
					pd->vkSurfaceFormat     = pd->vkSurfaceFormats[j].format;
					pd->vkSurfaceColorSpace = pd->vkSurfaceFormats[j].colorSpace;
					goto FoundSurfaceFormat;
				}
			}
			FoundSurfaceFormat:
			Logf("  {} surface formats:", pd->vkSurfaceFormats.len);
			for (u64 j = 0; j < pd->vkSurfaceFormats.len; j++) {
				Logf("    {}, {}", Vk::FormatStr(pd->vkSurfaceFormats[j].format), Vk::ColorSpaceStr(pd->vkSurfaceFormats[j].colorSpace));
			}
			Logf("  Selected surface format: {}/{}", Vk::FormatStr(pd->vkSurfaceFormat), Vk::ColorSpaceStr(pd->vkSurfaceColorSpace));

			pd->vkPresentModes.Init(mem);
			CheckVk(vkGetPhysicalDeviceSurfacePresentModesKHR(pd->vkPhysicalDevice, vkSurface, &n, 0));
			CheckVk(vkGetPhysicalDeviceSurfacePresentModesKHR(pd->vkPhysicalDevice, vkSurface, &n, pd->vkPresentModes.Resize(n)));
			Logf("  {} present modes:", pd->vkPresentModes.len);
			for (u64 j = 0; j < pd->vkPresentModes.len; j++) {
				Logf("    {}", Vk::PresentModeStr(pd->vkPresentModes[j]));
			}

			pd->queueFamilies.Init(mem);
			pd->queueFamilies.Resize(vkQueueFamilyProperties.len);
			Logf("  {} queue families:", pd->queueFamilies.len);
			for (u64 j = 0; j < pd->queueFamilies.len; j++) {
				const VkQueueFamilyProperties* props = &pd->queueFamilies[j].vkQueueFamilyProperties;
				VkBool32 supportsPresent = VK_FALSE;
				CheckVk(vkGetPhysicalDeviceSurfaceSupportKHR(pd->vkPhysicalDevice, (u32)j, vkSurface, &supportsPresent));
				pd->queueFamilies[j].vkQueueFamilyProperties = vkQueueFamilyProperties[j];
				pd->queueFamilies[j].supportsPresent         = (supportsPresent == VK_TRUE);
				Logf("    [{}] count={}, flags={}, supportsPresent={}", j, props->queueCount, Vk::QueueFlagsStr(props->queueFlags), pd->queueFamilies[j].supportsPresent);
				const VkQueueFlags flags = pd->queueFamilies[j].vkQueueFamilyProperties.queueFlags;
				if (pd->queueFamily == VK_QUEUE_FAMILY_IGNORED && (flags & VK_QUEUE_GRAPHICS_BIT) && pd->queueFamilies[j].supportsPresent) {
					pd->queueFamily = (u32)j;
				}
			}

			if (pd->vkSurfaceFormat == VK_FORMAT_UNDEFINED) {
				Logf("  Rejecting device: no suitable surface format");
				pd->score = 0;
			}
			if (pd->vkPresentModes.len == 0) {
				Logf("  Rejecting device: no present modes");
				pd->score = 0;
			}
			if (pd->queueFamily == VK_QUEUE_FAMILY_IGNORED) {
				Logf("  Rejecting device: no graphics queue supporting present");
				pd->score = 0;
			}

			Logf("  score: {}", pd->score);
			if (pd->score > bestScore) {
				physicalDevice = pd;
				bestScore = pd->score;
			}
		}

		if (bestScore == 0) {
			return MakeErr(Err_NoDevice);
		}
		Logf("Selected physical device '{}' with score={}", physicalDevice->vkPhysicalDeviceProperties.deviceName, physicalDevice->score);

		float queuePriority = 1.0f;
		const VkDeviceQueueCreateInfo vkDeviceQueueCreateInfo = {
			.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
			.pNext            = 0,
			.flags            = 0,
			.queueFamilyIndex = physicalDevice->queueFamily,
			.queueCount       = 1,
			.pQueuePriorities = &queuePriority,
		};

		VkPhysicalDeviceFeatures2 vkPhysicalDeviceFeatures2 = {};
		VkPhysicalDeviceVulkan12Features vkPhysicalDeviceVulkan12Features = {};
		VkPhysicalDeviceVulkan13Features vkPhysicalDeviceVulkan13Features = {};

		vkPhysicalDeviceFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
		vkPhysicalDeviceFeatures2.pNext = &vkPhysicalDeviceVulkan12Features;
		vkPhysicalDeviceFeatures2.features.shaderClipDistance = VK_TRUE;

		vkPhysicalDeviceVulkan12Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
		vkPhysicalDeviceVulkan12Features.pNext = &vkPhysicalDeviceVulkan13Features;
		vkPhysicalDeviceVulkan12Features.bufferDeviceAddress = true;
		vkPhysicalDeviceVulkan12Features.descriptorIndexing = true;

		vkPhysicalDeviceVulkan13Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
		vkPhysicalDeviceVulkan13Features.pNext = 0;
		vkPhysicalDeviceVulkan13Features.dynamicRendering = true;
		vkPhysicalDeviceVulkan13Features.synchronization2 = true;
			
		// TODO: possibly create more queues? one per type?
		const VkDeviceCreateInfo vkDeviceCreateInfo = {
			.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
			.pNext                   = &vkPhysicalDeviceFeatures2,
			.flags                   = 0,
			.queueCreateInfoCount    = 1,
			.pQueueCreateInfos       = &vkDeviceQueueCreateInfo,
			.enabledLayerCount       = 0,
			.ppEnabledLayerNames     = 0,
			.enabledExtensionCount   = LenOf(RequiredDeviceExts),
			.ppEnabledExtensionNames = RequiredDeviceExts,
			.pEnabledFeatures        = 0,
		};

		CheckVk(vkCreateDevice(physicalDevice->vkPhysicalDevice, &vkDeviceCreateInfo, vkAllocationCallbacks, &vkDevice));
		
		Vk::LoadDeviceFns(vkDevice);

		vkGetDeviceQueue(vkDevice, physicalDevice->queueFamily, 0, &vkQueue);

		return Ok();
	}

	//-------------------------------------------------------------------------------------------------

	// TODO: currently we pass width and height, see if VkSurfaceCapabilitiesKHR.currentExtent actually has the surfaces new width and height so we don't need to pass this
	Res<> CreateSwapchain(u32 width, u32 height) {
		u32 imageCount = Max((u32)3, physicalDevice->vkSurfaceCapabilities.minImageCount);
		if (physicalDevice->vkSurfaceCapabilities.maxImageCount > 0 && imageCount > physicalDevice->vkSurfaceCapabilities.maxImageCount) {
			imageCount = physicalDevice->vkSurfaceCapabilities.maxImageCount;
		}
		Logf("Selected swapchain image count: {}", imageCount);

		// TODO: check min/max extents
		const VkExtent2D vkExtent = (physicalDevice->vkSurfaceCapabilities.currentExtent.width != U32Max)
			? physicalDevice->vkSurfaceCapabilities.currentExtent
			: VkExtent2D {
				.width  = Clamp(width,  physicalDevice->vkSurfaceCapabilities.minImageExtent.width,  physicalDevice->vkSurfaceCapabilities.maxImageExtent.width),
				.height = Clamp(height, physicalDevice->vkSurfaceCapabilities.minImageExtent.height, physicalDevice->vkSurfaceCapabilities.maxImageExtent.height),
			};
		Logf("Selected swapchain extent: ({}, {})", vkExtent.width, vkExtent.height);

		VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;
		for (u64 i = 0; i < physicalDevice->vkPresentModes.len; i++) {
			if (physicalDevice->vkPresentModes[i] == VK_PRESENT_MODE_MAILBOX_KHR) {
				presentMode = VK_PRESENT_MODE_MAILBOX_KHR;
			}
		}
		Logf("Selected swapchain present mode: {}", Vk::PresentModeStr(presentMode));

		const VkSwapchainKHR vkOldSwapchain = vkSwapchain;
		const VkSwapchainCreateInfoKHR vkSwapchainCreateInfoKHR = {
			.sType                 = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
			.pNext                 = 0,
			.flags                 = 0,
			.surface               = vkSurface,
			.minImageCount         = imageCount,
			.imageFormat           = physicalDevice->vkSurfaceFormat,
			.imageColorSpace       = physicalDevice->vkSurfaceColorSpace,
			.imageExtent           = vkExtent,
			.imageArrayLayers      = 1,
			.imageUsage            = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
			// TODO: if we add multiple queue families, then we need to change this
			.imageSharingMode      = VK_SHARING_MODE_EXCLUSIVE,
			.queueFamilyIndexCount = 0,
			.pQueueFamilyIndices   = 0,
			.preTransform          = (physicalDevice->vkSurfaceCapabilities.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR) ? VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR : physicalDevice->vkSurfaceCapabilities.currentTransform,
			.compositeAlpha        = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
			.presentMode           = presentMode,
			.clipped               = VK_TRUE,
			.oldSwapchain          = vkOldSwapchain,
		};
		CheckVk(vkCreateSwapchainKHR(vkDevice, &vkSwapchainCreateInfoKHR, vkAllocationCallbacks, &vkSwapchain));

		vkSwapchainExtent2D.width  = width;
		vkSwapchainExtent2D.height = height;

		if (vkOldSwapchain != VK_NULL_HANDLE) {
			vkDestroySwapchainKHR(vkDevice, vkOldSwapchain, vkAllocationCallbacks);
		}

		u32 n = 0;
		vkSwapchainImages.Init(mem);
		CheckVk(vkGetSwapchainImagesKHR(vkDevice, vkSwapchain, &n, 0));
		CheckVk(vkGetSwapchainImagesKHR(vkDevice, vkSwapchain, &n, vkSwapchainImages.Resize(n)));

		vkSwapchainImageViews.Init(mem);
		vkSwapchainImageViews.Resize(vkSwapchainImages.len);
		for (u64 i = 0; i < vkSwapchainImages.len; i++) {
			const VkImageViewCreateInfo vkImageViewCreateInfo = {
				.sType              = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
				.pNext              = 0,
				.flags              = 0,
				.image              = vkSwapchainImages[i],
				.viewType           = VK_IMAGE_VIEW_TYPE_2D,
				.format             = physicalDevice->vkSurfaceFormat,
				.components         = {
					.r              = VK_COMPONENT_SWIZZLE_IDENTITY,
					.g              = VK_COMPONENT_SWIZZLE_IDENTITY,
					.b              = VK_COMPONENT_SWIZZLE_IDENTITY,
					.a              = VK_COMPONENT_SWIZZLE_IDENTITY,
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

		vkCommandBuffers.Init(mem);
		vkCommandBuffers.Resize(vkSwapchainImages.len);
		for (u64 i = 0; i < vkSwapchainImages.len; i++) {
			const VkCommandBufferAllocateInfo vkCommandBufferAllocateInfo = {
				.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
				.pNext              = 0,
				.commandPool        = vkCommandPool,
				.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
				.commandBufferCount = 1,
			};
			CheckVk(vkAllocateCommandBuffers(vkDevice, &vkCommandBufferAllocateInfo, &vkCommandBuffers[i]));
		}

		vkAcquireImageSemaphores.Init(mem);
		vkAcquireImageSemaphores.Resize(vkSwapchainImages.len);
		for (u64 i = 0; i < vkSwapchainImages.len; i++) {
			constexpr VkSemaphoreCreateInfo vkSemaphoreCreateInfo = {
				.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
				.pNext = 0,
				.flags = 0,
			};
			CheckVk(vkCreateSemaphore(vkDevice, &vkSemaphoreCreateInfo, vkAllocationCallbacks, &vkAcquireImageSemaphores[i]));
		}

		vkRenderSemaphores.Init(mem);
		vkRenderSemaphores.Resize(vkSwapchainImages.len);
		for (u64 i = 0; i < vkSwapchainImages.len; i++) {
			constexpr VkSemaphoreCreateInfo vkSemaphoreCreateInfo = {
				.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
				.pNext = 0,
				.flags = 0,
			};
			CheckVk(vkCreateSemaphore(vkDevice, &vkSemaphoreCreateInfo, vkAllocationCallbacks, &vkRenderSemaphores[i]));
		}

		vkRenderFences.Init(mem);
		vkRenderFences.Resize(vkSwapchainImages.len);
		for (u64 i = 0; i < vkSwapchainImages.len; i++) {
			constexpr VkFenceCreateInfo vkFenceCreateInfo  = {
				.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
				.pNext = 0,
				.flags = VK_FENCE_CREATE_SIGNALED_BIT,
			};
			CheckVk(vkCreateFence(vkDevice, &vkFenceCreateInfo, vkAllocationCallbacks, &vkRenderFences[i]));
		}

		return Ok();
	}

	//-------------------------------------------------------------------------------------------------

	Res<> CreateDrawImage(u32 width, u32 height) {
		vkDrawImageExtent2D.width  = width;
		vkDrawImageExtent2D.height = height;
		const VkImageCreateInfo vkImageCreateInfo = {
			.sType                 = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
			.pNext                 = 0,
			.flags                 = 0,
			.imageType             = VK_IMAGE_TYPE_2D,
			.format                = VK_FORMAT_R16G16B16A16_SFLOAT,
			.extent                = { .width = vkDrawImageExtent2D.width, .height = vkDrawImageExtent2D.height, .depth = 1 },
			.mipLevels             = 1,
			.arrayLayers           = 1,
			.samples               = VK_SAMPLE_COUNT_1_BIT,
			.tiling                = VK_IMAGE_TILING_OPTIMAL,
			.usage                 = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
			.sharingMode           = VK_SHARING_MODE_EXCLUSIVE,
			.queueFamilyIndexCount = 0,
			.pQueueFamilyIndices   = 0,
			.initialLayout         = VK_IMAGE_LAYOUT_UNDEFINED,
		};
		CheckVk(vkCreateImage(vkDevice, &vkImageCreateInfo, vkAllocationCallbacks, &vkDrawImage));

		VkMemoryRequirements vkMemoryRequirements = {};
		vkGetImageMemoryRequirements(vkDevice, vkDrawImage, &vkMemoryRequirements);
		u32 memType = U32Max;
		constexpr VkMemoryPropertyFlags vkMemoryPropertyFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
		for (u32 i = 0; i < physicalDevice->vkPhysicalDeviceMemoryProperties.memoryTypeCount; i++) {
			if (
				(vkMemoryRequirements.memoryTypeBits & (i << 1)) &&
				(physicalDevice->vkPhysicalDeviceMemoryProperties.memoryTypes[i].propertyFlags & vkMemoryPropertyFlags)
			) {
				memType = i;
			}
		}
		if (memType == U32Max) {
			return MakeErr(Err_NoMem);
		}

		const VkMemoryAllocateInfo vkMemoryAllocateInfo = {
			.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
			.pNext           = 0,
			.allocationSize  = vkMemoryRequirements.size,
			.memoryTypeIndex = memType,
		};
		CheckVk(vkAllocateMemory(vkDevice, &vkMemoryAllocateInfo, vkAllocationCallbacks, &vkDrawImageDeviceMemory));
		CheckVk(vkBindImageMemory(vkDevice, vkDrawImage, vkDrawImageDeviceMemory, 0));

		const VkImageViewCreateInfo vkImageViewCreateInfo = {
			.sType              = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			.pNext              = 0,
			.flags              = 0,
			.image              = vkDrawImage,
			.viewType           = VK_IMAGE_VIEW_TYPE_2D,
			.format             = VK_FORMAT_R16G16B16A16_SFLOAT,
			.components         = {
				.r              = VK_COMPONENT_SWIZZLE_IDENTITY,
				.g              = VK_COMPONENT_SWIZZLE_IDENTITY,
				.b              = VK_COMPONENT_SWIZZLE_IDENTITY,
				.a              = VK_COMPONENT_SWIZZLE_IDENTITY,
			},
			.subresourceRange   = {
				.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
				.baseMipLevel   = 0,
				.levelCount     = 1,
				.baseArrayLayer = 0,
				.layerCount     = 1,
			},
		};
		CheckVk(vkCreateImageView(vkDevice, &vkImageViewCreateInfo, vkAllocationCallbacks, &vkDrawImageView));

		return Ok();
	}

	//-------------------------------------------------------------------------------------------------

	Res<> CreateDescriptorPool() {
		constexpr VkDescriptorPoolSize vkDescriptorPoolSizes[] = {
			{ .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,         .descriptorCount = MaxDescriptorSets * MaxDescriptors },
			{ .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = MaxDescriptorSets * MaxDescriptors },
		};
		const VkDescriptorPoolCreateInfo vkDescriptorPoolCreateInfo = {
			.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
			.pNext         = 0,
			.flags         = 0,
			.maxSets       = MaxDescriptorSets,
			.poolSizeCount = LenOf(vkDescriptorPoolSizes),
			.pPoolSizes    = vkDescriptorPoolSizes,
		};
		CheckVk(vkCreateDescriptorPool(vkDevice, &vkDescriptorPoolCreateInfo, vkAllocationCallbacks, &vkDescriptorPool));

		return Ok();
	}

	//-------------------------------------------------------------------------------------------------

	Res<> CreateDescriptorSetLayouts() {
		constexpr VkDescriptorSetLayoutBinding vkComputeDescriptorSetLayoutBinding = {
			.binding            = 0,
			.descriptorType     = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
			.descriptorCount    = 1,
			.stageFlags         = VK_SHADER_STAGE_COMPUTE_BIT,
			.pImmutableSamplers = 0,
		};
		const VkDescriptorSetLayoutCreateInfo vkComputeDescriptorSetLayoutCreateInfo = {
			.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
			.pNext        = 0,
			.flags        = 0,
			.bindingCount = 1,
			.pBindings    = &vkComputeDescriptorSetLayoutBinding,
		};
		CheckVk(vkCreateDescriptorSetLayout(vkDevice, &vkComputeDescriptorSetLayoutCreateInfo, vkAllocationCallbacks, &vkComputeDescriptorSetLayout));

		constexpr VkDescriptorBindingFlags vkDescriptorBindingFlagss[2] = {
			VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT,
			VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT,
		};
		const VkDescriptorSetLayoutBindingFlagsCreateInfo vkDescriptorSetLayoutBindingFlagsCreateInfo = {
			.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
			.pNext         = 0,
			.bindingCount  = 2,
			.pBindingFlags = vkDescriptorBindingFlagss,

		};
		constexpr VkDescriptorSetLayoutBinding vkMeshDescriptorSetLayoutBindings[2] = {
			{
				.binding            = 0,
				.descriptorType     = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
				.descriptorCount    = 1,
				.stageFlags         = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
				.pImmutableSamplers = 0,
			},
			{
				.binding            = 1,
				.descriptorType     = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				.descriptorCount    = 1,
				.stageFlags         = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
				.pImmutableSamplers = 0,
			},
		};
		const VkDescriptorSetLayoutCreateInfo vkMeshDescriptorSetLayoutCreateInfo = {
			.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
			.pNext        = &vkDescriptorSetLayoutBindingFlagsCreateInfo,
			.flags        = 0,
			.bindingCount = LenOf(vkMeshDescriptorSetLayoutBindings),
			.pBindings    = vkMeshDescriptorSetLayoutBindings,
		};
		CheckVk(vkCreateDescriptorSetLayout(vkDevice, &vkMeshDescriptorSetLayoutCreateInfo, vkAllocationCallbacks, &vkMeshDescriptorSetLayout));

		return Ok();
	}

	//-------------------------------------------------------------------------------------------------

	Res<VkShaderModule> CreateShader(s8 path) {
		Span<u8> shaderBytes;
		if (Res<> r = fileApi->ReadAll(tempMem, path).To(shaderBytes); !r) {
			return r.err;
		}
		const VkShaderModuleCreateInfo vkShaderModuleCreateInfo = {
			.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
			.pNext    = 0,
			.flags    = 0,
			.codeSize = shaderBytes.len,
			.pCode    = (const u32*)shaderBytes.data,

		};
		VkShaderModule vkShaderModule = VK_NULL_HANDLE;
		CheckVk(vkCreateShaderModule(vkDevice, &vkShaderModuleCreateInfo, vkAllocationCallbacks, &vkShaderModule));

		return vkShaderModule;
	}

	Res<> CreateShaders() {
		if (Res<> r = CreateShader("Shaders/compute.comp.spv").To(vkComputeShaderModule); !r) { return r; }
		if (Res<> r = CreateShader("Shaders/mesh.vert.spv").To(vkMeshVertShaderModule); !r) { return r; }
		if (Res<> r = CreateShader("Shaders/mesh.frag.spv").To(vkMeshFragShaderModule); !r) { return r; }
		return Ok();
	}

	//-------------------------------------------------------------------------------------------------

	Res<> CreateComputePipeline() {
		constexpr VkPushConstantRange vkComputePushConstantRange = {
			.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
			.offset     = 0,
			.size       = sizeof(ComputePushConstants),
		};
		const VkPipelineLayoutCreateInfo vkComputePipelineLayoutCreateInfo = {
			.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
			.pNext                  = 0,
			.flags                  = 0,
			.setLayoutCount         = 1,
			.pSetLayouts            = &vkComputeDescriptorSetLayout,
			.pushConstantRangeCount = 1,
			.pPushConstantRanges    = &vkComputePushConstantRange,
		};
		CheckVk(vkCreatePipelineLayout(vkDevice, &vkComputePipelineLayoutCreateInfo, vkAllocationCallbacks, &vkComputePipelineLayout));

		const VkComputePipelineCreateInfo vkComputePipelineCreateInfo = {
			.sType                   = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
			.pNext                   = 0,
			.flags                   = 0,
			.stage                   = {
				.sType               = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
				.pNext               = 0,
				.flags               = 0,
				.stage               = VK_SHADER_STAGE_COMPUTE_BIT,
				.module              = vkComputeShaderModule,
				.pName               = "main",
				.pSpecializationInfo = 0,
			},
			.layout                  = vkComputePipelineLayout,
			.basePipelineHandle      = 0,
			.basePipelineIndex       = 0,
		};
		CheckVk(vkCreateComputePipelines(vkDevice, 0, 1, &vkComputePipelineCreateInfo, vkAllocationCallbacks, &vkComputePipeline));

		return Ok();
	}

	//----------------------------------------------------------------------------------------------

	Res<> CreateGraphicsPipeline() {
		constexpr VkPushConstantRange vkMeshPushConstantRange = {
			.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
			.offset     = 0,
			.size       = sizeof(MeshPushConstants),
		};
		const VkPipelineLayoutCreateInfo vkGraphicsPipelineLayoutCreateInfo = {
			.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
			.pNext                  = 0,
			.flags                  = 0,
			.setLayoutCount         = 1,
			.pSetLayouts            = &vkMeshDescriptorSetLayout,
			.pushConstantRangeCount = 1,
			.pPushConstantRanges    = &vkMeshPushConstantRange,
		};
		CheckVk(vkCreatePipelineLayout(vkDevice, &vkGraphicsPipelineLayoutCreateInfo, vkAllocationCallbacks, &vkGraphicsPipelineLayout));

		const VkPipelineShaderStageCreateInfo vkPipelineShaderStageCreateInfos[2] = {
			{
				.sType               = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
				.pNext               = 0,
				.flags               = 0,
				.stage               = VK_SHADER_STAGE_VERTEX_BIT,
				.module              = vkMeshVertShaderModule,
				.pName               = "main",
				.pSpecializationInfo = 0,
			},
			{
				.sType               = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
				.pNext               = 0,
				.flags               = 0,
				.stage               = VK_SHADER_STAGE_FRAGMENT_BIT,
				.module              = vkMeshFragShaderModule,
				.pName               = "main",
				.pSpecializationInfo = 0,
			},
		};
		constexpr VkVertexInputBindingDescription vkVertexInputBindingDescriptions[2] = {
			{
				.binding   = 0,
				.stride    = sizeof(Vertex),
				.inputRate = VK_VERTEX_INPUT_RATE_VERTEX
			},
			{
				.binding   = 1,
				.stride    = sizeof(Vertex),
				.inputRate = VK_VERTEX_INPUT_RATE_VERTEX
			}
		};

		constexpr VkPipelineVertexInputStateCreateInfo vkPipelineVertexInputStateCreateInfo = {
			.sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
			.pNext                           = 0,
			.flags                           = 0,
			.vertexBindingDescriptionCount   = 0,
			.pVertexBindingDescriptions      = 0,
			.vertexAttributeDescriptionCount = 0,
			.pVertexAttributeDescriptions    = 0,

		};
		constexpr VkPipelineInputAssemblyStateCreateInfo vkPipelineInputAssemblyStateCreateInfo = {
			.sType                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
			.pNext                  = 0,
			.flags                  = 0,
			.topology               = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
			.primitiveRestartEnable = VK_FALSE,
		};
		constexpr VkPipelineTessellationStateCreateInfo vkPipelineTessellationStateCreateInfo = {
			.sType              = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO,
			.pNext              = 0,
			.flags              = 0,
			.patchControlPoints = 0,
		};
		const VkViewport vkViewport = {
			.x        = 0.0f,
			.y        = 0.0f,
			.width    = (float)vkDrawImageExtent2D.width,
			.height   = (float)vkDrawImageExtent2D.height,
			.minDepth = 0.0f,
			.maxDepth = 1.0f,
		};
		const VkRect2D vkScissorRect2D = {
			.offset = { .x = 0, .y = 0 },
			.extent = vkDrawImageExtent2D,
		};
		const VkPipelineViewportStateCreateInfo vkPipelineViewportStateCreateInfo = {
			.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
			.pNext         = 0,
			.flags         = 0,
			.viewportCount = 1,
			.pViewports    = &vkViewport,
			.scissorCount  = 1,
			.pScissors     = &vkScissorRect2D,

		};
		constexpr VkPipelineRasterizationStateCreateInfo vkPipelineRasterizationStateCreateInfo = {
			.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
			.pNext                   = 0,
			.flags                   = 0,
			.depthClampEnable        = VK_FALSE,
			.rasterizerDiscardEnable = VK_FALSE,
			.polygonMode             = VK_POLYGON_MODE_FILL,
			.cullMode                = VK_CULL_MODE_NONE,
			.frontFace               = VK_FRONT_FACE_CLOCKWISE,
			.depthBiasEnable         = VK_FALSE,
			.depthBiasConstantFactor = 0.0f,
			.depthBiasClamp          = 0.0f,
			.depthBiasSlopeFactor    = 0.0f,
			.lineWidth               = 1.0f,
		};
		constexpr VkPipelineMultisampleStateCreateInfo vkPipelineMultisampleStateCreateInfo = {
			.sType                 = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
			.pNext                 = 0,
			.flags                 = 0,
			.rasterizationSamples  = VK_SAMPLE_COUNT_1_BIT,
			.sampleShadingEnable   = VK_FALSE,
			.minSampleShading      = 1.0F,
			.pSampleMask           = 0,
			.alphaToCoverageEnable = VK_FALSE,
			.alphaToOneEnable      = VK_FALSE,
		};
		constexpr VkStencilOpState vkNoOpStencilOpState = {
			.failOp      = VK_STENCIL_OP_KEEP,
			.passOp      = VK_STENCIL_OP_KEEP,
			.depthFailOp = VK_STENCIL_OP_KEEP,
			.compareOp   = VK_COMPARE_OP_NEVER,
			.compareMask = 0,
			.writeMask   = 0,
			.reference   = 0,
		};
		constexpr VkPipelineDepthStencilStateCreateInfo vkPipelineDepthStencilStateCreateInfo = {
			.sType                 = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
			.pNext                 = 0,
			.flags                 = 0,
			.depthTestEnable       = VK_FALSE,
			.depthWriteEnable      = VK_FALSE,
			.depthCompareOp        = VK_COMPARE_OP_NEVER,
			.depthBoundsTestEnable = VK_FALSE,
			.stencilTestEnable     = VK_FALSE,
			.front                 = vkNoOpStencilOpState,
			.back                  = vkNoOpStencilOpState,
			.minDepthBounds        = 0.0f,
			.maxDepthBounds        = 1.0f,
		};
		constexpr VkPipelineColorBlendAttachmentState vkPipelineColorBlendAttachmentState = {
			.blendEnable         = VK_FALSE,
			.srcColorBlendFactor = VK_BLEND_FACTOR_ZERO,
			.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO,
			.colorBlendOp        = VK_BLEND_OP_ADD,
			.srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
			.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
			.alphaBlendOp        = VK_BLEND_OP_ADD,
			.colorWriteMask      = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
		};
		constexpr VkPipelineColorBlendStateCreateInfo vkPipelineColorBlendStateCreateInfo = {
			.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
			.pNext           = 0,
			.flags           = 0,
			.logicOpEnable   = VK_FALSE,
			.logicOp         = VK_LOGIC_OP_CLEAR,
			.attachmentCount = 0,
			.pAttachments    = 0,
			.blendConstants  = 0.0f,
		};
		constexpr VkDynamicState vkDynamicStates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
		const VkPipelineDynamicStateCreateInfo vkPipelineDynamicStateCreateInfo = {
			.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
			.pNext             = 0,
			.flags             = 0,
			.dynamicStateCount = (u32)LenOf(vkDynamicStates),
			.pDynamicStates    = vkDynamicStates,
		};
		const VkGraphicsPipelineCreateInfo vkGraphicsPipelineCreateInfo = {
			.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
			.pNext               = 0,
			.flags               = 0,
			.stageCount          = 2,
			.pStages             = vkPipelineShaderStageCreateInfos,
			.pVertexInputState   = &vkPipelineVertexInputStateCreateInfo,
			.pInputAssemblyState = &vkPipelineInputAssemblyStateCreateInfo,
			.pTessellationState  = &vkPipelineTessellationStateCreateInfo,
			.pViewportState      = &vkPipelineViewportStateCreateInfo,
			.pRasterizationState = &vkPipelineRasterizationStateCreateInfo,
			.pMultisampleState   = &vkPipelineMultisampleStateCreateInfo,
			.pDepthStencilState  = &vkPipelineDepthStencilStateCreateInfo,
			.pColorBlendState    = &vkPipelineColorBlendStateCreateInfo,
			.pDynamicState       = &vkPipelineDynamicStateCreateInfo,
			.layout              = vkGraphicsPipelineLayout,
			.renderPass          = VK_NULL_HANDLE,
			.subpass             = 0,
			.basePipelineHandle  = VK_NULL_HANDLE,
			.basePipelineIndex   = 0,
		};
		CheckVk(vkCreateGraphicsPipelines(vkDevice, 0, 1, &vkGraphicsPipelineCreateInfo, vkAllocationCallbacks, &vkGraphicsPipeline));

		return Ok();
	}

	//-------------------------------------------------------------------------------------------------

	Res<> CreateSampler() {
		const VkSamplerCreateInfo vkSamplerCreateInfo = {
			.sType                   = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
			.pNext                   = 0,
			.flags                   = 0,
			.magFilter               = VK_FILTER_NEAREST,
			.minFilter               = VK_FILTER_NEAREST,
			.mipmapMode              = VK_SAMPLER_MIPMAP_MODE_LINEAR,
			.addressModeU            = VK_SAMPLER_ADDRESS_MODE_REPEAT,
			.addressModeV            = VK_SAMPLER_ADDRESS_MODE_REPEAT,
			.addressModeW            = VK_SAMPLER_ADDRESS_MODE_REPEAT,
			.mipLodBias              = 0.0f,
			.anisotropyEnable        = VK_FALSE,
			.maxAnisotropy           = 0.0f,
			.compareEnable           = VK_FALSE,
			.compareOp               = VK_COMPARE_OP_NEVER,
			.minLod                  = 0.0f,
			.maxLod                  = VK_LOD_CLAMP_NONE,
			.borderColor             = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,
			.unnormalizedCoordinates = VK_FALSE,
		};
		CheckVk(vkCreateSampler(vkDevice, &vkSamplerCreateInfo, vkAllocationCallbacks, &vkSampler));

		return Ok();
	}

	//-------------------------------------------------------------------------------------------------

	Res<> CreateDescriptorSets() {

		const VkDescriptorSetAllocateInfo vkComputeDescriptorSetAllocateInfo = {
			.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
			.pNext              = 0,
			.descriptorPool     = vkDescriptorPool,
			.descriptorSetCount = 1,
			.pSetLayouts        = &vkComputeDescriptorSetLayout,
		};
		CheckVk(vkAllocateDescriptorSets(vkDevice, &vkComputeDescriptorSetAllocateInfo, &vkComputeDescriptorSet));

		constexpr VkDescriptorSetLayoutBinding vkDescriptorSetLayoutBindings[2] = {
			{
				.binding            = 0,
				.descriptorType     = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
				.descriptorCount    = 1,
				.stageFlags         = VK_SHADER_STAGE_COMPUTE_BIT,
				.pImmutableSamplers = 0,
			},
			{
				.binding            = 0,
				.descriptorType     = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
				.descriptorCount    = 1,
				.stageFlags         = VK_SHADER_STAGE_COMPUTE_BIT,
				.pImmutableSamplers = 0,
			},
		};
		const VkDescriptorSetLayoutCreateInfo vkDescriptorSetLayoutCreateInfo = {
			.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
			.pNext        = 0,
			.flags        = 0,
			.bindingCount = 1,
			.pBindings    = &vkDescriptorSetLayoutBinding,
		};
		CheckVk(vkCreateDescriptorSetLayout(vkDevice, &vkDescriptorSetLayoutCreateInfo, vkAllocationCallbacks, &vkDescriptorSetLayout));


		const VkDescriptorImageInfo vkDescriptorImageInfo = {
			.sampler     = 0,
			.imageView   = vkDrawImageView,
			.imageLayout = VK_IMAGE_LAYOUT_GENERAL,
		};
		const VkWriteDescriptorSet vkWriteDescriptorSet = {
			.sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.pNext            = 0,
			.dstSet           = vkDescriptorSet,
			.dstBinding       = 0,
			.dstArrayElement  = 0,
			.descriptorCount  = 1,
			.descriptorType   = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
			.pImageInfo       = &vkDescriptorImageInfo,
			.pBufferInfo      = 0,
			.pTexelBufferView = 0,
		};
		vkUpdateDescriptorSets(vkDevice, 1, &vkWriteDescriptorSet, 0, 0);

		return Ok();
	}

	//-------------------------------------------------------------------------------------------------

	Res<> CreateMeshes() {
		constexpr Vertex vertices[4] = {
			{ .position = {  0.5f, -0.5f, 0.0f }, .color = { 0.0f, 0.0f, 0.0f, 1.0f } },
			{ .position = {  0.5f,  0.5f, 0.0f }, .color = { 0.5f, 0.5f, 0.5f, 1.0f } },
			{ .position = { -0.5f, -0.5f, 0.0f }, .color = { 1.0f, 0.0f, 0.0f, 1.0f } },
			{ .position = { -0.5f,  0.5f, 0.0f }, .color = { 0.0f, 1.0f, 0.0f, 1.0f } },
		};
		constexpr u32 indices[6] = {
			0, 1, 2,
			2, 1, 3,
		};

		const VkBufferCreateInfo vkBufferCreateInfo = {
			.sType                 = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
			.pNext                 = 0,
			.flags                 = 0,
			.size                  = sizeof(vertices),
			.usage                 = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
			.sharingMode           = VK_SHARING_MODE_EXCLUSIVE,
			.queueFamilyIndexCount = 1,
			.pQueueFamilyIndices   = &physicalDevice->queueFamily,
		};
		CheckVk(vkCreateBuffer(vkDevice, &vkBufferCreateInfo, vkAllocationCallbacks, &vkBuffer));

		return Ok();
	}

	//-------------------------------------------------------------------------------------------------
	//-------------------------------------------------------------------------------------------------
	//-------------------------------------------------------------------------------------------------

	Res<> Init(const RenderApiInit* init) override {
		fileApi = init->fileApi;
		log     = init->log;
		mem     = init->mem;
		tempMem = init->tempMem;

		if (Res<> r = Vk::LoadRootFns();                          !r) { return r; }
		if (Res<> r = CreateInstance();                           !r) { return r; }
		if (Res<> r = CreateSurface(init->windowPlatformData);    !r) { return r; }
		if (Res<> r = CreateDevice();                             !r) { return r; }
		if (Res<> r = CreateSwapchain(init->width, init->height); !r) { return r; }
		if (Res<> r = CreateDrawImage(init->width, init->height); !r) { return r; }
		if (Res<> r = CreateDescriptorPool();                     !r) { return r; }
		if (Res<> r = CreateDescriptorSetLayouts();               !r) { return r; }
		if (Res<> r = CreateShaders();                            !r) { return r; }
		if (Res<> r = CreateComputePipeline();                    !r) { return r; }
		if (Res<> r = CreateGraphicsPipeline();                   !r) { return r; }


		if (Res<> r = CreateSampler();                            !r) { return r; }
		if (Res<> r = CreateMeshes();                             !r) { return r; }

	VkDescriptorImageInfo imageInfo{};
	imageInfo.sampler = ctx->sampler;
	imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	imageInfo.imageView = ctx->textureView;

	VkWriteDescriptorSet textureDescriptorWrite{};
	textureDescriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	textureDescriptorWrite.dstSet = descriptorSet;
	textureDescriptorWrite.dstBinding = 1;
	textureDescriptorWrite.descriptorCount = 1;
	textureDescriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	textureDescriptorWrite.pImageInfo = &imageInfo;

	vkUpdateDescriptorSets(
		ctx->device,
		1,
		&textureDescriptorWrite,
		0,
		nullptr
	);

	static const float positions[4][2] = {
		{ -0.5, -0.25 }, { -0.5, +0.25 }, { +0.5, +0.25 }, { +0.5, -0.25 }
	};

	for (int i = 0; i < 4; i++) {
		entities.push_back(new Entity(this, positions[i][0], positions[i][1]));
	}


		return Ok();
	}

	//----------------------------------------------------------------------------------------------

	#define DestroyVkHandle(handle, DestroyFnInvocation) \
		if (handle != VK_NULL_HANDLE) { \
			DestroyFnInvocation; \
			handle = VK_NULL_HANDLE; \
		}

	void Shutdown() override {
		if (vkDevice) {
			vkDeviceWaitIdle(vkDevice);
		}

		DestroyVkHandle(vkGraphicsPipeline,         vkDestroyPipeline           (vkDevice, vkGraphicsPipeline, vkAllocationCallbacks));
		DestroyVkHandle(vkGraphicsPipelineLayout,   vkDestroyPipelineLayout     (vkDevice, vkGraphicsPipelineLayout, vkAllocationCallbacks));
		DestroyVkHandle(vkComputePipeline,          vkDestroyPipeline           (vkDevice, vkComputePipeline, vkAllocationCallbacks));
		DestroyVkHandle(vkComputePipelineLayout,    vkDestroyPipelineLayout     (vkDevice, vkComputePipelineLayout, vkAllocationCallbacks));
		DestroyVkHandle(vkGradientShaderModule,     vkDestroyShaderModule       (vkDevice, vkGradientShaderModule, vkAllocationCallbacks));
		DestroyVkHandle(vkTriangleVertShaderModule, vkDestroyShaderModule       (vkDevice, vkTriangleVertShaderModule, vkAllocationCallbacks));
		DestroyVkHandle(vkTriangleFragShaderModule, vkDestroyShaderModule       (vkDevice, vkTriangleFragShaderModule, vkAllocationCallbacks));
		DestroyVkHandle(vkDescriptorSet,            vkFreeDescriptorSets        (vkDevice, vkDescriptorPool, 1, &vkDescriptorSet));
		DestroyVkHandle(vkDescriptorPool,           vkDestroyDescriptorPool     (vkDevice, vkDescriptorPool, vkAllocationCallbacks));
		DestroyVkHandle(vkDescriptorSetLayout,      vkDestroyDescriptorSetLayout(vkDevice, vkDescriptorSetLayout, vkAllocationCallbacks));
		for (u32 i = 0; i < MaxFrames; i++) {
			DestroyVkHandle(frames[i].vkCommandPool,         vkDestroyCommandPool  (vkDevice, frames[i].vkCommandPool,        vkAllocationCallbacks));
			frames[i].vkCommandBuffer = VK_NULL_HANDLE;
			DestroyVkHandle(frames[i].vkSwapchainSemaphore,  vkDestroySemaphore    (vkDevice, frames[i].vkSwapchainSemaphore, vkAllocationCallbacks));
			DestroyVkHandle(frames[i].vkRenderSemaphore,     vkDestroySemaphore    (vkDevice, frames[i].vkRenderSemaphore,    vkAllocationCallbacks));
			DestroyVkHandle(frames[i].vkRenderFence,         vkDestroyFence        (vkDevice, frames[i].vkRenderFence,        vkAllocationCallbacks));
		}
		for (u64 i = 0; i < vkSwapchainImageViews.len; i++) {
			vkDestroyImageView(vkDevice, vkSwapchainImageViews[i], vkAllocationCallbacks);
		}
		vkSwapchainImageViews.Free();
		vkSwapchainImages.Free();
		DestroyVkHandle(vkSwapchain,           vkDestroySwapchainKHR(vkDevice, vkSwapchain, vkAllocationCallbacks));
		DestroyVkHandle(vkDevice,              vkDestroyDevice(vkDevice , vkAllocationCallbacks));
		physicalDevices.Free();
		DestroyVkHandle(vkSurface,             vkDestroySurfaceKHR(vkInstance, vkSurface, vkAllocationCallbacks));
		DestroyVkHandle(vkDebugUtilsMessenger, vkDestroyDebugUtilsMessengerEXT(vkInstance, vkDebugUtilsMessenger, vkAllocationCallbacks));
		DestroyVkHandle(vkInstance,            vkDestroyInstance(vkInstance, vkAllocationCallbacks));
	}

	//----------------------------------------------------------------------------------------------

	VkImageSubresourceRange MakeVkSubresourceRange(VkImageAspectFlags vkImageAspectFlags) {
		return VkImageSubresourceRange {
			.aspectMask      = vkImageAspectFlags,
			.baseMipLevel    = 0,
			.levelCount      = VK_REMAINING_MIP_LEVELS,
			.baseArrayLayer  = 0,
			.layerCount      = VK_REMAINING_ARRAY_LAYERS,
		};
	}

	//----------------------------------------------------------------------------------------------

	void TransitionImage(VkCommandBuffer vkCommandBuffer, VkImage vkImage, VkImageLayout vkCurrentImageLayout, VkImageLayout vkNewImageLayout) {
		const VkImageMemoryBarrier2 vkImageMemoryBarrier2 = {
			.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
			.pNext               = 0,
			.srcStageMask        = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
			.srcAccessMask       = VK_ACCESS_2_MEMORY_WRITE_BIT,
			.dstStageMask        = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
			.dstAccessMask       = VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT,
			.oldLayout           = vkCurrentImageLayout,
			.newLayout           = vkNewImageLayout,
			.srcQueueFamilyIndex = 0,
			.dstQueueFamilyIndex = 0,
			.image               = vkImage,
			.subresourceRange    = MakeVkSubresourceRange(vkNewImageLayout == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT),
		};
		const VkDependencyInfo vkDependencyInfo = {
			.sType                    = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
			.pNext                    = 0,
			.dependencyFlags          = 0,
			.memoryBarrierCount       = 0,
			.pMemoryBarriers          = 0,
			.bufferMemoryBarrierCount = 0,
			.pBufferMemoryBarriers    = 0,
			.imageMemoryBarrierCount  = 1,
			.pImageMemoryBarriers     = &vkImageMemoryBarrier2,
		};
		vkCmdPipelineBarrier2(vkCommandBuffer, &vkDependencyInfo);
	}

	//----------------------------------------------------------------------------------------------

	Res<> Draw() override {
		Frame* const frame = &frames[frameNumber % MaxFrames];
		CheckVk(vkWaitForFences(vkDevice, 1, &frame->vkRenderFence, VK_TRUE, U64Max));
		CheckVk(vkResetFences(vkDevice, 1, &frame->vkRenderFence));

		u32 swapchainImageIndex = 0;
		if (VkResult r = vkAcquireNextImageKHR(vkDevice, vkSwapchain, U64Max, frame->vkSwapchainSemaphore, 0, &swapchainImageIndex); r != VK_SUCCESS) {
			if (r == VK_SUBOPTIMAL_KHR) {
				// recreate
				return MakeVkErr(r, "vkAcquireNextImageKHR");
			} else if (r == VK_ERROR_OUT_OF_DATE_KHR) {
				// recreate
				return MakeVkErr(r, "vkAcquireNextImageKHR");
			} else if (r == VK_ERROR_DEVICE_LOST) {
				// ??
				return MakeVkErr(r, "vkAcquireNextImageKHR");
			} else if (r == VK_ERROR_SURFACE_LOST_KHR) {
				// ??
				return MakeVkErr(r, "vkAcquireNextImageKHR");
			} else {
				return MakeVkErr(r, "vkAcquireNextImageKHR");
			}
		}

		CheckVk(vkResetCommandBuffer(frame->vkCommandBuffer, 0));
		constexpr VkCommandBufferBeginInfo vkCommandBufferBeginInfo = {
			.sType            = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
			.pNext            = 0,
			.flags            = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
			.pInheritanceInfo = 0,
		};
		CheckVk(vkBeginCommandBuffer(frame->vkCommandBuffer, &vkCommandBufferBeginInfo));

		// Must be VK_IMAGE_LAYOUT_GENERAL here because vkCmdClearColorImage requires that layout
		TransitionImage(frame->vkCommandBuffer, vkDrawImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

		vkCmdBindPipeline(frame->vkCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, vkComputePipeline);
		vkCmdBindDescriptorSets(frame->vkCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, vkComputePipelineLayout, 0, 1, &vkDescriptorSet, 0, 0);
		constexpr PushConstants pushConstants =  {
			.data1 = { 1.0f, 0.0f, 0.0f, 1.0f },
			.data2 = { 0.0f, 0.0f, 1.0f, 1.0f },
		};
		vkCmdPushConstants(frame->vkCommandBuffer, vkComputePipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PushConstants), &pushConstants);
		vkCmdDispatch(frame->vkCommandBuffer, (u32)AlignUp(vkDrawImageExtent2D.width, 16) / 16, (u32)AlignUp(vkDrawImageExtent2D.height, 16) / 16, 1);

		TransitionImage(frame->vkCommandBuffer, vkDrawImage, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

		const VkRenderingAttachmentInfo VkRenderingAttachmentInfo = {
			.sType              = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
			.pNext              = 0,
			.imageView          = vkDrawImageView,
			.imageLayout        = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			.resolveMode        = VK_RESOLVE_MODE_NONE,
			.resolveImageView   = VK_NULL_HANDLE,
			.resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
			.loadOp             = VK_ATTACHMENT_LOAD_OP_LOAD,
			.storeOp            = VK_ATTACHMENT_STORE_OP_STORE,
			.clearValue         = {},
		};
		const VkRenderingInfo vkRenderingInfo = {
			.sType                = VK_STRUCTURE_TYPE_RENDERING_INFO,
			.pNext                = 0,
			.flags                = 0,
			.renderArea           = { .offset = { 0, 0 }, .extent = vkDrawImageExtent2D },
			.layerCount           = 1,
			.viewMask             = 0,
			.colorAttachmentCount = 1,
			.pColorAttachments    = &VkRenderingAttachmentInfo,
			.pDepthAttachment     = 0,
			.pStencilAttachment   = 0,
		};
		vkCmdBeginRendering(frame->vkCommandBuffer, &vkRenderingInfo);
		vkCmdBindPipeline(frame->vkCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vkGraphicsPipeline);
		const VkViewport vkViewport = {
			.x        = 0.0f,
			.y        = 0.0f,
			.width    = (float)vkDrawImageExtent2D.width,
			.height   = (float)vkDrawImageExtent2D.height,
			.minDepth = 0.0f,
			.maxDepth = 1.0f,
		};
		vkCmdSetViewport(frame->vkCommandBuffer, 0, 1, &vkViewport);
		const VkRect2D vkScissorRect2D = {
			.offset = { .x = 0, .y = 0 },
			.extent = vkDrawImageExtent2D,
		};
		vkCmdSetScissor(frame->vkCommandBuffer, 0, 1, &vkScissorRect2D);
		vkCmdDraw(frame->vkCommandBuffer, 3, 1, 0, 0);
		vkCmdEndRendering(frame->vkCommandBuffer);

		TransitionImage(frame->vkCommandBuffer, vkDrawImage, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
		TransitionImage(frame->vkCommandBuffer, vkSwapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

		const VkImageBlit2 vkImageBlit2 = {
			.sType              = VK_STRUCTURE_TYPE_IMAGE_BLIT_2,
			.pNext              = 0,
			.srcSubresource     = {
				.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
				.mipLevel       = 0,
				.baseArrayLayer = 0,
				.layerCount     = 1,
			},
			.srcOffsets         = { { .x = 0, .y = 0, .z = 0 }, { .x = (i32)vkDrawImageExtent2D.width, .y = (i32)vkDrawImageExtent2D.height, .z = 1 } },
			.dstSubresource     = {
				.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
				.mipLevel       = 0,
				.baseArrayLayer = 0,
				.layerCount     = 1,
			},
			.dstOffsets         = { { .x = 0, .y = 0, .z = 0 }, { .x = (i32)vkSwapchainExtent2D.width, .y = (i32)vkSwapchainExtent2D.height, .z = 1 } },
		};
		const VkBlitImageInfo2 vkBlitImageInfo2 = {
			.sType          = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2,
			.pNext          = 0,
			.srcImage       = vkDrawImage,
			.srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			.dstImage       = vkSwapchainImages[swapchainImageIndex],
			.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			.regionCount    = 1,
			.pRegions       = &vkImageBlit2,
			.filter         = VK_FILTER_NEAREST,

		};
		vkCmdBlitImage2(frame->vkCommandBuffer, &vkBlitImageInfo2);

		TransitionImage(frame->vkCommandBuffer, vkSwapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

		CheckVk(vkEndCommandBuffer(frame->vkCommandBuffer));

		const VkSemaphoreSubmitInfo vkWaitSemaphoreSubmitInfo = {
			.sType       = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
			.pNext       = 0,
			.semaphore   = frame->vkSwapchainSemaphore,
			.value       = 0,
			.stageMask   = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR,
			.deviceIndex = 0,
		};
		const VkCommandBufferSubmitInfo vkCommandBufferSubmitInfo = {
			.sType         = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
			.pNext         = 0,
			.commandBuffer = frame->vkCommandBuffer,
			.deviceMask    = 0,
		};
		const VkSemaphoreSubmitInfo vkSignalSemaphoreSubmitInfo = {
			.sType       = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
			.pNext       = 0,
			.semaphore   = frame->vkRenderSemaphore,
			.value       = 0,
			.stageMask   = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT,
			.deviceIndex = 0,
		};
		const VkSubmitInfo2 vkSubmitInfo2 = {
			.sType                    = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
			.pNext                    = 0,
			.flags                    = 0,
			.waitSemaphoreInfoCount   = 1,
			.pWaitSemaphoreInfos      = &vkWaitSemaphoreSubmitInfo,
			.commandBufferInfoCount   = 1,
			.pCommandBufferInfos      = &vkCommandBufferSubmitInfo,
			.signalSemaphoreInfoCount = 1,
			.pSignalSemaphoreInfos    = &vkSignalSemaphoreSubmitInfo,
		};
		CheckVk(vkQueueSubmit2(vkQueue, 1, &vkSubmitInfo2, frame->vkRenderFence));

		VkPresentInfoKHR vkPresentInfoKHR = {
			.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
			.pNext              = 0,
			.waitSemaphoreCount = 1,
			.pWaitSemaphores    = &frame->vkRenderSemaphore,
			.swapchainCount     = 1,
			.pSwapchains        = &vkSwapchain,
			.pImageIndices      = &swapchainImageIndex,
			.pResults           = 0,
		};
		//CheckVk(vkQueuePresentKHR(vkQueue, &vkPresentInfoKHR));
		if (VkResult r = vkQueuePresentKHR(vkQueue, &vkPresentInfoKHR); r != VK_SUCCESS) {
			return MakeVkErr(r, "vkQueuePresentKHR");
		}

		frameNumber++;

		return Ok();
	}
};

//--------------------------------------------------------------------------------------------------

static RenderApiObj renderApi;

RenderApi* GetRenderApi() {
	return &renderApi;
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC