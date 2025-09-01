#pragma once

#include "JC/Gpu.h"
#include "JC/Gpu_Vk.h"

#include "JC/Array.h"
#include "JC/Bit.h"
#include "JC/Config.h"
#include "JC/Fmt.h"
#include "JC/Log.h"
#include "JC/Math.h"
#include "JC/Pool.h"
#include "JC/Sys.h"
#include "JC/Window.h"

#include "spirv-reflect/spirv_reflect.h"

namespace JC::Gpu {

//--------------------------------------------------------------------------------------------------

DefErr(Gpu, Version);
DefErr(Gpu, NoLayer);
DefErr(Gpu, NoExt);
DefErr(Gpu, NoDevice);
DefErr(Gpu, NoMemType);
DefErr(Gpu, NoMem);
DefErr(Gpu, ShaderTooManyPushConstantBlocks);
DefErr(Gpu, SpvReflect);

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
	VkImage           vkImage;
	VkImageView       vkImageView;
	Allocation        allocation;
	U32               width;
	U32               height;
	VkFormat          vkFormat;
	U32               bindlessIdx;
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
//	discreteGpu = physicalDevice->vkPhysicalDeviceProperties2.properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;

using BufferPool   = HandlePool<BufferObj,   Buffer,   MaxBuffers>;
using ImagePool    = HandlePool<ImageObj,    Image,    MaxImages>;
using ShaderPool   = HandlePool<ShaderObj,   Shader,   MaxShaders>;
using PipelinePool = HandlePool<PipelineObj, Pipeline, MaxPipelines>;

static JC::Mem::Allocator*      allocator;
static JC::Mem::TempAllocator*  tempAllocator;
static Log::Logger*             logger;
static Sys::Mutex               mutex;
static BufferPool               bufferObjs;
static ImagePool                imageObjs;
static ShaderPool               shaderObjs;
static PipelinePool             pipelineObjs;
       VkAllocationCallbacks*   vkAllocationCallbacks;	// not static: referenced in Gpu_Vk_Util.cpp
static VkInstance               vkInstance;
static VkDebugUtilsMessengerEXT vkDebugUtilsMessenger;
static VkSurfaceKHR             vkSurface;
static Array<PhysicalDevice>    physicalDevices;
static PhysicalDevice*          physicalDevice;
       VkDevice                 vkDevice;		// not static: referenced in Gpu_Vk_Util.cpp
static VkQueue                  vkQueue;

static VkSwapchainKHR           vkSwapchain;
static Array<Image>             swapchainImages;
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
static Bool                     vkImmediateCommandBufferBegun;
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

//-------------------------------------------------------------------------------------------------

static Res<BufferObj> CreateBufferImpl(
	U64                   size,
	VkBufferUsageFlags    vkBufferUsageFlags,
	VkMemoryPropertyFlags vkMemoryPropertyFlagsNeed,
	VkMemoryPropertyFlags vkMemoryPropertyFlagsWant,
	VkMemoryPropertyFlags vkMemoryPropertyFlagsAvoid,
	VkMemoryAllocateFlags vkMemoryAllocateFlags
);

//-------------------------------------------------------------------------------------------------

static VkBool32 VKAPI_CALL DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT, const VkDebugUtilsMessengerCallbackDataEXT* data, void*) {
	if (data && data->pMessage) {
		Sys::LockMutex(&mutex);
		if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
			JC_LOG_ERROR("{}", data->pMessage);
			#if defined JC_DEBUG_BREAK_ON_ERR
				if (Sys::IsDebuggerPresent()) {
					JC_DEBUGGER_BREAK();
				}
			#endif	// JC_DEBUG_BREAK_ON_ERR
		} else if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
			//Errorf("{}", data->pMessage);
		} else {
			JC_LOG("{}", data->pMessage);
		}
		Sys::UnlockMutex(&mutex);
	}
	return VK_FALSE;
}
	
//-------------------------------------------------------------------------------------------------

static void VkNameImpl(SrcLoc sl, U64 handle, VkObjectType vkObjectType, const char* fmt, Span<const Arg> args) {
	Array<char> name(tempAllocator);
	Fmt::VPrintf(&name, fmt, args);
	Fmt::Printf(&name, " ({}:{})", sl.file, sl.line);
	name.Add('\0');

	const VkDebugUtilsObjectNameInfoEXT vkDebugUtilsObjectNameInfoEXT = {
		.sType        = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
		.pNext        = 0,
		.objectType   = vkObjectType,
		.objectHandle = handle,
		.pObjectName  = name.data,
	};
	VkResult vkResult = vkSetDebugUtilsObjectNameEXT(vkDevice, &vkDebugUtilsObjectNameInfoEXT);
	JC_ASSERT(vkResult == VK_SUCCESS);
}

#define JC_DEFINE_VK_NAME(VkType, vkObjectType) \
	void VkName(SrcLoc sl, VkType obj, const char* fmt, Span<const Arg> args) { VkNameImpl(sl, (U64)obj, vkObjectType, fmt, args); }

JC_DEFINE_VK_NAME(VkInstance,            VK_OBJECT_TYPE_INSTANCE)
JC_DEFINE_VK_NAME(VkPhysicalDevice,      VK_OBJECT_TYPE_PHYSICAL_DEVICE)
JC_DEFINE_VK_NAME(VkDevice,              VK_OBJECT_TYPE_DEVICE)
JC_DEFINE_VK_NAME(VkQueue,               VK_OBJECT_TYPE_QUEUE)
JC_DEFINE_VK_NAME(VkSemaphore,           VK_OBJECT_TYPE_SEMAPHORE)
JC_DEFINE_VK_NAME(VkCommandBuffer,       VK_OBJECT_TYPE_COMMAND_BUFFER)
JC_DEFINE_VK_NAME(VkFence,               VK_OBJECT_TYPE_FENCE)
JC_DEFINE_VK_NAME(VkDeviceMemory,        VK_OBJECT_TYPE_DEVICE_MEMORY)
JC_DEFINE_VK_NAME(VkBuffer,              VK_OBJECT_TYPE_BUFFER)
JC_DEFINE_VK_NAME(VkImage,               VK_OBJECT_TYPE_IMAGE)
JC_DEFINE_VK_NAME(VkEvent,               VK_OBJECT_TYPE_EVENT)
JC_DEFINE_VK_NAME(VkQueryPool,           VK_OBJECT_TYPE_QUERY_POOL)
JC_DEFINE_VK_NAME(VkBufferView,          VK_OBJECT_TYPE_BUFFER_VIEW)
JC_DEFINE_VK_NAME(VkImageView,           VK_OBJECT_TYPE_IMAGE_VIEW)
JC_DEFINE_VK_NAME(VkShaderModule,        VK_OBJECT_TYPE_SHADER_MODULE)
JC_DEFINE_VK_NAME(VkPipelineCache,       VK_OBJECT_TYPE_PIPELINE_CACHE)
JC_DEFINE_VK_NAME(VkPipelineLayout,      VK_OBJECT_TYPE_PIPELINE_LAYOUT)
JC_DEFINE_VK_NAME(VkPipeline,            VK_OBJECT_TYPE_PIPELINE)
JC_DEFINE_VK_NAME(VkDescriptorSetLayout, VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT)
JC_DEFINE_VK_NAME(VkSampler,             VK_OBJECT_TYPE_SAMPLER)
JC_DEFINE_VK_NAME(VkDescriptorPool,      VK_OBJECT_TYPE_DESCRIPTOR_POOL)
JC_DEFINE_VK_NAME(VkDescriptorSet,       VK_OBJECT_TYPE_DESCRIPTOR_SET)
JC_DEFINE_VK_NAME(VkCommandPool,         VK_OBJECT_TYPE_COMMAND_POOL)

