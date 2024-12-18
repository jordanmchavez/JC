#pragma once

#include "JC/Render.h"
#include "JC/Render_Vk.h"

#include "JC/Array.h"
#include "JC/Bit.h"
#include "JC/Fmt.h"
#include "JC/FS.h"
#include "JC/HandleArray.h"
#include "JC/Log.h"
#include "JC/Math.h"
#include "JC/Window.h"

#include "spirv_reflect/spirv_reflect.h"

#define Render_Debug

namespace JC::Render {

//--------------------------------------------------------------------------------------------------

#define DestroyVk(h, DestroyFn) \
	if (h != VK_NULL_HANDLE) { \
		DestroyFn(vkDevice, h, vkAllocationCallbacks); \
		h = VK_NULL_HANDLE; \
	}

#define DestroyVkArray(a, DestroyFn) \
	for (u64 i = 0; i < a.len; i++) { \
		if (a[i] != VK_NULL_HANDLE) { \
			DestroyFn(vkDevice, a[i], vkAllocationCallbacks); \
		} \
	} \

//--------------------------------------------------------------------------------------------------

static constexpr u32   MaxFrames                 = 3;
static constexpr u32   MaxBindlessSampledImages  = 64 * 1024;
static constexpr u32   MaxBindlessSamplers       = 8;
static constexpr u32   MaxBindlessDescriptorSets = 32;
static constexpr float MaxAnisotropy             = 8.0f;

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
	Array<VkExtensionProperties>     vkExtensionProperties            = {};
	Array<VkSurfaceFormatKHR>        vkSurfaceFormats                 = {};
	VkFormat                         vkSurfaceFormat                  = VK_FORMAT_UNDEFINED;
	VkColorSpaceKHR                  vkSurfaceColorSpace              = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
	Array<VkPresentModeKHR>          vkPresentModes                   = {};
	Array<QueueFamily>               queueFamilies                    = {};
	u32                              queueFamily                      = VK_QUEUE_FAMILY_IGNORED;
	u32                              score                            = 0;
};

struct DeviceMem {
	VkDeviceMemory        vkDeviceMemory        = VK_NULL_HANDLE;
	u64                   offset                = 0;
	u64                   size                  = 0;
	u32                   type                  = 0;
	VkMemoryPropertyFlags vkMemoryPropertyFlags = 0;
	VkMemoryAllocateFlags vkMemoryAllocateFlags = 0;
};

struct BufferObj {
	VkBuffer           vkBuffer           = VK_NULL_HANDLE;
	DeviceMem          deviceMem          = {};
	u64                offset             = 0;
	u64                size               = 0;
	VkBufferUsageFlags vkBufferUsageFlags = 0;
	u64                addr               = 0;
};

struct SamplerObj {
	VkSampler vkSampler = VK_NULL_HANDLE;
};

struct ImageObj {
	VkImage       vkImage       = VK_NULL_HANDLE;
	VkImageView   vkImageView   = VK_NULL_HANDLE;
	DeviceMem     deviceMem     = {};
	u32           width         = 0;
	u32           height        = 0;
	VkFormat      vkFormat      = VK_FORMAT_UNDEFINED;
	VkImageLayout vkImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	Sampler       sampler       = {};
};

struct ShaderObj {
	VkShaderModule      vkShaderModule      = VK_NULL_HANDLE;
	VkShaderStageFlags  vkShaderStageFlags  = 0;
	u32                 pushConstantsOffset = 0;
	u32                 pushConstantsSize   = 0;
};

struct PipelineObj {
	VkPipeline            vkPipeline            = VK_NULL_HANDLE;
	VkPipelineLayout      vkPipelineLayout      = VK_NULL_HANDLE;
	VkPipelineBindPoint   vkPipelineBindPoint   = {};
	VkPushConstantRange   vkPushConstantRange   = {};
};

static Arena*                             perm;
static Arena*                             temp;
static Log*                               log;
static HandleArray<BufferObj, Buffer>     bufferObjs;
static HandleArray<ImageObj, Image>       imageObjs;
static HandleArray<ShaderObj, Shader>     shaderObjs;
static HandleArray<PipelineObj, Pipeline> pipelineObjs;
static VkAllocationCallbacks*             vkAllocationCallbacks;
static VkInstance                         vkInstance;
static VkDebugUtilsMessengerEXT           vkDebugUtilsMessenger;
static VkSurfaceKHR                       vkSurface;
static Array<PhysicalDevice>              physicalDevices;
static PhysicalDevice*                    physicalDevice;
static VkDevice                           vkDevice;
static VkQueue                            vkQueue;
static VkSwapchainKHR                     vkSwapchain;
static VkExtent2D                         vkSwapchainExtent2D;
static Array<VkImage>                     vkSwapchainImages;
static Array<VkImageView>                 vkSwapchainImageViews;
static u32                                swapchainImageIndex;
static Array<VkSemaphore>                 vkFrameAcquireImageSemaphores;
static Array<VkSemaphore>                 vkFrameRenderSemaphores;
static Array<VkFence>                     vkFrameRenderFences;
static VkCommandPool                      vkCommandPool;
static Array<VkCommandBuffer>             vkFrameCommandBuffers;
static VkDescriptorPool                   vkDescriptorPool;
static VkDescriptorSetLayout              vkBindlessDescriptorSetLayout;
static VkDescriptorSet                    vkBindlessDescriptorSet;
static Array<VkSampler>                   vkBindlessSamplers;
static u64                                frameIndex;

//-------------------------------------------------------------------------------------------------

static VkBool32 VKAPI_CALL DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT, const VkDebugUtilsMessengerCallbackDataEXT* data, void*) {
	if (severity & (VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)) {
		log->Error("[{}] {}: {}", data->messageIdNumber, data->pMessageIdName, data->pMessage ? data->pMessage : "");
	} else {
		log->Print("[{}] {}: {}", data->messageIdNumber, data->pMessageIdName, data->pMessage ? data->pMessage : "");
	}
	return VK_FALSE;
}
	
//-------------------------------------------------------------------------------------------------

Res<> InitInstance() {
	u32 instanceVersion = 0;
	CheckVk(vkEnumerateInstanceVersion(&instanceVersion));
	if (instanceVersion < VK_API_VERSION_1_3) {
		return MakeErr(temp, Err_Version, "ver", instanceVersion);
	}

	u32 n = 0;
	Array<VkLayerProperties> layers(temp);
	CheckVk(vkEnumerateInstanceLayerProperties(&n, 0));
	CheckVk(vkEnumerateInstanceLayerProperties(&n, layers.Resize(n)));
	Logf("{} layers:", layers.len);
	for (u64 i = 0; i < layers.len; i++) {
		Logf(
			"  {}: implementationVersion={}, specVersion={}, description={}",
			layers[i].layerName,
			VersionStr(temp, layers[i].implementationVersion),
			VersionStr(temp, layers[i].specVersion),
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
			return MakeErr(temp, Err_NoLayer, "name", RequiredLayers[i]);
		}
	}
	
	Array<VkExtensionProperties> instExts(temp);
	CheckVk(vkEnumerateInstanceExtensionProperties(0, &n, 0));
	CheckVk(vkEnumerateInstanceExtensionProperties(0, &n, instExts.Resize(n)));

	Logf("{} instance extensions:", instExts.len);
	for (u64 i = 0; i < instExts.len; i++) {
		Logf("  {}: specVersion={}", instExts[i].extensionName, VersionStr(temp, instExts[i].specVersion));
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
			return MakeErr(temp, Err_NoLayer, "name", RequiredInstExts[i]);
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
			.pfnUserCallback = DebugCallback,
			.pUserData       = log,
		};
		vkInstanceCreateInfo.pNext = &debugCreateInfo;
	#endif	// Render_Debug

	CheckVk(vkCreateInstance(&vkInstanceCreateInfo, vkAllocationCallbacks, &vkInstance));
	LoadInstanceFns(vkInstance);

	#if defined Render_Debug
		CheckVk(vkCreateDebugUtilsMessengerEXT(vkInstance, &debugCreateInfo, vkAllocationCallbacks, &vkDebugUtilsMessenger));
	#endif	// Render_Debug

	return Ok();
}

