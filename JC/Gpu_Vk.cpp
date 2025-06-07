#pragma once

#include "JC/Gpu.h"
#include "JC/Gpu_Vk.h"

#include "JC/Array.h"
#include "JC/Bit.h"
#include "JC/Config.h"
#include "JC/Fmt.h"
#include "JC/HandleArray.h"
#include "JC/Log.h"
#include "JC/Math.h"
#include "JC/Sys.h"
#include "JC/Window.h"

#include "spirv-reflect/spirv_reflect.h"

namespace JC::Gpu {

//--------------------------------------------------------------------------------------------------

DefErr(Gpu, Version);
DefErr(Gpu, NoLayer);
DefErr(Gpu, NoExt);
DefErr(Gpu, NoDevice);
DefErr(Gpu, NoMem);
DefErr(Gpu, ShaderTooManyPushConstantBlocks);
DefErr(Gpu, SpvReflect);

//--------------------------------------------------------------------------------------------------

static constexpr U32 MaxBindlessSampledImages  = 64 * 1024;
static constexpr U32 MaxBindlessSamplers       = 8;
static constexpr U32 MaxBindlessDescriptorSets = 32;
static constexpr F32 MaxAnisotropy             = 8.0f;
static constexpr U64 StagingBufferPerFrameSize = 256 * 1024 * 1024;

struct QueueFamily {
	VkQueueFamilyProperties vkQueueFamilyProperties;
	Bool                    supportsPresent;
};

struct PhysicalDevice {
	VkPhysicalDevice                 vkPhysicalDevice;
	VkPhysicalDeviceProperties2      vkPhysicalDeviceProperties2;
	VkPhysicalDeviceFeatures2        vkPhysicalDeviceFeatures2;
	VkPhysicalDeviceVulkan12Features vkPhysicalDeviceVulkan12Features;
	VkPhysicalDeviceVulkan13Features vkPhysicalDeviceVulkan13Features;
	VkPhysicalDeviceMemoryProperties vkPhysicalDeviceMemoryProperties;
	Array<VkExtensionProperties>     vkExtensionProperties;
	Array<VkSurfaceFormatKHR>        vkSurfaceFormats;
	VkFormat                         vkSwapchainFormat;
	VkColorSpaceKHR                  vkSwapchainColorSpace;
	Array<VkPresentModeKHR>          vkPresentModes;
	Array<QueueFamily>               queueFamilies;
	U32                              queueFamily;
};

struct Mem {
	VkDeviceMemory        vkDeviceMemory;
	U64                   offset;
	U64                   size;
	U32                   type;
	VkMemoryPropertyFlags vkMemoryPropertyFlags;
	VkMemoryAllocateFlags vkMemoryAllocateFlags;
};

struct BufferObj {
	VkBuffer           vkBuffer;
	Mem                mem;
	U64                size;
	VkBufferUsageFlags vkBufferUsageFlags;
	U64                addr;
	U64                stagingOffset;
	U64                uploadOffset;
	U64                uploadSize;
};

struct ImageObj {
	VkImage           vkImage;
	VkImageView       vkImageView;
	Mem               mem;
	U32               width;
	U32               height;
	VkFormat          vkFormat;
	VkImageUsageFlags vkImageUsageFlags;
	VkImageLayout     vkImageLayout;
	U32               bindlessIdx;
	U64               stagingOffset;
};

struct ShaderObj {
	VkShaderModule        vkShaderModule;
	VkShaderStageFlagBits vkShaderStage;
	U32                   pushConstantsOffset;
	U32                   pushConstantsSize;
};

struct PipelineObj {
	VkPipeline          vkPipeline;
	VkPipelineLayout    vkPipelineLayout;
	VkPipelineBindPoint vkPipelineBindPoint;
	VkPushConstantRange vkPushConstantRange;
};

static JC::Mem::Allocator*                allocator;
static JC::Mem::TempAllocator*            tempAllocator;
static Log::Logger*                       logger;
static Sys::Mutex                         mutex;
static HandleArray<BufferObj, Buffer>     bufferObjs;
static HandleArray<ImageObj, Image>       imageObjs;
static HandleArray<ShaderObj, Shader>     shaderObjs;
static HandleArray<PipelineObj, Pipeline> pipelineObjs;
       VkAllocationCallbacks*             vkAllocationCallbacks;	// not static: referenced in Gpu_Vk_Util.cpp
static VkInstance                         vkInstance;
static VkDebugUtilsMessengerEXT           vkDebugUtilsMessenger;
static VkSurfaceKHR                       vkSurface;
static Array<PhysicalDevice>              physicalDevices;
static PhysicalDevice*                    physicalDevice;
       VkDevice                           vkDevice;		// not static: referenced in Gpu_Vk_Util.cpp
static VkQueue                            vkQueue;
static VkSwapchainKHR                     vkSwapchain;
static Array<Image>                       swapchainImages;
static U32                                swapchainImageIdx;
static VkCommandPool                      vkFrameCommandPool;
static VkCommandBuffer                    vkFrameCommandBuffers[MaxFrames];
static VkCommandPool                      vkImmediateCommandPool;
static VkCommandBuffer                    vkImmediateCommandBuffer;
static VkCommandBuffer                    cmdToVkCommandBuffer[1 + MaxFrames + 1];	// [0] invalid, then frame cmds, then immediate cmd
static VkDescriptorPool                   vkDescriptorPool;
static VkDescriptorSetLayout              vkBindlessDescriptorSetLayout;
static VkDescriptorSet                    vkBindlessDescriptorSet;
static U32                                nextBindlessDescriptorIdx;
static VkSampler                          vkBindlessSamplers[MaxBindlessSamplers];
static U32                                vkBindlessSamplersLen;
static BufferObj                          stagingBufferObj;
static U8*                                stagingBufferPtr;
static U64                                stagingBufferUsed;
static VkSemaphore                        vkFrameTimelineSemaphore;
static VkSemaphore                        vkFrameImageAcquiredSemaphores[MaxFrames];
static VkSemaphore                        vkFrameSubmitCompleteSemaphores[MaxFrames];
static VkSemaphore                        vkImmediateTimelineSemaphore;
static U64                                immediateTimeline;
static U32                                frameIdx;
static U64                                frame;

//-------------------------------------------------------------------------------------------------

static VkBool32 VKAPI_CALL DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT, const VkDebugUtilsMessengerCallbackDataEXT* data, void*) {
	if (data && data->pMessage) {
		Sys::LockMutex(&mutex);
		if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
			Errorf("{}", data->pMessage);
			#if defined DebugBreakOnErr
				if (Sys::IsDebuggerPresent()) {
					Sys_DebuggerBreak();
				}
			#endif	// DebugBreakOnErr
		} else if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
			//Errorf("{}", data->pMessage);
		} else {
			Logf("{}", data->pMessage);
		}
		Sys::UnlockMutex(&mutex);
	}
	return VK_FALSE;
}
	
//-------------------------------------------------------------------------------------------------

template <class T>
static constexpr VkObjectType GetVkObjectType() {
         if constexpr (IsSameType<T, VkInstance>)            { return VK_OBJECT_TYPE_INSTANCE; }
    else if constexpr (IsSameType<T, VkPhysicalDevice>)      { return VK_OBJECT_TYPE_PHYSICAL_DEVICE; }
    else if constexpr (IsSameType<T, VkDevice>)              { return VK_OBJECT_TYPE_DEVICE; }
    else if constexpr (IsSameType<T, VkQueue>)               { return VK_OBJECT_TYPE_QUEUE; }
    else if constexpr (IsSameType<T, VkSemaphore>)           { return VK_OBJECT_TYPE_SEMAPHORE; }
    else if constexpr (IsSameType<T, VkCommandBuffer>)       { return VK_OBJECT_TYPE_COMMAND_BUFFER; }
    else if constexpr (IsSameType<T, VkDeviceMemory>)        { return VK_OBJECT_TYPE_DEVICE_MEMORY; }
    else if constexpr (IsSameType<T, VkBuffer>)              { return VK_OBJECT_TYPE_BUFFER; }
    else if constexpr (IsSameType<T, VkImage>)               { return VK_OBJECT_TYPE_IMAGE; }
    else if constexpr (IsSameType<T, VkImageView>)           { return VK_OBJECT_TYPE_IMAGE_VIEW; }
    else if constexpr (IsSameType<T, VkShaderModule>)        { return VK_OBJECT_TYPE_SHADER_MODULE; }
    else if constexpr (IsSameType<T, VkPipelineCache>)       { return VK_OBJECT_TYPE_PIPELINE_CACHE; }
    else if constexpr (IsSameType<T, VkPipelineLayout>)      { return VK_OBJECT_TYPE_PIPELINE_LAYOUT; }
    else if constexpr (IsSameType<T, VkPipeline>)            { return VK_OBJECT_TYPE_PIPELINE; }
    else if constexpr (IsSameType<T, VkDescriptorSetLayout>) { return VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT; }
    else if constexpr (IsSameType<T, VkSampler>)             { return VK_OBJECT_TYPE_SAMPLER; }
    else if constexpr (IsSameType<T, VkDescriptorPool>)      { return VK_OBJECT_TYPE_DESCRIPTOR_POOL; }
    else if constexpr (IsSameType<T, VkDescriptorSet>)       { return VK_OBJECT_TYPE_DESCRIPTOR_SET; }
    else if constexpr (IsSameType<T, VkCommandPool>)         { return VK_OBJECT_TYPE_COMMAND_POOL; }
    else if constexpr (IsSameType<T, VkSurfaceKHR>)          { return VK_OBJECT_TYPE_SURFACE_KHR; }
    else if constexpr (IsSameType<T, VkSwapchainKHR>)        { return VK_OBJECT_TYPE_SWAPCHAIN_KHR; }
	else {
		static_assert(AlwaysFalse<T>, "Unhandled VkObjectType");
	}
}

static constexpr void SetDbgNameImpl(VkObjectType vkObjectType, U64 handle, const char* name) {
	const VkDebugUtilsObjectNameInfoEXT vkDebugUtilsObjectNameInfoEXT = {
		.sType        = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
		.pNext        = 0,
		.objectType   = vkObjectType,
		.objectHandle = handle,
		.pObjectName  = name,
	};
	VkResult vkResult = vkSetDebugUtilsObjectNameEXT(vkDevice, &vkDebugUtilsObjectNameInfoEXT);
	Assert(vkResult == VK_SUCCESS);
}