template <class T, class... Args>
void VkNameF(SrcLoc sl, T obj, FmtStr<Args...> fmt, Args... args) {
	VkName(sl, obj, fmt, { MakeArg(args)..., });
}

#define JC_VK_NAME(obj)            VkNameF(SrcLoc::Here(), obj, #obj)
#define JC_VK_NAMEF(obj, fmt, ...) VkNameF(SrcLoc::Here(), obj, fmt, ##__VA_ARGS__)

//----------------------------------------------------------------------------------------------

void Name(SrcLoc sl, Buffer   buffer,   const char* fmt, Span<const Arg> args) { VkNameImpl(sl, (U64)bufferObjs.Get(buffer)->vkBuffer,             VK_OBJECT_TYPE_BUFFER,          fmt, args); }
void Name(SrcLoc sl, Image    image,    const char* fmt, Span<const Arg> args) { VkNameImpl(sl, (U64)imageObjs.Get(image)->vkImage,                VK_OBJECT_TYPE_IMAGE,           fmt, args); }
void Name(SrcLoc sl, Shader   shader,   const char* fmt, Span<const Arg> args) { VkNameImpl(sl, (U64)shaderObjs.Get(shader)->vkShaderModule,       VK_OBJECT_TYPE_SHADER_MODULE,   fmt, args); }
void Name(SrcLoc sl, Pipeline pipeline, const char* fmt, Span<const Arg> args) { VkNameImpl(sl, (U64)pipelineObjs.Get(pipeline)->vkPipeline,       VK_OBJECT_TYPE_PIPELINE,        fmt, args);
	                                                                             VkNameImpl(sl, (U64)pipelineObjs.Get(pipeline)->vkPipelineLayout, VK_OBJECT_TYPE_PIPELINE_LAYOUT, fmt, args); }
//----------------------------------------------------------------------------------------------

static Res<> InitInstance() {
	LoadRootFns();

	U32 instanceVersion = 0;
	JC_CHECK_VK(vkEnumerateInstanceVersion(&instanceVersion));
	if (instanceVersion < VK_API_VERSION_1_3) {
		return Err_Version("version", instanceVersion);
	}

	U32 n = 0;
	Array<VkLayerProperties> layers(tempAllocator);
	JC_CHECK_VK(vkEnumerateInstanceLayerProperties(&n, 0));
	JC_CHECK_VK(vkEnumerateInstanceLayerProperties(&n, layers.Resize(n)));
	JC_LOG("{} layers:", layers.len);
	for (U64 i = 0; i < layers.len; i++) {
		JC_LOG(
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
				JC_LOG("Found required layer '{}'", RequiredLayers[i]);
				found = true;
				break;
			}
		}
		if (!found) {
			return Err_NoLayer("name", RequiredLayers[i]);
		}
	}

	Array<VkExtensionProperties> instExts(tempAllocator);
	JC_CHECK_VK(vkEnumerateInstanceExtensionProperties(0, &n, 0));
	JC_CHECK_VK(vkEnumerateInstanceExtensionProperties(0, &n, instExts.Resize(n)));
	JC_LOG("{} instance extensions:", instExts.len);
	for (U64 i = 0; i < instExts.len; i++) {
		JC_LOG("  {}: specVersion={}", instExts[i].extensionName, VersionStr(tempAllocator, instExts[i].specVersion));
	}
	Array<const char*> RequiredInstExts(tempAllocator);
	RequiredInstExts.Add("VK_KHR_surface");
	#if defined JC_PLATFORM_WINDOWS
		RequiredInstExts.Add("VK_KHR_win32_surface");
	#endif	//	Platform_
	if (Config::GetBool(Gpu::Cfg_EnableDebug, true)) {
		RequiredInstExts.Add("VK_EXT_debug_utils");
	}
	for (U32 i = 0; i < RequiredInstExts.len; i++) {
		Bool found = false;
		for (U64 j = 0; j < instExts.len; j++) {
			if (!strcmp(RequiredInstExts[i], instExts[j].extensionName)) {
				JC_LOG("Found required instance extension '{}'", RequiredInstExts[i]);
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

			{ "VK_LAYER_KHRONOS_validation", "debug_action",                   VK_LAYER_SETTING_TYPE_STRING_EXT, JC_LENOF(validationDebugActions), validationDebugActions},
			{ "VK_LAYER_KHRONOS_validation", "check_command_buffer",           VK_LAYER_SETTING_TYPE_BOOL32_EXT, 1,                             &vkTrue },
			{ "VK_LAYER_KHRONOS_validation", "check_image_layout",             VK_LAYER_SETTING_TYPE_BOOL32_EXT, 1,                             &vkTrue },
			{ "VK_LAYER_KHRONOS_validation", "check_object_in_use",            VK_LAYER_SETTING_TYPE_BOOL32_EXT, 1,                             &vkTrue },
			{ "VK_LAYER_KHRONOS_validation", "check_query",                    VK_LAYER_SETTING_TYPE_BOOL32_EXT, 1,                             &vkTrue },
			{ "VK_LAYER_KHRONOS_validation", "check_shaders",                  VK_LAYER_SETTING_TYPE_BOOL32_EXT, 1,                             &vkTrue },
			{ "VK_LAYER_KHRONOS_validation", "check_shaders_caching",          VK_LAYER_SETTING_TYPE_BOOL32_EXT, 1,                             &vkTrue },
			{ "VK_LAYER_KHRONOS_validation", "fine_grained_locking",           VK_LAYER_SETTING_TYPE_BOOL32_EXT, 1,                             &vkTrue },
			{ "VK_LAYER_KHRONOS_validation", "object_lifetime",                VK_LAYER_SETTING_TYPE_BOOL32_EXT, 1,                             &vkTrue },
			{ "VK_LAYER_KHRONOS_validation", "report_flags",                   VK_LAYER_SETTING_TYPE_STRING_EXT, JC_LENOF(validationReportFlags),  validationReportFlags },
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
			.settingCount = JC_LENOF(vkLayerSettings),
			.pSettings    = vkLayerSettings,

		};
		vkInstanceCreateInfo.pNext = &vkLayerSettingsCreateInfo;
	}

	JC_CHECK_VK(vkCreateInstance(&vkInstanceCreateInfo, vkAllocationCallbacks, &vkInstance));

	LoadInstanceFns(vkInstance);

	if (Config::GetBool(Gpu::Cfg_EnableDebug, true)) {
		JC_CHECK_VK(vkCreateDebugUtilsMessengerEXT(vkInstance, &vkDebugUtilsMessengerCreateInfo, vkAllocationCallbacks, &vkDebugUtilsMessenger));
	}

	return Ok();
}

