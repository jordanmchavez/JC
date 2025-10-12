#pragma once

#include "JC/Gpu.h"
#include "JC/Gpu_Vk.h"

#include "JC/Bit.h"
#include "JC/Common_Err.h"
#include "JC/Common_Fmt.h"
#include "JC/Handle.h"
#include "JC/Log.h"
#include "JC/Sys.h"
#include "JC/Window.h"

#include "spirv-reflect/spirv_reflect.h"

namespace JC::Gpu {

//--------------------------------------------------------------------------------------------------

Err_Def(Gpu, Version);
Err_Def(Gpu, NoLayer);
Err_Def(Gpu, NoExt);
Err_Def(Gpu, NoDevice);
Err_Def(Gpu, NoMemType);
Err_Def(Gpu, NoMem);
Err_Def(Gpu, ShaderTooManyPushConstantBlocks);
Err_Def(Gpu, SpvReflect);

//--------------------------------------------------------------------------------------------------

static constexpr U32 MaxBuffers	                      = 4096;
static constexpr U32 MaxImages	                      = 4096;
static constexpr U32 MaxShaders	                      = 1024;
static constexpr U32 MaxPipelines                     = 128;
static constexpr U32 MaxBindlessSampledImages         = 64 * 1024;
static constexpr U32 MaxBindlessSamplers              = 8;
static constexpr U32 MaxBindlessDescriptorSets        = 32;
static constexpr F32 MaxAnisotropy                    = 8.0f;
static constexpr U64 PerFrameStagingBufferSize        = 256 * MB;
static constexpr U64 ImmediateStagingBufferSize       = 64 * MB;

//--------------------------------------------------------------------------------------------------

struct QueueFamily {
	VkQueueFamilyProperties vkQueueFamilyProperties;
	bool                    supportsPresent;
};

struct PhysicalDevice {
	VkPhysicalDevice                 vkPhysicalDevice;
	VkPhysicalDeviceProperties2      vkPhysicalDeviceProperties2;
	VkPhysicalDeviceFeatures2        vkPhysicalDeviceFeatures2;
	VkPhysicalDeviceVulkan12Features vkPhysicalDeviceVulkan12Features;
	VkPhysicalDeviceVulkan13Features vkPhysicalDeviceVulkan13Features;
	VkPhysicalDeviceMemoryProperties vkPhysicalDeviceMemoryProperties;
	Span<VkExtensionProperties>      vkExtensionProperties;
	Span<VkSurfaceFormatKHR>         vkSurfaceFormats;
	VkFormat                         vkSwapchainFormat;
	VkColorSpaceKHR                  vkSwapchainColorSpace;
	Span<VkPresentModeKHR>           vkPresentModes;
	Span<QueueFamily>                queueFamilies;
	U32                              queueFamily;
};

struct Allocation {
	VkDeviceMemory vkDeviceMemory;
	U32            memTypeIdx;
	U64            size;
};

struct BufferObj {
	VkBuffer           vkBuffer;
	Allocation         allocation;
	U64                size;
	VkBufferUsageFlags vkBufferUsageFlags;
	U64                addr;
};

struct ImageObj {
	VkImage     vkImage;
	VkImageView vkImageView;
	Allocation  allocation;
	U32         width;
	U32         height;
	VkFormat    vkFormat;
	U32         bindlessIdx;
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

//--------------------------------------------------------------------------------------------------

using BufferPool   = HandleArray<BufferObj,   Buffer>;
using ImagePool    = HandleArray<ImageObj,    Image>;
using ShaderPool   = HandleArray<ShaderObj,   Shader>;
using PipelinePool = HandleArray<PipelineObj, Pipeline>;

static Mem::Mem                 permMem;
static Mem::Mem                 tempMem;
static bool                     enableDebug = true;	// TODO: expose via cfg
static Sys::Mutex               mutex;
static BufferPool               bufferObjs;
static ImagePool                imageObjs;
static ShaderPool               shaderObjs;
static PipelinePool             pipelineObjs;
       VkAllocationCallbacks*   vkAllocationCallbacks;	// not static: referenced in Vk_Util.cpp
static VkInstance               vkInstance;
static VkDebugUtilsMessengerEXT vkDebugUtilsMessenger;
static VkSurfaceKHR             vkSurface;
static Span<PhysicalDevice>     physicalDevices;
static PhysicalDevice*          physicalDevice;
       VkDevice                 vkDevice;		// not static: referenced in Vk_Util.cpp
static VkQueue                  vkQueue;
static VkSwapchainKHR           vkSwapchain;
static Span<Image>              swapchainImages;
static U32                      swapchainImageIdx;
static VkCommandPool            vkFrameCommandPool;
static VkCommandBuffer          vkFrameCommandBuffers[MaxFrames];
static BufferObj                frameStagingBuffer;
static U8*                      frameStagingBufferPtrs[MaxFrames];
static U64                      frameStagingBufferUsed[MaxFrames];
static VkSemaphore              vkFrameTimelineSemaphore;
static VkSemaphore              vkFrameImageAcquiredSemaphores[MaxFrames];
static VkSemaphore              vkFrameSubmitCompleteSemaphores[MaxFrames];
static U32                      frameIdx;
static U64                      frame;
static VkCommandPool            vkImmediateCommandPool;
static VkCommandBuffer          vkImmediateCommandBuffer;
static bool                     vkImmediateCommandBufferBegun;
static VkSemaphore              vkImmediateTimelineSemaphore;
static U64                      immediateTimeline;
static BufferObj                immediateStagingBuffer;
static U8*                      immediateStagingBufferPtr;
static U64                      immediateStagingBufferUsed;
static VkDescriptorPool         vkDescriptorPool;
static VkDescriptorSetLayout    vkBindlessDescriptorSetLayout;
static VkDescriptorSet          vkBindlessDescriptorSet;
static U32                      nextBindlessDescriptorIdx;
static VkSampler                vkBindlessSamplers[MaxBindlessSamplers];
static U32                      vkBindlessSamplersLen;

static Res<BufferObj> CreateBufferImpl(
	U64                   size,
	VkBufferUsageFlags    vkBufferUsageFlags,
	VkMemoryPropertyFlags vkMemoryPropertyFlagsNeed,
	VkMemoryPropertyFlags vkMemoryPropertyFlagsWant,
	VkMemoryPropertyFlags vkMemoryPropertyFlagsAvoid,
	VkMemoryAllocateFlags vkMemoryAllocateFlags
);

//-------------------------------------------------------------------------------------------------

static VkBool32 VKAPI_CALL DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT, VkDebugUtilsMessengerCallbackDataEXT const* data, void*) {
	if (data && data->pMessage) {
		Sys::LockMutex(&mutex);
		if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
			Errorf("%s", data->pMessage);
			#if defined BreakOnError
				if (Sys_DbgPresent()) {
					Dbg_Break;
				}
			#endif	// BreakOnError
		} else if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
			//Errorf("{}", data->pMessage);
		} else {
			Logf("%s", data->pMessage);
		}
		Sys::UnlockMutex(&mutex);
	}
	return VK_FALSE;
}
	
//-------------------------------------------------------------------------------------------------

static void VkNameImpl(SrcLoc sl, U64 handle, VkObjectType vkObjectType, char const* fmt, Span<Arg::Arg const> args) {
	if (!enableDebug) {
		return;
	}

	Fmt::PrintBuf pb(tempMem);
	pb.Printv(fmt, args);
	pb.Printf(" (%s:%u)", sl.file, sl.line);
	pb.Add('\0');

	VkDebugUtilsObjectNameInfoEXT const vkDebugUtilsObjectNameInfoEXT = {
		.sType        = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
		.pNext        = 0,
		.objectType   = vkObjectType,
		.objectHandle = handle,
		.pObjectName  = pb.data,
	};
	VkResult vkResult = vkSetDebugUtilsObjectNameEXT(vkDevice, &vkDebugUtilsObjectNameInfoEXT);
	Assert(vkResult == VK_SUCCESS);
}

#define DefVkName(VkType, vkObjectType) \
	void VkNamev(SrcLoc sl, VkType obj, char const* fmt, Span<Arg::Arg const> args) { VkNameImpl(sl, (U64)obj, vkObjectType, fmt, args); }

DefVkName(VkInstance,            VK_OBJECT_TYPE_INSTANCE)
DefVkName(VkPhysicalDevice,      VK_OBJECT_TYPE_PHYSICAL_DEVICE)
DefVkName(VkDevice,              VK_OBJECT_TYPE_DEVICE)
DefVkName(VkQueue,               VK_OBJECT_TYPE_QUEUE)
DefVkName(VkSemaphore,           VK_OBJECT_TYPE_SEMAPHORE)
DefVkName(VkCommandBuffer,       VK_OBJECT_TYPE_COMMAND_BUFFER)
DefVkName(VkFence,               VK_OBJECT_TYPE_FENCE)
DefVkName(VkDeviceMemory,        VK_OBJECT_TYPE_DEVICE_MEMORY)
DefVkName(VkBuffer,              VK_OBJECT_TYPE_BUFFER)
DefVkName(VkImage,               VK_OBJECT_TYPE_IMAGE)
DefVkName(VkEvent,               VK_OBJECT_TYPE_EVENT)
DefVkName(VkQueryPool,           VK_OBJECT_TYPE_QUERY_POOL)
DefVkName(VkBufferView,          VK_OBJECT_TYPE_BUFFER_VIEW)
DefVkName(VkImageView,           VK_OBJECT_TYPE_IMAGE_VIEW)
DefVkName(VkShaderModule,        VK_OBJECT_TYPE_SHADER_MODULE)
DefVkName(VkPipelineCache,       VK_OBJECT_TYPE_PIPELINE_CACHE)
DefVkName(VkPipelineLayout,      VK_OBJECT_TYPE_PIPELINE_LAYOUT)
DefVkName(VkPipeline,            VK_OBJECT_TYPE_PIPELINE)
DefVkName(VkDescriptorSetLayout, VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT)
DefVkName(VkSampler,             VK_OBJECT_TYPE_SAMPLER)
DefVkName(VkDescriptorPool,      VK_OBJECT_TYPE_DESCRIPTOR_POOL)
DefVkName(VkDescriptorSet,       VK_OBJECT_TYPE_DESCRIPTOR_SET)
DefVkName(VkCommandPool,         VK_OBJECT_TYPE_COMMAND_POOL)

template <class T, class... A>
void VkNamefImpl(SrcLoc sl, T obj, Fmt::CheckStr<A...> fmt, A... args) {
	VkNamev(sl, obj, fmt, { Arg::Make(args)..., });
}

#define VkName(obj)            VkNamefImpl(SrcLoc::Here(), obj, #obj)
#define VkNamef(obj, fmt, ...) VkNamefImpl(SrcLoc::Here(), obj, fmt, ##__VA_ARGS__)

void Namev(SrcLoc sl, Buffer   buffer,   char const* fmt, Span<Arg::Arg const> args) { VkNameImpl(sl, (U64)bufferObjs.Get(buffer)->vkBuffer,             VK_OBJECT_TYPE_BUFFER,          fmt, args); }
void Namev(SrcLoc sl, Image    image,    char const* fmt, Span<Arg::Arg const> args) { VkNameImpl(sl, (U64)imageObjs.Get(image)->vkImage,                VK_OBJECT_TYPE_IMAGE,           fmt, args); }
void Namev(SrcLoc sl, Shader   shader,   char const* fmt, Span<Arg::Arg const> args) { VkNameImpl(sl, (U64)shaderObjs.Get(shader)->vkShaderModule,       VK_OBJECT_TYPE_SHADER_MODULE,   fmt, args); }
void Namev(SrcLoc sl, Pipeline pipeline, char const* fmt, Span<Arg::Arg const> args) { VkNameImpl(sl, (U64)pipelineObjs.Get(pipeline)->vkPipeline,       VK_OBJECT_TYPE_PIPELINE,        fmt, args);
                                                                                       VkNameImpl(sl, (U64)pipelineObjs.Get(pipeline)->vkPipelineLayout, VK_OBJECT_TYPE_PIPELINE_LAYOUT, fmt, args); }