template <class T>
static constexpr void SetDbgName(T obj, const char* name) {
	SetDbgNameImpl(GetVkObjectType<T>(), (U64)obj, name);
}

template <class T>
static constexpr void SetDbgNameI(T obj, const char* name, U64 i) {
	SetDbgNameImpl(GetVkObjectType<T>(), (U64)obj, Fmt::Printfz(tempAllocator, "{}[{}]", name, i));
}

template <class T>
static constexpr void SetDbgNameA(const T* objArr, const char* name, U64 i) {
	SetDbgNameImpl(GetVkObjectType<T>(), (U64)objArr[i], Fmt::Printfz(tempAllocator, "{}[{}]", name, i));
}

// TODO: FileNameOnly
#define DbgName(obj) (SetDbgName(obj, #obj " (" __FILE__ ":" LineStr ")"))
#define DbgNameI(obj, i) (SetDbgNameI(obj, #obj " (" __FILE__ ":" LineStr ")", (i)))
#define DbgNameA(obj, i) (SetDbgNameA(obj, #obj " (" __FILE__ ":" LineStr ")", (i)))

//-------------------------------------------------------------------------------------------------

static Res<> InitInstance() {
	LoadRootFns();

	U32 instanceVersion = 0;
	CheckVk(vkEnumerateInstanceVersion(&instanceVersion));
	if (instanceVersion < VK_API_VERSION_1_3) {
		return Err_Version("version", instanceVersion);
	}

	U32 n = 0;
	Array<VkLayerProperties> layers(tempAllocator);
	CheckVk(vkEnumerateInstanceLayerProperties(&n, 0));
	CheckVk(vkEnumerateInstanceLayerProperties(&n, layers.Resize(n)));
	Logf("{} layers:", layers.len);
	for (U64 i = 0; i < layers.len; i++) {
		Logf(
			"  {}: implementationVersion={}, specVersion={}, description={}",
			layers[i].layerName,
			VersionStr(tempAllocator, layers[i].implementationVersion),
			VersionStr(tempAllocator, layers[i].specVersion),
			layers[i].description
		);
	}
	Array<const char*> RequiredLayers(tempAllocator);
	if (Config::GetBool(Gpu::Cfg_EnableVkValidation, true)) {
		RequiredLayers.Add("VK_LAYER_KHRONOS_validation");
	}
	for (U32 i = 0; i < RequiredLayers.len; i++) {
		Bool found = false;
		for (U64 j = 0; j < layers.len; j++) {
			if (!strcmp(RequiredLayers[i], layers[j].layerName)) {
				Logf("Found required layer '{}'", RequiredLayers[i]);
				found = true;
				break;
			}
		}
		if (!found) {
			return Err_NoLayer("name", RequiredLayers[i]);
		}
	}

	Array<VkExtensionProperties> instExts(tempAllocator);
	CheckVk(vkEnumerateInstanceExtensionProperties(0, &n, 0));
	CheckVk(vkEnumerateInstanceExtensionProperties(0, &n, instExts.Resize(n)));
	Logf("{} instance extensions:", instExts.len);
	for (U64 i = 0; i < instExts.len; i++) {
		Logf("  {}: specVersion={}", instExts[i].extensionName, VersionStr(tempAllocator, instExts[i].specVersion));
	}
	Array<const char*> RequiredInstExts(tempAllocator);
	RequiredInstExts.Add("VK_KHR_surface");
	#if defined Platform_Windows
		RequiredInstExts.Add("VK_KHR_win32_surface");
	#endif	//	Platform_
	if (Config::GetBool(Gpu::Cfg_EnableDebug, true)) {
		RequiredInstExts.Add("VK_EXT_debug_utils");
	}
	for (U32 i = 0; i < RequiredInstExts.len; i++) {
		Bool found = false;
		for (U64 j = 0; j < instExts.len; j++) {
			if (!strcmp(RequiredInstExts[i], instExts[j].extensionName)) {
				Logf("Found required instance extension '{}'", RequiredInstExts[i]);
				found = true;
				break;
			}
		}
		if (!found) {
			return Err_NoExt("ext", RequiredInstExts[i]);
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
		.enabledLayerCount       = (U32)RequiredLayers.len,
		.ppEnabledLayerNames     = RequiredLayers.data,
		.enabledExtensionCount   = (U32)RequiredInstExts.len,
		.ppEnabledExtensionNames = RequiredInstExts.data,
	};

	VkDebugUtilsMessengerCreateInfoEXT vkDebugUtilsMessengerCreateInfo;
	if (Config::GetBool(Gpu::Cfg_EnableDebug, true)) {
		vkDebugUtilsMessengerCreateInfo = {
			.sType           = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
			.pNext           = vkInstanceCreateInfo.pNext,
			.flags           = 0,
			.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
			.messageType     = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
			.pfnUserCallback = DebugCallback,
			.pUserData       = 0,
		};
		vkInstanceCreateInfo.pNext = &vkDebugUtilsMessengerCreateInfo;

		const VkBool32 vkTrue = VK_TRUE;
		constexpr const char* validationDebugActions[] = { "VK_DBG_LAYER_ACTION_LOG_MSG" };  // "VK_DBG_LAYER_ACTION_DEBUG_OUTPUT", "VK_DBG_LAYER_ACTION_BREAK"
		constexpr const char* validationReportFlags[]  = { "error" };
		const VkLayerSettingEXT vkLayerSettings[] = {

			{ "VK_LAYER_KHRONOS_validation", "debug_action",                   VK_LAYER_SETTING_TYPE_STRING_EXT, LenOf(validationDebugActions), validationDebugActions},
			{ "VK_LAYER_KHRONOS_validation", "check_command_buffer",           VK_LAYER_SETTING_TYPE_BOOL32_EXT, 1,                             &vkTrue },
			{ "VK_LAYER_KHRONOS_validation", "check_image_layout",             VK_LAYER_SETTING_TYPE_BOOL32_EXT, 1,                             &vkTrue },
			{ "VK_LAYER_KHRONOS_validation", "check_object_in_use",            VK_LAYER_SETTING_TYPE_BOOL32_EXT, 1,                             &vkTrue },
			{ "VK_LAYER_KHRONOS_validation", "check_query",                    VK_LAYER_SETTING_TYPE_BOOL32_EXT, 1,                             &vkTrue },
			{ "VK_LAYER_KHRONOS_validation", "check_shaders",                  VK_LAYER_SETTING_TYPE_BOOL32_EXT, 1,                             &vkTrue },
			{ "VK_LAYER_KHRONOS_validation", "check_shaders_caching",          VK_LAYER_SETTING_TYPE_BOOL32_EXT, 1,                             &vkTrue },
			{ "VK_LAYER_KHRONOS_validation", "fine_grained_locking",           VK_LAYER_SETTING_TYPE_BOOL32_EXT, 1,                             &vkTrue },
			{ "VK_LAYER_KHRONOS_validation", "object_lifetime",                VK_LAYER_SETTING_TYPE_BOOL32_EXT, 1,                             &vkTrue },
			{ "VK_LAYER_KHRONOS_validation", "report_flags",                   VK_LAYER_SETTING_TYPE_STRING_EXT, LenOf(validationReportFlags),  validationReportFlags },
			{ "VK_LAYER_KHRONOS_validation", "stateless_param",                VK_LAYER_SETTING_TYPE_BOOL32_EXT, 1,                             &vkTrue },
			{ "VK_LAYER_KHRONOS_validation", "unique_handles",                 VK_LAYER_SETTING_TYPE_BOOL32_EXT, 1,                             &vkTrue },
			{ "VK_LAYER_KHRONOS_validation", "validate_best_practices",        VK_LAYER_SETTING_TYPE_BOOL32_EXT, 1,                             &vkTrue },
			{ "VK_LAYER_KHRONOS_validation", "validate_best_practices_nvidia", VK_LAYER_SETTING_TYPE_BOOL32_EXT, 1,                             &vkTrue },
			{ "VK_LAYER_KHRONOS_validation", "validate_core",                  VK_LAYER_SETTING_TYPE_BOOL32_EXT, 1,                             &vkTrue },
			{ "VK_LAYER_KHRONOS_validation", "validate_gpu_based",             VK_LAYER_SETTING_TYPE_BOOL32_EXT, 1,                             &vkTrue },
			{ "VK_LAYER_KHRONOS_validation", "validate_sync",                  VK_LAYER_SETTING_TYPE_BOOL32_EXT, 1,                             &vkTrue },
		};
		const VkLayerSettingsCreateInfoEXT vkLayerSettingsCreateInfo = {
			.sType        = VK_STRUCTURE_TYPE_LAYER_SETTINGS_CREATE_INFO_EXT,
			.pNext        = vkInstanceCreateInfo.pNext,
			.settingCount = LenOf(vkLayerSettings),
			.pSettings    = vkLayerSettings,

		};
		vkInstanceCreateInfo.pNext = &vkLayerSettingsCreateInfo;
	}

	CheckVk(vkCreateInstance(&vkInstanceCreateInfo, vkAllocationCallbacks, &vkInstance));

	LoadInstanceFns(vkInstance);

	if (Config::GetBool(Gpu::Cfg_EnableDebug, true)) {
		CheckVk(vkCreateDebugUtilsMessengerEXT(vkInstance, &vkDebugUtilsMessengerCreateInfo, vkAllocationCallbacks, &vkDebugUtilsMessenger));
	}

	return Ok();
}

//-------------------------------------------------------------------------------------------------

static Res<> InitSurface(const Window::PlatformDesc* windowPlatformDesc) {
	#if defined Platform_Windows
		VkWin32SurfaceCreateInfoKHR win32SurfaceCreateInfo = {
			.sType     = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
			.pNext     = 0,
			.flags     = 0,
			.hinstance = (HINSTANCE)windowPlatformDesc->hinstance,
			.hwnd      = (HWND)windowPlatformDesc->hwnd,
		};
		CheckVk(vkCreateWin32SurfaceKHR(vkInstance, &win32SurfaceCreateInfo, vkAllocationCallbacks, &vkSurface));
	#endif	// Platform_

	return Ok();
}

//-------------------------------------------------------------------------------------------------

static Res<> InitDevice() {
	constexpr const char* RequiredDeviceExts[] = {
		"VK_KHR_swapchain",
	};

	U32 n = 0;
	Array<VkPhysicalDevice> vkPhysicalDevices(tempAllocator);
	CheckVk(vkEnumeratePhysicalDevices(vkInstance, &n, nullptr));
	CheckVk(vkEnumeratePhysicalDevices(vkInstance, &n, vkPhysicalDevices.Resize(n)));

	// TODO: support cvar for selecting explicit device

	Logf("{} physical devices:", vkPhysicalDevices.len);
	physicalDevices.Resize(vkPhysicalDevices.len);

	U32 bestScore = 0;
	for (U64 i = 0; i < physicalDevices.len; i++) {
		PhysicalDevice* pd = &physicalDevices[i];
		pd->vkPhysicalDevice = vkPhysicalDevices[i];
		pd->vkPhysicalDeviceProperties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
		vkGetPhysicalDeviceProperties2(vkPhysicalDevices[i], &pd->vkPhysicalDeviceProperties2);
		Logf(
			"{}: apiVersion={}, driverVersion={}, vendorID={}, deviceId={}, deviceType={}",
			pd->vkPhysicalDeviceProperties2.properties.deviceName,
			VersionStr(tempAllocator, pd->vkPhysicalDeviceProperties2.properties.apiVersion),
			VersionStr(tempAllocator, pd->vkPhysicalDeviceProperties2.properties.driverVersion),
			pd->vkPhysicalDeviceProperties2.properties.vendorID,
			pd->vkPhysicalDeviceProperties2.properties.deviceID,
			PhysicalDeviceTypeStr(pd->vkPhysicalDeviceProperties2.properties.deviceType)
		);

		U32 score = 0;
		switch (pd->vkPhysicalDeviceProperties2.properties.deviceType) {
			case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:   score += 1000; break;
			case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: score +=  100; break;
			case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:    score +=   10; break;
			case VK_PHYSICAL_DEVICE_TYPE_CPU:            score +=    1; break;
			case VK_PHYSICAL_DEVICE_TYPE_OTHER:          score +=    1; break;
		};
		if (pd->vkPhysicalDeviceProperties2.properties.apiVersion < VK_API_VERSION_1_3) {
			Logf("  Rejecting device: need Vulkan 1.3: apiVersion={}", VersionStr(tempAllocator, pd->vkPhysicalDeviceProperties2.properties.apiVersion));
			score = 0;
		}

		pd->vkPhysicalDeviceFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
		pd->vkPhysicalDeviceFeatures2.pNext = &pd->vkPhysicalDeviceVulkan12Features;
		pd->vkPhysicalDeviceVulkan12Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
		pd->vkPhysicalDeviceVulkan12Features.pNext = &pd->vkPhysicalDeviceVulkan13Features;
		pd->vkPhysicalDeviceVulkan13Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
		pd->vkPhysicalDeviceVulkan13Features.pNext = 0;
		vkGetPhysicalDeviceFeatures2(pd->vkPhysicalDevice, &pd->vkPhysicalDeviceFeatures2);
		#define CheckVkFeature(path) \
			if (!pd->path) { \
				Logf("  Rejecting device: doesn't support " #path); \
				score = 0; \
			}
		CheckVkFeature(vkPhysicalDeviceFeatures2.features.shaderClipDistance);
		CheckVkFeature(vkPhysicalDeviceFeatures2.features.samplerAnisotropy);
		CheckVkFeature(vkPhysicalDeviceVulkan12Features.bufferDeviceAddress);
		CheckVkFeature(vkPhysicalDeviceVulkan12Features.descriptorIndexing);
		CheckVkFeature(vkPhysicalDeviceVulkan12Features.descriptorBindingPartiallyBound);
		CheckVkFeature(vkPhysicalDeviceVulkan12Features.descriptorBindingSampledImageUpdateAfterBind);
		CheckVkFeature(vkPhysicalDeviceVulkan12Features.runtimeDescriptorArray);
		CheckVkFeature(vkPhysicalDeviceVulkan12Features.timelineSemaphore);
		CheckVkFeature(vkPhysicalDeviceVulkan13Features.dynamicRendering);
		CheckVkFeature(vkPhysicalDeviceVulkan13Features.synchronization2);
		#undef CheckVkFeature


		vkGetPhysicalDeviceMemoryProperties(pd->vkPhysicalDevice, &pd->vkPhysicalDeviceMemoryProperties);
		Logf("  {} memory types:", pd->vkPhysicalDeviceMemoryProperties.memoryTypeCount);
		for (U64 j = 0; j < pd->vkPhysicalDeviceMemoryProperties.memoryTypeCount; j++) {
			const VkMemoryType mt = pd->vkPhysicalDeviceMemoryProperties.memoryTypes[j];
			Logf("    [{}] heapIndex={}, flags={}", j, mt.heapIndex, MemoryPropertyFlagsStr(tempAllocator, mt.propertyFlags));
		}
		Logf("  {} memory heaps:", pd->vkPhysicalDeviceMemoryProperties.memoryHeapCount);
		for (U64 j = 0; j < pd->vkPhysicalDeviceMemoryProperties.memoryHeapCount; j++) {
			const VkMemoryHeap mh = pd->vkPhysicalDeviceMemoryProperties.memoryHeaps[j];
			Logf("    [{}] size={}, flags={}", j, SizeStr(tempAllocator, mh.size), MemoryHeapFlagsStr(tempAllocator, mh.flags));
		}

		Array<VkQueueFamilyProperties> vkQueueFamilyProperties(tempAllocator);
		vkGetPhysicalDeviceQueueFamilyProperties(pd->vkPhysicalDevice, &n, nullptr);
		vkGetPhysicalDeviceQueueFamilyProperties(pd->vkPhysicalDevice, &n, vkQueueFamilyProperties.Resize(n));

		pd->vkExtensionProperties.Init(allocator);
		CheckVk(vkEnumerateDeviceExtensionProperties(pd->vkPhysicalDevice, 0, &n, 0));
		CheckVk(vkEnumerateDeviceExtensionProperties(pd->vkPhysicalDevice, 0, &n, pd->vkExtensionProperties.Resize(n)));

		Array<char> extensionsStr(tempAllocator);
		for (U64 j = 0; j < pd->vkExtensionProperties.len; j++) {
			Fmt::Printf(&extensionsStr, "{}(specVersion={}), ", pd->vkExtensionProperties[j].extensionName, VersionStr(tempAllocator, pd->vkExtensionProperties[j].specVersion));
		}
		if (extensionsStr.len >= 2) {
			extensionsStr.len -= 2;
		}
		Logf("  {} device extensions: {}",  pd->vkExtensionProperties.len, Str(extensionsStr.data, extensionsStr.len));
		for (U64 j = 0; j < LenOf(RequiredDeviceExts); j++) {
			Bool found = false;
			for (U64 k = 0; k < pd->vkExtensionProperties.len; k++) {
				if (!strcmp(RequiredDeviceExts[j], pd->vkExtensionProperties[k].extensionName)) {
					found = true;
					break;
				}
			}
			if (!found) {
				Logf("Rejecting device: doesn't support extension '{}'", RequiredDeviceExts[j]);
				score = 0;
			}
		}

		pd->vkSurfaceFormats.Init(allocator);
		CheckVk(vkGetPhysicalDeviceSurfaceFormatsKHR(pd->vkPhysicalDevice, vkSurface, &n, 0));
		CheckVk(vkGetPhysicalDeviceSurfaceFormatsKHR(pd->vkPhysicalDevice, vkSurface, &n, pd->vkSurfaceFormats.Resize(n)));
		pd->vkSwapchainFormat = VK_FORMAT_UNDEFINED;
		for (U64 j = 0; j < pd->vkSurfaceFormats.len; j++) {
			if (
				(pd->vkSurfaceFormats[j].format == VK_FORMAT_B8G8R8A8_UNORM || pd->vkSurfaceFormats[j].format == VK_FORMAT_R8G8B8A8_UNORM) &&
				pd->vkSurfaceFormats[j].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR
			) {
				pd->vkSwapchainFormat     = pd->vkSurfaceFormats[j].format;
				pd->vkSwapchainColorSpace = pd->vkSurfaceFormats[j].colorSpace;
				break;
			}
		}
		if (pd->vkSwapchainFormat == VK_FORMAT_UNDEFINED) {
			Logf("  Rejecting device: no suitable swapchain format");
			score = 0;
		}
		Logf("  {} surface formats:", pd->vkSurfaceFormats.len);
		for (U64 j = 0; j < pd->vkSurfaceFormats.len; j++) {
			Logf("    {}, {}", FormatStr(pd->vkSurfaceFormats[j].format), ColorSpaceStr(pd->vkSurfaceFormats[j].colorSpace));
		}
		Logf("  Selected surface format: {}/{}", FormatStr(pd->vkSwapchainFormat), ColorSpaceStr(pd->vkSwapchainColorSpace));

		pd->vkPresentModes.Init(allocator);
		CheckVk(vkGetPhysicalDeviceSurfacePresentModesKHR(pd->vkPhysicalDevice, vkSurface, &n, 0));
		CheckVk(vkGetPhysicalDeviceSurfacePresentModesKHR(pd->vkPhysicalDevice, vkSurface, &n, pd->vkPresentModes.Resize(n)));
		Logf("  {} present modes:", pd->vkPresentModes.len);
		for (U64 j = 0; j < pd->vkPresentModes.len; j++) {
			Logf("    {}", PresentModeStr(pd->vkPresentModes[j]));
		}
		if (pd->vkPresentModes.len == 0) {
			Logf("  Rejecting device: no present modes");
			score = 0;
		}

		pd->queueFamilies.Init(allocator);
		pd->queueFamilies.Resize(vkQueueFamilyProperties.len);
		Logf("  {} queue families:", pd->queueFamilies.len);
		for (U64 j = 0; j < pd->queueFamilies.len; j++) {
			const VkQueueFamilyProperties* props = &pd->queueFamilies[j].vkQueueFamilyProperties;
			VkBool32 supportsPresent = VK_FALSE;
			CheckVk(vkGetPhysicalDeviceSurfaceSupportKHR(pd->vkPhysicalDevice, (U32)j, vkSurface, &supportsPresent));
			pd->queueFamilies[j].vkQueueFamilyProperties = vkQueueFamilyProperties[j];
			pd->queueFamilies[j].supportsPresent         = (supportsPresent == VK_TRUE);
			Logf("    [{}] count={}, flags={}, supportsPresent={}", j, props->queueCount, QueueFlagsStr(tempAllocator, props->queueFlags), pd->queueFamilies[j].supportsPresent);
			const VkQueueFlags flags = pd->queueFamilies[j].vkQueueFamilyProperties.queueFlags;
			if (pd->queueFamily == VK_QUEUE_FAMILY_IGNORED && (flags & VK_QUEUE_GRAPHICS_BIT) && pd->queueFamilies[j].supportsPresent) {
				pd->queueFamily = (U32)j;
			}
		}
		if (pd->queueFamily == VK_QUEUE_FAMILY_IGNORED) {
			Logf("  Rejecting device: no graphics queue supporting present");
			score = 0;
		}

		Logf("  score: {}", score);
		if (score > bestScore) {
			physicalDevice = pd;
			bestScore = score;
		}
	}

	if (bestScore == 0) {
		return Err_NoDevice();
	}
	Logf("Selected physical device '{}' with score={}", physicalDevice->vkPhysicalDeviceProperties2.properties.deviceName, bestScore);

	F32 queuePriority = 1.0f;
	const VkDeviceQueueCreateInfo vkDeviceQueueCreateInfo = {
		.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
		.pNext            = 0,
		.flags            = 0,
		.queueFamilyIndex = physicalDevice->queueFamily,
		.queueCount       = 1,
		.pQueuePriorities = &queuePriority,
	};

	// TODO: possibly create more queues? one per type?
	const VkDeviceCreateInfo vkDeviceCreateInfo = {
		.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
		.pNext                   = &physicalDevice->vkPhysicalDeviceFeatures2,
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
	DbgName(vkDevice);

	LoadDeviceFns(vkDevice);

	vkGetDeviceQueue(vkDevice, physicalDevice->queueFamily, 0, &vkQueue);

	return Ok();
}

//-------------------------------------------------------------------------------------------------

static Res<> InitSwapchain(U32 width, U32 height) {
	VkSurfaceCapabilitiesKHR vkSurfaceCapabilities;
	CheckVk(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice->vkPhysicalDevice, vkSurface, &vkSurfaceCapabilities));
	
	U32 imageCount = Max((U32)3, vkSurfaceCapabilities.minImageCount);
	if (vkSurfaceCapabilities.maxImageCount > 0 && imageCount > vkSurfaceCapabilities.maxImageCount) {
		imageCount = vkSurfaceCapabilities.maxImageCount;
	}
	Logf("Selected swapchain image count: {}", imageCount);

	// TODO: check min/max extents
	const VkExtent2D vkSwapchainExtent = (vkSurfaceCapabilities.currentExtent.width != U32Max)
		? vkSurfaceCapabilities.currentExtent
		: VkExtent2D {
			.width  = Clamp(width,  vkSurfaceCapabilities.minImageExtent.width,  vkSurfaceCapabilities.maxImageExtent.width),
			.height = Clamp(height, vkSurfaceCapabilities.minImageExtent.height, vkSurfaceCapabilities.maxImageExtent.height),
		};
	Logf("Selected swapchain extent: ({}, {})", vkSwapchainExtent.width, vkSwapchainExtent.height);

	VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;
	for (U64 i = 0; i < physicalDevice->vkPresentModes.len; i++) {
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
		.imageFormat           = physicalDevice->vkSwapchainFormat,
		.imageColorSpace       = physicalDevice->vkSwapchainColorSpace,
		.imageExtent           = vkSwapchainExtent,
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
	DbgName(vkSwapchain);

	U32 n = 0;
	CheckVk(vkGetSwapchainImagesKHR(vkDevice, vkSwapchain, &n, 0));
	VkImage* const vkSwapchainImages = tempAllocator->AllocT<VkImage>(n);
	CheckVk(vkGetSwapchainImagesKHR(vkDevice, vkSwapchain, &n, vkSwapchainImages));

	swapchainImages.Resize(n);
	for (U64 i = 0; i < n; i++) {
		DbgNameA(vkSwapchainImages, i);

		const VkImageViewCreateInfo vkImageViewCreateInfo = {
			.sType              = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			.pNext              = 0,
			.flags              = 0,
			.image              = vkSwapchainImages[i],
			.viewType           = VK_IMAGE_VIEW_TYPE_2D,
			.format             = physicalDevice->vkSwapchainFormat,
			.components         = {
				.r              = VK_COMPONENT_SWIZZLE_IDENTITY,
				.g              = VK_COMPONENT_SWIZZLE_IDENTITY,
				.b              = VK_COMPONENT_SWIZZLE_IDENTITY,
				.a              = VK_COMPONENT_SWIZZLE_IDENTITY,
			},
			.subresourceRange   = MakeVkSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT),
		};
		VkImageView vkSwapchainImageView = VK_NULL_HANDLE;
		CheckVk(vkCreateImageView(vkDevice, &vkImageViewCreateInfo, vkAllocationCallbacks, &vkSwapchainImageView));
		DbgNameI(vkSwapchainImageView, i);

		swapchainImages[i] = imageObjs.Alloc();
		ImageObj* imageObj = imageObjs.Get(swapchainImages[i]);
		imageObj->vkImage       = vkSwapchainImages[i];
		imageObj->vkImageView   = vkSwapchainImageView;
		imageObj->mem           = {};
		imageObj->width         = vkSwapchainExtent.width;
		imageObj->height        = vkSwapchainExtent.height;
		imageObj->vkFormat      = physicalDevice->vkSwapchainFormat;
		imageObj->vkImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	}

	vkDeviceWaitIdle(vkDevice);

	return Ok();
}

//----------------------------------------------------------------------------------------------

static Res<> InitFrameSyncObjects() {
	if (Res<> r = CreateTimelineSemaphore(MaxFrames - 1).To(vkFrameTimelineSemaphore); !r) { return r; }
	DbgName(vkFrameTimelineSemaphore);

	for (U64 i = 0; i < MaxFrames; i++) {
		if (Res<> r = CreateSemaphore().To(vkFrameImageAcquiredSemaphores[i]);   !r) { return r; }
		DbgNameA(vkFrameImageAcquiredSemaphores, i);
		if (Res<> r = CreateSemaphore().To(vkFrameSubmitCompleteSemaphores[i]); !r) { return r; }
		DbgNameA(vkFrameSubmitCompleteSemaphores, i);
	}
	if (Res<> r = CreateTimelineSemaphore(0).To(vkImmediateTimelineSemaphore); !r) { return r; }
	DbgName(vkImmediateTimelineSemaphore);
	return Ok();
}

//-------------------------------------------------------------------------------------------------

static Res<> InitCommandBuffers() {
	if (Res<> r = CreateCommandPool(physicalDevice->queueFamily, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT).To(vkFrameCommandPool); !r) { return r; }
	DbgName(vkFrameCommandPool);
	if (Res<> r = CreateCommandPool(physicalDevice->queueFamily, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT).To(vkImmediateCommandPool); !r) { return r; }
	DbgName(vkImmediateCommandPool);
	if (Res<> r = AllocCommandBuffers(vkFrameCommandPool, MaxFrames, vkFrameCommandBuffers); !r) { return r; }
	for (U64 i = 0; i < MaxFrames; i++) {
		DbgNameA(vkFrameCommandBuffers, i);
	}
	if (Res<> r = AllocCommandBuffers(vkImmediateCommandPool, 1, &vkImmediateCommandBuffer); !r) { return r; }
	DbgName(vkImmediateCommandBuffer);

	cmdToVkCommandBuffer[0] = VK_NULL_HANDLE;	// reserve for invalid
	for (U32 i = 0; i < MaxFrames; i++) {
		cmdToVkCommandBuffer[i + 1] = vkFrameCommandBuffers[i];
	}
	cmdToVkCommandBuffer[1 + MaxFrames] = vkImmediateCommandBuffer;

	return Ok();
}

//----------------------------------------------------------------------------------------------

static Res<> InitBindlessDescriptors() {
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
	DbgName(vkDescriptorPool);

	constexpr VkDescriptorBindingFlags vkDescriptorBindingFlags[] = {
		VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT_EXT,
		VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT_EXT,
	};
	const VkDescriptorSetLayoutBindingFlagsCreateInfo vkDescriptorSetLayoutBindingFlagsCreateInfo = {
		.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
		.pNext         = 0,
		.bindingCount  = LenOf(vkDescriptorBindingFlags),
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
	CheckVk(vkCreateDescriptorSetLayout(vkDevice, &vkDescriptorSetLayoutCreateInfo, vkAllocationCallbacks, &vkBindlessDescriptorSetLayout));
	DbgName(vkBindlessDescriptorSetLayout);

	const VkDescriptorSetAllocateInfo vkDescriptorSetAllocateInfo = {
		.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
		.pNext              = 0,
		.descriptorPool     = vkDescriptorPool,
		.descriptorSetCount = 1,
		.pSetLayouts        = &vkBindlessDescriptorSetLayout,
	};
	CheckVk(vkAllocateDescriptorSets(vkDevice, &vkDescriptorSetAllocateInfo, &vkBindlessDescriptorSet));
	DbgName(vkBindlessDescriptorSet);

	nextBindlessDescriptorIdx = 1;	// reserve index 0 for invalid

	return Ok();
}

//-------------------------------------------------------------------------------------------------

static void AddBindlessSampler(VkSampler vkSampler) {
	Assert(vkBindlessSamplersLen < MaxBindlessSamplers);
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
		.dstArrayElement  = vkBindlessSamplersLen,
		.descriptorCount  = 1,
		.descriptorType   = VK_DESCRIPTOR_TYPE_SAMPLER,
		.pImageInfo       = &vkDescriptorImageInfo,
		.pBufferInfo      = 0,
		.pTexelBufferView = 0,
	};
	vkUpdateDescriptorSets(vkDevice, 1, &vkWriteDescriptorSet, 0, 0);
	vkBindlessSamplers[vkBindlessSamplersLen] = vkSampler;
	vkBindlessSamplersLen++;
}

static Res<> InitBindlessSamplers() {
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
	VkSampler vkNearestSampler = VK_NULL_HANDLE;
	CheckVk(vkCreateSampler(vkDevice, &vkSamplerCreateInfo, vkAllocationCallbacks, &vkNearestSampler));
	DbgName(vkNearestSampler);
	AddBindlessSampler(vkNearestSampler);

	vkSamplerCreateInfo.magFilter        = VK_FILTER_LINEAR;
	vkSamplerCreateInfo.minFilter        = VK_FILTER_LINEAR;
	vkSamplerCreateInfo.mipmapMode       = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	vkSamplerCreateInfo.anisotropyEnable = VK_TRUE;
	vkSamplerCreateInfo.maxAnisotropy    = MaxAnisotropy;
	VkSampler vkLinearSampler = VK_NULL_HANDLE;
	CheckVk(vkCreateSampler(vkDevice, &vkSamplerCreateInfo, vkAllocationCallbacks, &vkLinearSampler));
	DbgName(vkLinearSampler);
	AddBindlessSampler(vkLinearSampler);

	return Ok();
}

//----------------------------------------------------------------------------------------------
	
// TODO: buffer alignment
static Res<Mem> AllocateMem(VkMemoryRequirements vkMemoryRequirements, VkMemoryPropertyFlags vkMemoryPropertyFlags, VkMemoryAllocateFlags vkMemoryAllocateFlags) {
	U32 memType = U32Max;
	for (U32 i = 0; i < physicalDevice->vkPhysicalDeviceMemoryProperties.memoryTypeCount; i++) {
		if (
			(vkMemoryRequirements.memoryTypeBits & (1 << i)) &&
			(physicalDevice->vkPhysicalDeviceMemoryProperties.memoryTypes[i].propertyFlags & vkMemoryPropertyFlags)
		) {
			memType = i;
		}
	}
	if (memType == U32Max) {
		return Err_NoMem();
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

	return Mem {
		.vkDeviceMemory        = vkDeviceMemory,
		.offset                = 0,
		.size                  = vkMemoryRequirements.size,
		.type                  = memType,
		.vkMemoryPropertyFlags = vkMemoryPropertyFlags,
		.vkMemoryAllocateFlags = vkMemoryAllocateFlags,
	};
}

//-------------------------------------------------------------------------------------------------

static void FreeMem(Mem mem) {
	if (mem.vkDeviceMemory != VK_NULL_HANDLE) {
		vkFreeMemory(vkDevice, mem.vkDeviceMemory, vkAllocationCallbacks);
	}
}

//-------------------------------------------------------------------------------------------------

static Res<BufferObj> CreateBufferImpl(
	U64                   size,
	VkBufferUsageFlags    vkBufferUsageFlags,
	VkMemoryPropertyFlags vkMemoryPropertyFlags,
	VkMemoryAllocateFlags vkMemoryAllocateFlags,
	const char*           name
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
	SetDbgName(vkBuffer, name);

	VkMemoryRequirements vkMemoryRequirements = {};
	vkGetBufferMemoryRequirements(vkDevice, vkBuffer, &vkMemoryRequirements);

	Mem mem = {};
	if (Res<> r = AllocateMem(vkMemoryRequirements, vkMemoryPropertyFlags, vkMemoryAllocateFlags).To(mem); !r) {
		vkDestroyBuffer(vkDevice, vkBuffer, vkAllocationCallbacks);
		return r.err;
	}

	if (const VkResult r = vkBindBufferMemory(vkDevice, vkBuffer, mem.vkDeviceMemory, 0); r != VK_SUCCESS) {
		vkDestroyBuffer(vkDevice, vkBuffer, vkAllocationCallbacks);
		FreeMem(mem);
		return Err_Vk(r, "vkBindBufferMemory");
	}

	U64 addr = 0;
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
		.mem                = mem,
		.size               = size,
		.vkBufferUsageFlags = vkBufferUsageFlags,
		.addr               = addr,
		.stagingOffset      = U64Max,
		.uploadOffset       = U64Max,
		.uploadSize         = U64Max,
	};
}

//-------------------------------------------------------------------------------------------------

static Res<> InitStagingBuffer() {
	constexpr U64 StagingBufferSize = MaxFrames * StagingBufferPerFrameSize;

	if (Res<> r = CreateBufferImpl(
		StagingBufferSize,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		0,
		"Staging"
	).To(stagingBufferObj); !r) {
		return r;
	}
	CheckVk(vkMapMemory(vkDevice, stagingBufferObj.mem.vkDeviceMemory, 0, StagingBufferSize, 0, (void**)&stagingBufferPtr));
	stagingBufferUsed = 0;

	return Ok();
}

//-------------------------------------------------------------------------------------------------

Res<> Init(const InitDesc* initDesc) {
	allocator     = initDesc->allocator;
	tempAllocator = initDesc->tempAllocator;
	logger        = initDesc->logger;
	Sys::InitMutex(&mutex);
	bufferObjs.Init(allocator);
	imageObjs.Init(allocator);
	shaderObjs.Init(allocator);
	pipelineObjs.Init(allocator);
	physicalDevices.Init(allocator);
	swapchainImages.Init(allocator);

	if (Res<> r = InitInstance();                                               !r) { return r; }
	if (Res<> r = InitSurface(initDesc->windowPlatformDesc);                    !r) { return r; }
	if (Res<> r = InitDevice();                                                 !r) { return r; }
	if (Res<> r = InitSwapchain(initDesc->windowWidth, initDesc->windowHeight); !r) { return r; }
	if (Res<> r = InitFrameSyncObjects();                                       !r) { return r; }
	if (Res<> r = InitCommandBuffers();                                         !r) { return r; }
	if (Res<> r = InitBindlessDescriptors();                                    !r) { return r; }
	if (Res<> r = InitBindlessSamplers();                                       !r) { return r; }
	if (Res<> r = InitStagingBuffer();                                          !r) { return r; }

	immediateTimeline = 0;
	frame             = 0;
	frameIdx          = 0;

	return Ok();
}

//----------------------------------------------------------------------------------------------

#define DestroyVk(h, DestroyFn) \
	if (h != VK_NULL_HANDLE) { \
		DestroyFn(vkDevice, h, vkAllocationCallbacks); \
		h = VK_NULL_HANDLE; \
	}

#define DestroyVkArray(a, DestroyFn) \
	for (U64 i = 0; i < a.len; i++) { \
		if (a[i] != VK_NULL_HANDLE) { \
			DestroyFn(vkDevice, a[i], vkAllocationCallbacks); \
		} \
	} \
	a.len = 0;

#define DestroyVkCArray(a, DestroyFn) \
	for (U64 i = 0; i < LenOf(a); i++) { \
		if (a[i] != VK_NULL_HANDLE) { \
			DestroyFn(vkDevice, a[i], vkAllocationCallbacks); \
		} \
	} \

//--------------------------------------------------------------------------------------------------

void Shutdown() {
	DestroyVk(stagingBufferObj.vkBuffer, vkDestroyBuffer);
	FreeMem(stagingBufferObj.mem);
	DestroyVkCArray(vkBindlessSamplers, vkDestroySampler);
	vkBindlessSamplersLen = 0;
	DestroyVk(vkBindlessDescriptorSetLayout, vkDestroyDescriptorSetLayout);
	DestroyVk(vkDescriptorPool, vkDestroyDescriptorPool);
	if (vkFrameCommandPool != VK_NULL_HANDLE) {
		vkFreeCommandBuffers(vkDevice, vkFrameCommandPool, MaxFrames, vkFrameCommandBuffers);
	}
	DestroyVk(vkFrameCommandPool, vkDestroyCommandPool);
	if (vkImmediateCommandPool != VK_NULL_HANDLE) {
		vkFreeCommandBuffers(vkDevice, vkImmediateCommandPool, 1, &vkImmediateCommandBuffer);
	}
	DestroyVk(vkImmediateCommandPool, vkDestroyCommandPool);
	DestroyVk(vkFrameTimelineSemaphore, vkDestroySemaphore);
	DestroyVk(vkImmediateTimelineSemaphore, vkDestroySemaphore);
	DestroyVkCArray(vkFrameImageAcquiredSemaphores, vkDestroySemaphore);
	DestroyVkCArray(vkFrameSubmitCompleteSemaphores, vkDestroySemaphore);
	for (U64 i = 0; i < swapchainImages.len; i++) {
		ImageObj* const imageObj = imageObjs.Get(swapchainImages[i]);
		DestroyVk(imageObj->vkImageView, vkDestroyImageView);
		imageObjs.Free(swapchainImages[i]);
	}
	swapchainImages.len = 0;
	DestroyVk(vkSwapchain, vkDestroySwapchainKHR);
	if (vkDevice != VK_NULL_HANDLE) { vkDestroyDevice(vkDevice, vkAllocationCallbacks); vkDevice = VK_NULL_HANDLE; }
	if (vkSurface != VK_NULL_HANDLE) { vkDestroySurfaceKHR(vkInstance, vkSurface, vkAllocationCallbacks); vkSurface = VK_NULL_HANDLE; }
	if (vkDebugUtilsMessenger != VK_NULL_HANDLE) { vkDestroyDebugUtilsMessengerEXT(vkInstance, vkDebugUtilsMessenger, vkAllocationCallbacks); vkDebugUtilsMessenger = VK_NULL_HANDLE; }
	if (vkInstance != VK_NULL_HANDLE) { vkDestroyInstance(vkInstance, vkAllocationCallbacks); vkInstance = VK_NULL_HANDLE; }
	Sys::ShutdownMutex(&mutex);
}

//----------------------------------------------------------------------------------------------

U32 GetFrameIdx() { return frameIdx; }

//----------------------------------------------------------------------------------------------

Res<> RecreateSwapchain(U32 width, U32 height) {
	vkDeviceWaitIdle(vkDevice);

	for (U64 i = 0; i < swapchainImages.len; i++) {
		ImageObj* const imageObj = imageObjs.Get(swapchainImages[i]);
		DestroyVk(imageObj->vkImageView, vkDestroyImageView);
		imageObjs.Free(swapchainImages[i]);
	}
	swapchainImages.len = 0;
	DestroyVk(vkSwapchain, vkDestroySwapchainKHR);

	Res<> r = InitSwapchain(width, height);

	vkDeviceWaitIdle(vkDevice);

	return r;
}

//----------------------------------------------------------------------------------------------

ImageFormat GetSwapchainImageFormat() {
	return VkFormatToImageFormat(physicalDevice->vkSwapchainFormat);
}

//----------------------------------------------------------------------------------------------

Res<Buffer> CreateBuffer(U64 size, BufferUsage::Flags usageFlags, const char* dbgName) {
	VkBufferUsageFlags vkBufferUsageFlags = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
	if (usageFlags & BufferUsage::Storage)  { vkBufferUsageFlags |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT; }
	if (usageFlags & BufferUsage::Index)    { vkBufferUsageFlags |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT; }
	if (usageFlags & BufferUsage::CpuWrite) { vkBufferUsageFlags |= VK_BUFFER_USAGE_TRANSFER_DST_BIT; }

	BufferObj bufferObj;
	CheckRes(CreateBufferImpl(
		size,
		vkBufferUsageFlags,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT,
		dbgName
	).To(bufferObj));

	const Buffer buffer = bufferObjs.Alloc();
	*bufferObjs.Get(buffer) = bufferObj;
	return buffer;
}

//----------------------------------------------------------------------------------------------

void DestroyBuffer(Buffer buffer) {
	if (buffer.handle) {
		BufferObj* const bufferObj = bufferObjs.Get(buffer);
		DestroyVk(bufferObj->vkBuffer, vkDestroyBuffer);
		FreeMem(bufferObj->mem);
		memset(bufferObj, 0, sizeof(*bufferObj));
		bufferObjs.Free(buffer);
	}
}

//----------------------------------------------------------------------------------------------

U64 GetBufferAddr(Buffer buffer) {
	return bufferObjs.Get(buffer)->addr;
}

//----------------------------------------------------------------------------------------------

Res<void*> MapBuffer(Buffer buffer, U64 offset, U64 size) {
    BufferObj* const bufferObj = bufferObjs.Get(buffer);
	U8* ptr = 0;
	if (size == 0) { size = bufferObj->size; }
	CheckVk(vkMapMemory(vkDevice, bufferObj->mem.vkDeviceMemory, offset, size, 0, (void**)&ptr));
    return ptr;
}

//----------------------------------------------------------------------------------------------

Res<Image> CreateImage(U32 width, U32 height, ImageFormat format, ImageUsage::Flags usageFlags) {
	// TODO: check format properties optimal tiling supports the requested format/feature
	const VkFormat vkFormat = ImageFormatToVkFormat(format);

	VkImageUsageFlags vkImageUsageFlags = 0;
	if (usageFlags & ImageUsage::Sampled)         { vkImageUsageFlags |= VK_IMAGE_USAGE_SAMPLED_BIT; }
	if (usageFlags & ImageUsage::ColorAttachment) { vkImageUsageFlags |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT; }
	if (usageFlags & ImageUsage::DepthAttachment) { vkImageUsageFlags |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT; }
	if (usageFlags & ImageUsage::CpuWrite)        { vkImageUsageFlags |= VK_IMAGE_USAGE_TRANSFER_DST_BIT; }

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

	Mem mem = {};
	if (Res<> r = AllocateMem(vkMemoryRequirements, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 0).To(mem); !r) {
		vkDestroyImage(vkDevice, vkImage, vkAllocationCallbacks);
		return r.err;
	}
		
	if (const VkResult r = vkBindImageMemory(vkDevice, vkImage, mem.vkDeviceMemory, 0); r != VK_SUCCESS) {
		vkDestroyImage(vkDevice, vkImage, vkAllocationCallbacks);
		FreeMem(mem);
		return Err_Vk(r, "vkBindImageMemory");
	}

	const VkImageViewCreateInfo vkImageViewCreateInfo = {
		.sType              = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.pNext              = 0,
		.flags              = 0,
		.image              = vkImage,
		.viewType           = VK_IMAGE_VIEW_TYPE_2D,
		.format             = vkFormat,
		.components         = { .r = VK_COMPONENT_SWIZZLE_IDENTITY, .g = VK_COMPONENT_SWIZZLE_IDENTITY, .b = VK_COMPONENT_SWIZZLE_IDENTITY, .a = VK_COMPONENT_SWIZZLE_IDENTITY },
		.subresourceRange   = MakeVkSubresourceRange(IsDepthFormat(vkFormat) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT),
	};
	VkImageView vkImageView = VK_NULL_HANDLE;
	if (VkResult r = vkCreateImageView(vkDevice, &vkImageViewCreateInfo, vkAllocationCallbacks, &vkImageView); r != VK_SUCCESS) {
		vkDestroyImage(vkDevice, vkImage, vkAllocationCallbacks);
		FreeMem(mem);
		return Err_Vk(r, "vkCreateImageView");
	}

	U32 bindlessIdx = 0;
	if (vkImageUsageFlags & VK_IMAGE_USAGE_SAMPLED_BIT) {
		bindlessIdx = nextBindlessDescriptorIdx++;
		const VkDescriptorImageInfo vkDescriptorImageInfo = {
			.sampler     = VK_NULL_HANDLE,
			.imageView   = vkImageView,
			.imageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
		};
		const VkWriteDescriptorSet vkWriteDescriptorSet = {
			.sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.pNext            = 0,
			.dstSet           = vkBindlessDescriptorSet,
			.dstBinding       = 0,
			.dstArrayElement  = bindlessIdx,
			.descriptorCount  = 1,
			.descriptorType   = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
			.pImageInfo       = &vkDescriptorImageInfo,
			.pBufferInfo      = 0,
			.pTexelBufferView = 0,
		};
		vkUpdateDescriptorSets(vkDevice, 1, &vkWriteDescriptorSet, 0, 0);
	}

	const Image image = imageObjs.Alloc();
	*imageObjs.Get(image) = {
		.vkImage           = vkImage,
		.vkImageView       = vkImageView,
		.mem               = mem,
		.width             = width,
		.height            = height,
		.vkFormat          = vkFormat,
		.vkImageUsageFlags = vkImageUsageFlags,
		.vkImageLayout     = VK_IMAGE_LAYOUT_UNDEFINED,
		.bindlessIdx       = bindlessIdx,
		.stagingOffset     = U64Max,
	};

	return image;
}
//-------------------------------------------------------------------------------------------------

void DestroyImage(Image image) {
	if (image.handle) {
		ImageObj* const imageObj = imageObjs.Get(image);
		DestroyVk(imageObj->vkImageView, vkDestroyImageView);
		FreeMem(imageObj->mem);
		DestroyVk(imageObj->vkImage, vkDestroyImage);
		imageObjs.Free(image);
	}
}

//-------------------------------------------------------------------------------------------------

U32         GetImageWidth (Image image)  { return imageObjs.Get(image)->width;  }
U32         GetImageHeight(Image image)  { return imageObjs.Get(image)->height; }
ImageFormat GetImageFormat(Image image)  { return VkFormatToImageFormat(imageObjs.Get(image)->vkFormat); }
U32         GetImageBindIdx(Image image) { return imageObjs.Get(image)->bindlessIdx; }

//-------------------------------------------------------------------------------------------------

Res<Shader> CreateShader(const void* data, U64 len) {
	SpvReflectShaderModule spvReflectShaderModule = {};
	if (const SpvReflectResult r = spvReflectCreateShaderModule2(SPV_REFLECT_MODULE_FLAG_NO_COPY, len, data, &spvReflectShaderModule); r != SPV_REFLECT_RESULT_SUCCESS) {
		return Err_SpvReflect("code", r, "fn", "spvReflectCreateShaderModule2");
	}

	if (spvReflectShaderModule.push_constant_block_count > 1) {
		spvReflectDestroyShaderModule(&spvReflectShaderModule);
		return Err_SpvReflect("push_constant_block_count", spvReflectShaderModule.push_constant_block_count, "fn", "Err_ShaderTooManyPushConstantBlocks");
	}

	const VkShaderModuleCreateInfo vkShaderModuleCreateInfo = {
		.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		.pNext    = 0,
		.flags    = 0,
		.codeSize = (U32)len,
		.pCode    = (const U32*)data,
	};
	VkShaderModule vkShaderModule = VK_NULL_HANDLE;
	if (const VkResult r = vkCreateShaderModule(vkDevice, &vkShaderModuleCreateInfo, vkAllocationCallbacks, &vkShaderModule); r != VK_SUCCESS) {
		spvReflectDestroyShaderModule(&spvReflectShaderModule);
		return Err_Vk(r, "vkCreateShaderModule");
	}

	const Shader shader = shaderObjs.Alloc();
	ShaderObj* const shaderObj = shaderObjs.Get(shader);
	shaderObj->vkShaderModule = vkShaderModule;
	shaderObj->vkShaderStage = (VkShaderStageFlagBits)spvReflectShaderModule.shader_stage;	// SPV flags defined 1:1 with VK flags
	if (spvReflectShaderModule.push_constant_block_count && spvReflectShaderModule.push_constant_blocks) {
		shaderObj->pushConstantsOffset = spvReflectShaderModule.push_constant_blocks[0].offset;
		shaderObj->pushConstantsSize   = spvReflectShaderModule.push_constant_blocks[0].size;
	} else {
		shaderObj->pushConstantsOffset = 0;
		shaderObj->pushConstantsSize   = 0;
	}

	spvReflectDestroyShaderModule(&spvReflectShaderModule);

	return shader;
}

//-------------------------------------------------------------------------------------------------

void DestroyShader(Shader shader) {
	if (shader.handle) {
		ShaderObj* const shaderObj = shaderObjs.Get(shader);
		DestroyVk(shaderObj->vkShaderModule, vkDestroyShaderModule);
		shaderObjs.Free(shader);
	}
}

//----------------------------------------------------------------------------------------------

Res<Pipeline> CreateGraphicsPipeline(Span<Shader> shaders, Span<ImageFormat> colorAttachmentFormats, ImageFormat depthFormat) {
	Assert(shaders.len > 0);

	VkShaderStageFlags vkShaderStageFlags = 0;
	VkPushConstantRange vkPushConstantRange = {};
	VkPipelineShaderStageCreateInfo* const vkPipelineShaderStageCreateInfos = tempAllocator->AllocT<VkPipelineShaderStageCreateInfo>(shaders.len);
	for (U64 i = 0; i < shaders.len; i++) {
		ShaderObj* const shaderObj = shaderObjs.Get(shaders[i]);

		vkShaderStageFlags |= shaderObj->vkShaderStage;

		if (shaderObj->pushConstantsSize) {
			vkPushConstantRange.stageFlags |= shaderObj->vkShaderStage;
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
			.stage               = shaderObj->vkShaderStage,
			.module              = shaderObj->vkShaderModule,
			.pName               = "main",
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
		return Err_Vk(r, "vkCreatePipelineLayout");
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
	const ImageObj* swapchainImageObj = imageObjs.Get(swapchainImages[0]);
	const VkViewport vkViewport = {
		.x        = 0.0f,
		.y        = 0.0f,
		.width    = (F32)swapchainImageObj->width,
		.height   = (F32)swapchainImageObj->height,
		.minDepth = 0.0f,
		.maxDepth = 1.0f,
	};
	const VkRect2D vkScissorRect2D = {
		.offset     = {
			.x      = 0,
			.y      = 0,
		},
		.extent     = {
			.width  = swapchainImageObj->width,
			.height = swapchainImageObj->height,
		},
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
		.depthCompareOp        = VK_COMPARE_OP_LESS,
		.depthBoundsTestEnable = VK_FALSE,
		.stencilTestEnable     = VK_FALSE,
		.front                 = vkNoOpStencilOpState,
		.back                  = vkNoOpStencilOpState,
		.minDepthBounds        = 0.0f,
		.maxDepthBounds        = 1.0f,
	};
	VkPipelineColorBlendAttachmentState* const vkPipelineColorBlendAttachmentStates = tempAllocator->AllocT<VkPipelineColorBlendAttachmentState>(colorAttachmentFormats.len);
	for (U64 i = 0; i < colorAttachmentFormats.len; i++) {
		vkPipelineColorBlendAttachmentStates[i] = {
			.blendEnable         = VK_TRUE,
			.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
			.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
			.colorBlendOp        = VK_BLEND_OP_ADD,
			.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
			.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
			.alphaBlendOp        = VK_BLEND_OP_ADD,
			.colorWriteMask      = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
		};
	}
	const VkPipelineColorBlendStateCreateInfo vkPipelineColorBlendStateCreateInfo = {
		.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
		.pNext           = 0,
		.flags           = 0,
		.logicOpEnable   = VK_FALSE,
		.logicOp         = VK_LOGIC_OP_CLEAR,
		.attachmentCount = (U32)colorAttachmentFormats.len,
		.pAttachments    = vkPipelineColorBlendAttachmentStates,
		.blendConstants  = 0.0f,
	};
	constexpr VkDynamicState vkDynamicStates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
	const VkPipelineDynamicStateCreateInfo vkPipelineDynamicStateCreateInfo = {
		.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
		.pNext             = 0,
		.flags             = 0,
		.dynamicStateCount = (U32)LenOf(vkDynamicStates),
		.pDynamicStates    = vkDynamicStates,
	};
	VkFormat* const vkColorAttachmentFormats = tempAllocator->AllocT<VkFormat>(colorAttachmentFormats.len);
	for (U64 i = 0; i < colorAttachmentFormats.len; i++) {
		vkColorAttachmentFormats[i] = ImageFormatToVkFormat(colorAttachmentFormats[i]);
	}
	const VkPipelineRenderingCreateInfo vkPipelineRenderingCreateInfo = {
		.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
		.pNext                   = 0,
		.viewMask                = 0,
		.colorAttachmentCount    = (U32)colorAttachmentFormats.len,
		.pColorAttachmentFormats = vkColorAttachmentFormats,
		.depthAttachmentFormat   = ImageFormatToVkFormat(depthFormat),
		.stencilAttachmentFormat = VK_FORMAT_UNDEFINED,
	};
	const VkGraphicsPipelineCreateInfo vkGraphicsPipelineCreateInfo = {
		.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
		.pNext               = &vkPipelineRenderingCreateInfo,
		.flags               = 0,
		.stageCount          = (U32)shaders.len,
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
		return Err_Vk(r, "vkCreateGraphicsPipelines");
	}

	const Pipeline pipeline = pipelineObjs.Alloc();
	PipelineObj* const pipelineObj = pipelineObjs.Get(pipeline);
	pipelineObj->vkPipelineLayout    = vkPipelineLayout;
	pipelineObj->vkPipeline          = vkPipeline;
	pipelineObj->vkPipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	pipelineObj->vkPushConstantRange = vkPushConstantRange;

	return pipeline;
}

//-------------------------------------------------------------------------------------------------

void DestroyPipeline(Pipeline pipeline) {
	if (pipeline.handle) {
		PipelineObj* const pipelineObj = pipelineObjs.Get(pipeline);
		DestroyVk(pipelineObj->vkPipeline, vkDestroyPipeline);
		DestroyVk(pipelineObj->vkPipelineLayout, vkDestroyPipelineLayout);
		pipelineObjs.Free(pipeline);
	}
}

//-------------------------------------------------------------------------------------------------

static U64 AllocStagingMem(U64 size) {
	stagingBufferUsed = Bit::AlignUp(stagingBufferUsed, 8);
	U64 offset = stagingBufferUsed;
	stagingBufferUsed += size;
	Assert(stagingBufferUsed < StagingBufferPerFrameSize);
	return offset;
}

//-------------------------------------------------------------------------------------------------

static VkCommandBuffer CmdToVkCommandBuffer(Cmd cmd) {
	Assert(cmd.handle > 0 && cmd.handle < LenOf(cmdToVkCommandBuffer));
	return cmdToVkCommandBuffer[cmd.handle];
}

//----------------------------------------------------------------------------------------------

Res<Frame> BeginFrame() {
	CheckRes(WaitTimelineSemaphore(vkFrameTimelineSemaphore, frame));

	if (VkResult r = vkAcquireNextImageKHR(vkDevice, vkSwapchain, U64Max, vkFrameImageAcquiredSemaphores[frameIdx], 0, &swapchainImageIdx); r != VK_SUCCESS) {
		if (r == VK_SUBOPTIMAL_KHR || r == VK_ERROR_OUT_OF_DATE_KHR) {
			return Err_RecreateSwapchain();
		} else {
			return Err_Vk(r, "vkAcquireNextImageKHR");
		}
	}

	CheckRes(BeginCommandBuffer(vkFrameCommandBuffers[frameIdx], VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT));
	const Cmd cmd = { .handle = 1 + frameIdx };


	CmdImageBarrier(cmd, swapchainImages[swapchainImageIdx], Stage::PresentSrc, Stage::ColorAttachmentOutput);
	//VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT -> VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT
	//0 -> VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT


	stagingBufferUsed = 0;

	return Frame {
		.cmd            = cmd,
		.swapchainImage = swapchainImages[swapchainImageIdx],
	};
};

//----------------------------------------------------------------------------------------------

Res<> EndFrame() {
	CmdImageBarrier(Cmd { .handle = 1 + frameIdx }, swapchainImages[swapchainImageIdx], Stage::ColorAttachmentOutput, Stage::PresentSrc);

	CheckVk(vkEndCommandBuffer(vkFrameCommandBuffers[frameIdx]));

	CheckRes(SubmitQueue(
		tempAllocator,
		vkQueue,
		{ vkFrameCommandBuffers[frameIdx] },
		{
			{ vkFrameImageAcquiredSemaphores[frameIdx], VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR, 0 }
		},
		{
			{ vkFrameSubmitCompleteSemaphores[frameIdx], VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR, 0 },
			{ vkFrameTimelineSemaphore, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR, frame + MaxFrames },
			
		}
	));

	//vkDeviceWaitIdle(vkDevice);

	VkPresentInfoKHR vkPresentInfoKHR = {
		.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
		.pNext              = 0,
		.waitSemaphoreCount = 1,
		.pWaitSemaphores    = &vkFrameSubmitCompleteSemaphores[frameIdx],
		.swapchainCount     = 1,
		.pSwapchains        = &vkSwapchain,
		.pImageIndices      = &swapchainImageIdx,
		.pResults           = 0,
	};
	if (VkResult r = vkQueuePresentKHR(vkQueue, &vkPresentInfoKHR); r != VK_SUCCESS) {
		if (r == VK_SUBOPTIMAL_KHR || r == VK_ERROR_OUT_OF_DATE_KHR) {
			return Err_RecreateSwapchain();
		} else {
			return Err_Vk(r, "vkAcquireNextImageKHR");
		}
	}

	frame++;
	frameIdx = frame % MaxFrames;

	return Ok();
}

//-------------------------------------------------------------------------------------------------

Res<Cmd> BeginImmediateCmds() {
	return Cmd { .handle = 1 + MaxFrames };
}

//-------------------------------------------------------------------------------------------------

Res<> EndImmediateCmds(Cmd cmd, Stage waitStage) {
	CheckVk(vkEndCommandBuffer(CmdToVkCommandBuffer(cmd)));

	immediateTimeline++;

	CheckRes(SubmitQueue(
		tempAllocator,
		vkQueue,
		{ vkImmediateCommandBuffer },
		{ },
		{ { vkImmediateTimelineSemaphore, GetStageFlags(waitStage).vkPipelineStageFlagBits2, immediateTimeline } }
	));

	CheckRes(WaitTimelineSemaphore(vkImmediateTimelineSemaphore, immediateTimeline));

	return Ok();
}

//-------------------------------------------------------------------------------------------------

void CmdBeginPass(Cmd cmd, const Pass* pass) {
	const VkCommandBuffer vkCommandBuffer = CmdToVkCommandBuffer(cmd);

	VkRenderingAttachmentInfo* const vkColorRenderingAttachmentInfos = tempAllocator->AllocT<VkRenderingAttachmentInfo>(pass->colorAttachments.len);
	for (U64 i = 0; i < pass->colorAttachments.len; i++) {
		ImageObj* const imageObj = imageObjs.Get(pass->colorAttachments[i]);
		vkColorRenderingAttachmentInfos[i] = {
			.sType              = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
			.pNext              = 0,
			.imageView          = imageObj->vkImageView,
			.imageLayout        = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			.resolveMode        = VK_RESOLVE_MODE_NONE,
			.resolveImageView   = VK_NULL_HANDLE,
			.resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
			.loadOp             = pass->clear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD,
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
			.loadOp             = VK_ATTACHMENT_LOAD_OP_CLEAR,
			.storeOp            = VK_ATTACHMENT_STORE_OP_STORE,
			.clearValue         = { .depthStencil = { .depth = 1.0f, .stencil = 0 } },
		};
	}

	const VkRenderingInfo vkRenderingInfo = {
		.sType                = VK_STRUCTURE_TYPE_RENDERING_INFO,
		.pNext                = 0,
		.flags                = 0,
		.renderArea           = { .offset = { 0, 0 }, .extent = { .width = (U32)pass->viewport.w, .height = (U32)pass->viewport.h} },
		.layerCount           = 1,
		.viewMask             = 0,
		.colorAttachmentCount = (U32)pass->colorAttachments.len,
		.pColorAttachments    = vkColorRenderingAttachmentInfos,
		.pDepthAttachment     = depthImageObj ? &vkDepthRenderingAttachmentInfo : 0,
		.pStencilAttachment   = 0,
	};
	vkCmdBeginRendering(vkCommandBuffer, &vkRenderingInfo);

	const VkViewport vkViewport = {
		.x        = pass->viewport.x,
		.y        = pass->viewport.y,
		.width    = pass->viewport.w,
		.height   = pass->viewport.h,
		.minDepth = 0.0f,
		.maxDepth = 1.0f,
	};
	vkCmdSetViewport(vkCommandBuffer, 0, 1, &vkViewport);

	const VkRect2D vkScissorRect2D = {
		.offset     = {
			.x      = pass->scissor.x,
			.y      = pass->scissor.y,
		},
		.extent     = {
			.width  = (U32)pass->scissor.w,
			.height = (U32)pass->scissor.h
		},
	};
	vkCmdSetScissor(vkCommandBuffer, 0, 1, &vkScissorRect2D);

	PipelineObj* const pipelineObj = pipelineObjs.Get(pass->pipeline);
	vkCmdBindPipeline(vkCommandBuffer, pipelineObj->vkPipelineBindPoint, pipelineObj->vkPipeline);

	vkCmdBindDescriptorSets(vkCommandBuffer, pipelineObj->vkPipelineBindPoint, pipelineObj->vkPipelineLayout, 0, 1, &vkBindlessDescriptorSet, 0, 0);
}

//----------------------------------------------------------------------------------------------

void CmdEndPass(Cmd cmd) {
	vkCmdEndRendering(CmdToVkCommandBuffer(cmd));
}

//----------------------------------------------------------------------------------------------

void* CmdBeginBufferUpload(Cmd, Buffer buffer, U64 offset, U64 size) {
	BufferObj* const bufferObj = bufferObjs.Get(buffer);
	Assert(bufferObj->stagingOffset == U64Max);
	Assert(offset + size < bufferObj->size);
	bufferObj->stagingOffset = AllocStagingMem(size);
	bufferObj->uploadOffset  = offset;
	bufferObj->uploadSize    = size;
	return stagingBufferPtr + (frameIdx * StagingBufferPerFrameSize) + bufferObj->stagingOffset;
}

void CmdEndBufferUpload(Cmd cmd, Buffer buffer) {
	const VkCommandBuffer vkCommandBuffer = CmdToVkCommandBuffer(cmd);
	BufferObj* const bufferObj = bufferObjs.Get(buffer);
	const VkBufferCopy2 vkBufferCopy2 = {
		.sType     = VK_STRUCTURE_TYPE_BUFFER_COPY_2,
		.pNext     = 0,
		.srcOffset = (frameIdx * StagingBufferPerFrameSize) + bufferObj->stagingOffset,
		.dstOffset = bufferObj->uploadOffset,
		.size      = bufferObj->uploadSize,
	};
	const VkCopyBufferInfo2 vkCopyBufferInfo2 = {
		.sType       = VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2,
		.pNext       = 0,
		.srcBuffer   = stagingBufferObj.vkBuffer,
		.dstBuffer   = bufferObj->vkBuffer,
		.regionCount = 1,
		.pRegions    = &vkBufferCopy2,
	};
	vkCmdCopyBuffer2(vkCommandBuffer, &vkCopyBufferInfo2);

	bufferObj->stagingOffset = U64Max;
	bufferObj->uploadOffset  = U64Max;
	bufferObj->uploadSize    = U64Max;
}

//-------------------------------------------------------------------------------------------------

void* CmdBeginImageUpload(Cmd, Image image) {
	ImageObj* const imageObj = imageObjs.Get(image);
	Assert(imageObj->stagingOffset == U64Max);
	imageObj->stagingOffset = AllocStagingMem(imageObj->width * imageObj->height * FormatSize(imageObj->vkFormat));
	return stagingBufferPtr + (frameIdx * StagingBufferPerFrameSize) + imageObj->stagingOffset;
}

void CmdEndImageUpload(Cmd cmd, Image image) {
	const VkCommandBuffer vkCommandBuffer = CmdToVkCommandBuffer(cmd);
	ImageObj* const imageObj = imageObjs.Get(image);
	const VkBufferImageCopy2 vkBufferImageCopy2 = {
		.sType             = VK_STRUCTURE_TYPE_BUFFER_IMAGE_COPY_2,
		.pNext             = 0,
		.bufferOffset      = imageObj->stagingOffset,
		.bufferRowLength   = 0,
		.bufferImageHeight = 0,
		.imageSubresource  = MakeVkImageSubresourceLayers(IsDepthFormat(imageObj->vkFormat) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT),
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
	vkCmdCopyBufferToImage2(vkCommandBuffer, &vkCopyBufferToImageInfo2);

	imageObj->stagingOffset = U64Max;
}

//-------------------------------------------------------------------------------------------------

void CmdBufferBarrier(Cmd cmd, Buffer buffer, Stage srcStage, Stage dstStage) {
	const StageFlags srcStageFlags = GetStageFlags(srcStage);
	const StageFlags dstStageFlags = GetStageFlags(dstStage);
	const VkBufferMemoryBarrier2 vkBufferMemoryBarrier2 = {
		.sType               = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
		.pNext               = 0,
		.srcStageMask        = srcStageFlags.vkPipelineStageFlagBits2,
		.srcAccessMask       = srcStageFlags.vkAccessFlagBits2,
		.dstStageMask        = dstStageFlags.vkPipelineStageFlagBits2,
		.dstAccessMask       = dstStageFlags.vkAccessFlagBits2,
		.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.buffer              = bufferObjs.Get(buffer)->vkBuffer,
		.offset              = 0,
		.size                = VK_WHOLE_SIZE,
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
	vkCmdPipelineBarrier2(CmdToVkCommandBuffer(cmd), &vkDependencyInfo);
}

//-------------------------------------------------------------------------------------------------

void CmdImageBarrier(Cmd cmd, Image image, Stage srcStage, Stage dstStage) {
	ImageObj* const imageObj = imageObjs.Get(image);
	const StageFlags srcStageFlags = GetStageFlags(srcStage);
	const StageFlags dstStageFlags = GetStageFlags(dstStage);
	const VkImageMemoryBarrier2 vkImageMemoryBarrier2 = {
		.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
		.pNext               = 0,
		.srcStageMask        = srcStageFlags.vkPipelineStageFlagBits2,
		.srcAccessMask       = srcStageFlags.vkAccessFlagBits2,
		.dstStageMask        = dstStageFlags.vkPipelineStageFlagBits2,
		.dstAccessMask       = dstStageFlags.vkAccessFlagBits2,
		.oldLayout           = imageObj->vkImageLayout,
		.newLayout           = dstStageFlags.vkImageLayout,
		.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.image               = imageObj->vkImage,
		.subresourceRange    = MakeVkSubresourceRange(IsDepthFormat(imageObj->vkFormat) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT),
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
	vkCmdPipelineBarrier2(CmdToVkCommandBuffer(cmd), &vkDependencyInfo);
	imageObj->vkImageLayout = dstStageFlags.vkImageLayout;
}

//-------------------------------------------------------------------------------------------------

void CmdDebugBarrier(Cmd cmd) {
	const VkCommandBuffer vkCommandBuffer = CmdToVkCommandBuffer(cmd);
	const VkMemoryBarrier2 vkMemoryBarrier2 = {
		.sType          = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
		.pNext          = 0,
		.srcStageMask   = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
		.srcAccessMask  = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
		.dstStageMask   = VK_ACCESS_2_MEMORY_READ_BIT,
		.dstAccessMask  = VK_ACCESS_2_MEMORY_WRITE_BIT,
	};
	const VkDependencyInfo vkDependencyInfo = {
		.sType                    = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
		.pNext                    = 0,
		.dependencyFlags          = 0,
		.memoryBarrierCount       = 1,
		.pMemoryBarriers          = &vkMemoryBarrier2,
		.bufferMemoryBarrierCount = 0,
		.pBufferMemoryBarriers    = 0,
		.imageMemoryBarrierCount  = 0,
		.pImageMemoryBarriers     = 0,
	};
	vkCmdPipelineBarrier2(vkCommandBuffer, &vkDependencyInfo);
}

//-------------------------------------------------------------------------------------------------

void CmdBindIndexBuffer(Cmd cmd, Buffer buffer) {
	const VkCommandBuffer vkCommandBuffer = CmdToVkCommandBuffer(cmd);
	vkCmdBindIndexBuffer(vkCommandBuffer, bufferObjs.Get(buffer)->vkBuffer, 0, VK_INDEX_TYPE_UINT32);
}

//----------------------------------------------------------------------------------------------

void CmdPushConstants(Cmd cmd, Pipeline pipeline, const void* ptr, U32 len) {
	const VkCommandBuffer vkCommandBuffer = CmdToVkCommandBuffer(cmd);
	PipelineObj* const pipelineObj = pipelineObjs.Get(pipeline);
	vkCmdPushConstants(vkCommandBuffer, pipelineObj->vkPipelineLayout, pipelineObj->vkPushConstantRange.stageFlags, 0, len, ptr);
}

//----------------------------------------------------------------------------------------------

void CmdDraw(Cmd cmd, U32 vertexCount, U32 instanceCount) {
	const VkCommandBuffer vkCommandBuffer = CmdToVkCommandBuffer(cmd);
	vkCmdDraw(vkCommandBuffer, vertexCount, instanceCount, 0, 0);
}

void CmdDrawIndexed(Cmd cmd, U32 indexCount) {
	const VkCommandBuffer vkCommandBuffer = CmdToVkCommandBuffer(cmd);
	vkCmdDrawIndexed(vkCommandBuffer, indexCount, 1, 0, 0, 0);
}

//----------------------------------------------------------------------------------------------

void WaitIdle() {
	if (vkDevice) {
		vkDeviceWaitIdle(vkDevice);
	}
}

//----------------------------------------------------------------------------------------------

void SetName(Buffer  buffer, const char* name) {
	BufferObj* const bufferObj = bufferObjs.Get(buffer);
	SetDbgName(bufferObj->mem.vkDeviceMemory, Fmt::Printfz(tempAllocator, "{}Mem", name));
	SetDbgName(bufferObj->vkBuffer, name);
}

void SetName(Image image, const char* name) {
	ImageObj* const imageObj = imageObjs.Get(image);
	SetDbgName(imageObj->mem.vkDeviceMemory, Fmt::Printfz(tempAllocator, "{}Mem", name));
	SetDbgName(imageObj->vkImage, name);
}

void SetName(Shader shader, const char* name) {
	SetDbgName(shaderObjs.Get(shader)->vkShaderModule, name);
}

void SetName(Pipeline pipeline, const char* name) {
	SetDbgName(pipelineObjs.Get(pipeline)->vkPipeline, name);
	SetDbgName(pipelineObjs.Get(pipeline)->vkPipelineLayout, Fmt::Printfz(tempAllocator, "{}Layout", name));
}

//----------------------------------------------------------------------------------------------

}	// namespace JC::Gpu