//-------------------------------------------------------------------------------------------------

Res<> InitSurface(Window::PlatformInfo* windowPlatformInfo) {
	#if defined Platform_Windows
		VkWin32SurfaceCreateInfoKHR win32SurfaceCreateInfo = {
			.sType     = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
			.pNext     = 0,
			.flags     = 0,
			.hinstance = (HINSTANCE)windowPlatformInfo->hinstance,
			.hwnd      = (HWND)windowPlatformInfo->hwnd,
		};
		CheckVk(vkCreateWin32SurfaceKHR(vkInstance, &win32SurfaceCreateInfo, vkAllocationCallbacks, &vkSurface));
	#endif	// Platform_

	return Ok();
}

//-------------------------------------------------------------------------------------------------

Res<> InitDevice() {
	constexpr const char* RequiredDeviceExts[] = {
		"VK_KHR_swapchain",
	};

	u32 n = 0;
	Array<VkPhysicalDevice> vkPhysicalDevices(temp);
	CheckVk(vkEnumeratePhysicalDevices(vkInstance, &n, nullptr));
	CheckVk(vkEnumeratePhysicalDevices(vkInstance, &n, vkPhysicalDevices.Resize(n)));

	physicalDevices.Resize(vkPhysicalDevices.len);
	Logf("{} physical devices:", physicalDevices.len);
	u32 bestScore = 0;
	for (u64 i = 0; i < physicalDevices.len; i++) {
		PhysicalDevice* const pd = &physicalDevices[i];

		pd->vkPhysicalDevice = vkPhysicalDevices[i];
		pd->score = 0;

		vkGetPhysicalDeviceProperties(pd->vkPhysicalDevice, &pd->vkPhysicalDeviceProperties);
		Logf(
			"{}: apiVersion={}, driverVersion={}, vendorID={}, deviceId={}, deviceType={}",
			pd->vkPhysicalDeviceProperties.deviceName,
			VersionStr(temp, pd->vkPhysicalDeviceProperties.apiVersion),
			VersionStr(temp, pd->vkPhysicalDeviceProperties.driverVersion),
			pd->vkPhysicalDeviceProperties.vendorID,
			pd->vkPhysicalDeviceProperties.deviceID,
			PhysicalDeviceTypeStr(pd->vkPhysicalDeviceProperties.deviceType)
		);
		switch (pd->vkPhysicalDeviceProperties.deviceType) {
			case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:   pd->score += 1000; break;
			case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: pd->score +=  100; break;
			case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:    pd->score +=   10; break;
			case VK_PHYSICAL_DEVICE_TYPE_CPU:            pd->score +=    1; break;
			case VK_PHYSICAL_DEVICE_TYPE_OTHER:          pd->score +=    1; break;
		};
		if (pd->vkPhysicalDeviceProperties.apiVersion < VK_API_VERSION_1_3) {
			Logf("  Rejecting device: need Vulkan 1.3: apiVersion={}", VersionStr(temp, pd->vkPhysicalDeviceProperties.apiVersion));
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
			Logf("    [{}] heapIndex={}, flags={}", j, mt.heapIndex, MemoryPropertyFlagsStr(temp, mt.propertyFlags));
		}
		Logf("  {} memory heaps:", pd->vkPhysicalDeviceMemoryProperties.memoryHeapCount);
		for (u64 j = 0; j < pd->vkPhysicalDeviceMemoryProperties.memoryHeapCount; j++) {
			const VkMemoryHeap mh = pd->vkPhysicalDeviceMemoryProperties.memoryHeaps[j];
			Logf("    [{}] size={}, flags={}", j, SizeStr(temp, mh.size), MemoryHeapFlagsStr(temp, mh.flags));
		}

		Array<VkQueueFamilyProperties> vkQueueFamilyProperties(temp);
		vkGetPhysicalDeviceQueueFamilyProperties(pd->vkPhysicalDevice, &n, nullptr);
		vkGetPhysicalDeviceQueueFamilyProperties(pd->vkPhysicalDevice, &n, vkQueueFamilyProperties.Resize(n));

		pd->vkExtensionProperties.Init(perm);
		CheckVk(vkEnumerateDeviceExtensionProperties(pd->vkPhysicalDevice, 0, &n, 0));
		CheckVk(vkEnumerateDeviceExtensionProperties(pd->vkPhysicalDevice, 0, &n, pd->vkExtensionProperties.Resize(n)));

		Array<char> extensionsStr(temp);
		for (u64 j = 0; j < pd->vkExtensionProperties.len; j++) {
			Fmt(&extensionsStr, "{}(specVersion={}), ", pd->vkExtensionProperties[j].extensionName, VersionStr(temp, pd->vkExtensionProperties[j].specVersion));
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

		pd->vkSurfaceFormats.Init(perm);
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
			Logf("    {}, {}", FormatStr(pd->vkSurfaceFormats[j].format), ColorSpaceStr(pd->vkSurfaceFormats[j].colorSpace));
		}
		Logf("  Selected surface format: {}/{}", FormatStr(pd->vkSurfaceFormat), ColorSpaceStr(pd->vkSurfaceColorSpace));

		pd->vkPresentModes.Init(perm);
		CheckVk(vkGetPhysicalDeviceSurfacePresentModesKHR(pd->vkPhysicalDevice, vkSurface, &n, 0));
		CheckVk(vkGetPhysicalDeviceSurfacePresentModesKHR(pd->vkPhysicalDevice, vkSurface, &n, pd->vkPresentModes.Resize(n)));
		Logf("  {} present modes:", pd->vkPresentModes.len);
		for (u64 j = 0; j < pd->vkPresentModes.len; j++) {
			Logf("    {}", PresentModeStr(pd->vkPresentModes[j]));
		}

		pd->queueFamilies.Init(perm);
		pd->queueFamilies.Resize(vkQueueFamilyProperties.len);
		Logf("  {} queue families:", pd->queueFamilies.len);
		for (u64 j = 0; j < pd->queueFamilies.len; j++) {
			const VkQueueFamilyProperties* props = &pd->queueFamilies[j].vkQueueFamilyProperties;
			VkBool32 supportsPresent = VK_FALSE;
			CheckVk(vkGetPhysicalDeviceSurfaceSupportKHR(pd->vkPhysicalDevice, (u32)j, vkSurface, &supportsPresent));
			pd->queueFamilies[j].vkQueueFamilyProperties = vkQueueFamilyProperties[j];
			pd->queueFamilies[j].supportsPresent         = (supportsPresent == VK_TRUE);
			Logf("    [{}] count={}, flags={}, supportsPresent={}", j, props->queueCount, QueueFlagsStr(temp, props->queueFlags), pd->queueFamilies[j].supportsPresent);
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
		return MakeErr(temp, Err_NoDevice);
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
	vkPhysicalDeviceVulkan12Features.descriptorBindingPartiallyBound = true;
	vkPhysicalDeviceVulkan12Features.runtimeDescriptorArray = true;
	vkPhysicalDeviceVulkan12Features.descriptorBindingSampledImageUpdateAfterBind = true;

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
		
	LoadDeviceFns(vkDevice);

	vkGetDeviceQueue(vkDevice, physicalDevice->queueFamily, 0, &vkQueue);

	return Ok();
}

//-------------------------------------------------------------------------------------------------

Res<> InitSwapchain(u32 width, u32 height) {
	VkSurfaceCapabilitiesKHR vkSurfaceCapabilities;
	CheckVk(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice->vkPhysicalDevice, vkSurface, &vkSurfaceCapabilities));
	
	u32 imageCount = Max((u32)3, vkSurfaceCapabilities.minImageCount);
	if (vkSurfaceCapabilities.maxImageCount > 0 && imageCount > vkSurfaceCapabilities.maxImageCount) {
		imageCount = vkSurfaceCapabilities.maxImageCount;
	}
	Logf("Selected swapchain image count: {}", imageCount);

	// TODO: check min/max extents
	const VkExtent2D vkExtent = (vkSurfaceCapabilities.currentExtent.width != U32Max)
		? vkSurfaceCapabilities.currentExtent
		: VkExtent2D {
			.width  = Clamp(width,  vkSurfaceCapabilities.minImageExtent.width,  vkSurfaceCapabilities.maxImageExtent.width),
			.height = Clamp(height, vkSurfaceCapabilities.minImageExtent.height, vkSurfaceCapabilities.maxImageExtent.height),
		};
	Logf("Selected swapchain extent: ({}, {})", vkExtent.width, vkExtent.height);

	VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;
	for (u64 i = 0; i < physicalDevice->vkPresentModes.len; i++) {
		if (physicalDevice->vkPresentModes[i] == VK_PRESENT_MODE_MAILBOX_KHR) {
			presentMode = VK_PRESENT_MODE_MAILBOX_KHR;
		}
	}
	Logf("Selected swapchain present mode: {}", PresentModeStr(presentMode));

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
		.preTransform          = (vkSurfaceCapabilities.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR) ? VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR : vkSurfaceCapabilities.currentTransform,
		.compositeAlpha        = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
		.presentMode           = presentMode,
		.clipped               = VK_TRUE,
		.oldSwapchain          = 0,
	};
	CheckVk(vkCreateSwapchainKHR(vkDevice, &vkSwapchainCreateInfoKHR, vkAllocationCallbacks, &vkSwapchain));
	vkSwapchainExtent2D.width  = width;
	vkSwapchainExtent2D.height = height;

	u32 n = 0;
	CheckVk(vkGetSwapchainImagesKHR(vkDevice, vkSwapchain, &n, 0));
	CheckVk(vkGetSwapchainImagesKHR(vkDevice, vkSwapchain, &n, vkSwapchainImages.Resize(n)));

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

	return Ok();
}