//-------------------------------------------------------------------------------------------------

static Res<> InitSurface(const Window::PlatformDesc* windowPlatformDesc) {
	#if defined JC_PLATFORM_WINDOWS
		VkWin32SurfaceCreateInfoKHR win32SurfaceCreateInfo = {
			.sType     = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
			.pNext     = 0,
			.flags     = 0,
			.hinstance = (HINSTANCE)windowPlatformDesc->hinstance,
			.hwnd      = (HWND)windowPlatformDesc->hwnd,
		};
		JC_CHECK_VK(vkCreateWin32SurfaceKHR(vkInstance, &win32SurfaceCreateInfo, vkAllocationCallbacks, &vkSurface));
	#endif	// JC_PLATFORM

	return Ok();
}

//-------------------------------------------------------------------------------------------------

static Res<> InitDevice() {
	constexpr const char* RequiredDeviceExts[] = {
		"VK_KHR_swapchain",
	};

	U32 n = 0;
	Array<VkPhysicalDevice> vkPhysicalDevices(tempAllocator);
	JC_CHECK_VK(vkEnumeratePhysicalDevices(vkInstance, &n, nullptr));
	JC_CHECK_VK(vkEnumeratePhysicalDevices(vkInstance, &n, vkPhysicalDevices.Resize(n)));

	// TODO: support cvar for selecting explicit device

	JC_LOG("{} physical devices:", vkPhysicalDevices.len);
	physicalDevices.Resize(vkPhysicalDevices.len);

	U32 bestScore = 0;
	for (U64 i = 0; i < physicalDevices.len; i++) {
		PhysicalDevice* pd = &physicalDevices[i];
		pd->vkPhysicalDevice = vkPhysicalDevices[i];
		pd->vkPhysicalDeviceProperties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
		vkGetPhysicalDeviceProperties2(vkPhysicalDevices[i], &pd->vkPhysicalDeviceProperties2);
		JC_LOG(
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
			JC_LOG("  Rejecting device: need Vulkan 1.3: apiVersion={}", VersionStr(tempAllocator, pd->vkPhysicalDeviceProperties2.properties.apiVersion));
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
				JC_LOG("  Rejecting device: doesn't support " #path); \
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
		JC_LOG("  {} memory types:", pd->vkPhysicalDeviceMemoryProperties.memoryTypeCount);
		for (U64 j = 0; j < pd->vkPhysicalDeviceMemoryProperties.memoryTypeCount; j++) {
			const VkMemoryType mt = pd->vkPhysicalDeviceMemoryProperties.memoryTypes[j];
			JC_LOG("    [{}] heapIndex={}, flags={}", j, mt.heapIndex, MemoryPropertyFlagsStr(tempAllocator, mt.propertyFlags));
		}
		JC_LOG("  {} memory heaps:", pd->vkPhysicalDeviceMemoryProperties.memoryHeapCount);
		for (U64 j = 0; j < pd->vkPhysicalDeviceMemoryProperties.memoryHeapCount; j++) {
			const VkMemoryHeap mh = pd->vkPhysicalDeviceMemoryProperties.memoryHeaps[j];
			JC_LOG("    [{}] size={}, flags={}", j, SizeStr(tempAllocator, mh.size), MemoryHeapFlagsStr(tempAllocator, mh.flags));
		}

		Array<VkQueueFamilyProperties> vkQueueFamilyProperties(tempAllocator);
		vkGetPhysicalDeviceQueueFamilyProperties(pd->vkPhysicalDevice, &n, nullptr);
		vkGetPhysicalDeviceQueueFamilyProperties(pd->vkPhysicalDevice, &n, vkQueueFamilyProperties.Resize(n));

		pd->vkExtensionProperties.Init(allocator);
		JC_CHECK_VK(vkEnumerateDeviceExtensionProperties(pd->vkPhysicalDevice, 0, &n, 0));
		JC_CHECK_VK(vkEnumerateDeviceExtensionProperties(pd->vkPhysicalDevice, 0, &n, pd->vkExtensionProperties.Resize(n)));

		Array<char> extensionsStr(tempAllocator);
		for (U64 j = 0; j < pd->vkExtensionProperties.len; j++) {
			Fmt::Printf(&extensionsStr, "{}(specVersion={}), ", pd->vkExtensionProperties[j].extensionName, VersionStr(tempAllocator, pd->vkExtensionProperties[j].specVersion));
		}
		if (extensionsStr.len >= 2) {
			extensionsStr.len -= 2;
		}
		JC_LOG("  {} device extensions: {}",  pd->vkExtensionProperties.len, Str(extensionsStr.data, extensionsStr.len));
		for (U64 j = 0; j < JC_LENOF(RequiredDeviceExts); j++) {
			Bool found = false;
			for (U64 k = 0; k < pd->vkExtensionProperties.len; k++) {
				if (!strcmp(RequiredDeviceExts[j], pd->vkExtensionProperties[k].extensionName)) {
					found = true;
					break;
				}
			}
			if (!found) {
				JC_LOG("Rejecting device: doesn't support extension '{}'", RequiredDeviceExts[j]);
				score = 0;
			}
		}

		pd->vkSurfaceFormats.Init(allocator);
		JC_CHECK_VK(vkGetPhysicalDeviceSurfaceFormatsKHR(pd->vkPhysicalDevice, vkSurface, &n, 0));
		JC_CHECK_VK(vkGetPhysicalDeviceSurfaceFormatsKHR(pd->vkPhysicalDevice, vkSurface, &n, pd->vkSurfaceFormats.Resize(n)));
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
			JC_LOG("  Rejecting device: no suitable swapchain format");
			score = 0;
		}
		JC_LOG("  {} surface formats:", pd->vkSurfaceFormats.len);
		for (U64 j = 0; j < pd->vkSurfaceFormats.len; j++) {
			JC_LOG("    {}, {}", FormatStr(pd->vkSurfaceFormats[j].format), ColorSpaceStr(pd->vkSurfaceFormats[j].colorSpace));
		}
		JC_LOG("  Selected surface format: {}/{}", FormatStr(pd->vkSwapchainFormat), ColorSpaceStr(pd->vkSwapchainColorSpace));

		pd->vkPresentModes.Init(allocator);
		JC_CHECK_VK(vkGetPhysicalDeviceSurfacePresentModesKHR(pd->vkPhysicalDevice, vkSurface, &n, 0));
		JC_CHECK_VK(vkGetPhysicalDeviceSurfacePresentModesKHR(pd->vkPhysicalDevice, vkSurface, &n, pd->vkPresentModes.Resize(n)));
		JC_LOG("  {} present modes:", pd->vkPresentModes.len);
		for (U64 j = 0; j < pd->vkPresentModes.len; j++) {
			JC_LOG("    {}", PresentModeStr(pd->vkPresentModes[j]));
		}
		if (pd->vkPresentModes.len == 0) {
			JC_LOG("  Rejecting device: no present modes");
			score = 0;
		}

		pd->queueFamilies.Init(allocator);
		pd->queueFamilies.Resize(vkQueueFamilyProperties.len);
		JC_LOG("  {} queue families:", pd->queueFamilies.len);
		for (U64 j = 0; j < pd->queueFamilies.len; j++) {
			const VkQueueFamilyProperties* props = &pd->queueFamilies[j].vkQueueFamilyProperties;
			VkBool32 supportsPresent = VK_FALSE;
			JC_CHECK_VK(vkGetPhysicalDeviceSurfaceSupportKHR(pd->vkPhysicalDevice, (U32)j, vkSurface, &supportsPresent));
			pd->queueFamilies[j].vkQueueFamilyProperties = vkQueueFamilyProperties[j];
			pd->queueFamilies[j].supportsPresent         = (supportsPresent == VK_TRUE);
			JC_LOG("    [{}] count={}, flags={}, supportsPresent={}", j, props->queueCount, QueueFlagsStr(tempAllocator, props->queueFlags), pd->queueFamilies[j].supportsPresent);
			const VkQueueFlags flags = pd->queueFamilies[j].vkQueueFamilyProperties.queueFlags;
			if (pd->queueFamily == VK_QUEUE_FAMILY_IGNORED && (flags & VK_QUEUE_GRAPHICS_BIT) && pd->queueFamilies[j].supportsPresent) {
				pd->queueFamily = (U32)j;
			}
		}
		if (pd->queueFamily == VK_QUEUE_FAMILY_IGNORED) {
			JC_LOG("  Rejecting device: no graphics queue supporting present");
			score = 0;
		}

		JC_LOG("  score: {}", score);
		if (score > bestScore) {
			physicalDevice = pd;
			bestScore = score;
		}
	}

	if (bestScore == 0) {
		return Err_NoDevice();
	}
	JC_LOG("Selected physical device '{}' with score={}", physicalDevice->vkPhysicalDeviceProperties2.properties.deviceName, bestScore);

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
		.enabledExtensionCount   = JC_LENOF(RequiredDeviceExts),
		.ppEnabledExtensionNames = RequiredDeviceExts,
		.pEnabledFeatures        = 0,
	};

	JC_CHECK_VK(vkCreateDevice(physicalDevice->vkPhysicalDevice, &vkDeviceCreateInfo, vkAllocationCallbacks, &vkDevice));
	JC_VK_NAME(vkDevice);

	LoadDeviceFns(vkDevice);

	vkGetDeviceQueue(vkDevice, physicalDevice->queueFamily, 0, &vkQueue);

	return Ok();
}