//----------------------------------------------------------------------------------------------

static Res<> InitInstance() {
	LoadRootFns();

	U32 instanceVersion = 0;
	Gpu_CheckVk(vkEnumerateInstanceVersion(&instanceVersion));
	if (instanceVersion < VK_API_VERSION_1_3) {
		return Err_Version("version", instanceVersion);
	}

	U32 layersLen = 0;
	Gpu_CheckVk(vkEnumerateInstanceLayerProperties(&layersLen, 0));
	VkLayerProperties* const layers = Mem::AllocT<VkLayerProperties>(tempMem, layersLen);
	Gpu_CheckVk(vkEnumerateInstanceLayerProperties(&layersLen, layers));
	Logf("%u layers:", layersLen);
	for (U64 i = 0; i < layersLen; i++) {
		Logf(
			"  %s: implementationVersion=%s, specVersion=%s, description=%s",
			layers[i].layerName,
			VersionStr(tempMem, layers[i].implementationVersion),
			VersionStr(tempMem, layers[i].specVersion),
			layers[i].description
		);
	}
	char const* requiredLayers[1];
	U32 requiredLayersLen = 0;
	if (enableDebug) {
		requiredLayers[requiredLayersLen++] = "VK_LAYER_KHRONOS_validation";
	}
	for (U32 i = 0; i < requiredLayersLen; i++) {
		bool found = false;
		for (U64 j = 0; j < layersLen; j++) {
			if (!strcmp(requiredLayers[i], layers[j].layerName)) {
				Logf("Found required layer '%s'", requiredLayers[i]);
				found = true;
				break;
			}
		}
		if (!found) {
			return Err_NoLayer("name", requiredLayers[i]);
		}
	}

	U32 instExtsLen = 0;
	Gpu_CheckVk(vkEnumerateInstanceExtensionProperties(0, &instExtsLen, 0));
	VkExtensionProperties* const instExts = Mem::AllocT<VkExtensionProperties>(tempMem, instExtsLen);
	Gpu_CheckVk(vkEnumerateInstanceExtensionProperties(0, &instExtsLen, instExts));
	Logf("%u instance extensions:", instExtsLen);
	for (U64 i = 0; i < instExtsLen; i++) {
		Logf("  %s specVersion=%s", instExts[i].extensionName, VersionStr(tempMem, instExts[i].specVersion));
	}
	char const* requiredInstExts[3];
	U32 requiredInstExtsLen = 0;
	requiredInstExts[requiredInstExtsLen++] = "VK_KHR_surface";
	#if defined Platform_Windows
		requiredInstExts[requiredInstExtsLen++] = "VK_KHR_win32_surface";
	#endif	//	Platform_
	if (enableDebug) {
		requiredInstExts[requiredInstExtsLen++] = "VK_EXT_debug_utils";
	}
	for (U32 i = 0; i < requiredInstExtsLen; i++) {
		bool found = false;
		for (U64 j = 0; j < instExtsLen; j++) {
			if (!strcmp(requiredInstExts[i], instExts[j].extensionName)) {
				Logf("Found required instance extension '%s'", requiredInstExts[i]);
				found = true;
				break;
			}
		}
		if (!found) {
			return Err_NoExt("ext", requiredInstExts[i]);
		}
	}

	VkApplicationInfo const vkApplicationInfo = {
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
		.enabledLayerCount       = (U32)requiredLayersLen,
		.ppEnabledLayerNames     = requiredLayers,
		.enabledExtensionCount   = (U32)requiredInstExtsLen,
		.ppEnabledExtensionNames = requiredInstExts,
	};

	VkDebugUtilsMessengerCreateInfoEXT vkDebugUtilsMessengerCreateInfo;
	if (enableDebug) {
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

		constexpr VkBool32 vkTrue = VK_TRUE;
		constexpr char const* validationDebugActions[] = { "VK_DBG_LAYER_ACTION_LOG_MSG" };  // "VK_DBG_LAYER_ACTION_DEBUG_OUTPUT", "VK_DBG_LAYER_ACTION_BREAK"
		constexpr char const* validationReportFlags[]  = { "error" };
		VkLayerSettingEXT const vkLayerSettings[] = {

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
		VkLayerSettingsCreateInfoEXT const vkLayerSettingsCreateInfo = {
			.sType        = VK_STRUCTURE_TYPE_LAYER_SETTINGS_CREATE_INFO_EXT,
			.pNext        = vkInstanceCreateInfo.pNext,
			.settingCount = LenOf(vkLayerSettings),
			.pSettings    = vkLayerSettings,

		};
		vkInstanceCreateInfo.pNext = &vkLayerSettingsCreateInfo;
	}

	Gpu_CheckVk(vkCreateInstance(&vkInstanceCreateInfo, vkAllocationCallbacks, &vkInstance));

	LoadInstanceFns(vkInstance);

	if (enableDebug) {
		Gpu_CheckVk(vkCreateDebugUtilsMessengerEXT(vkInstance, &vkDebugUtilsMessengerCreateInfo, vkAllocationCallbacks, &vkDebugUtilsMessenger));
	}

	return Ok();
}

//-------------------------------------------------------------------------------------------------

static Res<> InitSurface(Window::PlatformDesc const* windowPlatformDesc) {
	#if defined Platform_Windows
		VkWin32SurfaceCreateInfoKHR win32SurfaceCreateInfo = {
			.sType     = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
			.pNext     = 0,
			.flags     = 0,
			.hinstance = (HINSTANCE)windowPlatformDesc->hinstance,
			.hwnd      = (HWND)windowPlatformDesc->hwnd,
		};
		Gpu_CheckVk(vkCreateWin32SurfaceKHR(vkInstance, &win32SurfaceCreateInfo, vkAllocationCallbacks, &vkSurface));
	#endif	// Platform

	return Ok();
}

//-------------------------------------------------------------------------------------------------

static Res<> InitDevice() {
	constexpr char const* RequiredDeviceExts[] = {
		"VK_KHR_swapchain",
	};

	U32 vkPhysicalDevicesLen = 0;
	Gpu_CheckVk(vkEnumeratePhysicalDevices(vkInstance, &vkPhysicalDevicesLen, nullptr));
	VkPhysicalDevice* const vkPhysicalDevices = Mem::AllocT<VkPhysicalDevice>(tempMem, vkPhysicalDevicesLen);
	Gpu_CheckVk(vkEnumeratePhysicalDevices(vkInstance, &vkPhysicalDevicesLen, vkPhysicalDevices));

	// TODO: support cvar for selecting explicit device

	Logf("%u physical devices:", vkPhysicalDevicesLen);
	physicalDevices = Mem::AllocSpan<PhysicalDevice>(permMem, vkPhysicalDevicesLen);

	U32 bestScore = 0;
	for (U64 i = 0; i < physicalDevices.len; i++) {
		PhysicalDevice* pd = &physicalDevices[i];
		pd->vkPhysicalDevice = vkPhysicalDevices[i];
		pd->vkPhysicalDeviceProperties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
		vkGetPhysicalDeviceProperties2(vkPhysicalDevices[i], &pd->vkPhysicalDeviceProperties2);
		Logf(
			"%s: apiVersion=%s, driverVersion=%s, vendorID=%u, deviceId=%u, deviceType=%s",
			pd->vkPhysicalDeviceProperties2.properties.deviceName,
			VersionStr(tempMem, pd->vkPhysicalDeviceProperties2.properties.apiVersion),
			VersionStr(tempMem, pd->vkPhysicalDeviceProperties2.properties.driverVersion),
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
			Logf("  Rejecting device: need Vulkan 1.3: apiVersion=%s", VersionStr(tempMem, pd->vkPhysicalDeviceProperties2.properties.apiVersion));
			score = 0;
		}

		pd->vkPhysicalDeviceFeatures2.sType        = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
		pd->vkPhysicalDeviceFeatures2.pNext        = &pd->vkPhysicalDeviceVulkan12Features;
		pd->vkPhysicalDeviceVulkan12Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
		pd->vkPhysicalDeviceVulkan12Features.pNext = &pd->vkPhysicalDeviceVulkan13Features;
		pd->vkPhysicalDeviceVulkan13Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
		pd->vkPhysicalDeviceVulkan13Features.pNext = 0;
		vkGetPhysicalDeviceFeatures2(pd->vkPhysicalDevice, &pd->vkPhysicalDeviceFeatures2);
		#define Gpu_CheckVkFeature(path) \
			if (!pd->path) { \
				Logf("  Rejecting device: doesn't support " #path); \
				score = 0; \
			}

		Gpu_CheckVkFeature(vkPhysicalDeviceFeatures2.features.shaderClipDistance);
		Gpu_CheckVkFeature(vkPhysicalDeviceFeatures2.features.samplerAnisotropy);
		Gpu_CheckVkFeature(vkPhysicalDeviceVulkan12Features.bufferDeviceAddress);
		Gpu_CheckVkFeature(vkPhysicalDeviceVulkan12Features.descriptorIndexing);
		Gpu_CheckVkFeature(vkPhysicalDeviceVulkan12Features.descriptorBindingPartiallyBound);
		Gpu_CheckVkFeature(vkPhysicalDeviceVulkan12Features.descriptorBindingSampledImageUpdateAfterBind);
		Gpu_CheckVkFeature(vkPhysicalDeviceVulkan12Features.runtimeDescriptorArray);
		Gpu_CheckVkFeature(vkPhysicalDeviceVulkan12Features.timelineSemaphore);
		Gpu_CheckVkFeature(vkPhysicalDeviceVulkan13Features.dynamicRendering);
		Gpu_CheckVkFeature(vkPhysicalDeviceVulkan13Features.synchronization2);
		#undef Gpu_CheckVkFeature


		vkGetPhysicalDeviceMemoryProperties(pd->vkPhysicalDevice, &pd->vkPhysicalDeviceMemoryProperties);
		Logf("  %u memory types:", pd->vkPhysicalDeviceMemoryProperties.memoryTypeCount);
		for (U64 j = 0; j < pd->vkPhysicalDeviceMemoryProperties.memoryTypeCount; j++) {
			VkMemoryType const mt = pd->vkPhysicalDeviceMemoryProperties.memoryTypes[j];
			Logf("    [%u] heapIndex=%u, flags=%s", j, mt.heapIndex, MemoryPropertyFlagsStr(tempMem, mt.propertyFlags));
		}
		Logf("  %u memory heaps:", pd->vkPhysicalDeviceMemoryProperties.memoryHeapCount);
		for (U64 j = 0; j < pd->vkPhysicalDeviceMemoryProperties.memoryHeapCount; j++) {
			VkMemoryHeap const mh = pd->vkPhysicalDeviceMemoryProperties.memoryHeaps[j];
			Logf("    [%u] size=%s, flags=%s", j, SizeStr(tempMem, mh.size), MemoryHeapFlagsStr(tempMem, mh.flags));
		}

		U32 vkQueueFamilyPropertiesLen = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(pd->vkPhysicalDevice, &vkQueueFamilyPropertiesLen, nullptr);
		VkQueueFamilyProperties* const vkQueueFamilyProperties = Mem::AllocT<VkQueueFamilyProperties>(tempMem, vkQueueFamilyPropertiesLen);
		vkGetPhysicalDeviceQueueFamilyProperties(pd->vkPhysicalDevice, &vkQueueFamilyPropertiesLen, vkQueueFamilyProperties);

		U32 vkExtensionPropertiesLen = 0;
		Gpu_CheckVk(vkEnumerateDeviceExtensionProperties(pd->vkPhysicalDevice, 0, &vkExtensionPropertiesLen, 0));
		pd->vkExtensionProperties = Mem::AllocSpan<VkExtensionProperties>(permMem, vkExtensionPropertiesLen);
		Gpu_CheckVk(vkEnumerateDeviceExtensionProperties(pd->vkPhysicalDevice, 0, &vkExtensionPropertiesLen, pd->vkExtensionProperties.data));

		Fmt::PrintBuf extensionsPb(tempMem);
		for (U64 j = 0; j < pd->vkExtensionProperties.len; j++) {
			extensionsPb.Printf("%s(specVersion=%s), ", pd->vkExtensionProperties[j].extensionName, VersionStr(tempMem, pd->vkExtensionProperties[j].specVersion));
		}
		if (extensionsPb.len >= 2) {
			extensionsPb.len -= 2;
		}
		Logf("  %u device extensions: %s",  pd->vkExtensionProperties.len, extensionsPb.ToStr());
		for (U64 j = 0; j < LenOf(RequiredDeviceExts); j++) {
			bool found = false;
			for (U64 k = 0; k < pd->vkExtensionProperties.len; k++) {
				if (!strcmp(RequiredDeviceExts[j], pd->vkExtensionProperties[k].extensionName)) {
					found = true;
					break;
				}
			}
			if (!found) {
				Logf("Rejecting device: doesn't support extension '%s'", RequiredDeviceExts[j]);
				score = 0;
			}
		}

		U32 vkSurfaceFormatsLen = 0;
		Gpu_CheckVk(vkGetPhysicalDeviceSurfaceFormatsKHR(pd->vkPhysicalDevice, vkSurface, &vkSurfaceFormatsLen, 0));
		pd->vkSurfaceFormats = Mem::AllocSpan<VkSurfaceFormatKHR>(permMem, vkSurfaceFormatsLen);
		Gpu_CheckVk(vkGetPhysicalDeviceSurfaceFormatsKHR(pd->vkPhysicalDevice, vkSurface, &vkSurfaceFormatsLen, pd->vkSurfaceFormats.data));
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
		Logf("  %u surface formats:", pd->vkSurfaceFormats.len);
		for (U64 j = 0; j < pd->vkSurfaceFormats.len; j++) {
			Logf("    %s, %s", FormatStr(pd->vkSurfaceFormats[j].format), ColorSpaceStr(pd->vkSurfaceFormats[j].colorSpace));
		}
		Logf("  Selected surface format: %s/%s", FormatStr(pd->vkSwapchainFormat), ColorSpaceStr(pd->vkSwapchainColorSpace));

		U32 vkPresentModesLen = 0;
		Gpu_CheckVk(vkGetPhysicalDeviceSurfacePresentModesKHR(pd->vkPhysicalDevice, vkSurface, &vkPresentModesLen, 0));
		pd->vkPresentModes = Mem::AllocSpan<VkPresentModeKHR>(permMem, vkPresentModesLen);
		Gpu_CheckVk(vkGetPhysicalDeviceSurfacePresentModesKHR(pd->vkPhysicalDevice, vkSurface, &vkPresentModesLen, pd->vkPresentModes.data));
		Logf("  %u present modes:", pd->vkPresentModes.len);
		for (U64 j = 0; j < pd->vkPresentModes.len; j++) {
			Logf("    %s", PresentModeStr(pd->vkPresentModes[j]));
		}
		if (pd->vkPresentModes.len == 0) {
			Logf("  Rejecting device: no present modes");
			score = 0;
		}

		pd->queueFamilies = Mem::AllocSpan<QueueFamily>(permMem, vkQueueFamilyPropertiesLen);
		Logf("  %u queue families:", pd->queueFamilies.len);
		for (U32 j = 0; j < (U32)pd->queueFamilies.len; j++) {
			VkQueueFamilyProperties const* props = &pd->queueFamilies[j].vkQueueFamilyProperties;
			VkBool32 supportsPresent = VK_FALSE;
			Gpu_CheckVk(vkGetPhysicalDeviceSurfaceSupportKHR(pd->vkPhysicalDevice, (U32)j, vkSurface, &supportsPresent));
			pd->queueFamilies[j].vkQueueFamilyProperties = vkQueueFamilyProperties[j];
			pd->queueFamilies[j].supportsPresent         = (supportsPresent == VK_TRUE);
			Logf("    [%u] count=%u, flags=%s, supportsPresent=%t", j, props->queueCount, QueueFlagsStr(tempMem, props->queueFlags), pd->queueFamilies[j].supportsPresent);
			VkQueueFlags const flags = pd->queueFamilies[j].vkQueueFamilyProperties.queueFlags;
			if (pd->queueFamily == VK_QUEUE_FAMILY_IGNORED && (flags & VK_QUEUE_GRAPHICS_BIT) && pd->queueFamilies[j].supportsPresent) {
				pd->queueFamily = (U32)j;
			}
		}
		if (pd->queueFamily == VK_QUEUE_FAMILY_IGNORED) {
			Logf("  Rejecting device: no graphics queue supporting present");
			score = 0;
		}

		Logf("  score: %u", score);
		if (score > bestScore) {
			physicalDevice = pd;
			bestScore = score;
		}
	}

	if (bestScore == 0) {
		return Err_NoDevice();
	}
	Logf("Selected physical device '%s' with score=%u", physicalDevice->vkPhysicalDeviceProperties2.properties.deviceName, bestScore);

	F32 queuePriority = 1.0f;
	VkDeviceQueueCreateInfo const vkDeviceQueueCreateInfo = {
		.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
		.pNext            = 0,
		.flags            = 0,
		.queueFamilyIndex = physicalDevice->queueFamily,
		.queueCount       = 1,
		.pQueuePriorities = &queuePriority,
	};

	// TODO: possibly create more queues? one per type?
	VkDeviceCreateInfo const vkDeviceCreateInfo = {
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

	Gpu_CheckVk(vkCreateDevice(physicalDevice->vkPhysicalDevice, &vkDeviceCreateInfo, vkAllocationCallbacks, &vkDevice));
	VkName(vkDevice);

	LoadDeviceFns(vkDevice);

	vkGetDeviceQueue(vkDevice, physicalDevice->queueFamily, 0, &vkQueue);

	return Ok();
}

//-------------------------------------------------------------------------------------------------

static Res<> InitSwapchain(U32 width, U32 height) {
	VkSurfaceCapabilitiesKHR vkSurfaceCapabilities;
	Gpu_CheckVk(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice->vkPhysicalDevice, vkSurface, &vkSurfaceCapabilities));
	
	U32 imageCount = Max((U32)3, vkSurfaceCapabilities.minImageCount);
	if (vkSurfaceCapabilities.maxImageCount > 0 && imageCount > vkSurfaceCapabilities.maxImageCount) {
		imageCount = vkSurfaceCapabilities.maxImageCount;
	}
	Logf("Selected swapchain image count: %u", imageCount);

	// TODO: check min/max extents
	VkExtent2D const vkSwapchainExtent = (vkSurfaceCapabilities.currentExtent.width != U32Max)
		? vkSurfaceCapabilities.currentExtent
		: VkExtent2D {
			.width  = Clamp(width,  vkSurfaceCapabilities.minImageExtent.width,  vkSurfaceCapabilities.maxImageExtent.width),
			.height = Clamp(height, vkSurfaceCapabilities.minImageExtent.height, vkSurfaceCapabilities.maxImageExtent.height),
		};
	Logf("Selected swapchain extent: (%u, %u)", vkSwapchainExtent.width, vkSwapchainExtent.height);

	VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;
	for (U64 i = 0; i < physicalDevice->vkPresentModes.len; i++) {
		if (physicalDevice->vkPresentModes[i] == VK_PRESENT_MODE_MAILBOX_KHR) {
			presentMode = VK_PRESENT_MODE_MAILBOX_KHR;
		}
	}
	Logf("Selected swapchain present mode: %s", PresentModeStr(presentMode));

	VkSwapchainCreateInfoKHR const vkSwapchainCreateInfoKHR = {
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
	Gpu_CheckVk(vkCreateSwapchainKHR(vkDevice, &vkSwapchainCreateInfoKHR, vkAllocationCallbacks, &vkSwapchain));

	U32 vkSwapchainImagesLen = 0;
	Gpu_CheckVk(vkGetSwapchainImagesKHR(vkDevice, vkSwapchain, &vkSwapchainImagesLen, 0));
	VkImage* const vkSwapchainImages = Mem::AllocT<VkImage>(tempMem, vkSwapchainImagesLen);
	Gpu_CheckVk(vkGetSwapchainImagesKHR(vkDevice, vkSwapchain, &vkSwapchainImagesLen, vkSwapchainImages));
	swapchainImages = Mem::AllocSpan<Image>(permMem, vkSwapchainImagesLen);
	for (U64 i = 0; i < vkSwapchainImagesLen; i++) {
		VkNamef(vkSwapchainImages[i], "vkSwapchainImages[%u]", i);

		VkImageViewCreateInfo const vkImageViewCreateInfo = {
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
			.subresourceRange   = {
				.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
				.baseMipLevel   = 0,
				.levelCount     = VK_REMAINING_MIP_LEVELS,
				.baseArrayLayer = 0,
				.layerCount     = VK_REMAINING_ARRAY_LAYERS,
			},
		};
		VkImageView vkSwapchainImageView = VK_NULL_HANDLE;
		Gpu_CheckVk(vkCreateImageView(vkDevice, &vkImageViewCreateInfo, vkAllocationCallbacks, &vkSwapchainImageView));
		VkNamef(vkSwapchainImageView, "vkSwapchainImageView#%u", i);

		auto* const entry = imageObjs.Alloc();
		swapchainImages[i] = entry->Handle();
		entry->obj.vkImage       = vkSwapchainImages[i];
		entry->obj.vkImageView   = vkSwapchainImageView;
		entry->obj.allocation    = {};
		entry->obj.width         = vkSwapchainExtent.width;
		entry->obj.height        = vkSwapchainExtent.height;
		entry->obj.vkFormat      = physicalDevice->vkSwapchainFormat;
	}

	vkDeviceWaitIdle(vkDevice);

	return Ok();
}

//----------------------------------------------------------------------------------------------

static Res<> InitFrame() {
	VkCommandPoolCreateInfo const vkCommandPoolCreateInfo = {
		.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
		.pNext            = 0,
		.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
		.queueFamilyIndex = physicalDevice->queueFamily,
	};
	Gpu_CheckVk(vkCreateCommandPool(vkDevice, &vkCommandPoolCreateInfo, vkAllocationCallbacks, &vkFrameCommandPool));
	VkName(vkFrameCommandPool);

	VkCommandBufferAllocateInfo const vkCommandBufferAllocateInfo = {
		.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.pNext              = 0,
		.commandPool        = vkFrameCommandPool,
		.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		.commandBufferCount = MaxFrames,
	};
	Gpu_CheckVk(vkAllocateCommandBuffers(vkDevice, &vkCommandBufferAllocateInfo, vkFrameCommandBuffers));
	for (U64 i = 0; i < MaxFrames; i++) {
		VkNamef(vkFrameCommandBuffers[i], "vkFrameCommandBuffers[%u]", i);
	}

	VkSemaphoreTypeCreateInfo const vkSemaphoreTypeCreateInfo = {
		.sType         = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
		.pNext         = 0,
		.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE,
		.initialValue  = MaxFrames - 1,
	};
	{
		VkSemaphoreCreateInfo const vkSemaphoreCreateInfo = {
			.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
			.pNext = &vkSemaphoreTypeCreateInfo,
			.flags = 0,
		};
		Gpu_CheckVk(vkCreateSemaphore(vkDevice, &vkSemaphoreCreateInfo, vkAllocationCallbacks, &vkFrameTimelineSemaphore));
		VkName(vkFrameTimelineSemaphore);
	}

	for (U64 i = 0; i < MaxFrames; i++) {
		VkSemaphoreCreateInfo const vkSemaphoreCreateInfo = {
			.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
			.pNext = 0,
			.flags = 0,
		};
		Gpu_CheckVk(vkCreateSemaphore(vkDevice, &vkSemaphoreCreateInfo, vkAllocationCallbacks, &vkFrameImageAcquiredSemaphores[i]));
		Gpu_CheckVk(vkCreateSemaphore(vkDevice, &vkSemaphoreCreateInfo, vkAllocationCallbacks, &vkFrameSubmitCompleteSemaphores[i]));
		VkNamef(vkFrameImageAcquiredSemaphores[i],  "vkFrameImageAcquiredSemaphores[%u]",  i);
		VkNamef(vkFrameSubmitCompleteSemaphores[i], "vkFrameSubmitCompleteSemaphores[%u]", i);
	}

	if (Res<> r = CreateBufferImpl(
		PerFrameStagingBufferSize * MaxFrames,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		0,
		VK_MEMORY_PROPERTY_HOST_CACHED_BIT | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		0
	).To(frameStagingBuffer); !r) {
		return r.err;
	}
	VkName(frameStagingBuffer.vkBuffer);

	U8* ptr = 0;
	Gpu_CheckVk(vkMapMemory(vkDevice, frameStagingBuffer.allocation.vkDeviceMemory, 0, PerFrameStagingBufferSize * MaxFrames, 0, (void**)&ptr));

	for (U32 i = 0; i < MaxFrames; i++) {
		frameStagingBufferPtrs[i] = ptr + (i * PerFrameStagingBufferSize);
		frameStagingBufferUsed[i] = 0;
	}

	frame    = 0;
	frameIdx = 0;

	return Ok();
}

//----------------------------------------------------------------------------------------------

static Res<> InitImmediate() {
	VkCommandPoolCreateInfo const vkCommandPoolCreateInfo = {
		.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
		.pNext            = 0,
		.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
		.queueFamilyIndex = physicalDevice->queueFamily,
	};
	Gpu_CheckVk(vkCreateCommandPool(vkDevice, &vkCommandPoolCreateInfo, vkAllocationCallbacks, &vkImmediateCommandPool));
	VkName(vkImmediateCommandPool);

	VkCommandBufferAllocateInfo const vkCommandBufferAllocateInfo = {
		.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.pNext              = 0,
		.commandPool        = vkImmediateCommandPool,
		.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		.commandBufferCount = 1,
	};
	Gpu_CheckVk(vkAllocateCommandBuffers(vkDevice, &vkCommandBufferAllocateInfo, &vkImmediateCommandBuffer));
	VkName(vkImmediateCommandBuffer);
	vkImmediateCommandBufferBegun = false;

	VkSemaphoreTypeCreateInfo const vkSemaphoreTypeCreateInfo = {
		.sType         = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
		.pNext         = 0,
		.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE,
		.initialValue  = 0,
	};
	VkSemaphoreCreateInfo const vkSemaphoreCreateInfo = {
		.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
		.pNext = &vkSemaphoreTypeCreateInfo,
		.flags = 0,
	};
	Gpu_CheckVk(vkCreateSemaphore(vkDevice, &vkSemaphoreCreateInfo, vkAllocationCallbacks, &vkImmediateTimelineSemaphore));
	VkName(vkImmediateTimelineSemaphore);
	immediateTimeline = 0;

	if (Res<> r = CreateBufferImpl(
		ImmediateStagingBufferSize,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		0,
		VK_MEMORY_PROPERTY_HOST_CACHED_BIT | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		0
	).To(immediateStagingBuffer); !r) {
		return r.err;
	}
	VkName(immediateStagingBuffer.vkBuffer);

	Gpu_CheckVk(vkMapMemory(vkDevice, immediateStagingBuffer.allocation.vkDeviceMemory, 0, ImmediateStagingBufferSize, 0, (void**)&immediateStagingBufferPtr));

	immediateStagingBufferUsed = 0;

	return Ok();
}

//-------------------------------------------------------------------------------------------------

static void AddBindlessSampler(VkSampler vkSampler) {
	Assert(vkBindlessSamplersLen < MaxBindlessSamplers);
	VkDescriptorImageInfo const vkDescriptorImageInfo = {
		.sampler     = vkSampler,
		.imageView   = 0,
		.imageLayout = VK_IMAGE_LAYOUT_GENERAL,
	};
	VkWriteDescriptorSet const vkWriteDescriptorSet = {
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

static Res<> InitBindless() {
	constexpr VkDescriptorPoolSize vkDescriptorPoolSizes[] = {
		{ .type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, .descriptorCount = MaxBindlessSampledImages },
		{ .type = VK_DESCRIPTOR_TYPE_SAMPLER,       .descriptorCount = MaxBindlessSamplers },
	};
	VkDescriptorPoolCreateInfo const vkDescriptorPoolCreateInfo = {
		.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
		.pNext         = 0,
		.flags         = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT,
		.maxSets       = MaxBindlessDescriptorSets,
		.poolSizeCount = LenOf(vkDescriptorPoolSizes),
		.pPoolSizes    = vkDescriptorPoolSizes,
	};
	Gpu_CheckVk(vkCreateDescriptorPool(vkDevice, &vkDescriptorPoolCreateInfo, vkAllocationCallbacks, &vkDescriptorPool));
	VkName(vkDescriptorPool);

	constexpr VkDescriptorBindingFlags vkDescriptorBindingFlags[] = {
		VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT_EXT,
		VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT_EXT,
	};
	VkDescriptorSetLayoutBindingFlagsCreateInfo const vkDescriptorSetLayoutBindingFlagsCreateInfo = {
		.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
		.pNext         = 0,
		.bindingCount  = LenOf(vkDescriptorBindingFlags),
		.pBindingFlags = vkDescriptorBindingFlags,
	};
	VkDescriptorSetLayoutBinding const vkMeshDescriptorSetLayoutBindings[] = {
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
	VkDescriptorSetLayoutCreateInfo const vkDescriptorSetLayoutCreateInfo = {
		.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		.pNext        = &vkDescriptorSetLayoutBindingFlagsCreateInfo,
		.flags        = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT,
		.bindingCount = LenOf(vkMeshDescriptorSetLayoutBindings),
		.pBindings    = vkMeshDescriptorSetLayoutBindings,
	};
	Gpu_CheckVk(vkCreateDescriptorSetLayout(vkDevice, &vkDescriptorSetLayoutCreateInfo, vkAllocationCallbacks, &vkBindlessDescriptorSetLayout));
	VkName(vkBindlessDescriptorSetLayout);

	VkDescriptorSetAllocateInfo const vkDescriptorSetAllocateInfo = {
		.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
		.pNext              = 0,
		.descriptorPool     = vkDescriptorPool,
		.descriptorSetCount = 1,
		.pSetLayouts        = &vkBindlessDescriptorSetLayout,
	};
	Gpu_CheckVk(vkAllocateDescriptorSets(vkDevice, &vkDescriptorSetAllocateInfo, &vkBindlessDescriptorSet));
	VkName(vkBindlessDescriptorSet);

	nextBindlessDescriptorIdx = 1;	// reserve index 0 for invalid

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
	Gpu_CheckVk(vkCreateSampler(vkDevice, &vkSamplerCreateInfo, vkAllocationCallbacks, &vkNearestSampler));
	VkName(vkNearestSampler);
	AddBindlessSampler(vkNearestSampler);

	vkSamplerCreateInfo.magFilter        = VK_FILTER_LINEAR;
	vkSamplerCreateInfo.minFilter        = VK_FILTER_LINEAR;
	vkSamplerCreateInfo.mipmapMode       = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	vkSamplerCreateInfo.anisotropyEnable = VK_TRUE;
	vkSamplerCreateInfo.maxAnisotropy    = MaxAnisotropy;
	VkSampler vkLinearSampler = VK_NULL_HANDLE;
	Gpu_CheckVk(vkCreateSampler(vkDevice, &vkSamplerCreateInfo, vkAllocationCallbacks, &vkLinearSampler));
	VkName(vkLinearSampler);
	AddBindlessSampler(vkLinearSampler);

	return Ok();
}

//-------------------------------------------------------------------------------------------------

Res<> Init(InitDesc const* initDesc) {
	permMem = initDesc->permMem;
	tempMem = initDesc->tempMem;

	Sys::InitMutex(&mutex);

	bufferObjs.Init(permMem, MaxBuffers);
	imageObjs.Init(permMem, MaxImages);
	shaderObjs.Init(permMem, MaxShaders);
	pipelineObjs.Init(permMem, MaxPipelines);

	Try(InitInstance());
	Try(InitSurface(initDesc->windowPlatformDesc));
	Try(InitDevice());
	Try(InitSwapchain(initDesc->windowWidth, initDesc->windowHeight));
	Try(InitFrame());
	Try(InitImmediate());
	Try(InitBindless());

	VkCommandBufferBeginInfo const vkCommandBufferBeginInfo = {
		.sType            = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.pNext            = 0,
		.flags            = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
		.pInheritanceInfo = 0,
	};
	vkBeginCommandBuffer(vkImmediateCommandBuffer, &vkCommandBufferBeginInfo);
	vkImmediateCommandBufferBegun = true;

	for (U64 i = 0; i < swapchainImages.len; i++) {
		ImageMemoryBarrier(
			vkImmediateCommandBuffer,
			imageObjs.Get(swapchainImages[i])->vkImage,
			VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
			VK_ACCESS_2_NONE,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
			VK_ACCESS_2_NONE,
			VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
			VK_IMAGE_ASPECT_COLOR_BIT
		);
	}

	Try(ImmediateWait());

	return Ok();
}

//----------------------------------------------------------------------------------------------

#define DestroyVk(h, DestroyFn) \
	if (h != VK_NULL_HANDLE) { \
		DestroyFn(vkDevice, h, vkAllocationCallbacks); \
		h = VK_NULL_HANDLE; \
	}

#define DestroyVkSpan(a, DestroyFn) \
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

	for (U64 i = 0; i < swapchainImages.len; i++) {
		ImageObj* const imageObj = imageObjs.Get(swapchainImages[i]);
		DestroyVk(imageObj->vkImageView, vkDestroyImageView);
		imageObjs.Free(swapchainImages[i]);
	}
	swapchainImages.len = 0;
	DestroyVk(vkSwapchain, vkDestroySwapchainKHR);

	if (vkFrameCommandPool != VK_NULL_HANDLE) {
		vkFreeCommandBuffers(vkDevice, vkFrameCommandPool, MaxFrames, vkFrameCommandBuffers);
	}
	DestroyVk(vkFrameCommandPool, vkDestroyCommandPool);
	DestroyVk(frameStagingBuffer.vkBuffer, vkDestroyBuffer);
	DestroyVk(frameStagingBuffer.allocation.vkDeviceMemory, vkFreeMemory);
	DestroyVk(vkFrameTimelineSemaphore, vkDestroySemaphore);
	DestroyVkCArray(vkFrameImageAcquiredSemaphores, vkDestroySemaphore);
	DestroyVkCArray(vkFrameSubmitCompleteSemaphores, vkDestroySemaphore);

	if (vkImmediateCommandPool != VK_NULL_HANDLE) {
		vkFreeCommandBuffers(vkDevice, vkImmediateCommandPool, 1, &vkImmediateCommandBuffer);
	}
	DestroyVk(vkImmediateCommandPool, vkDestroyCommandPool);
	DestroyVk(vkImmediateTimelineSemaphore, vkDestroySemaphore);
	DestroyVk(immediateStagingBuffer.vkBuffer, vkDestroyBuffer);
	DestroyVk(immediateStagingBuffer.allocation.vkDeviceMemory, vkFreeMemory);

	DestroyVkCArray(vkBindlessSamplers, vkDestroySampler);
	vkBindlessSamplersLen = 0;
	DestroyVk(vkBindlessDescriptorSetLayout, vkDestroyDescriptorSetLayout);
	DestroyVk(vkDescriptorPool, vkDestroyDescriptorPool);

	if (vkDevice != VK_NULL_HANDLE) { vkDestroyDevice(vkDevice, vkAllocationCallbacks); vkDevice = VK_NULL_HANDLE; }
	if (vkSurface != VK_NULL_HANDLE) { vkDestroySurfaceKHR(vkInstance, vkSurface, vkAllocationCallbacks); vkSurface = VK_NULL_HANDLE; }
	if (vkDebugUtilsMessenger != VK_NULL_HANDLE) { vkDestroyDebugUtilsMessengerEXT(vkInstance, vkDebugUtilsMessenger, vkAllocationCallbacks); vkDebugUtilsMessenger = VK_NULL_HANDLE; }
	if (vkInstance != VK_NULL_HANDLE) { vkDestroyInstance(vkInstance, vkAllocationCallbacks); vkInstance = VK_NULL_HANDLE; }
	Sys::ShutdownMutex(&mutex);
}

//----------------------------------------------------------------------------------------------

ImageFormat GetSwapchainImageFormat() {
	return VkFormatToImageFormat(physicalDevice->vkSwapchainFormat);
}

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

static U32 SelectMemTypeIdx(
	U32                   memTypeBits,
	VkMemoryPropertyFlags vkMemoryPropertyFlagsNeed,
	VkMemoryPropertyFlags vkMemoryPropertyFlagsWant,
	VkMemoryPropertyFlags vkMemoryPropertyFlagsAvoid
) {
	U32 minCost = U32Max;
	U32 selectedMemTypeIdx = VK_MAX_MEMORY_TYPES;
	for (U32 i = 0, memTypeIdx = 1; i < physicalDevice->vkPhysicalDeviceMemoryProperties.memoryTypeCount; i++, memTypeIdx <<= 1) {
		if (!(memTypeBits & memTypeIdx)) {
			continue;
		}
		VkMemoryPropertyFlags const typeFlags = physicalDevice->vkPhysicalDeviceMemoryProperties.memoryTypes[i].propertyFlags;
		if ((typeFlags & vkMemoryPropertyFlagsNeed) != vkMemoryPropertyFlagsNeed) {
			continue;
		}
		U32 const wantCost  = Bit::PopCount32(vkMemoryPropertyFlagsWant  & ~typeFlags);
		U32 const avoidCost = Bit::PopCount32(vkMemoryPropertyFlagsAvoid & typeFlags);
		U32 const cost      = wantCost + avoidCost;
		if (cost == 0) {
			return i;
		} else if (cost < minCost) {
			selectedMemTypeIdx = i;
			minCost = cost;
		}
	}
	return selectedMemTypeIdx;
}

//----------------------------------------------------------------------------------------------

// TODO: add tracking here: file/line associated with each block
static Res<Allocation> AllocVkMem(
	VkMemoryRequirements  vkMemoryRequirements,
	VkMemoryPropertyFlags vkMemoryPropertyFlagsNeed,
	VkMemoryPropertyFlags vkMemoryPropertyFlagsWant,
	VkMemoryPropertyFlags vkMemoryPropertyFlagsAvoid,
	VkMemoryAllocateFlags vkMemoryAllocateFlags
) {
	U32 const memTypeIdx = SelectMemTypeIdx(vkMemoryRequirements.memoryTypeBits, vkMemoryPropertyFlagsNeed, vkMemoryPropertyFlagsWant, vkMemoryPropertyFlagsAvoid);
	if (memTypeIdx == VK_MAX_MEMORY_TYPES) {
		return Err_NoMemType(
			"memTypeBits",                vkMemoryRequirements.memoryTypeBits,
			"vkMemoryPropertyFlagsNeed",  vkMemoryPropertyFlagsNeed,
			"vkMemoryPropertyFlagsWant",  vkMemoryPropertyFlagsWant,
			"vkMemoryPropertyFlagsAvoid", vkMemoryPropertyFlagsAvoid
		);
	}

	VkMemoryAllocateFlagsInfo const vkMemoryAllocateFlagsInfo = {
		.sType      = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO,
		.pNext      = 0,
		.flags      = vkMemoryAllocateFlags,
		.deviceMask = 0,
	};
	VkMemoryAllocateInfo const vkMemoryAllocateInfo = {
		.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
		.pNext           = &vkMemoryAllocateFlagsInfo,
		.allocationSize  = vkMemoryRequirements.size,
		.memoryTypeIndex = memTypeIdx,
	};
	VkDeviceMemory vkDeviceMemory = VK_NULL_HANDLE;
	if (VkResult vkResult = vkAllocateMemory(vkDevice, &vkMemoryAllocateInfo, vkAllocationCallbacks, &vkDeviceMemory); vkResult != VK_SUCCESS) {
		return Err_Vk(vkResult, "vkAllocateMemory", "memTypeIdx", memTypeIdx, "size", vkMemoryRequirements.size);
	}

	return Allocation {
		.vkDeviceMemory = vkDeviceMemory,
		.memTypeIdx     = memTypeIdx,
		.size           = vkMemoryRequirements.size,
	};
}

//--------------------------------------------------------------------------------------------------

#pragma warning (disable: 4505)
static Res<BufferObj> CreateBufferImpl(
	U64                   size,
	VkBufferUsageFlags    vkBufferUsageFlags,
	VkMemoryPropertyFlags vkMemoryPropertyFlagsNeed,
	VkMemoryPropertyFlags vkMemoryPropertyFlagsWant,
	VkMemoryPropertyFlags vkMemoryPropertyFlagsAvoid,
	VkMemoryAllocateFlags vkMemoryAllocateFlags
) {
	VkBufferCreateInfo const vkBufferCreateInfo = {
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
	Gpu_CheckVk(vkCreateBuffer(vkDevice, &vkBufferCreateInfo, vkAllocationCallbacks, &vkBuffer));

	VkMemoryRequirements vkMemoryRequirements;
	vkGetBufferMemoryRequirements(vkDevice, vkBuffer, &vkMemoryRequirements);

	Allocation allocation;
	if (Res<> r = AllocVkMem(
		vkMemoryRequirements,
		vkMemoryPropertyFlagsNeed,
		vkMemoryPropertyFlagsWant,
		vkMemoryPropertyFlagsAvoid,
		vkMemoryAllocateFlags
	  ).To(allocation); !r) {
		vkDestroyBuffer(vkDevice, vkBuffer, vkAllocationCallbacks);
		return r.err;
	  }

	if (VkResult const r = vkBindBufferMemory(vkDevice, vkBuffer, allocation.vkDeviceMemory, 0); r != VK_SUCCESS) {
		vkDestroyBuffer(vkDevice, vkBuffer, vkAllocationCallbacks);
		vkFreeMemory(vkDevice, allocation.vkDeviceMemory, vkAllocationCallbacks);
		return Err_Vk(r, "vkBindBufferMemory");
	}

	U64 addr = 0;
	if (vkBufferUsageFlags & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT) {
		VkBufferDeviceAddressInfo const vkBufferDeviceAddressInfo = {
			.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
			.pNext = 0,
			.buffer = vkBuffer,
		};
		addr = vkGetBufferDeviceAddress(vkDevice, &vkBufferDeviceAddressInfo);
	}

	return BufferObj {
		.vkBuffer           = vkBuffer,
		.allocation         = allocation,
		.size               = size,
		.vkBufferUsageFlags = vkBufferUsageFlags,
		.addr               = addr,
	};
}

//-------------------------------------------------------------------------------------------------

// From gpuinfo.org as of 7/2025:
//                                                           |  Win |  Lin |  Mac
// ----------------------------------------------------------+------+------+-----
// DEVICE_LOCAL |              |               |             | 89.4 | 81.5 | 99.0
//              | HOST_VISIBLE | HOST_COHERENT | HOST_CACHED | 82.6 | 76.2 | 62.5
//              | HOST_VISIBLE | HOST_COHERENT |             | 82.5 | 73.3 |  3.1
// DEVICE_LOCAL | HOST_VISIBLE | HOST_COHERENT |             | 82.5 | 73.3 |  2.1
// DEVICE_LOCAL | HOST_VISIBLE | HOST_COHERENT | HOST_CACHED | 17.1 | 21.2 | 39.6
// DEVICE_LOCAL | HOST_VISIBLE |               | HOST_CACHED |  0.0 |  0.0 | 95.8

Res<Buffer> CreateBuffer(U64 size, BufferUsage::Flags bufferUsageFlags) {
	VkBufferUsageFlags vkBufferUsageFlags = 0;
	if (bufferUsageFlags & BufferUsage::Storage)      { vkBufferUsageFlags |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT; }
	if (bufferUsageFlags & BufferUsage::Index)        { vkBufferUsageFlags |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT; }
	if (bufferUsageFlags & BufferUsage::DrawIndirect) { vkBufferUsageFlags |= VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT; }
	if (bufferUsageFlags & BufferUsage::Copy)         { vkBufferUsageFlags |= VK_BUFFER_USAGE_TRANSFER_DST_BIT; }

	VkMemoryAllocateFlags vkMemoryAllocateFlags = 0;
	if (bufferUsageFlags & BufferUsage::Addr) {
		vkBufferUsageFlags    |= VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
		vkMemoryAllocateFlags |= VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;
	}

	BufferObj bufferObj;
	Try(CreateBufferImpl(
		size,
		vkBufferUsageFlags,
		0,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		0,
		vkMemoryAllocateFlags
	).To(bufferObj));

	BufferPool::Entry* const entry = bufferObjs.Alloc();
	entry->obj = bufferObj;
	return entry->Handle();
}

//----------------------------------------------------------------------------------------------

void DestroyBuffer(Buffer buffer) {
	if (buffer) {
		BufferObj* const bufferObj = bufferObjs.Get(buffer);
		DestroyVk(bufferObj->vkBuffer, vkDestroyBuffer);
		DestroyVk(bufferObj->allocation.vkDeviceMemory, vkFreeMemory);
		memset(bufferObj, 0, sizeof(*bufferObj));
		bufferObjs.Free(buffer);
	}
}

//----------------------------------------------------------------------------------------------

U64 GetBufferAddr(Buffer buffer) {
	BufferObj* const bufferObj = bufferObjs.Get(buffer);
	Assert(bufferObj->addr);
	return bufferObj->addr;
}

//----------------------------------------------------------------------------------------------

static void BufferBarrierImpl(
	VkCommandBuffer       vkCommandBuffer,
	VkBuffer              vkBuffer,
	U64                   offset,
	U64                   size,
	VkPipelineStageFlags2 vkSrcPipelineStageFlags2,
	VkAccessFlags2        vkSrcAccessFlags2,
	VkPipelineStageFlags2 vkDstPipelineStageFlags2,
	VkAccessFlags2        vkDstAccessFlags2
) {
	VkBufferMemoryBarrier2 const vkBufferMemoryBarrier2 = {
		.sType               = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
		.pNext               = 0,
		.srcStageMask        = vkSrcPipelineStageFlags2,
		.srcAccessMask       = vkSrcAccessFlags2,
		.dstStageMask        = vkDstPipelineStageFlags2,
		.dstAccessMask       = vkDstAccessFlags2,
		.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.buffer              = vkBuffer,
		.offset              = offset,
		.size                = size,
	};
	VkDependencyInfo const vkDependencyInfo = {
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
	vkCmdPipelineBarrier2(vkCommandBuffer, &vkDependencyInfo);
}

//-------------------------------------------------------------------------------------------------

Res<Image> CreateImage(U32 width, U32 height, ImageFormat format, ImageUsage::Flags imageUsageFlags) {
	// TODO: check format properties optimal tiling supports the requested format/feature
	VkFormat const vkFormat = ImageFormatToVkFormat(format);

	VkImageUsageFlags vkImageUsageFlags = 0;
	if (imageUsageFlags & ImageUsage::Sampled) { vkImageUsageFlags |= VK_IMAGE_USAGE_SAMPLED_BIT; }
	if (imageUsageFlags & ImageUsage::Color)   { vkImageUsageFlags |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT; }
	if (imageUsageFlags & ImageUsage::Depth)   { vkImageUsageFlags |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT; }
	if (imageUsageFlags & ImageUsage::Copy)    { vkImageUsageFlags |= VK_IMAGE_USAGE_TRANSFER_DST_BIT; }

	VkImageCreateInfo const vkImageCreateInfo = {
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
	Gpu_CheckVk(vkCreateImage(vkDevice, &vkImageCreateInfo, vkAllocationCallbacks, &vkImage));

	VkMemoryRequirements vkMemoryRequirements;
	vkGetImageMemoryRequirements(vkDevice, vkImage, &vkMemoryRequirements);

	Allocation allocation;
	if (Res<> r = AllocVkMem(vkMemoryRequirements, 0, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 0, 0).To(allocation); !r) {
		vkDestroyImage(vkDevice, vkImage, vkAllocationCallbacks);
		return r.err;
	}
	
	if (VkResult const r = vkBindImageMemory(vkDevice, vkImage, allocation.vkDeviceMemory, 0); r != VK_SUCCESS) {
		vkDestroyImage(vkDevice, vkImage, vkAllocationCallbacks);
		return Err_Vk(r, "vkBindImageMemory");
	}

	VkImageViewCreateInfo const vkImageViewCreateInfo = {
		.sType              = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.pNext              = 0,
		.flags              = 0,
		.image              = vkImage,
		.viewType           = VK_IMAGE_VIEW_TYPE_2D,
		.format             = vkFormat,
		.components         = { .r = VK_COMPONENT_SWIZZLE_IDENTITY, .g = VK_COMPONENT_SWIZZLE_IDENTITY, .b = VK_COMPONENT_SWIZZLE_IDENTITY, .a = VK_COMPONENT_SWIZZLE_IDENTITY },
		.subresourceRange   = {
			.aspectMask     = (VkImageAspectFlags)(IsDepthFormat(vkFormat) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT),
			.baseMipLevel   = 0,
			.levelCount     = VK_REMAINING_MIP_LEVELS,
			.baseArrayLayer = 0,
			.layerCount     = VK_REMAINING_ARRAY_LAYERS,
		},
	};
	VkImageView vkImageView = VK_NULL_HANDLE;
	if (VkResult r = vkCreateImageView(vkDevice, &vkImageViewCreateInfo, vkAllocationCallbacks, &vkImageView); r != VK_SUCCESS) {
		vkDestroyImage(vkDevice, vkImage, vkAllocationCallbacks);
		return Err_Vk(r, "vkCreateImageView");
	}

	U32 bindlessIdx = 0;
	if (vkImageUsageFlags & VK_IMAGE_USAGE_SAMPLED_BIT) {
		bindlessIdx = nextBindlessDescriptorIdx++;
		VkDescriptorImageInfo const vkDescriptorImageInfo = {
			.sampler     = VK_NULL_HANDLE,
			.imageView   = vkImageView,
			.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		};
		VkWriteDescriptorSet const vkWriteDescriptorSet = {
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

	ImagePool::Entry* const entry = imageObjs.Alloc();
	entry->obj.vkImage           = vkImage;
	entry->obj.vkImageView       = vkImageView;
	entry->obj.allocation        = allocation;
	entry->obj.width             = width;
	entry->obj.height            = height;
	entry->obj.vkFormat          = vkFormat;
	entry->obj.bindlessIdx       = bindlessIdx;

	return entry->Handle();
}

//-------------------------------------------------------------------------------------------------

void DestroyImage(Image image) {
	if (image) {
		ImageObj* const imageObj = imageObjs.Get(image);
		DestroyVk(imageObj->vkImageView, vkDestroyImageView);
		DestroyVk(imageObj->vkImage, vkDestroyImage);
		DestroyVk(imageObj->allocation.vkDeviceMemory, vkFreeMemory);
		memset(imageObj, 0, sizeof(*imageObj));
		imageObjs.Free(image);
	}
}

//-------------------------------------------------------------------------------------------------

U32             GetImageWidth (Image image)  { return imageObjs.Get(image)->width;  }
U32             GetImageHeight(Image image)  { return imageObjs.Get(image)->height; }
ImageFormat GetImageFormat(Image image)  { return VkFormatToImageFormat(imageObjs.Get(image)->vkFormat); }
U32             GetImageBindIdx(Image image) { return imageObjs.Get(image)->bindlessIdx; }

//-------------------------------------------------------------------------------------------------

Res<Shader> CreateShader(void const* data, U64 len) {
	SpvReflectShaderModule spvReflectShaderModule = {};
	if (SpvReflectResult const r = spvReflectCreateShaderModule2(SPV_REFLECT_MODULE_FLAG_NO_COPY, len, data, &spvReflectShaderModule); r != SPV_REFLECT_RESULT_SUCCESS) {
		return Err_SpvReflect("code", r, "fn", "spvReflectCreateShaderModule2");
	}

	if (spvReflectShaderModule.push_constant_block_count > 1) {
		spvReflectDestroyShaderModule(&spvReflectShaderModule);
		return Err_SpvReflect("push_constant_block_count", spvReflectShaderModule.push_constant_block_count, "fn", "Err_ShaderTooManyPushConstantBlocks");
	}

	VkShaderModuleCreateInfo const vkShaderModuleCreateInfo = {
		.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		.pNext    = 0,
		.flags    = 0,
		.codeSize = (U32)len,
		.pCode    = (U32 const*)data,
	};
	VkShaderModule vkShaderModule = VK_NULL_HANDLE;
	if (VkResult const r = vkCreateShaderModule(vkDevice, &vkShaderModuleCreateInfo, vkAllocationCallbacks, &vkShaderModule); r != VK_SUCCESS) {
		spvReflectDestroyShaderModule(&spvReflectShaderModule);
		return Err_Vk(r, "vkCreateShaderModule");
	}

	ShaderPool::Entry* const entry = shaderObjs.Alloc();
	entry->obj.vkShaderModule = vkShaderModule;
	entry->obj.vkShaderStage = (VkShaderStageFlagBits)spvReflectShaderModule.shader_stage;	// SPV flags defined 1:1 with VK flags
	if (spvReflectShaderModule.push_constant_block_count && spvReflectShaderModule.push_constant_blocks) {
		entry->obj.pushConstantsOffset = spvReflectShaderModule.push_constant_blocks[0].offset;
		entry->obj.pushConstantsSize   = spvReflectShaderModule.push_constant_blocks[0].size;
	} else {
		entry->obj.pushConstantsOffset = 0;
		entry->obj.pushConstantsSize   = 0;
	}

	spvReflectDestroyShaderModule(&spvReflectShaderModule);

	return entry->Handle();
}

//-------------------------------------------------------------------------------------------------

void DestroyShader(Shader shader) {
	if (shader) {
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
	VkPipelineShaderStageCreateInfo* const vkPipelineShaderStageCreateInfos = Mem::AllocT<VkPipelineShaderStageCreateInfo>(tempMem, shaders.len);
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

	VkPipelineLayoutCreateInfo const vkPipelineLayoutCreateInfo = {
		.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.pNext                  = 0,
		.flags                  = 0,
		.setLayoutCount         = 1,
		.pSetLayouts            = &vkBindlessDescriptorSetLayout,
		.pushConstantRangeCount = 1,
		.pPushConstantRanges    = &vkPushConstantRange,
	};
	VkPipelineLayout vkPipelineLayout;
	if (VkResult const r = vkCreatePipelineLayout(vkDevice, &vkPipelineLayoutCreateInfo, vkAllocationCallbacks, &vkPipelineLayout); r != VK_SUCCESS) {
		return Err_Vk(r, "vkCreatePipelineLayout");
	}

	VkPipelineVertexInputStateCreateInfo const vkPipelineVertexInputStateCreateInfo = {
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
	VkPipelineTessellationStateCreateInfo const vkPipelineTessellationStateCreateInfo = {
		.sType              = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO,
		.pNext              = 0,
		.flags              = 0,
		.patchControlPoints = 0,
	};
	ImageObj const* swapchainImageObj = imageObjs.Get(swapchainImages[0]);
	VkViewport const vkViewport = {
		.x        = 0.0f,
		.y        = 0.0f,
		.width    = (F32)swapchainImageObj->width,
		.height   = (F32)swapchainImageObj->height,
		.minDepth = 0.0f,
		.maxDepth = 1.0f,
	};
	VkRect2D const vkScissorRect2D = {
		.offset     = {
			.x      = 0,
			.y      = 0,
		},
		.extent     = {
			.width  = swapchainImageObj->width,
			.height = swapchainImageObj->height,
		},
	};
	VkPipelineViewportStateCreateInfo const vkPipelineViewportStateCreateInfo = {
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
	VkPipelineColorBlendAttachmentState* const vkPipelineColorBlendAttachmentStates = Mem::AllocT<VkPipelineColorBlendAttachmentState>(tempMem, colorAttachmentFormats.len);
	for (U64 i = 0; i < colorAttachmentFormats.len; i++) {
		vkPipelineColorBlendAttachmentStates[i] = {
			.blendEnable         = VK_TRUE,
			.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
			.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
			.colorBlendOp        = VK_BLEND_OP_ADD,
			.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
			.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
			.alphaBlendOp        = VK_BLEND_OP_ADD,
			.colorWriteMask      = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
		};
	}
	VkPipelineColorBlendStateCreateInfo const vkPipelineColorBlendStateCreateInfo = {
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
	VkPipelineDynamicStateCreateInfo const vkPipelineDynamicStateCreateInfo = {
		.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
		.pNext             = 0,
		.flags             = 0,
		.dynamicStateCount = (U32)LenOf(vkDynamicStates),
		.pDynamicStates    = vkDynamicStates,
	};
	VkFormat* const vkColorAttachmentFormats = Mem::AllocT<VkFormat>(tempMem, colorAttachmentFormats.len);
	for (U64 i = 0; i < colorAttachmentFormats.len; i++) {
		vkColorAttachmentFormats[i] = ImageFormatToVkFormat(colorAttachmentFormats[i]);
	}
	VkPipelineRenderingCreateInfo const vkPipelineRenderingCreateInfo = {
		.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
		.pNext                   = 0,
		.viewMask                = 0,
		.colorAttachmentCount    = (U32)colorAttachmentFormats.len,
		.pColorAttachmentFormats = vkColorAttachmentFormats,
		.depthAttachmentFormat   = ImageFormatToVkFormat(depthFormat),
		.stencilAttachmentFormat = VK_FORMAT_UNDEFINED,
	};
	VkGraphicsPipelineCreateInfo const vkGraphicsPipelineCreateInfo = {
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
	if (VkResult const r = vkCreateGraphicsPipelines(vkDevice, 0, 1, &vkGraphicsPipelineCreateInfo, vkAllocationCallbacks, &vkPipeline); r != VK_SUCCESS) {
		vkDestroyPipelineLayout(vkDevice, vkPipelineLayout, vkAllocationCallbacks);
		return Err_Vk(r, "vkCreateGraphicsPipelines");
	}

	PipelinePool::Entry* const entry = pipelineObjs.Alloc();
	entry->obj.vkPipelineLayout    = vkPipelineLayout;
	entry->obj.vkPipeline          = vkPipeline;
	entry->obj.vkPipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	entry->obj.vkPushConstantRange = vkPushConstantRange;

	return entry->Handle();
}

//-------------------------------------------------------------------------------------------------

void DestroyPipeline(Pipeline pipeline) {
	if (pipeline) {
		PipelineObj* const pipelineObj = pipelineObjs.Get(pipeline);
		DestroyVk(pipelineObj->vkPipeline, vkDestroyPipeline);
		DestroyVk(pipelineObj->vkPipelineLayout, vkDestroyPipelineLayout);
		pipelineObjs.Free(pipeline);
	}
}

//-------------------------------------------------------------------------------------------------

Res<> ImmediateWait() {
	if (!vkImmediateCommandBufferBegun) {
		return Ok();
	}

	vkEndCommandBuffer(vkImmediateCommandBuffer);
	vkImmediateCommandBufferBegun = false;

	immediateTimeline++;

	VkCommandBufferSubmitInfo const vkCommandBufferSubmitInfo = {
		.sType         = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
		.pNext         = 0,
		.commandBuffer = vkImmediateCommandBuffer,
		.deviceMask    = 0,
	};
	VkSemaphoreSubmitInfo const vkSignalSemaphoreSubmitInfo = {
		.sType       = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
		.pNext       = 0,
		.semaphore   = vkImmediateTimelineSemaphore,
		.value       = immediateTimeline,
		.stageMask   = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT,
		.deviceIndex = 0,
	};
	VkSubmitInfo2 const vkSubmitInfo2 = {
		.sType                    = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
		.pNext                    = 0,
		.flags                    = 0,
		.waitSemaphoreInfoCount   = 0,
		.pWaitSemaphoreInfos      = 0,
		.commandBufferInfoCount   = 1,
		.pCommandBufferInfos      = &vkCommandBufferSubmitInfo,
		.signalSemaphoreInfoCount = 1,
		.pSignalSemaphoreInfos    = &vkSignalSemaphoreSubmitInfo,
	};
	Gpu_CheckVk(vkQueueSubmit2(vkQueue, 1, &vkSubmitInfo2, 0));

	VkSemaphoreWaitInfo const vkSemaphoreWaitInfo = {
		.sType          = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO,
		.pNext          = 0,
		.flags          = 0,
		.semaphoreCount = 1,
		.pSemaphores    = &vkImmediateTimelineSemaphore,
		.pValues        = &immediateTimeline,

	};
	Gpu_CheckVk(vkWaitSemaphores(vkDevice, &vkSemaphoreWaitInfo, U64Max));

	vkResetCommandPool(vkDevice, vkImmediateCommandPool, 0);

	immediateStagingBufferUsed = 0;

	return Ok();
}

//-------------------------------------------------------------------------------------------------

Res<> ImmediateCopyToBuffer(
	void const* data,
	U64         len,
	Buffer  buffer,
	U64         offset
) {
	BufferObj const* const bufferObj = bufferObjs.Get(buffer);
	while (len > 0) {
		U64 const amt = Min(len, ImmediateStagingBufferSize - immediateStagingBufferUsed);
		memcpy(immediateStagingBufferPtr + immediateStagingBufferUsed, data, amt);

		if (!vkImmediateCommandBufferBegun) {
			VkCommandBufferBeginInfo const vkCommandBufferBeginInfo = {
				.sType            = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
				.pNext            = 0,
				.flags            = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
				.pInheritanceInfo = 0,
			};
			Gpu_CheckVk(vkBeginCommandBuffer(vkImmediateCommandBuffer, &vkCommandBufferBeginInfo));
			vkImmediateCommandBufferBegun = true;
		}

		VkBufferCopy2 const vkBufferCopy2 = {
			.sType     = VK_STRUCTURE_TYPE_BUFFER_COPY_2,
			.pNext     = 0,
			.srcOffset = immediateStagingBufferUsed,
			.dstOffset = offset,
			.size      = amt,
		};
		VkCopyBufferInfo2 const vkCopyBufferInfo2 = {
			.sType       = VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2,
			.pNext       = 0,
			.srcBuffer   = immediateStagingBuffer.vkBuffer,
			.dstBuffer   = bufferObj->vkBuffer,
			.regionCount = 1,
			.pRegions    = &vkBufferCopy2,
		};
		vkCmdCopyBuffer2(vkImmediateCommandBuffer, &vkCopyBufferInfo2);

		immediateStagingBufferUsed += amt;

		if (amt == len) {
			return Ok();	// do not submit
		}

		len -= amt;
		data = (U8*)data + amt;
		offset += amt;

		Try(ImmediateWait());
	}

	return Ok();
}

//-------------------------------------------------------------------------------------------------

Res<> ImmediateCopyToImage(void const* data, Image image, BarrierStage::Flags finalBarrierStageFlags, ImageLayout finalImageLayout) {
	ImageObj const* const imageObj = imageObjs.Get(image);
	U64 len = imageObj->width * imageObj->height * FormatSize(imageObj->vkFormat);
	Assert(len <= ImmediateStagingBufferSize);
	if (len > ImmediateStagingBufferSize - immediateStagingBufferUsed) {
		Try(ImmediateWait());
	}

	memcpy(immediateStagingBufferPtr + immediateStagingBufferUsed, data, len);

	if (!vkImmediateCommandBufferBegun) {
		VkCommandBufferBeginInfo const vkCommandBufferBeginInfo = {
			.sType            = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
			.pNext            = 0,
			.flags            = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
			.pInheritanceInfo = 0,
		};
		Gpu_CheckVk(vkBeginCommandBuffer(vkImmediateCommandBuffer, &vkCommandBufferBeginInfo));
		vkImmediateCommandBufferBegun = true;
	}

	ImageMemoryBarrier(
		vkImmediateCommandBuffer,
		imageObj->vkImage,
		VK_PIPELINE_STAGE_2_NONE,
		VK_ACCESS_2_NONE,
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_PIPELINE_STAGE_2_COPY_BIT,
		VK_ACCESS_2_TRANSFER_WRITE_BIT,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		IsDepthFormat(imageObj->vkFormat) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT 
	);

	VkBufferImageCopy2 const vkBufferImageCopy2 = {
		.sType             = VK_STRUCTURE_TYPE_BUFFER_IMAGE_COPY_2,
		.pNext             = 0,
		.bufferOffset      = immediateStagingBufferUsed,
		.bufferRowLength   = 0,
		.bufferImageHeight = 0,
		.imageSubresource  = {
			.aspectMask      = (VkImageAspectFlags)(IsDepthFormat(imageObj->vkFormat) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT),
			.mipLevel        = 0,
			.baseArrayLayer  = 0,
			.layerCount      = 1,
		},
		.imageOffset       = { .x = 0, .y = 0, .z = 0 },
		.imageExtent       = { .width = imageObj->width, .height = imageObj->height, .depth = 1 },
	};
	VkCopyBufferToImageInfo2 const vkCopyBufferToImageInfo2 = {
		.sType          = VK_STRUCTURE_TYPE_COPY_BUFFER_TO_IMAGE_INFO_2,
		.pNext          = 0,
		.srcBuffer      = immediateStagingBuffer.vkBuffer,
		.dstImage       = imageObj->vkImage,
		.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		.regionCount    = 1,
		.pRegions       = &vkBufferImageCopy2,
	};
	vkCmdCopyBufferToImage2(vkImmediateCommandBuffer, &vkCopyBufferToImageInfo2);

	immediateStagingBufferUsed += len;

	ImageMemoryBarrier(
		vkImmediateCommandBuffer,
		imageObj->vkImage,
		VK_PIPELINE_STAGE_2_COPY_BIT,
		VK_ACCESS_2_TRANSFER_WRITE_BIT,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		BarrierStageFlagsToVkPipelineStageFlags2(finalBarrierStageFlags),
		BarrierStageFlagsToVkAccessFlags2(finalBarrierStageFlags),
		ImageLayoutToVkImageLayout(finalImageLayout),
		IsDepthFormat(imageObj->vkFormat) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT 
	);

	return Ok();
}

//-------------------------------------------------------------------------------------------------

Res<Frame> BeginFrame() {
	VkSemaphoreWaitInfo const vkSemaphoreWaitInfo = {
		.sType          = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO,
		.pNext          = 0,
		.flags          = 0,
		.semaphoreCount = 1,
		.pSemaphores    = &vkFrameTimelineSemaphore,
		.pValues        = &frame,

	};
	Gpu_CheckVk(vkWaitSemaphores(vkDevice, &vkSemaphoreWaitInfo, U64Max));

	frameStagingBufferUsed[frameIdx] = 0;

	if (VkResult r = vkAcquireNextImageKHR(vkDevice, vkSwapchain, U64Max, vkFrameImageAcquiredSemaphores[frameIdx], 0, &swapchainImageIdx); r != VK_SUCCESS) {
		if (r == VK_SUBOPTIMAL_KHR || r == VK_ERROR_OUT_OF_DATE_KHR) {
			return Err_RecreateSwapchain();
		} else {
			return Err_Vk(r, "vkAcquireNextImageKHR");
		}
	}

	VkCommandBufferBeginInfo const vkCommandBufferBeginInfo = {
		.sType            = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.pNext            = 0,
		.flags            = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
		.pInheritanceInfo = 0,
	};
	Gpu_CheckVk(vkBeginCommandBuffer(vkFrameCommandBuffers[frameIdx], &vkCommandBufferBeginInfo));

	ImageMemoryBarrier(
		vkFrameCommandBuffers[frameIdx],
		imageObjs.Get(swapchainImages[swapchainImageIdx])->vkImage,
		VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
		VK_ACCESS_2_NONE,
		VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
		VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
		VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		VK_IMAGE_ASPECT_COLOR_BIT
	);

	return Frame {
		.frame          = frame,
		.swapchainImage = swapchainImages[swapchainImageIdx],
	};
};

//----------------------------------------------------------------------------------------------

Res<> EndFrame() {
	ImageMemoryBarrier(
		vkFrameCommandBuffers[frameIdx],
		imageObjs.Get(swapchainImages[swapchainImageIdx])->vkImage,
		VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
		VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
		VK_ACCESS_2_NONE,
		VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
		VK_IMAGE_ASPECT_COLOR_BIT
	);

	Gpu_CheckVk(vkEndCommandBuffer(vkFrameCommandBuffers[frameIdx]));

	VkSemaphoreSubmitInfo const vkWaitSemaphoreSubmitInfo = {
		.sType       = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
		.pNext       = 0,
		.semaphore   = vkFrameImageAcquiredSemaphores[frameIdx],
		.value       = 0,
		.stageMask   = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
		.deviceIndex = 0,
	};
	VkCommandBufferSubmitInfo const vkCommandBufferSubmitInfo = {
		.sType         = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
		.pNext         = 0,
		.commandBuffer = vkFrameCommandBuffers[frameIdx],
		.deviceMask    = 0,
	};
	VkSemaphoreSubmitInfo const vkSignalSemaphoreSubmitInfos[2] = {
		{
			.sType       = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
			.pNext       = 0,
			.semaphore   = vkFrameSubmitCompleteSemaphores[frameIdx],
			.value       = 0,
			.stageMask   = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
			.deviceIndex = 0,
		},
		{
			.sType       = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
			.pNext       = 0,
			.semaphore   = vkFrameTimelineSemaphore,
			.value       = frame + MaxFrames,
			.stageMask   = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
			.deviceIndex = 0,
		},
	};
	VkSubmitInfo2 const vkSubmitInfo2 = {
		.sType                    = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
		.pNext                    = 0,
		.flags                    = 0,
		.waitSemaphoreInfoCount   = 1,
		.pWaitSemaphoreInfos      = &vkWaitSemaphoreSubmitInfo,
		.commandBufferInfoCount   = 1,
		.pCommandBufferInfos      = &vkCommandBufferSubmitInfo,
		.signalSemaphoreInfoCount = 2,
		.pSignalSemaphoreInfos    = vkSignalSemaphoreSubmitInfos,
	};
	Gpu_CheckVk(vkQueueSubmit2(vkQueue, 1, &vkSubmitInfo2, 0));

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

void WaitIdle() {
	if (vkDevice) {
		vkDeviceWaitIdle(vkDevice);
	}
}

//----------------------------------------------------------------------------------------------

void* AllocStaging(U64 len) {
	Assert(frameStagingBufferUsed[frameIdx] + len <= PerFrameStagingBufferSize);
	U8* const ptr = frameStagingBufferPtrs[frameIdx] + frameStagingBufferUsed[frameIdx];
	frameStagingBufferUsed[frameIdx] += len;
	return ptr;
}

//----------------------------------------------------------------------------------------------

void CopyStagingToBuffer(void* staging, U64 len, Buffer buffer, U64 offset) {
	Assert((U8*)staging >= frameStagingBufferPtrs[frameIdx]);
	Assert((U8*)staging + len < frameStagingBufferPtrs[frameIdx] + PerFrameStagingBufferSize);

	BufferObj const* const bufferObj = bufferObjs.Get(buffer);
	VkDeviceSize const srcOffset = 
		(frameIdx * PerFrameStagingBufferSize) + 
		(VkDeviceSize)((U8*)staging - frameStagingBufferPtrs[frameIdx]);
	VkBufferCopy2 const vkBufferCopy2 = {
		.sType     = VK_STRUCTURE_TYPE_BUFFER_COPY_2,
		.pNext     = 0,
		.srcOffset = srcOffset,
		.dstOffset = offset,
		.size      = len,
	};
	VkCopyBufferInfo2 const vkCopyBufferInfo2 = {
		.sType       = VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2,
		.pNext       = 0,
		.srcBuffer   = frameStagingBuffer.vkBuffer,
		.dstBuffer   = bufferObj->vkBuffer,
		.regionCount = 1,
		.pRegions    = &vkBufferCopy2,
	};
	vkCmdCopyBuffer2(vkFrameCommandBuffers[frameIdx], &vkCopyBufferInfo2);
}

//-------------------------------------------------------------------------------------------------

void CopyStagingToImage(void* staging, Image image) {
	ImageObj const* const imageObj = imageObjs.Get(image);
	U64 const len = imageObj->width * imageObj->height * FormatSize(imageObj->vkFormat);
	Assert((U8*)staging>= frameStagingBufferPtrs[frameIdx]);
	Assert((U8*)staging + len < frameStagingBufferPtrs[frameIdx] + PerFrameStagingBufferSize);

	VkDeviceSize const srcOffset = 
		(frameIdx * PerFrameStagingBufferSize) + 
		(VkDeviceSize)((U8*)staging - frameStagingBufferPtrs[frameIdx]);

	VkBufferImageCopy2 const vkBufferImageCopy2 = {
		.sType             = VK_STRUCTURE_TYPE_BUFFER_IMAGE_COPY_2,
		.pNext             = 0,
		.bufferOffset      = srcOffset,
		.bufferRowLength   = 0,
		.bufferImageHeight = 0,
		.imageSubresource  = {
			.aspectMask      = (VkImageAspectFlags)(IsDepthFormat(imageObj->vkFormat) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT),
			.mipLevel        = 0,
			.baseArrayLayer  = 0,
			.layerCount      = 1,
		},
		.imageOffset       = { .x = 0, .y = 0, .z = 0 },
		.imageExtent       = { .width = imageObj->width, .height = imageObj->height, .depth = 1 },
	};
	VkCopyBufferToImageInfo2 const vkCopyBufferToImageInfo2 = {
		.sType          = VK_STRUCTURE_TYPE_COPY_BUFFER_TO_IMAGE_INFO_2,
		.pNext          = 0,
		.srcBuffer      = frameStagingBuffer.vkBuffer,
		.dstImage       = imageObj->vkImage,
		.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		.regionCount    = 1,
		.pRegions       = &vkBufferImageCopy2,
	};
	vkCmdCopyBufferToImage2(vkFrameCommandBuffers[frameIdx], &vkCopyBufferToImageInfo2);
}

//-------------------------------------------------------------------------------------------------

void BufferBarrier(Buffer buffer, U64 offset, U64 size, BarrierStage::Flags srcBarrierStageFlags, BarrierStage::Flags dstBarrierStageFlags) {
	VkBufferMemoryBarrier2 const vkBufferMemoryBarrier2 = {
		.sType               = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
		.pNext               = 0,
		.srcStageMask        = BarrierStageFlagsToVkPipelineStageFlags2(srcBarrierStageFlags),
		.srcAccessMask       = BarrierStageFlagsToVkAccessFlags2(srcBarrierStageFlags),
		.dstStageMask        = BarrierStageFlagsToVkPipelineStageFlags2(dstBarrierStageFlags),
		.dstAccessMask       = BarrierStageFlagsToVkAccessFlags2(dstBarrierStageFlags),
		.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.buffer              = bufferObjs.Get(buffer)->vkBuffer,
		.offset              = offset,
		.size                = size ? size : VK_WHOLE_SIZE ,
	};
	VkDependencyInfo const vkDependencyInfo = {
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
	vkCmdPipelineBarrier2(vkFrameCommandBuffers[frameIdx], &vkDependencyInfo);
}

//-------------------------------------------------------------------------------------------------

void ImageBarrier(Image image, BarrierStage::Flags srcBarrierStageFlags, ImageLayout srcImageLayout, BarrierStage::Flags dstBarrierStageFlags, ImageLayout dstImageLayout) {
	ImageObj* const imageObj = imageObjs.Get(image);
	ImageMemoryBarrier(
		vkFrameCommandBuffers[frameIdx],
		imageObj->vkImage,
		BarrierStageFlagsToVkPipelineStageFlags2(srcBarrierStageFlags),
		BarrierStageFlagsToVkAccessFlags2(srcBarrierStageFlags),
		ImageLayoutToVkImageLayout(srcImageLayout),
		BarrierStageFlagsToVkPipelineStageFlags2(dstBarrierStageFlags),
		BarrierStageFlagsToVkAccessFlags2(dstBarrierStageFlags),
		ImageLayoutToVkImageLayout(dstImageLayout),
		IsDepthFormat(imageObj->vkFormat) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT
	);
}

//-------------------------------------------------------------------------------------------------

void DebugBarrier() {
	VkMemoryBarrier2 const vkMemoryBarrier2 = {
		.sType          = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
		.pNext          = 0,
		.srcStageMask   = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
		.srcAccessMask  = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
		.dstStageMask   = VK_ACCESS_2_MEMORY_READ_BIT,
		.dstAccessMask  = VK_ACCESS_2_MEMORY_WRITE_BIT,
	};
	VkDependencyInfo const vkDependencyInfo = {
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
	vkCmdPipelineBarrier2(vkFrameCommandBuffers[frameIdx], &vkDependencyInfo);
}

//-------------------------------------------------------------------------------------------------

void BeginPass(Pass const* pass) {
	VkRenderingAttachmentInfo* const vkColorRenderingAttachmentInfos = Mem::AllocT<VkRenderingAttachmentInfo>(tempMem, pass->colorAttachments.len);
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
	if (pass->depthAttachment) {
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

	VkRenderingInfo const vkRenderingInfo = {
		.sType                = VK_STRUCTURE_TYPE_RENDERING_INFO,
		.pNext                = 0,
		.flags                = 0,
		.renderArea           = { .offset = { 0, 0 }, .extent = { .width = (U32)pass->viewport.width, .height = (U32)pass->viewport.height } },
		.layerCount           = 1,
		.viewMask             = 0,
		.colorAttachmentCount = (U32)pass->colorAttachments.len,
		.pColorAttachments    = vkColorRenderingAttachmentInfos,
		.pDepthAttachment     = depthImageObj ? &vkDepthRenderingAttachmentInfo : 0,
		.pStencilAttachment   = 0,
	};
	vkCmdBeginRendering(vkFrameCommandBuffers[frameIdx], &vkRenderingInfo);

	VkViewport const vkViewport = {
		.x        = pass->viewport.x,
		.y        = pass->viewport.y,
		.width    = pass->viewport.width,
		.height   = pass->viewport.height,
		.minDepth = 0.0f,
		.maxDepth = 1.0f,
	};
	vkCmdSetViewport(vkFrameCommandBuffers[frameIdx], 0, 1, &vkViewport);

	VkRect2D const vkScissorRect2D = {
		.offset     = {
			.x      = pass->scissor.x,
			.y      = pass->scissor.y,
		},
		.extent     = {
			.width  = pass->scissor.width,
			.height = pass->scissor.height
		},
	};
	vkCmdSetScissor(vkFrameCommandBuffers[frameIdx], 0, 1, &vkScissorRect2D);

	PipelineObj* const pipelineObj = pipelineObjs.Get(pass->pipeline);
	vkCmdBindPipeline(vkFrameCommandBuffers[frameIdx], pipelineObj->vkPipelineBindPoint, pipelineObj->vkPipeline);

	vkCmdBindDescriptorSets(vkFrameCommandBuffers[frameIdx], pipelineObj->vkPipelineBindPoint, pipelineObj->vkPipelineLayout, 0, 1, &vkBindlessDescriptorSet, 0, 0);
}

//----------------------------------------------------------------------------------------------

void EndPass() {
	vkCmdEndRendering(vkFrameCommandBuffers[frameIdx]);
}

//----------------------------------------------------------------------------------------------

void BindIndexBuffer(Buffer buffer) {
	vkCmdBindIndexBuffer(vkFrameCommandBuffers[frameIdx], bufferObjs.Get(buffer)->vkBuffer, 0, VK_INDEX_TYPE_UINT32);
}

//----------------------------------------------------------------------------------------------

void PushConstants(Pipeline pipeline, void const* ptr, U32 len) {
	PipelineObj* const pipelineObj = pipelineObjs.Get(pipeline);
	vkCmdPushConstants(vkFrameCommandBuffers[frameIdx], pipelineObj->vkPipelineLayout, pipelineObj->vkPushConstantRange.stageFlags, 0, len, ptr);
}

//----------------------------------------------------------------------------------------------

void Draw(U32 vertexCount, U32 instanceCount) {
	vkCmdDraw(vkFrameCommandBuffers[frameIdx], vertexCount, instanceCount, 0, 0);
}

void DrawIndexed(U32 indexCount) {
	vkCmdDrawIndexed(vkFrameCommandBuffers[frameIdx], indexCount, 1, 0, 0, 0);
}

void DrawIndexedIndirect(Buffer indirectBuffer, U32 drawCount) {
	BufferObj* const bufferObj = bufferObjs.Get(indirectBuffer);
	vkCmdDrawIndexedIndirect(vkFrameCommandBuffers[frameIdx], bufferObj->vkBuffer, 0, drawCount, 0);
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Gpu