//----------------------------------------------------------------------------------------------

Res<> InitFrameSyncObjects() {
	for (u64 i = 0; i < MaxFrames; i++) {
		constexpr VkSemaphoreCreateInfo vkSemaphoreCreateInfo = {
			.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
			.pNext = 0,
			.flags = 0,
		};
		CheckVk(vkCreateSemaphore(vkDevice, &vkSemaphoreCreateInfo, vkAllocationCallbacks, &vkFrameAcquireImageSemaphores[i]));
		CheckVk(vkCreateSemaphore(vkDevice, &vkSemaphoreCreateInfo, vkAllocationCallbacks, &vkFrameRenderSemaphores[i]));

		constexpr VkFenceCreateInfo vkFenceCreateInfo  = {
			.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
			.pNext = 0,
			.flags = VK_FENCE_CREATE_SIGNALED_BIT,
		};
		CheckVk(vkCreateFence(vkDevice, &vkFenceCreateInfo, vkAllocationCallbacks, &vkFrameRenderFences[i]));
	}

	return Ok();
}

//-------------------------------------------------------------------------------------------------

Res<> InitCommandBuffers() {
	const VkCommandPoolCreateInfo vkCommandPoolCreateInfo = {
		.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
		.pNext            = 0,
		.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
		.queueFamilyIndex = physicalDevice->queueFamily,
	};
	CheckVk(vkCreateCommandPool(vkDevice, &vkCommandPoolCreateInfo, vkAllocationCallbacks, &vkCommandPool));

	for (u64 i = 0; i < MaxFrames; i++) {
		const VkCommandBufferAllocateInfo vkCommandBufferAllocateInfo = {
			.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
			.pNext              = 0,
			.commandPool        = vkCommandPool,
			.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
			.commandBufferCount = 1,
		};
		CheckVk(vkAllocateCommandBuffers(vkDevice, &vkCommandBufferAllocateInfo, &vkFrameCommandBuffers[i]));
	}

	return Ok();
}

//----------------------------------------------------------------------------------------------

Res<> InitBindlessDescriptors() {
	constexpr VkDescriptorPoolSize vkDescriptorPoolSizes[] = {
		{ .type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, .descriptorCount = MaxBindlessSampledImages },
		{ .type = VK_DESCRIPTOR_TYPE_SAMPLER,       .descriptorCount = MaxBindlessSamplers },
	};
	const VkDescriptorPoolCreateInfo vkDescriptorPoolCreateInfo = {
		.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
		.pNext         = 0,
		.flags         = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT,
		.maxSets       = MaxBindlessDescriptorSets,
		.poolSizeCount = LenOf(vkDescriptorPoolSizes),
		.pPoolSizes    = vkDescriptorPoolSizes,
	};
	CheckVk(vkCreateDescriptorPool(vkDevice, &vkDescriptorPoolCreateInfo, vkAllocationCallbacks, &vkDescriptorPool));

	constexpr VkDescriptorBindingFlags vkDescriptorBindingFlags[] = {
		VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT_EXT,
	};
	const VkDescriptorSetLayoutBindingFlagsCreateInfo vkDescriptorSetLayoutBindingFlagsCreateInfo = {
		.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
		.pNext         = 0,
		.bindingCount  = 1,
		.pBindingFlags = vkDescriptorBindingFlags,
	};
	const VkDescriptorSetLayoutBinding vkMeshDescriptorSetLayoutBindings[] = {
		{
			.binding            = 0,
			.descriptorType     = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
			.descriptorCount    = MaxBindlessSampledImages,
			.stageFlags         = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
			.pImmutableSamplers = 0,
		},
		{
			.binding            = 1,
			.descriptorType     = VK_DESCRIPTOR_TYPE_SAMPLER,
			.descriptorCount    = MaxBindlessSamplers,
			.stageFlags         = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
			.pImmutableSamplers = 0,
		},
	};
	const VkDescriptorSetLayoutCreateInfo vkDescriptorSetLayoutCreateInfo = {
		.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		.pNext        = &vkDescriptorSetLayoutBindingFlagsCreateInfo,
		.flags        = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT,
		.bindingCount = LenOf(vkMeshDescriptorSetLayoutBindings),
		.pBindings    = vkMeshDescriptorSetLayoutBindings,
	};
	VkDescriptorSetLayout vkDescriptorSetLayout = VK_NULL_HANDLE;
	CheckVk(vkCreateDescriptorSetLayout(vkDevice, &vkDescriptorSetLayoutCreateInfo, vkAllocationCallbacks, &vkDescriptorSetLayout));

	const VkDescriptorSetAllocateInfo vkDescriptorSetAllocateInfo = {
		.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
		.pNext              = 0,
		.descriptorPool     = vkDescriptorPool,
		.descriptorSetCount = 1,
		.pSetLayouts        = &vkBindlessDescriptorSetLayout,
	};
	CheckVk(vkAllocateDescriptorSets(vkDevice, &vkDescriptorSetAllocateInfo, &vkBindlessDescriptorSet));

	return Ok();
}

//-------------------------------------------------------------------------------------------------

void AddBindlessSampler(VkSampler vkSampler) {
	Assert(vkBindlessSamplers.len < MaxBindlessSamplers);
	const VkDescriptorImageInfo vkDescriptorImageInfo = {
		.sampler     = vkSampler,
		.imageView   = 0,
		.imageLayout = VK_IMAGE_LAYOUT_GENERAL,
	};
	const VkWriteDescriptorSet vkWriteDescriptorSet = {
		.sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		.pNext            = 0,
		.dstSet           = vkBindlessDescriptorSet,
		.dstBinding       = 1,
		.dstArrayElement  = (u32)vkBindlessSamplers.len,
		.descriptorCount  = 1,
		.descriptorType   = VK_DESCRIPTOR_TYPE_SAMPLER,
		.pImageInfo       = &vkDescriptorImageInfo,
		.pBufferInfo      = 0,
		.pTexelBufferView = 0,
	};
	vkUpdateDescriptorSets(vkDevice, 1, &vkWriteDescriptorSet, 0, 0);
}