//-------------------------------------------------------------------------------------------------

static Res<> InitSwapchain(U32 width, U32 height) {
	VkSurfaceCapabilitiesKHR vkSurfaceCapabilities;
	JC_CHECK_VK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice->vkPhysicalDevice, vkSurface, &vkSurfaceCapabilities));
	
	U32 imageCount = Max((U32)3, vkSurfaceCapabilities.minImageCount);
	if (vkSurfaceCapabilities.maxImageCount > 0 && imageCount > vkSurfaceCapabilities.maxImageCount) {
		imageCount = vkSurfaceCapabilities.maxImageCount;
	}
	JC_LOG("Selected swapchain image count: {}", imageCount);

	// TODO: check min/max extents
	const VkExtent2D vkSwapchainExtent = (vkSurfaceCapabilities.currentExtent.width != U32Max)
		? vkSurfaceCapabilities.currentExtent
		: VkExtent2D {
			.width  = Clamp(width,  vkSurfaceCapabilities.minImageExtent.width,  vkSurfaceCapabilities.maxImageExtent.width),
			.height = Clamp(height, vkSurfaceCapabilities.minImageExtent.height, vkSurfaceCapabilities.maxImageExtent.height),
		};
	JC_LOG("Selected swapchain extent: ({}, {})", vkSwapchainExtent.width, vkSwapchainExtent.height);

	VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;
	for (U64 i = 0; i < physicalDevice->vkPresentModes.len; i++) {
		if (physicalDevice->vkPresentModes[i] == VK_PRESENT_MODE_MAILBOX_KHR) {
			presentMode = VK_PRESENT_MODE_MAILBOX_KHR;
		}
	}
	JC_LOG("Selected swapchain present mode: {}", PresentModeStr(presentMode));

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
	JC_CHECK_VK(vkCreateSwapchainKHR(vkDevice, &vkSwapchainCreateInfoKHR, vkAllocationCallbacks, &vkSwapchain));

	U32 n = 0;
	JC_CHECK_VK(vkGetSwapchainImagesKHR(vkDevice, vkSwapchain, &n, 0));
	VkImage* const vkSwapchainImages = tempAllocator->AllocT<VkImage>(n);
	JC_CHECK_VK(vkGetSwapchainImagesKHR(vkDevice, vkSwapchain, &n, vkSwapchainImages));

	swapchainImages.Resize(n);
	for (U64 i = 0; i < n; i++) {
		JC_VK_NAMEF(vkSwapchainImages[i], "vkSwapchainImages[{}]", i);

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
			.subresourceRange   = {
				.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
				.baseMipLevel   = 0,
				.levelCount     = VK_REMAINING_MIP_LEVELS,
				.baseArrayLayer = 0,
				.layerCount     = VK_REMAINING_ARRAY_LAYERS,
			},
		};
		VkImageView vkSwapchainImageView = VK_NULL_HANDLE;
		JC_CHECK_VK(vkCreateImageView(vkDevice, &vkImageViewCreateInfo, vkAllocationCallbacks, &vkSwapchainImageView));
		JC_VK_NAMEF(vkSwapchainImageView, "vkSwapchainImageView#{}", i);

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
	const VkCommandPoolCreateInfo vkCommandPoolCreateInfo = {
		.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
		.pNext            = 0,
		.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
		.queueFamilyIndex = physicalDevice->queueFamily,
	};
	JC_CHECK_VK(vkCreateCommandPool(vkDevice, &vkCommandPoolCreateInfo, vkAllocationCallbacks, &vkFrameCommandPool));
	JC_VK_NAME(vkFrameCommandPool);

	const VkCommandBufferAllocateInfo vkCommandBufferAllocateInfo = {
		.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.pNext              = 0,
		.commandPool        = vkFrameCommandPool,
		.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		.commandBufferCount = MaxFrames,
	};
	JC_CHECK_VK(vkAllocateCommandBuffers(vkDevice, &vkCommandBufferAllocateInfo, vkFrameCommandBuffers));
	for (U64 i = 0; i < MaxFrames; i++) {
		JC_VK_NAMEF(vkFrameCommandBuffers[i], "vkFrameCommandBuffers[{}]", i);
	}

	const VkSemaphoreTypeCreateInfo vkSemaphoreTypeCreateInfo = {
		.sType         = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
		.pNext         = 0,
		.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE,
		.initialValue  = MaxFrames - 1,
	};
	{
		const VkSemaphoreCreateInfo vkSemaphoreCreateInfo = {
			.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
			.pNext = &vkSemaphoreTypeCreateInfo,
			.flags = 0,
		};
		JC_CHECK_VK(vkCreateSemaphore(vkDevice, &vkSemaphoreCreateInfo, vkAllocationCallbacks, &vkFrameTimelineSemaphore));
		JC_VK_NAME(vkFrameTimelineSemaphore);
	}

	for (U64 i = 0; i < MaxFrames; i++) {
		const VkSemaphoreCreateInfo vkSemaphoreCreateInfo = {
			.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
			.pNext = 0,
			.flags = 0,
		};
		JC_CHECK_VK(vkCreateSemaphore(vkDevice, &vkSemaphoreCreateInfo, vkAllocationCallbacks, &vkFrameImageAcquiredSemaphores[i]));
		JC_CHECK_VK(vkCreateSemaphore(vkDevice, &vkSemaphoreCreateInfo, vkAllocationCallbacks, &vkFrameSubmitCompleteSemaphores[i]));
		JC_VK_NAMEF(vkFrameImageAcquiredSemaphores[i],  "vkFrameImageAcquiredSemaphores[{}]",  i);
		JC_VK_NAMEF(vkFrameSubmitCompleteSemaphores[i], "vkFrameSubmitCompleteSemaphores[{}]", i);
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
	JC_VK_NAME(frameStagingBuffer.vkBuffer);

	U8* ptr = 0;
	JC_CHECK_VK(vkMapMemory(vkDevice, frameStagingBuffer.allocation.vkDeviceMemory, 0, PerFrameStagingBufferSize * MaxFrames, 0, (void**)&ptr));

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
	const VkCommandPoolCreateInfo vkCommandPoolCreateInfo = {
		.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
		.pNext            = 0,
		.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
		.queueFamilyIndex = physicalDevice->queueFamily,
	};
	JC_CHECK_VK(vkCreateCommandPool(vkDevice, &vkCommandPoolCreateInfo, vkAllocationCallbacks, &vkImmediateCommandPool));
	JC_VK_NAME(vkImmediateCommandPool);

	const VkCommandBufferAllocateInfo vkCommandBufferAllocateInfo = {
		.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.pNext              = 0,
		.commandPool        = vkImmediateCommandPool,
		.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		.commandBufferCount = 1,
	};
	JC_CHECK_VK(vkAllocateCommandBuffers(vkDevice, &vkCommandBufferAllocateInfo, &vkImmediateCommandBuffer));
	JC_VK_NAME(vkImmediateCommandBuffer);
	vkImmediateCommandBufferBegun = false;

	const VkSemaphoreTypeCreateInfo vkSemaphoreTypeCreateInfo = {
		.sType         = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
		.pNext         = 0,
		.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE,
		.initialValue  = 0,
	};
	const VkSemaphoreCreateInfo vkSemaphoreCreateInfo = {
		.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
		.pNext = &vkSemaphoreTypeCreateInfo,
		.flags = 0,
	};
	JC_CHECK_VK(vkCreateSemaphore(vkDevice, &vkSemaphoreCreateInfo, vkAllocationCallbacks, &vkImmediateTimelineSemaphore));
	JC_VK_NAME(vkImmediateTimelineSemaphore);
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
	JC_VK_NAME(immediateStagingBuffer.vkBuffer);

	JC_CHECK_VK(vkMapMemory(vkDevice, immediateStagingBuffer.allocation.vkDeviceMemory, 0, ImmediateStagingBufferSize, 0, (void**)&immediateStagingBufferPtr));

	immediateStagingBufferUsed = 0;

	return Ok();
}