Res<> InitBindlessSamplers() {
	VkSampler vkSampler = VK_NULL_HANDLE;
	VkSamplerCreateInfo vkSamplerCreateInfo = {
		.sType                   = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
		.pNext                   = 0,
		.flags                   = 0,
		.magFilter               = VK_FILTER_NEAREST,
		.minFilter               = VK_FILTER_NEAREST,
		.mipmapMode              = VK_SAMPLER_MIPMAP_MODE_NEAREST,
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
	AddBindlessSampler(vkSampler);

	vkSamplerCreateInfo.magFilter  = VK_FILTER_LINEAR;
	vkSamplerCreateInfo.minFilter  = VK_FILTER_LINEAR;
	vkSamplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	vkSamplerCreateInfo.anisotropyEnable = VK_TRUE;
	vkSamplerCreateInfo.maxAnisotropy    = MaxAnisotropy;
	CheckVk(vkCreateSampler(vkDevice, &vkSamplerCreateInfo, vkAllocationCallbacks, &vkSampler));
	AddBindlessSampler(vkSampler);

	return Ok();
}

//----------------------------------------------------------------------------------------------
	
Res<> Init(const InitInfo* initInfo) {
	perm = initInfo->perm;
	temp = initInfo->temp;
	log  = initInfo->log;

	physicalDevices.Init(perm);
	vkSwapchainImages.Init(perm);
	vkSwapchainImageViews.Init(perm);
	vkFrameAcquireImageSemaphores.Init(perm, MaxFrames);
	vkFrameRenderSemaphores.Init(perm, MaxFrames);
	vkFrameRenderFences.Init(perm, MaxFrames);
	vkFrameCommandBuffers.Init(perm, MaxFrames);
	vkBindlessSamplers.Init(perm, MaxBindlessSamplers);

	LoadRootFns();
	if (Res<> r = InitInstance();                                   !r) { return r; }
	if (Res<> r = InitSurface(initInfo->windowPlatformInfo);        !r) { return r; }
	if (Res<> r = InitDevice();                                     !r) { return r; }
	if (Res<> r = InitSwapchain(initInfo->width, initInfo->height); !r) { return r; }
	if (Res<> r = InitFrameSyncObjects();                           !r) { return r; }
	if (Res<> r = InitCommandBuffers();                             !r) { return r; }
	if (Res<> r = InitBindlessDescriptors();                        !r) { return r; }
	if (Res<> r = InitBindlessSamplers();                           !r) { return r; }

	return Ok();
}

//----------------------------------------------------------------------------------------------

void Shutdown() {
	if (vkDevice) {
		vkDeviceWaitIdle(vkDevice);
	}

	DestroyVkArray(vkFrameAcquireImageSemaphores, vkDestroySemaphore);
	DestroyVkArray(vkFrameRenderSemaphores, vkDestroySemaphore);
	DestroyVkArray(vkFrameRenderFences, vkDestroyFence);
	vkFreeCommandBuffers(vkDevice, vkCommandPool, (u32)vkFrameCommandBuffers.len, vkFrameCommandBuffers.data);
	DestroyVk(vkCommandPool, vkDestroyCommandPool);
	DestroyVkArray(vkSwapchainImageViews, vkDestroyImageView);
	DestroyVk(vkSwapchain, vkDestroySwapchainKHR);
	if (vkDevice != VK_NULL_HANDLE) { vkDestroyDevice(vkDevice, vkAllocationCallbacks); vkDevice = VK_NULL_HANDLE; }
	if (vkSurface != VK_NULL_HANDLE) { vkDestroySurfaceKHR(vkInstance, vkSurface, vkAllocationCallbacks); vkSurface = VK_NULL_HANDLE; }
	if (vkDebugUtilsMessenger != VK_NULL_HANDLE) { vkDestroyDebugUtilsMessengerEXT(vkInstance, vkDebugUtilsMessenger, vkAllocationCallbacks); vkDebugUtilsMessenger = VK_NULL_HANDLE; }
	if (vkInstance != VK_NULL_HANDLE) { vkDestroyInstance(vkInstance, vkAllocationCallbacks); vkInstance = VK_NULL_HANDLE; }
}

//----------------------------------------------------------------------------------------------

Res<> ResizeSwapchain(u32 width, u32 height) {
	vkDeviceWaitIdle(vkDevice);

	DestroyVkArray(vkSwapchainImageViews, vkDestroyImageView);
	DestroyVk(vkSwapchain, vkDestroySwapchainKHR);

	return InitSwapchain(width, height);
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

VkImageSubresourceLayers MakeVkImageSubresourceLayers(VkImageAspectFlags vkImageAspectFlags) {
	return VkImageSubresourceLayers {
		.aspectMask      = vkImageAspectFlags,
		.mipLevel        = 0,
		.baseArrayLayer  = 0,
		.layerCount      = VK_REMAINING_ARRAY_LAYERS,
	};
}

//----------------------------------------------------------------------------------------------

// TODO: buffer alignment
Res<DeviceMem> AllocateDeviceMem(VkMemoryRequirements vkMemoryRequirements, VkMemoryPropertyFlags vkMemoryPropertyFlags, VkMemoryAllocateFlags vkMemoryAllocateFlags) {
	u32 memType = U32Max;
	for (u32 i = 0; i < physicalDevice->vkPhysicalDeviceMemoryProperties.memoryTypeCount; i++) {
		if (
			(vkMemoryRequirements.memoryTypeBits & (1 << i)) &&
			(physicalDevice->vkPhysicalDeviceMemoryProperties.memoryTypes[i].propertyFlags & vkMemoryPropertyFlags)
		) {
			memType = i;
		}
	}
	if (memType == U32Max) {
		return MakeErr(temp, Err_NoMem);
	}
	const VkMemoryAllocateFlagsInfo vkMemoryAllocateFlagsInfo = {
		.sType      = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO,
		.pNext      = 0,
		.flags      = vkMemoryAllocateFlags,
		.deviceMask = 0,
	};
	const VkMemoryAllocateInfo vkMemoryAllocateInfo = {
		.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
		.pNext           = &vkMemoryAllocateFlagsInfo,
		.allocationSize  = vkMemoryRequirements.size,
		.memoryTypeIndex = memType,
	};
	VkDeviceMemory vkDeviceMemory = VK_NULL_HANDLE;
	CheckVk(vkAllocateMemory(vkDevice, &vkMemoryAllocateInfo, vkAllocationCallbacks, &vkDeviceMemory));

	return DeviceMem {
		.vkDeviceMemory        = vkDeviceMemory,
		.offset                = 0,
		.size                  = vkMemoryRequirements.size,
		.type                  = memType,
		.vkMemoryPropertyFlags = vkMemoryPropertyFlags,
		.vkMemoryAllocateFlags = vkMemoryAllocateFlags,
	};
}

//-------------------------------------------------------------------------------------------------

void FreeDeviceMem(DeviceMem deviceMem) {
	if (deviceMem.vkDeviceMemory != VK_NULL_HANDLE) {
		vkFreeMemory(vkDevice, deviceMem.vkDeviceMemory, vkAllocationCallbacks);
	}
}

//-------------------------------------------------------------------------------------------------

Res<BufferObj> CreateBufferInternal(
	u64                   size,
	VkBufferUsageFlags    vkBufferUsageFlags,
	VkMemoryPropertyFlags vkMemoryPropertyFlags,
	VkMemoryAllocateFlags vkMemoryAllocateFlags
) {
	const VkBufferCreateInfo vkBufferCreateInfo = {
		.sType                 = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.pNext                 = 0,
		.flags                 = 0,
		.size                  = size,
		.usage                 = vkBufferUsageFlags,
		.sharingMode           = VK_SHARING_MODE_EXCLUSIVE,
		.queueFamilyIndexCount = 1,
		.pQueueFamilyIndices   = &physicalDevice->queueFamily,
	};
	VkBuffer vkBuffer = VK_NULL_HANDLE;
	CheckVk(vkCreateBuffer(vkDevice, &vkBufferCreateInfo, vkAllocationCallbacks, &vkBuffer));

	VkMemoryRequirements vkMemoryRequirements = {};
	vkGetBufferMemoryRequirements(vkDevice, vkBuffer, &vkMemoryRequirements);

	DeviceMem deviceMem = {};
	if (Res<> r = AllocateDeviceMem(vkMemoryRequirements, vkMemoryPropertyFlags, vkMemoryAllocateFlags).To(deviceMem); !r) {
		vkDestroyBuffer(vkDevice, vkBuffer, vkAllocationCallbacks);
		return r.err;
	}

	if (const VkResult r = vkBindBufferMemory(vkDevice, vkBuffer, deviceMem.vkDeviceMemory, 0); r != VK_SUCCESS) {
		vkDestroyBuffer(vkDevice, vkBuffer, vkAllocationCallbacks);
		FreeDeviceMem(deviceMem);
		return MakeVkErr(temp, r, vkBindBufferMemory);
	}

	u64 addr = 0;
	if (vkBufferUsageFlags & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT) {
		const VkBufferDeviceAddressInfo vkBufferDeviceAddressInfo = {
			.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
			.pNext = 0,
			.buffer = vkBuffer,
		};
		addr = vkGetBufferDeviceAddress(vkDevice, &vkBufferDeviceAddressInfo);
	}

	return BufferObj {
		.vkBuffer           = vkBuffer,
		.deviceMem          = deviceMem,
		.size               = size,
		.vkBufferUsageFlags = vkBufferUsageFlags,
		.addr               = addr,
	};
}

//----------------------------------------------------------------------------------------------

Res<Buffer> CreateBuffer(u64 size, BufferUsage usage) {
	Buffer buffer = {};

	VkBufferUsageFlags    vkBufferUsageFlags    = 0;
	VkMemoryPropertyFlags vkMemoryPropertyFlags = 0;
	VkMemoryAllocateFlags vkMemoryAllocateFlags = 0;
	switch (usage) {
		case BufferUsage::Vertex:
			vkBufferUsageFlags    = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT  | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
			vkMemoryPropertyFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
			vkMemoryAllocateFlags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;
			break;

		case BufferUsage::Index:
			vkBufferUsageFlags    = VK_BUFFER_USAGE_INDEX_BUFFER_BIT   | VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
			vkMemoryPropertyFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
			vkMemoryAllocateFlags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;
			break;

		case BufferUsage::Uniform:
			vkBufferUsageFlags    = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
			vkMemoryPropertyFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
			vkMemoryAllocateFlags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;
			break;

		case BufferUsage::Storage:
			vkBufferUsageFlags    = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
			vkMemoryPropertyFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
			vkMemoryAllocateFlags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;
			break;

		case BufferUsage::Indirect:
			vkBufferUsageFlags    = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
			vkMemoryPropertyFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
			vkMemoryAllocateFlags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;
			break;

		case BufferUsage::Readback:
			vkBufferUsageFlags    = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
			vkMemoryPropertyFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
			vkMemoryAllocateFlags = 0;
			break;

		default: Panic("Unhandled BufferUsage {}", usage);
	};

	BufferObj* const bufferObj = bufferObjs.Alloc();
	if (Res<> r = CreateBufferInternal(
		size,
		vkBufferUsageFlags,
		vkMemoryPropertyFlags,
		vkMemoryAllocateFlags
	).To(*bufferObj); !r) {
		bufferObjs.Free(bufferObj);
		return r.err;
	}

	return bufferObjs.GetHandle(bufferObj);
}

//----------------------------------------------------------------------------------------------
	
void DestroyBuffer(Buffer buffer) {
	if (buffer.handle) {
		BufferObj* const bufferObj = bufferObjs.Get(buffer);
		FreeDeviceMem(bufferObj->deviceMem);
		DestroyVk(bufferObj->vkBuffer, vkDestroyBuffer);
		MemSet(bufferObj, 0, sizeof(*bufferObj));
		bufferObjs.Free(bufferObj);
	}
}

//----------------------------------------------------------------------------------------------

u64 GetBufferAddr(Buffer buffer) {
	return bufferObjs.Get(buffer)->addr;
}

//----------------------------------------------------------------------------------------------

struct StagingAllocation {
	u64 offset = 0;
	u64 size   = 0;
};

static constexpr u32 MaxStagingAllocations = 1024;
static constexpr u64 StagingBufferSize     = 64 * 1024 * 1024;

static BufferObj         stagingBufferObj;
static u8*               stagingBufferMappedPtr;
static u64               stagingBufferUsed;
static StagingAllocation stagingAllocations[MaxStagingAllocations];
static u32               stagingAllocationsLen;

Res<> InitStaging() {
	if (Res<> r = CreateBufferInternal(
		StagingBufferSize,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		0
	).To(stagingBufferObj); !r) {
		return r.err;
	}

	CheckVk(vkMapMemory(vkDevice, stagingBufferObj.deviceMem.vkDeviceMemory, 0, StagingBufferSize, 0, (void**)&stagingBufferMappedPtr));
	stagingBufferUsed     = 0;
	stagingAllocationsLen = 1;

	return Ok();
};

void ResetStaging() {
	stagingBufferUsed     = 0;
	stagingAllocationsLen = 1;
}

Res<StagingAllocation*> CreateStagingAllocation(u64 size) {
	Assert(stagingAllocationsLen < MaxStagingAllocations);
	Assert(stagingBufferUsed + size < stagingBufferObj.size);

	StagingAllocation* const stagingAllocation = &stagingAllocations[stagingAllocationsLen - 1];
	stagingAllocationsLen++;

	stagingAllocation->offset = stagingBufferUsed;
	stagingAllocation->size   = size;
	stagingBufferUsed += size;

	return stagingAllocation;
};

//-------------------------------------------------------------------------------------------------

Res<BufferUpdate> BeginBufferUpdate(Buffer buffer) {
	BufferObj* const bufferObj = bufferObjs.Get(buffer);

	StagingAllocation* stagingAllocation = 0;
	if (Res<> r = CreateStagingAllocation(bufferObj->size).To(stagingAllocation); !r) {
		return r.err;
	}
	
	return BufferUpdate {
		.handle = (u64)(stagingAllocation - stagingAllocations),
		.buffer = buffer,
		.ptr    = stagingBufferMappedPtr + stagingAllocation->offset,
	};
}

//-------------------------------------------------------------------------------------------------

void EndBufferUpdate(BufferUpdate bufferUpdate) {
	Assert(bufferUpdate.handle > 0 && bufferUpdate.handle < (u64)stagingAllocationsLen);
	StagingAllocation* const stagingAllocation = &stagingAllocations[bufferUpdate.handle];
	BufferObj* const bufferObj = bufferObjs.Get(bufferUpdate.buffer);
	const VkBufferCopy2 vkBufferCopy2 = {
		.sType     = VK_STRUCTURE_TYPE_BUFFER_COPY_2,
		.pNext     = 0,
		.srcOffset = stagingAllocation->offset,
		.dstOffset = bufferObj->deviceMem.offset,
		.size      = stagingAllocation->size,
	};
	const VkCopyBufferInfo2 vkCopyBufferInfo2 = {
		.sType       = VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2,
		.pNext       = 0,
		.srcBuffer   = stagingBufferObj.vkBuffer,
		.dstBuffer   = bufferObj->vkBuffer,
		.regionCount = 1,
		.pRegions    = &vkBufferCopy2,
	};
	vkCmdCopyBuffer2(vkFrameCommandBuffers[frameIndex], &vkCopyBufferInfo2);
}

//-------------------------------------------------------------------------------------------------

void BufferBarrier(Buffer buffer, u64 srcStage, u64 srcAccess, u64 dstStage, u64 dstAccess) {
	BufferObj* const bufferObj = bufferObjs.Get(buffer);
	const VkBufferMemoryBarrier2 vkBufferMemoryBarrier2 = {
		.sType               = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
		.pNext               = 0,
		.srcStageMask        = srcStage,
		.srcAccessMask       = srcAccess,
		.dstStageMask        = dstStage,
		.dstAccessMask       = dstAccess,
		.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.buffer              = bufferObj->vkBuffer,
		.offset              = bufferObj->deviceMem.offset,
		.size                = bufferObj->size,
	};
	const VkDependencyInfo vkDependencyInfo = {
		.sType                    = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
		.pNext                    = 0,
		.dependencyFlags          = 0,
		.memoryBarrierCount       = 0,
		.pMemoryBarriers          = 0,
		.bufferMemoryBarrierCount = 1,
		.pBufferMemoryBarriers    = &vkBufferMemoryBarrier2,
		.imageMemoryBarrierCount  = 0,
		.pImageMemoryBarriers     = 0,
	};
	vkCmdPipelineBarrier2(vkFrameCommandBuffers[frameIndex], &vkDependencyInfo);
}

//-------------------------------------------------------------------------------------------------

static VkFormat ImageFormatToVkFormat(ImageFormat format) {
	switch (format) {
		case ImageFormat::Undefined: return VK_FORMAT_UNDEFINED;
		default: Panic("Unhandled ImageFormat {}", format);
	}
}

//-------------------------------------------------------------------------------------------------

Res<Image> CreateImage(u32 width, u32 height, ImageFormat format, ImageUsage usage, Sampler sampler) {
	const VkFormat vkFormat = ImageFormatToVkFormat(format);

	VkImageUsageFlags     vkImageUsageFlags     = 0;
	VkMemoryPropertyFlags vkMemoryPropertyFlags = 0;
	switch (usage) {
		case ImageUsage::Sampled:
			vkImageUsageFlags     = VK_IMAGE_USAGE_SAMPLED_BIT;
			vkMemoryPropertyFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
			break;

		case ImageUsage::ColorAttachment:
			vkImageUsageFlags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
			vkMemoryPropertyFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
			break;

		case ImageUsage::DepthStencilAttachment:
			vkImageUsageFlags = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
			vkMemoryPropertyFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
			break;

		default: Panic("Unhandled ImageUsage {}", usage);
	}

	const VkImageCreateInfo vkImageCreateInfo = {
		.sType                 = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		.pNext                 = 0,
		.flags                 = 0,
		.imageType             = VK_IMAGE_TYPE_2D,
		.format                = vkFormat,
		.extent                = { .width = width, .height = height, .depth = 1 },
		.mipLevels             = 1,
		.arrayLayers           = 1,
		.samples               = VK_SAMPLE_COUNT_1_BIT,
		.tiling                = VK_IMAGE_TILING_OPTIMAL,
		.usage                 = vkImageUsageFlags,
		.sharingMode           = VK_SHARING_MODE_EXCLUSIVE,
		.queueFamilyIndexCount = 0,
		.pQueueFamilyIndices   = 0,
		.initialLayout         = VK_IMAGE_LAYOUT_UNDEFINED,
	};
	VkImage vkImage = VK_NULL_HANDLE;
	CheckVk(vkCreateImage(vkDevice, &vkImageCreateInfo, vkAllocationCallbacks, &vkImage));

	VkMemoryRequirements vkMemoryRequirements = {};
	vkGetImageMemoryRequirements(vkDevice, vkImage, &vkMemoryRequirements);

	DeviceMem deviceMem = {};
	if (Res<> r = AllocateDeviceMem(vkMemoryRequirements, vkMemoryPropertyFlags, 0).To(deviceMem); !r) {
		vkDestroyImage(vkDevice, vkImage, vkAllocationCallbacks);
		return r.err;
	}
		
	if (const VkResult r = vkBindImageMemory(vkDevice, vkImage, deviceMem.vkDeviceMemory, 0); r != VK_SUCCESS) {
		vkDestroyImage(vkDevice, vkImage, vkAllocationCallbacks);
		FreeDeviceMem(deviceMem);
		return MakeVkErr(temp, r, vkBindImageMemory);
	}

	const VkImageViewCreateInfo vkImageViewCreateInfo = {
		.sType              = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.pNext              = 0,
		.flags              = 0,
		.image              = vkImage,
		.viewType           = VK_IMAGE_VIEW_TYPE_2D,
		.format             = vkFormat,
		.components         = { .r = VK_COMPONENT_SWIZZLE_IDENTITY, .g = VK_COMPONENT_SWIZZLE_IDENTITY, .b = VK_COMPONENT_SWIZZLE_IDENTITY, .a = VK_COMPONENT_SWIZZLE_IDENTITY },
		.subresourceRange   = MakeVkSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT),
	};
	VkImageView vkImageView = VK_NULL_HANDLE;
	if (VkResult r = vkCreateImageView(vkDevice, &vkImageViewCreateInfo, vkAllocationCallbacks, &vkImageView); r != VK_SUCCESS) {
		vkDestroyImage(vkDevice, vkImage, vkAllocationCallbacks);
		FreeDeviceMem(deviceMem);
		return MakeVkErr(temp, r, vkCreateImageView);
	}

	ImageObj* const imageObj = imageObjs.Alloc();
	imageObj->vkImage       = vkImage;
	imageObj->vkImageView   = vkImageView;
	imageObj->deviceMem     = deviceMem;
	imageObj->width         = width;
	imageObj->height        = height;
	imageObj->vkFormat      = vkFormat;
	imageObj->vkImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageObj->sampler       = sampler;

	return imageObjs.GetHandle(imageObj);

}
//-------------------------------------------------------------------------------------------------

void DestroyImage(Image image) {
	if (image.handle) {
		ImageObj* const imageObj = imageObjs.Get(image);
		FreeDeviceMem(imageObj->deviceMem);
		DestroyVk(imageObj->vkImageView, vkDestroyImageView);
		DestroyVk(imageObj->vkImage, vkDestroyImage);
	}
}

//-------------------------------------------------------------------------------------------------

static VkImageLayout ImageLayoutToVkLayout(ImageLayout layout) {
	switch (layout) {
		case ImageLayout::Undefined: return VK_IMAGE_LAYOUT_UNDEFINED;
		default: Panic("Unhandled ImageLayout {}", layout);
	}
}

//-------------------------------------------------------------------------------------------------

void BindImage(Image image, u32 idx, ImageLayout layout) {
	ImageObj* const imageObj = imageObjs.Get(image);
	const VkDescriptorImageInfo vkDescriptorImageInfo = {
		.sampler     = 0,
		.imageView   = imageObj->vkImageView,
		.imageLayout = ImageLayoutToVkLayout(layout),
	};
	const VkWriteDescriptorSet vkWriteDescriptorSet = {
		.sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		.pNext            = 0,
		.dstSet           = vkBindlessDescriptorSet,
		.dstBinding       = 1,
		.dstArrayElement  = idx,
		.descriptorCount  = 1,
		.descriptorType   = VK_DESCRIPTOR_TYPE_SAMPLER,
		.pImageInfo       = &vkDescriptorImageInfo,
		.pBufferInfo      = 0,
		.pTexelBufferView = 0,
	};
	vkUpdateDescriptorSets(vkDevice, 1, &vkWriteDescriptorSet, 0, 0);

}

//-------------------------------------------------------------------------------------------------

Res<ImageUpdate> BeginImageUpdate(Image image, u32 rowLen) {
	ImageObj* const imageObj = imageObjs.Get(image);

	StagingAllocation* stagingAllocation = 0;
	if (Res<> r = CreateStagingAllocation(imageObj->height * rowLen).To(stagingAllocation); !r) {
		return r.err;
	}
	
	return ImageUpdate {
		.handle = (u64)(stagingAllocation - stagingAllocations),
		.image  = image,
		.ptr    = stagingBufferMappedPtr + stagingAllocation->offset,
		.rowLen = rowLen,
	};
}

//-------------------------------------------------------------------------------------------------

void EndImageUpdate(ImageUpdate imageUpdate) {
	Assert(imageUpdate.handle > 0 && imageUpdate.handle < (u64)stagingAllocationsLen);
	StagingAllocation* const stagingAllocation = &stagingAllocations[imageUpdate.handle];
	ImageObj* const imageObj = imageObjs.Get(imageUpdate.image);
	const VkBufferImageCopy2 vkBufferImageCopy2 = {
		.sType             = VK_STRUCTURE_TYPE_BUFFER_IMAGE_COPY_2,
		.pNext             = 0,
		.bufferOffset      = stagingAllocation->offset,
		.bufferRowLength   = imageUpdate.rowLen,
		.bufferImageHeight = imageObj->height,
		.imageSubresource  = MakeVkImageSubresourceLayers(imageObj->vkImageLayout == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT),
		.imageOffset       = { .x = 0, .y = 0, .z = 0 },
		.imageExtent       = { .width = imageObj->width, .height = imageObj->height, .depth = 1 },

	};
	const VkCopyBufferToImageInfo2 vkCopyBufferToImageInfo2 = {
		.sType          = VK_STRUCTURE_TYPE_COPY_BUFFER_TO_IMAGE_INFO_2,
		.pNext          = 0,
		.srcBuffer      = stagingBufferObj.vkBuffer,
		.dstImage       = imageObj->vkImage,
		.dstImageLayout = imageObj->vkImageLayout,
		.regionCount    = 1,
		.pRegions       = &vkBufferImageCopy2,
	};
	vkCmdCopyBufferToImage2(vkFrameCommandBuffers[frameIndex], &vkCopyBufferToImageInfo2);
}

//-------------------------------------------------------------------------------------------------

void ImageBarrier(Image image, u64 srcStage, u64 srcAccess, u64 dstLayout, u64 dstStage, u64 dstAccess) {
	ImageObj* const imageObj = imageObjs.Get(image);
	const VkImageMemoryBarrier2 vkImageMemoryBarrier2 = {
		.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
		.pNext               = 0,
		.srcStageMask        = srcStage,
		.srcAccessMask       = srcAccess,
		.dstStageMask        = dstStage,
		.dstAccessMask       = dstAccess,
		.oldLayout           = imageObj->vkImageLayout,
		.newLayout           = (VkImageLayout)dstLayout,
		.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.image               = imageObj->vkImage,
		.subresourceRange    = MakeVkSubresourceRange(imageObj->vkImageLayout == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT),
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
	vkCmdPipelineBarrier2(vkFrameCommandBuffers[frameIndex], &vkDependencyInfo);
	imageObj->vkImageLayout = (VkImageLayout)dstLayout;
}

//----------------------------------------------------------------------------------------------


//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------


Res<Shader> CreateShader(const void* data, u64 len) {
	const VkShaderModuleCreateInfo vkShaderModuleCreateInfo = {
		.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		.pNext    = 0,
		.flags    = 0,
		.codeSize = (u32)len,
		.pCode    = (const u32*)data,

	};
	VkShaderModule vkShaderModule = VK_NULL_HANDLE;
	CheckVk(vkCreateShaderModule(vkDevice, &vkShaderModuleCreateInfo, vkAllocationCallbacks, &vkShaderModule));

	SpvReflectShaderModule spvReflectShaderModule = {};
	if (const SpvReflectResult r = spvReflectCreateShaderModule(len, data, &spvReflectShaderModule); r != SPV_REFLECT_RESULT_SUCCESS) {
		return MakeErr(temp, ErrCode { .ns = "spirv", .code = (i64)r });
	}

	
	if (spvReflectShaderModule.push_constant_block_count > 1) {
		return MakeErr(temp, Err_ShaderTooManyPushConstantBlocks
	}

	spvReflectShaderModule.push_constant_blocks
	ShaderObj* const shaderObj = shaderObjs.Alloc();
	shaderObj->vkShaderModule      = vkShaderModule;
	shaderObj->vkShaderStageFlags  = (VkShaderStageFlags)spvReflectShaderModule.shader_stage;	// SPV flags defined 1:1 with VK flags
	shaderObj->pushConstantsOffset = ;
	shaderObj->pushConstantsSize   = ;

	return shaderObjs.GetHandle(shaderObj);
}

//-------------------------------------------------------------------------------------------------

void DestroyShader(Shader shader) {
	if (shader.handle) {
		ShaderObj* const shaderObj = shaderObjs.Get(shader);
		DestroyVk(shaderObj->vkShaderModule, vkDestroyShaderModule);
		shaderObjs.Free(shaderObj);
	}
}

//----------------------------------------------------------------------------------------------

Res<Pipeline> CreateGraphicsPipeline(Span<Shader> shaders) {
	Assert(shaders.len > 0);

	VkShaderStageFlags vkShaderStageFlags = 0;
	VkPushConstantRange vkPushConstantRange = {};
	VkPipelineShaderStageCreateInfo* const vkPipelineShaderStageCreateInfos = temp->AllocT<VkPipelineShaderStageCreateInfo>(shaders.len);
	for (u64 i = 0; i < shaders.len; i++) {
		ShaderObj* const shaderObj = shaderObjs.Get(shaders[i]);

		vkShaderStageFlags |= shaderObj->vkShaderStageFlags;

		if (shaderObj->pushConstantsSize) {
			vkPushConstantRange.stageFlags |= shaderObj->vkShaderStageFlags;
			if (!vkPushConstantRange.size) {
				vkPushConstantRange.offset = shaderObj->pushConstantsOffset;
				vkPushConstantRange.size   = shaderObj->pushConstantsSize;
			} else {
				Assert(vkPushConstantRange.offset == shaderObj->pushConstantsOffset);
				Assert(vkPushConstantRange.size   == shaderObj->pushConstantsSize);
			}
		}

		vkPipelineShaderStageCreateInfos[i] = VkPipelineShaderStageCreateInfo {
			.sType               = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.pNext               = 0,
			.flags               = 0,
			.stage               = VK_SHADER_STAGE_VERTEX_BIT,
			.module              = shaderObj->vkShaderModule,
			.pName               = shaderObj->entry,
			.pSpecializationInfo = 0,
		};
	}

	const VkPipelineLayoutCreateInfo vkPipelineLayoutCreateInfo = {
		.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.pNext                  = 0,
		.flags                  = 0,
		.setLayoutCount         = 1,
		.pSetLayouts            = &vkBindlessDescriptorSetLayout,
		.pushConstantRangeCount = 1,
		.pPushConstantRanges    = &vkPushConstantRange,
	};
	VkPipelineLayout vkPipelineLayout;
	if (const VkResult r = vkCreatePipelineLayout(vkDevice, &vkPipelineLayoutCreateInfo, vkAllocationCallbacks, &vkPipelineLayout); r != VK_SUCCESS) {
		return MakeVkErr(temp, r, vkCreatePipelineLayout);
	}

	const VkPipelineVertexInputStateCreateInfo vkPipelineVertexInputStateCreateInfo = {
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
	const VkPipelineTessellationStateCreateInfo vkPipelineTessellationStateCreateInfo = {
		.sType              = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO,
		.pNext              = 0,
		.flags              = 0,
		.patchControlPoints = 0,
	};
	const VkPipelineViewportStateCreateInfo vkPipelineViewportStateCreateInfo = {
		.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
		.pNext         = 0,
		.flags         = 0,
		.viewportCount = 0,
		.pViewports    = 0,
		.scissorCount  = 0,
		.pScissors     = 0,
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
		.depthTestEnable       = VK_TRUE,
		.depthWriteEnable      = VK_TRUE,
		.depthCompareOp        = VK_COMPARE_OP_LESS,
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
		.stageCount          = (u32)shaders.len,
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
		.layout              = vkPipelineLayout,
		.renderPass          = VK_NULL_HANDLE,
		.subpass             = 0,
		.basePipelineHandle  = VK_NULL_HANDLE,
		.basePipelineIndex   = 0,
	};
	VkPipeline vkPipeline = VK_NULL_HANDLE;
	if (const VkResult r = vkCreateGraphicsPipelines(vkDevice, 0, 1, &vkGraphicsPipelineCreateInfo, vkAllocationCallbacks, &vkPipeline); r != VK_SUCCESS) {
		vkDestroyPipelineLayout(vkDevice, vkPipelineLayout, vkAllocationCallbacks);
		return MakeVkErr(temp, r, vkCreateGraphicsPipelines);
	}

	PipelineObj* const pipelineObj = pipelineObjs.Alloc();
	pipelineObj->vkPipelineLayout    = vkPipelineLayout;
	pipelineObj->vkPipeline          = vkPipeline;
	pipelineObj->vkPipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	pipelineObj->vkPushConstantRange = vkPushConstantRange;

	return pipelineObjs.GetHandle(pipelineObj);
}

//-------------------------------------------------------------------------------------------------

void DestroyPipeline(Pipeline pipeline) {
	if (pipeline.handle) {
		PipelineObj* const pipelineObj = pipelineObjs.Get(pipeline);
		DestroyVk(pipelineObj->vkPipeline, vkDestroyPipeline);
		DestroyVk(pipelineObj->vkPipelineLayout, vkDestroyPipelineLayout);
		pipelineObjs.Free(pipelineObj);
	}
}

//-------------------------------------------------------------------------------------------------

Res<> BeginFrame() {
	CheckVk(vkWaitForFences(vkDevice, 1, &vkFrameRenderFences[frameIndex], VK_TRUE, U64Max));
	CheckVk(vkResetFences(vkDevice, 1, &vkFrameRenderFences[frameIndex]));

	if (VkResult r = vkAcquireNextImageKHR(vkDevice, vkSwapchain, U64Max, vkFrameAcquireImageSemaphores[frameIndex], 0, &swapchainImageIndex); r != VK_SUCCESS) {
		if (r == VK_SUBOPTIMAL_KHR || r == VK_ERROR_OUT_OF_DATE_KHR) {
			return MakeVkErr(temp, r, "vkAcquireNextImageKHR")->Push(Err_Resize);
		} else {
			return MakeVkErr(temp, r, "vkAcquireNextImageKHR");
		}
	}

	constexpr VkCommandBufferBeginInfo vkCommandBufferBeginInfo = {
		.sType            = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.pNext            = 0,
		.flags            = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
		.pInheritanceInfo = 0,
	};
	CheckVk(vkBeginCommandBuffer(vkFrameCommandBuffers[frameIndex], &vkCommandBufferBeginInfo));

	return Ok();
};

//----------------------------------------------------------------------------------------------

Res<> EndFrame() {
	CheckVk(vkEndCommandBuffer(vkFrameCommandBuffers[frameIndex]));

	const VkSemaphoreSubmitInfo vkWaitSemaphoreSubmitInfo = {
		.sType       = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
		.pNext       = 0,
		.semaphore   = vkFrameAcquireImageSemaphores[frameIndex],
		.value       = 0,
		.stageMask   = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR,
		.deviceIndex = 0,
	};
	const VkCommandBufferSubmitInfo vkCommandBufferSubmitInfo = {
		.sType         = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
		.pNext         = 0,
		.commandBuffer = vkFrameCommandBuffers[frameIndex],
		.deviceMask    = 0,
	};
	const VkSemaphoreSubmitInfo vkSignalSemaphoreSubmitInfo = {
		.sType       = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
		.pNext       = 0,
		.semaphore   = vkFrameRenderSemaphores[frameIndex],
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
	CheckVk(vkQueueSubmit2(vkQueue, 1, &vkSubmitInfo2, vkFrameRenderFences[frameIndex]));

	VkPresentInfoKHR vkPresentInfoKHR = {
		.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
		.pNext              = 0,
		.waitSemaphoreCount = 1,
		.pWaitSemaphores    = &vkFrameRenderSemaphores[frameIndex],
		.swapchainCount     = 1,
		.pSwapchains        = &vkSwapchain,
		.pImageIndices      = &swapchainImageIndex,
		.pResults           = 0,
	};
	if (VkResult r = vkQueuePresentKHR(vkQueue, &vkPresentInfoKHR); r != VK_SUCCESS) {
		if (r == VK_SUBOPTIMAL_KHR || r == VK_ERROR_OUT_OF_DATE_KHR) {
			return MakeVkErr(temp, r, "vkAcquireNextImageKHR")->Push(Err_Resize);
		} else {
			return MakeVkErr(temp, r, "vkAcquireNextImageKHR");
		}
	}

	frameIndex = (frameIndex + 1) % MaxFrames;

	return Ok();
}

//----------------------------------------------------------------------------------------------

void BeginPass(const Pass* pass) {
	VkRenderingAttachmentInfo* const vkColorRenderingAttachmentInfos = temp->AllocT<VkRenderingAttachmentInfo>(pass->colorAttachments.len);
	for (u64 i = 0; i < pass->colorAttachments.len; i++) {
		ImageObj* const imageObj = imageObjs.Get(pass->colorAttachments[i]);
		vkColorRenderingAttachmentInfos[i] = {
			.sType              = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
			.pNext              = 0,
			.imageView          = imageObj->vkImageView,
			.imageLayout        = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			.resolveMode        = VK_RESOLVE_MODE_NONE,
			.resolveImageView   = VK_NULL_HANDLE,
			.resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
			.loadOp             = VK_ATTACHMENT_LOAD_OP_LOAD,
			.storeOp            = VK_ATTACHMENT_STORE_OP_STORE,
			.clearValue         = { .color = { .float32 = { 0.0f, 0.0f, 0.0f, 1.0f } } },
		};
	}

	ImageObj* depthImageObj = 0;
	VkRenderingAttachmentInfo vkDepthRenderingAttachmentInfo;
	if (pass->depthAttachment.handle) {
		depthImageObj = imageObjs.Get(pass->depthAttachment);
		vkDepthRenderingAttachmentInfo = {
			.sType              = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
			.pNext              = 0,
			.imageView          = depthImageObj->vkImageView,
			.imageLayout        = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
			.resolveMode        = VK_RESOLVE_MODE_NONE,
			.resolveImageView   = VK_NULL_HANDLE,
			.resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
			.loadOp             = VK_ATTACHMENT_LOAD_OP_LOAD,
			.storeOp            = VK_ATTACHMENT_STORE_OP_STORE,
			.clearValue         = { .depthStencil = { .depth = 1.0f, .stencil = 0 } },
		};
	}

	const VkRenderingInfo vkRenderingInfo = {
		.sType                = VK_STRUCTURE_TYPE_RENDERING_INFO,
		.pNext                = 0,
		.flags                = 0,
		.renderArea           = { .offset = { 0, 0 }, .extent = { .width = (u32)pass->viewport.width, .height = (u32)pass->viewport.height } },
		.layerCount           = 1,
		.viewMask             = 0,
		.colorAttachmentCount = (u32)pass->colorAttachments.len,
		.pColorAttachments    = vkColorRenderingAttachmentInfos,
		.pDepthAttachment     = depthImageObj ? &vkDepthRenderingAttachmentInfo : 0,
		.pStencilAttachment   = 0,
	};
	vkCmdBeginRendering(vkFrameCommandBuffers[frameIndex], &vkRenderingInfo);

	const VkViewport vkViewport = {
		.x        = pass->viewport.x,
		.y        = pass->viewport.y,
		.width    = pass->viewport.width,
		.height   = pass->viewport.height,
		.minDepth = 0.0f,
		.maxDepth = 1.0f,
	};
	vkCmdSetViewport(vkFrameCommandBuffers[frameIndex], 0, 1, &vkViewport);

	const VkRect2D vkScissorRect2D = {
		.offset     = {
			.x      = pass->scissor.x,
			.y      = pass->scissor.y,
		},
		.extent     = {
			.width  = (u32)pass->scissor.width,
			.height = (u32)pass->scissor.height
		},
	};
	vkCmdSetScissor(vkFrameCommandBuffers[frameIndex], 0, 1, &vkScissorRect2D);

	PipelineObj* const pipelineObj = pipelineObjs.Get(pass->pipeline);
	vkCmdBindPipeline(vkFrameCommandBuffers[frameIndex], pipelineObj->vkPipelineBindPoint, pipelineObj->vkPipeline);

	vkCmdBindDescriptorSets(vkFrameCommandBuffers[frameIndex], pipelineObj->vkPipelineBindPoint, pipelineObj->vkPipelineLayout, 0, 1, &vkBindlessDescriptorSet, 0, 0);
}

//----------------------------------------------------------------------------------------------

void EndPass() {
	vkCmdEndRendering(vkFrameCommandBuffers[frameIndex]);
}

//----------------------------------------------------------------------------------------------

}	// namespace JC::Render