//-------------------------------------------------------------------------------------------------

static void AddBindlessSampler(VkSampler vkSampler) {
	JC_ASSERT(vkBindlessSamplersLen < MaxBindlessSamplers);
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

static Res<> InitBindless() {
	constexpr VkDescriptorPoolSize vkDescriptorPoolSizes[] = {
		{ .type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, .descriptorCount = MaxBindlessSampledImages },
		{ .type = VK_DESCRIPTOR_TYPE_SAMPLER,       .descriptorCount = MaxBindlessSamplers },
	};
	const VkDescriptorPoolCreateInfo vkDescriptorPoolCreateInfo = {
		.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
		.pNext         = 0,
		.flags         = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT,
		.maxSets       = MaxBindlessDescriptorSets,
		.poolSizeCount = JC_LENOF(vkDescriptorPoolSizes),
		.pPoolSizes    = vkDescriptorPoolSizes,
	};
	JC_CHECK_VK(vkCreateDescriptorPool(vkDevice, &vkDescriptorPoolCreateInfo, vkAllocationCallbacks, &vkDescriptorPool));
	JC_VK_NAME(vkDescriptorPool);

	constexpr VkDescriptorBindingFlags vkDescriptorBindingFlags[] = {
		VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT_EXT,
		VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT_EXT,
	};
	const VkDescriptorSetLayoutBindingFlagsCreateInfo vkDescriptorSetLayoutBindingFlagsCreateInfo = {
		.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
		.pNext         = 0,
		.bindingCount  = JC_LENOF(vkDescriptorBindingFlags),
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
		.bindingCount = JC_LENOF(vkMeshDescriptorSetLayoutBindings),
		.pBindings    = vkMeshDescriptorSetLayoutBindings,
	};
	JC_CHECK_VK(vkCreateDescriptorSetLayout(vkDevice, &vkDescriptorSetLayoutCreateInfo, vkAllocationCallbacks, &vkBindlessDescriptorSetLayout));
	JC_VK_NAME(vkBindlessDescriptorSetLayout);

	const VkDescriptorSetAllocateInfo vkDescriptorSetAllocateInfo = {
		.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
		.pNext              = 0,
		.descriptorPool     = vkDescriptorPool,
		.descriptorSetCount = 1,
		.pSetLayouts        = &vkBindlessDescriptorSetLayout,
	};
	JC_CHECK_VK(vkAllocateDescriptorSets(vkDevice, &vkDescriptorSetAllocateInfo, &vkBindlessDescriptorSet));
	JC_VK_NAME(vkBindlessDescriptorSet);

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
	JC_CHECK_VK(vkCreateSampler(vkDevice, &vkSamplerCreateInfo, vkAllocationCallbacks, &vkNearestSampler));
	JC_VK_NAME(vkNearestSampler);
	AddBindlessSampler(vkNearestSampler);

	vkSamplerCreateInfo.magFilter        = VK_FILTER_LINEAR;
	vkSamplerCreateInfo.minFilter        = VK_FILTER_LINEAR;
	vkSamplerCreateInfo.mipmapMode       = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	vkSamplerCreateInfo.anisotropyEnable = VK_TRUE;
	vkSamplerCreateInfo.maxAnisotropy    = MaxAnisotropy;
	VkSampler vkLinearSampler = VK_NULL_HANDLE;
	JC_CHECK_VK(vkCreateSampler(vkDevice, &vkSamplerCreateInfo, vkAllocationCallbacks, &vkLinearSampler));
	JC_VK_NAME(vkLinearSampler);
	AddBindlessSampler(vkLinearSampler);

	return Ok();
}

//-------------------------------------------------------------------------------------------------

Res<> Init(const InitDesc* initDesc) {
	allocator     = initDesc->allocator;
	tempAllocator = initDesc->tempAllocator;
	logger        = initDesc->logger;
	Sys::InitMutex(&mutex);
	physicalDevices.Init(allocator);
	swapchainImages.Init(allocator);

	JC_CHECK_RES(InitInstance());
	JC_CHECK_RES(InitSurface(initDesc->windowPlatformDesc));
	JC_CHECK_RES(InitDevice());
	JC_CHECK_RES(InitSwapchain(initDesc->windowWidth, initDesc->windowHeight));
	JC_CHECK_RES(InitFrame());
	JC_CHECK_RES(InitImmediate());
	JC_CHECK_RES(InitBindless());

	const VkCommandBufferBeginInfo vkCommandBufferBeginInfo = {
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

	JC_CHECK_RES(ImmediateWait());

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
	for (U64 i = 0; i < JC_LENOF(a); i++) { \
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
		const VkMemoryPropertyFlags typeFlags = physicalDevice->vkPhysicalDeviceMemoryProperties.memoryTypes[i].propertyFlags;
		if ((typeFlags & vkMemoryPropertyFlagsNeed) != vkMemoryPropertyFlagsNeed) {
			continue;
		}
		const U32 wantCost  = Bit::PopCount32(vkMemoryPropertyFlagsWant  & ~typeFlags);
		const U32 avoidCost = Bit::PopCount32(vkMemoryPropertyFlagsAvoid & typeFlags);
		const U32 cost      = wantCost + avoidCost;
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
	const U32 memTypeIdx = SelectMemTypeIdx(vkMemoryRequirements.memoryTypeBits, vkMemoryPropertyFlagsNeed, vkMemoryPropertyFlagsWant, vkMemoryPropertyFlagsAvoid);
	if (memTypeIdx == VK_MAX_MEMORY_TYPES) {
		return Err_NoMemType(
			"memTypeBits",                vkMemoryRequirements.memoryTypeBits,
			"vkMemoryPropertyFlagsNeed",  vkMemoryPropertyFlagsNeed,
			"vkMemoryPropertyFlagsWant",  vkMemoryPropertyFlagsWant,
			"vkMemoryPropertyFlagsAvoid", vkMemoryPropertyFlagsAvoid
		);
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
	JC_CHECK_VK(vkCreateBuffer(vkDevice, &vkBufferCreateInfo, vkAllocationCallbacks, &vkBuffer));

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

	if (const VkResult r = vkBindBufferMemory(vkDevice, vkBuffer, allocation.vkDeviceMemory, 0); r != VK_SUCCESS) {
		vkDestroyBuffer(vkDevice, vkBuffer, vkAllocationCallbacks);
		vkFreeMemory(vkDevice, allocation.vkDeviceMemory, vkAllocationCallbacks);
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
	JC_CHECK_RES(CreateBufferImpl(
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
	if (buffer.handle) {
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
	JC_ASSERT(bufferObj->addr);
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
	const VkBufferMemoryBarrier2 vkBufferMemoryBarrier2 = {
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
	vkCmdPipelineBarrier2(vkCommandBuffer, &vkDependencyInfo);
}

//-------------------------------------------------------------------------------------------------

Res<Image> CreateImage(U32 width, U32 height, ImageFormat format, ImageUsage::Flags imageUsageFlags) {
	// TODO: check format properties optimal tiling supports the requested format/feature
	const VkFormat vkFormat = ImageFormatToVkFormat(format);

	VkImageUsageFlags vkImageUsageFlags = 0;
	if (imageUsageFlags & ImageUsage::Sampled) { vkImageUsageFlags |= VK_IMAGE_USAGE_SAMPLED_BIT; }
	if (imageUsageFlags & ImageUsage::Color)   { vkImageUsageFlags |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT; }
	if (imageUsageFlags & ImageUsage::Depth)   { vkImageUsageFlags |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT; }
	if (imageUsageFlags & ImageUsage::Copy)    { vkImageUsageFlags |= VK_IMAGE_USAGE_TRANSFER_DST_BIT; }

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
	JC_CHECK_VK(vkCreateImage(vkDevice, &vkImageCreateInfo, vkAllocationCallbacks, &vkImage));

	VkMemoryRequirements vkMemoryRequirements;
	vkGetImageMemoryRequirements(vkDevice, vkImage, &vkMemoryRequirements);

	Allocation allocation;
	if (Res<> r = AllocVkMem(vkMemoryRequirements, 0, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 0, 0).To(allocation); !r) {
		vkDestroyImage(vkDevice, vkImage, vkAllocationCallbacks);
		return r.err;
	}
	
	if (const VkResult r = vkBindImageMemory(vkDevice, vkImage, allocation.vkDeviceMemory, 0); r != VK_SUCCESS) {
		vkDestroyImage(vkDevice, vkImage, vkAllocationCallbacks);
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
		const VkDescriptorImageInfo vkDescriptorImageInfo = {
			.sampler     = VK_NULL_HANDLE,
			.imageView   = vkImageView,
			.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
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
	if (image.handle) {
		ImageObj* const imageObj = imageObjs.Get(image);
		DestroyVk(imageObj->vkImageView, vkDestroyImageView);
		DestroyVk(imageObj->vkImage, vkDestroyImage);
		DestroyVk(imageObj->allocation.vkDeviceMemory, vkFreeMemory);
		memset(imageObj, 0, sizeof(*imageObj));
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
	if (shader.handle) {
		ShaderObj* const shaderObj = shaderObjs.Get(shader);
		DestroyVk(shaderObj->vkShaderModule, vkDestroyShaderModule);
		shaderObjs.Free(shader);
	}
}

//----------------------------------------------------------------------------------------------

Res<Pipeline> CreateGraphicsPipeline(Span<Shader> shaders, Span<ImageFormat> colorAttachmentFormats, ImageFormat depthFormat) {
	JC_ASSERT(shaders.len > 0);

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
				JC_ASSERT(vkPushConstantRange.offset == shaderObj->pushConstantsOffset);
				JC_ASSERT(vkPushConstantRange.size   == shaderObj->pushConstantsSize);
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
		.dynamicStateCount = (U32)JC_LENOF(vkDynamicStates),
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

	PipelinePool::Entry* const entry = pipelineObjs.Alloc();
	entry->obj.vkPipelineLayout    = vkPipelineLayout;
	entry->obj.vkPipeline          = vkPipeline;
	entry->obj.vkPipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	entry->obj.vkPushConstantRange = vkPushConstantRange;

	return entry->Handle();
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

Res<> ImmediateWait() {
	if (!vkImmediateCommandBufferBegun) {
		return Ok();
	}

	vkEndCommandBuffer(vkImmediateCommandBuffer);
	vkImmediateCommandBufferBegun = false;

	immediateTimeline++;

	const VkCommandBufferSubmitInfo vkCommandBufferSubmitInfo = {
		.sType         = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
		.pNext         = 0,
		.commandBuffer = vkImmediateCommandBuffer,
		.deviceMask    = 0,
	};
	const VkSemaphoreSubmitInfo vkSignalSemaphoreSubmitInfo = {
		.sType       = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
		.pNext       = 0,
		.semaphore   = vkImmediateTimelineSemaphore,
		.value       = immediateTimeline,
		.stageMask   = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT,
		.deviceIndex = 0,
	};
	const VkSubmitInfo2 vkSubmitInfo2 = {
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
	JC_CHECK_VK(vkQueueSubmit2(vkQueue, 1, &vkSubmitInfo2, 0));

	const VkSemaphoreWaitInfo vkSemaphoreWaitInfo = {
		.sType          = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO,
		.pNext          = 0,
		.flags          = 0,
		.semaphoreCount = 1,
		.pSemaphores    = &vkImmediateTimelineSemaphore,
		.pValues        = &immediateTimeline,

	};
	JC_CHECK_VK(vkWaitSemaphores(vkDevice, &vkSemaphoreWaitInfo, U64Max));

	vkResetCommandPool(vkDevice, vkImmediateCommandPool, 0);

	immediateStagingBufferUsed = 0;

	return Ok();
}

//-------------------------------------------------------------------------------------------------

Res<> ImmediateCopyToBuffer(
	const void*         data,
	U64                 len,
	Buffer              buffer,
	U64                 offset
) {
	const BufferObj* const bufferObj = bufferObjs.Get(buffer);
	while (len > 0) {
		const U64 amt = Min(len, ImmediateStagingBufferSize - immediateStagingBufferUsed);
		memcpy(immediateStagingBufferPtr + immediateStagingBufferUsed, data, amt);

		if (!vkImmediateCommandBufferBegun) {
			const VkCommandBufferBeginInfo vkCommandBufferBeginInfo = {
				.sType            = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
				.pNext            = 0,
				.flags            = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
				.pInheritanceInfo = 0,
			};
			JC_CHECK_VK(vkBeginCommandBuffer(vkImmediateCommandBuffer, &vkCommandBufferBeginInfo));
			vkImmediateCommandBufferBegun = true;
		}

		const VkBufferCopy2 vkBufferCopy2 = {
			.sType     = VK_STRUCTURE_TYPE_BUFFER_COPY_2,
			.pNext     = 0,
			.srcOffset = immediateStagingBufferUsed,
			.dstOffset = offset,
			.size      = amt,
		};
		const VkCopyBufferInfo2 vkCopyBufferInfo2 = {
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

		JC_CHECK_RES(ImmediateWait());
	}

	return Ok();
}

//-------------------------------------------------------------------------------------------------

Res<> ImmediateCopyToImage(const void* data, Image image, BarrierStage::Flags finalBarrierStageFlags, ImageLayout finalImageLayout) {
	const ImageObj* const imageObj = imageObjs.Get(image);
	U64 len = imageObj->width * imageObj->height * FormatSize(imageObj->vkFormat);
	JC_ASSERT(len <= ImmediateStagingBufferSize);
	if (len > ImmediateStagingBufferSize - immediateStagingBufferUsed) {
		JC_CHECK_RES(ImmediateWait());
	}

	memcpy(immediateStagingBufferPtr + immediateStagingBufferUsed, data, len);

	if (!vkImmediateCommandBufferBegun) {
		const VkCommandBufferBeginInfo vkCommandBufferBeginInfo = {
			.sType            = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
			.pNext            = 0,
			.flags            = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
			.pInheritanceInfo = 0,
		};
		JC_CHECK_VK(vkBeginCommandBuffer(vkImmediateCommandBuffer, &vkCommandBufferBeginInfo));
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

	const VkBufferImageCopy2 vkBufferImageCopy2 = {
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
	const VkCopyBufferToImageInfo2 vkCopyBufferToImageInfo2 = {
		.sType          = VK_STRUCTURE_TYPE_COPY_BUFFER_TO_IMAGE_INFO_2,
		.pNext          = 0,
		.srcBuffer      = immediateStagingBuffer.vkBuffer,
		.dstImage       = imageObj->vkImage,
		.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		.regionCount    = 1,
		.pRegions       = &vkBufferImageCopy2,
	};
	vkCmdCopyBufferToImage2(vkImmediateCommandBuffer, &vkCopyBufferToImageInfo2);

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
	const VkSemaphoreWaitInfo vkSemaphoreWaitInfo = {
		.sType          = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO,
		.pNext          = 0,
		.flags          = 0,
		.semaphoreCount = 1,
		.pSemaphores    = &vkFrameTimelineSemaphore,
		.pValues        = &frame,

	};
	JC_CHECK_VK(vkWaitSemaphores(vkDevice, &vkSemaphoreWaitInfo, U64Max));

	frameStagingBufferUsed[frameIdx] = 0;

	if (VkResult r = vkAcquireNextImageKHR(vkDevice, vkSwapchain, U64Max, vkFrameImageAcquiredSemaphores[frameIdx], 0, &swapchainImageIdx); r != VK_SUCCESS) {
		if (r == VK_SUBOPTIMAL_KHR || r == VK_ERROR_OUT_OF_DATE_KHR) {
			return Err_RecreateSwapchain();
		} else {
			return Err_Vk(r, "vkAcquireNextImageKHR");
		}
	}

	const VkCommandBufferBeginInfo vkCommandBufferBeginInfo = {
		.sType            = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.pNext            = 0,
		.flags            = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
		.pInheritanceInfo = 0,
	};
	JC_CHECK_VK(vkBeginCommandBuffer(vkFrameCommandBuffers[frameIdx], &vkCommandBufferBeginInfo));

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

	JC_CHECK_VK(vkEndCommandBuffer(vkFrameCommandBuffers[frameIdx]));

	const VkSemaphoreSubmitInfo vkWaitSemaphoreSubmitInfo = {
		.sType       = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
		.pNext       = 0,
		.semaphore   = vkFrameImageAcquiredSemaphores[frameIdx],
		.value       = 0,
		.stageMask   = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
		.deviceIndex = 0,
	};
	const VkCommandBufferSubmitInfo vkCommandBufferSubmitInfo = {
		.sType         = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
		.pNext         = 0,
		.commandBuffer = vkFrameCommandBuffers[frameIdx],
		.deviceMask    = 0,
	};
	const VkSemaphoreSubmitInfo vkSignalSemaphoreSubmitInfos[2] = {
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
	const VkSubmitInfo2 vkSubmitInfo2 = {
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
	JC_CHECK_VK(vkQueueSubmit2(vkQueue, 1, &vkSubmitInfo2, 0));

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
	JC_ASSERT(frameStagingBufferUsed[frameIdx] + len <= PerFrameStagingBufferSize);
	U8* const ptr = frameStagingBufferPtrs[frameIdx] + frameStagingBufferUsed[frameIdx];
	frameStagingBufferUsed[frameIdx] += len;
	return ptr;
}

//----------------------------------------------------------------------------------------------

void CopyStagingToBuffer(void* staging, U64 len, Buffer buffer, U64 offset) {
	JC_ASSERT((U8*)staging >= frameStagingBufferPtrs[frameIdx]);
	JC_ASSERT((U8*)staging + len < frameStagingBufferPtrs[frameIdx] + PerFrameStagingBufferSize);

	const BufferObj* const bufferObj = bufferObjs.Get(buffer);
	const VkDeviceSize srcOffset = 
		(frameIdx * PerFrameStagingBufferSize) + 
		(VkDeviceSize)((U8*)staging - frameStagingBufferPtrs[frameIdx]);
	const VkBufferCopy2 vkBufferCopy2 = {
		.sType     = VK_STRUCTURE_TYPE_BUFFER_COPY_2,
		.pNext     = 0,
		.srcOffset = srcOffset,
		.dstOffset = offset,
		.size      = len,
	};
	const VkCopyBufferInfo2 vkCopyBufferInfo2 = {
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
	const ImageObj* const imageObj = imageObjs.Get(image);
	const U64 len = imageObj->width * imageObj->height * FormatSize(imageObj->vkFormat);
	JC_ASSERT((U8*)staging>= frameStagingBufferPtrs[frameIdx]);
	JC_ASSERT((U8*)staging + len < frameStagingBufferPtrs[frameIdx] + PerFrameStagingBufferSize);

	const VkDeviceSize srcOffset = 
		(frameIdx * PerFrameStagingBufferSize) + 
		(VkDeviceSize)((U8*)staging - frameStagingBufferPtrs[frameIdx]);

	const VkBufferImageCopy2 vkBufferImageCopy2 = {
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
	const VkCopyBufferToImageInfo2 vkCopyBufferToImageInfo2 = {
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
	const VkBufferMemoryBarrier2 vkBufferMemoryBarrier2 = {
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
	vkCmdPipelineBarrier2(vkFrameCommandBuffers[frameIdx], &vkDependencyInfo);
}

//-------------------------------------------------------------------------------------------------

void BeginPass(const Pass* pass) {
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
	vkCmdBeginRendering(vkFrameCommandBuffers[frameIdx], &vkRenderingInfo);

	const VkViewport vkViewport = {
		.x        = pass->viewport.x,
		.y        = pass->viewport.y,
		.width    = pass->viewport.w,
		.height   = pass->viewport.h,
		.minDepth = 0.0f,
		.maxDepth = 1.0f,
	};
	vkCmdSetViewport(vkFrameCommandBuffers[frameIdx], 0, 1, &vkViewport);

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

void PushConstants(Pipeline pipeline, const void* ptr, U32 len) {
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
//----------------------------------------------------------------------------------------------

}	// namespace JC::Gpu