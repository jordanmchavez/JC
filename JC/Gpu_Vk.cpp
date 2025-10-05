#pragma once

#include "JC/Gpu.h"
#include "JC/Gpu_Vk.h"

#include "JC/Array.h"
#include "JC/Bit.h"
#include "JC/Fmt.h"
#include "JC/Log.h"
#include "JC/Math.h"
#include "JC/Mem.h"
#include "JC/Pool.h"
#include "JC/Sys.h"
#include "JC/Window.h"

#include "spirv-reflect/spirv_reflect.h"

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

static constexpr U32 Gpu_MaxBuffers	                      = 4096;
static constexpr U32 Gpu_MaxImages	                      = 4096;
static constexpr U32 Gpu_MaxShaders	                      = 1024;
static constexpr U32 Gpu_MaxPipelines                     = 128;
static constexpr U32 Gpu_MaxBindlessSampledImages         = 64 * 1024;
static constexpr U32 Gpu_MaxBindlessSamplers              = 8;
static constexpr U32 Gpu_MaxBindlessDescriptorSets        = 32;
static constexpr F32 Gpu_MaxAnisotropy                    = 8.0f;
static constexpr U64 Gpu_PerFrameStagingBufferSize        = 256 * MB;
static constexpr U64 Gpu_ImmediateStagingBufferSize       = 64 * MB;

//--------------------------------------------------------------------------------------------------

struct Gpu_QueueFamily {
	VkQueueFamilyProperties vkQueueFamilyProperties;
	Bool                    supportsPresent;
};

struct Gpu_PhysicalDevice {
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
	Span<Gpu_QueueFamily>            queueFamilies;
	U32                              queueFamily;
};

struct Gpu_Allocation {
	VkDeviceMemory vkDeviceMemory;
	U32            memTypeIdx;
	U64            size;
};

struct Gpu_BufferObj {
	VkBuffer           vkBuffer;
	Gpu_Allocation     allocation;
	U64                size;
	VkBufferUsageFlags vkBufferUsageFlags;
	U64                addr;
};

struct Gpu_ImageObj {
	VkImage           vkImage;
	VkImageView       vkImageView;
	Gpu_Allocation    allocation;
	U32               width;
	U32               height;
	VkFormat          vkFormat;
	U32               bindlessIdx;
};

struct Gpu_ShaderObj {
	VkShaderModule        vkShaderModule;
	VkShaderStageFlagBits vkShaderStage;
	U32                   pushConstantsOffset;
	U32                   pushConstantsSize;
};

struct Gpu_PipelineObj {
	VkPipeline          vkPipeline;
	VkPipelineLayout    vkPipelineLayout;
	VkPipelineBindPoint vkPipelineBindPoint;
	VkPushConstantRange vkPushConstantRange;
};

template <class T>
Span<T> Span_Make(Mem* mem, U64 n) {
	return Span<T>(Mem_Alloc(mem, n * sizeof(T), n));
}

//--------------------------------------------------------------------------------------------------
//	discreteGpu = gpu_physicalDevice->vkPhysicalDeviceProperties2.properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;

using Gpu_BufferPool   = HandlePool<Gpu_BufferObj,   Gpu_Buffer,   Gpu_MaxBuffers>;
using Gpu_ImagePool    = HandlePool<Gpu_ImageObj,    Gpu_Image,    Gpu_MaxImages>;
using Gpu_ShaderPool   = HandlePool<Gpu_ShaderObj,   Gpu_Shader,   Gpu_MaxShaders>;
using Gpu_PipelinePool = HandlePool<Gpu_PipelineObj, Gpu_Pipeline, Gpu_MaxPipelines>;

static Mem*                      gpu_permMem;
static Mem*                      gpu_tempMem;
static bool                      gpu_enableDebug;
static Sys_Mutex                 gpu_mutex;
static Gpu_BufferPool            gpu_bufferObjs;
static Gpu_ImagePool             gpu_imageObjs;
static Gpu_ShaderPool            gpu_shaderObjs;
static Gpu_PipelinePool          gpu_pipelineObjs;
       VkAllocationCallbacks*    gpu_vkAllocationCallbacks;	// not static: referenced in Gpu_Vk_Util.cpp
static VkInstance                gpu_vkInstance;
static VkDebugUtilsMessengerEXT  gpu_vkDebugUtilsMessenger;
static VkSurfaceKHR              gpu_vkSurface;
static Span<Gpu_PhysicalDevice>  gpu_physicalDevices;
static Gpu_PhysicalDevice*       gpu_physicalDevice;
       VkDevice                  gpu_vkDevice;		// not static: referenced in Gpu_Vk_Util.cpp
static VkQueue                   gpu_vkQueue;
static VkSwapchainKHR            gpu_vkSwapchain;
static Span<Gpu_Image>           gpu_swapchainImages;
static U32                       gpu_swapchainImageIdx;
static VkCommandPool             gpu_vkFrameCommandPool;
static VkCommandBuffer           gpu_vkFrameCommandBuffers[Gpu_MaxFrames];
static Gpu_BufferObj             gpu_frameStagingBuffer;
static U8*                       gpu_frameStagingBufferPtrs[Gpu_MaxFrames];
static U64                       gpu_frameStagingBufferUsed[Gpu_MaxFrames];
static VkSemaphore               gpu_vkFrameTimelineSemaphore;
static VkSemaphore               gpu_vkFrameImageAcquiredSemaphores[Gpu_MaxFrames];
static VkSemaphore               gpu_vkFrameSubmitCompleteSemaphores[Gpu_MaxFrames];
static U32                       gpu_frameIdx;
static U64                       gpu_frame;
static VkCommandPool             gpu_vkImmediateCommandPool;
static VkCommandBuffer           gpu_vkImmediateCommandBuffer;
static Bool                      gpu_vkImmediateCommandBufferBegun;
static VkSemaphore               gpu_vkImmediateTimelineSemaphore;
static U64                       gpu_immediateTimeline;
static Gpu_BufferObj             gpu_immediateStagingBuffer;
static U8*                       gpu_immediateStagingBufferPtr;
static U64                       gpu_immediateStagingBufferUsed;
static VkDescriptorPool          gpu_vkDescriptorPool;
static VkDescriptorSetLayout     gpu_vkBindlessDescriptorSetLayout;
static VkDescriptorSet           gpu_vkBindlessDescriptorSet;
static U32                       gpu_nextBindlessDescriptorIdx;
static VkSampler                 gpu_vkBindlessSamplers[Gpu_MaxBindlessSamplers];
static U32                       gpu_vkBindlessSamplersLen;

static Res<Gpu_BufferObj> Gpu_CreateBufferImpl(
	U64                   size,
	VkBufferUsageFlags    vkBufferUsageFlags,
	VkMemoryPropertyFlags vkMemoryPropertyFlagsNeed,
	VkMemoryPropertyFlags vkMemoryPropertyFlagsWant,
	VkMemoryPropertyFlags vkMemoryPropertyFlagsAvoid,
	VkMemoryAllocateFlags vkMemoryAllocateFlags
);

//-------------------------------------------------------------------------------------------------

static VkBool32 VKAPI_CALL Gpu_DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT, VkDebugUtilsMessengerCallbackDataEXT const* data, void*) {
	if (data && data->pMessage) {
		Sys_LockMutex(&gpu_mutex);
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
		Sys_UnlockMutex(&gpu_mutex);
	}
	return VK_FALSE;
}
	
//-------------------------------------------------------------------------------------------------

static void Gpu_VkNameImpl(SrcLoc sl, U64 handle, VkObjectType vkObjectType, char const* fmt, Span<Arg const> args) {
	Array<char> name(gpu_tempMem);
	Fmt_Printv(&name, fmt, args);
	Fmt_Printf(&name, " (%s:%u)", sl.file, sl.line);
	name.Add('\0');

	VkDebugUtilsObjectNameInfoEXT const vkDebugUtilsObjectNameInfoEXT = {
		.sType        = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
		.pNext        = 0,
		.objectType   = vkObjectType,
		.objectHandle = handle,
		.pObjectName  = name.data,
	};
	VkResult vkResult = vkSetDebugUtilsObjectNameEXT(gpu_vkDevice, &vkDebugUtilsObjectNameInfoEXT);
	Assert(vkResult == VK_SUCCESS);
}

#define Gpu_DefVkName(VkType, vkObjectType) \
	void Gpu_VkName(SrcLoc sl, VkType obj, char const* fmt, Span<Arg const> args) { Gpu_VkNameImpl(sl, (U64)obj, vkObjectType, fmt, args); }

Gpu_DefVkName(VkInstance,            VK_OBJECT_TYPE_INSTANCE)
Gpu_DefVkName(VkPhysicalDevice,      VK_OBJECT_TYPE_PHYSICAL_DEVICE)
Gpu_DefVkName(VkDevice,              VK_OBJECT_TYPE_DEVICE)
Gpu_DefVkName(VkQueue,               VK_OBJECT_TYPE_QUEUE)
Gpu_DefVkName(VkSemaphore,           VK_OBJECT_TYPE_SEMAPHORE)
Gpu_DefVkName(VkCommandBuffer,       VK_OBJECT_TYPE_COMMAND_BUFFER)
Gpu_DefVkName(VkFence,               VK_OBJECT_TYPE_FENCE)
Gpu_DefVkName(VkDeviceMemory,        VK_OBJECT_TYPE_DEVICE_MEMORY)
Gpu_DefVkName(VkBuffer,              VK_OBJECT_TYPE_BUFFER)
Gpu_DefVkName(VkImage,               VK_OBJECT_TYPE_IMAGE)
Gpu_DefVkName(VkEvent,               VK_OBJECT_TYPE_EVENT)
Gpu_DefVkName(VkQueryPool,           VK_OBJECT_TYPE_QUERY_POOL)
Gpu_DefVkName(VkBufferView,          VK_OBJECT_TYPE_BUFFER_VIEW)
Gpu_DefVkName(VkImageView,           VK_OBJECT_TYPE_IMAGE_VIEW)
Gpu_DefVkName(VkShaderModule,        VK_OBJECT_TYPE_SHADER_MODULE)
Gpu_DefVkName(VkPipelineCache,       VK_OBJECT_TYPE_PIPELINE_CACHE)
Gpu_DefVkName(VkPipelineLayout,      VK_OBJECT_TYPE_PIPELINE_LAYOUT)
Gpu_DefVkName(VkPipeline,            VK_OBJECT_TYPE_PIPELINE)
Gpu_DefVkName(VkDescriptorSetLayout, VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT)
Gpu_DefVkName(VkSampler,             VK_OBJECT_TYPE_SAMPLER)
Gpu_DefVkName(VkDescriptorPool,      VK_OBJECT_TYPE_DESCRIPTOR_POOL)
Gpu_DefVkName(VkDescriptorSet,       VK_OBJECT_TYPE_DESCRIPTOR_SET)
Gpu_DefVkName(VkCommandPool,         VK_OBJECT_TYPE_COMMAND_POOL)

template <class T, class... A>
void Gpu_VkNameF(SrcLoc sl, T obj, FmtStr<A...> fmt, A... args) {
	Gpu_VkName(sl, obj, fmt, { Arg_Make(args)..., });
}

#define Gpu_VkName(obj)            Gpu_VkNameF(SrcLoc_Here(), obj, #obj)
#define Gpu_VkNamef(obj, fmt, ...) Gpu_VkNameF(SrcLoc_Here(), obj, fmt, ##__VA_ARGS__)

//----------------------------------------------------------------------------------------------

void Gpu_Name(SrcLoc sl, Gpu_Buffer   buffer,   char const* fmt, Span<Arg const> args) { Gpu_VkNameImpl(sl, (U64)gpu_bufferObjs.Get(buffer)->vkBuffer,             VK_OBJECT_TYPE_BUFFER,          fmt, args); }
void Gpu_Name(SrcLoc sl, Gpu_Image    image,    char const* fmt, Span<Arg const> args) { Gpu_VkNameImpl(sl, (U64)gpu_imageObjs.Get(image)->vkImage,                VK_OBJECT_TYPE_IMAGE,           fmt, args); }
void Gpu_Name(SrcLoc sl, Gpu_Shader   shader,   char const* fmt, Span<Arg const> args) { Gpu_VkNameImpl(sl, (U64)gpu_shaderObjs.Get(shader)->vkShaderModule,       VK_OBJECT_TYPE_SHADER_MODULE,   fmt, args); }
void Gpu_Name(SrcLoc sl, Gpu_Pipeline pipeline, char const* fmt, Span<Arg const> args) { Gpu_VkNameImpl(sl, (U64)gpu_pipelineObjs.Get(pipeline)->vkPipeline,       VK_OBJECT_TYPE_PIPELINE,        fmt, args);
                                                                                         Gpu_VkNameImpl(sl, (U64)gpu_pipelineObjs.Get(pipeline)->vkPipelineLayout, VK_OBJECT_TYPE_PIPELINE_LAYOUT, fmt, args); }
//----------------------------------------------------------------------------------------------

static Res<> Gpu_InitInstance() {
	Gpu_LoadRootFns();

	U32 instanceVersion = 0;
	Gpu_CheckVk(vkEnumerateInstanceVersion(&instanceVersion));
	if (instanceVersion < VK_API_VERSION_1_3) {
		return Err_Version("version", instanceVersion);
	}

	U32 n = 0;
	Array<VkLayerProperties> layers(gpu_tempMem);
	Gpu_CheckVk(vkEnumerateInstanceLayerProperties(&n, 0));
	Gpu_CheckVk(vkEnumerateInstanceLayerProperties(&n, layers.Resize(n)));
	Logf("%u layers:", layers.len);
	for (U64 i = 0; i < layers.len; i++) {
		Logf(
			"  %s: implementationVersion=%s, specVersion=%s, description=%s",
			layers[i].layerName,
			Gpu_VersionStr(gpu_tempMem, layers[i].implementationVersion),
			Gpu_VersionStr(gpu_tempMem, layers[i].specVersion),
			layers[i].description
		);
	}
	Array<char const*> RequiredLayers(gpu_tempMem);
	if (gpu_enableDebug) {
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

	Array<VkExtensionProperties> instExts(gpu_tempMem);
	Gpu_CheckVk(vkEnumerateInstanceExtensionProperties(0, &n, 0));
	Gpu_CheckVk(vkEnumerateInstanceExtensionProperties(0, &n, instExts.Resize(n)));
	Logf("%u instance extensions:", instExts.len);
	for (U64 i = 0; i < instExts.len; i++) {
		Logf("  %s specVersion=%s", instExts[i].extensionName, Gpu_VersionStr(gpu_tempMem, instExts[i].specVersion));
	}
	Array<char const*> RequiredInstExts(gpu_tempMem);
	RequiredInstExts.Add("VK_KHR_surface");
	#if defined JC_PLATFORM_WINDOWS
		RequiredInstExts.Add("VK_KHR_win32_surface");
	#endif	//	Platform_
	if (gpu_enableDebug) {
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
		.enabledLayerCount       = (U32)RequiredLayers.len,
		.ppEnabledLayerNames     = RequiredLayers.data,
		.enabledExtensionCount   = (U32)RequiredInstExts.len,
		.ppEnabledExtensionNames = RequiredInstExts.data,
	};

	VkDebugUtilsMessengerCreateInfoEXT vkDebugUtilsMessengerCreateInfo;
	if (gpu_enableDebug) {
		vkDebugUtilsMessengerCreateInfo = {
			.sType           = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
			.pNext           = vkInstanceCreateInfo.pNext,
			.flags           = 0,
			.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
			.messageType     = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
			.pfnUserCallback = Gpu_DebugCallback,
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

	Gpu_CheckVk(vkCreateInstance(&vkInstanceCreateInfo, gpu_vkAllocationCallbacks, &gpu_vkInstance));

	Gpu_LoadInstanceFns(gpu_vkInstance);

	if (gpu_enableDebug) {
		Gpu_CheckVk(vkCreateDebugUtilsMessengerEXT(gpu_vkInstance, &vkDebugUtilsMessengerCreateInfo, gpu_vkAllocationCallbacks, &gpu_vkDebugUtilsMessenger));
	}

	return Ok();
}

//-------------------------------------------------------------------------------------------------

static Res<> Gpu_InitSurface(Wnd_PlatformDesc const* wndPlatformDesc) {
	#if defined Platform_Windows
		VkWin32SurfaceCreateInfoKHR win32SurfaceCreateInfo = {
			.sType     = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
			.pNext     = 0,
			.flags     = 0,
			.hinstance = (HINSTANCE)wndPlatformDesc->hinstance,
			.hwnd      = (HWND)wndPlatformDesc->hwnd,
		};
		Gpu_CheckVk(vkCreateWin32SurfaceKHR(gpu_vkInstance, &win32SurfaceCreateInfo, gpu_vkAllocationCallbacks, &gpu_vkSurface));
	#endif	// Platform

	return Ok();
}

//-------------------------------------------------------------------------------------------------

static Res<> Gpu_InitDevice() {
	constexpr char const* RequiredDeviceExts[] = {
		"VK_KHR_swapchain",
	};

	U32 n = 0;
	Array<VkPhysicalDevice> vkPhysicalDevices(gpu_tempMem);
	Gpu_CheckVk(vkEnumeratePhysicalDevices(gpu_vkInstance, &n, nullptr));
	Gpu_CheckVk(vkEnumeratePhysicalDevices(gpu_vkInstance, &n, vkPhysicalDevices.Resize(n)));

	// TODO: support cvar for selecting explicit device

	Logf("%u physical devices:", vkPhysicalDevices.len);
	gpu_physicalDevices = Span_Make<Gpu_PhysicalDevice>(gpu_permMem, vkPhysicalDevices.len);

	U32 bestScore = 0;
	for (U64 i = 0; i < gpu_physicalDevices.len; i++) {
		Gpu_PhysicalDevice* pd = &gpu_physicalDevices[i];
		pd->vkPhysicalDevice = vkPhysicalDevices[i];
		pd->vkPhysicalDeviceProperties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
		vkGetPhysicalDeviceProperties2(vkPhysicalDevices[i], &pd->vkPhysicalDeviceProperties2);
		Logf(
			"%s: apiVersion=%s, driverVersion=%s, vendorID=%s, deviceId=%s, deviceType=%s",
			pd->vkPhysicalDeviceProperties2.properties.deviceName,
			Gpu_VersionStr(gpu_tempMem, pd->vkPhysicalDeviceProperties2.properties.apiVersion),
			Gpu_VersionStr(gpu_tempMem, pd->vkPhysicalDeviceProperties2.properties.driverVersion),
			pd->vkPhysicalDeviceProperties2.properties.vendorID,
			pd->vkPhysicalDeviceProperties2.properties.deviceID,
			Gpu_PhysicalDeviceTypeStr(pd->vkPhysicalDeviceProperties2.properties.deviceType)
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
			Logf("  Rejecting device: need Vulkan 1.3: apiVersion=%s", Gpu_VersionStr(gpu_tempMem, pd->vkPhysicalDeviceProperties2.properties.apiVersion));
			score = 0;
		}

		pd->vkPhysicalDeviceFeatures2.sType        = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
		pd->vkPhysicalDeviceFeatures2.pNext        = &pd->vkPhysicalDeviceVulkan12Features;
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
		Logf("  %u memory types:", pd->vkPhysicalDeviceMemoryProperties.memoryTypeCount);
		for (U64 j = 0; j < pd->vkPhysicalDeviceMemoryProperties.memoryTypeCount; j++) {
			VkMemoryType const mt = pd->vkPhysicalDeviceMemoryProperties.memoryTypes[j];
			Logf("    [%u] heapIndex=%s, flags=%s", j, mt.heapIndex, Gpu_MemoryPropertyFlagsStr(gpu_tempMem, mt.propertyFlags));
		}
		Logf("  %u memory heaps:", pd->vkPhysicalDeviceMemoryProperties.memoryHeapCount);
		for (U64 j = 0; j < pd->vkPhysicalDeviceMemoryProperties.memoryHeapCount; j++) {
			VkMemoryHeap const mh = pd->vkPhysicalDeviceMemoryProperties.memoryHeaps[j];
			Logf("    [%u] size=%s, flags=%s", j, Gpu_SizeStr(gpu_tempMem, mh.size), Gpu_MemoryHeapFlagsStr(gpu_tempMem, mh.flags));
		}

		Array<VkQueueFamilyProperties> vkQueueFamilyProperties(gpu_tempMem);
		vkGetPhysicalDeviceQueueFamilyProperties(pd->vkPhysicalDevice, &n, nullptr);
		vkGetPhysicalDeviceQueueFamilyProperties(pd->vkPhysicalDevice, &n, vkQueueFamilyProperties.Resize(n));

		Gpu_CheckVk(vkEnumerateDeviceExtensionProperties(pd->vkPhysicalDevice, 0, &n, 0));
		pd->vkExtensionProperties = Span_Make<VkExtensionProperties>(gpu_permMem, n);
		Gpu_CheckVk(vkEnumerateDeviceExtensionProperties(pd->vkPhysicalDevice, 0, &n, pd->vkExtensionProperties.data));

		Array<char> extensionsStr(gpu_tempMem);
		for (U64 j = 0; j < pd->vkExtensionProperties.len; j++) {
			Fmt_Printf(&extensionsStr, "%s(specVersion=%s), ", pd->vkExtensionProperties[j].extensionName, Gpu_VersionStr(gpu_tempMem, pd->vkExtensionProperties[j].specVersion));
		}
		if (extensionsStr.len >= 2) {
			extensionsStr.len -= 2;
		}
		Logf("  %u device extensions: %s",  pd->vkExtensionProperties.len, Str(extensionsStr.data, extensionsStr.len));
		for (U64 j = 0; j < LenOf(RequiredDeviceExts); j++) {
			Bool found = false;
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

		Gpu_CheckVk(vkGetPhysicalDeviceSurfaceFormatsKHR(pd->vkPhysicalDevice, gpu_vkSurface, &n, 0));
		pd->vkSurfaceFormats = Span_Make<VkSurfaceFormatKHR>(gpu_permMem, n);
		Gpu_CheckVk(vkGetPhysicalDeviceSurfaceFormatsKHR(pd->vkPhysicalDevice, gpu_vkSurface, &n, pd->vkSurfaceFormats.data));
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
			Logf("    %s, %s", Gpu_FormatStr(pd->vkSurfaceFormats[j].format), Gpu_ColorSpaceStr(pd->vkSurfaceFormats[j].colorSpace));
		}
		Logf("  Selected surface format: %s/%s", Gpu_FormatStr(pd->vkSwapchainFormat), Gpu_ColorSpaceStr(pd->vkSwapchainColorSpace));

		Gpu_CheckVk(vkGetPhysicalDeviceSurfacePresentModesKHR(pd->vkPhysicalDevice, gpu_vkSurface, &n, 0));
		pd->vkPresentModes = Span_Make<VkPresentModeKHR>(gpu_permMem, n);
		Gpu_CheckVk(vkGetPhysicalDeviceSurfacePresentModesKHR(pd->vkPhysicalDevice, gpu_vkSurface, &n, pd->vkPresentModes.data));
		Logf("  %u present modes:", pd->vkPresentModes.len);
		for (U64 j = 0; j < pd->vkPresentModes.len; j++) {
			Logf("    %s", Gpu_PresentModeStr(pd->vkPresentModes[j]));
		}
		if (pd->vkPresentModes.len == 0) {
			Logf("  Rejecting device: no present modes");
			score = 0;
		}

		pd->queueFamilies = Span_Make<Gpu_QueueFamily>(gpu_permMem, vkQueueFamilyProperties.len);
		Logf("  %u queue families:", pd->queueFamilies.len);
		for (U32 j = 0; j < (U32)pd->queueFamilies.len; j++) {
			VkQueueFamilyProperties const* props = &pd->queueFamilies[j].vkQueueFamilyProperties;
			VkBool32 supportsPresent = VK_FALSE;
			Gpu_CheckVk(vkGetPhysicalDeviceSurfaceSupportKHR(pd->vkPhysicalDevice, (U32)j, gpu_vkSurface, &supportsPresent));
			pd->queueFamilies[j].vkQueueFamilyProperties = vkQueueFamilyProperties[j];
			pd->queueFamilies[j].supportsPresent         = (supportsPresent == VK_TRUE);
			Logf("    [%u] count=%u, flags=%s, supportsPresent=%t", j, props->queueCount, Gpu_QueueFlagsStr(gpu_tempMem, props->queueFlags), pd->queueFamilies[j].supportsPresent);
			VkQueueFlags const flags = pd->queueFamilies[j].vkQueueFamilyProperties.queueFlags;
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
			gpu_physicalDevice = pd;
			bestScore = score;
		}
	}

	if (bestScore == 0) {
		return Err_NoDevice();
	}
	Logf("Selected physical device '%s' with score=%u", gpu_physicalDevice->vkPhysicalDeviceProperties2.properties.deviceName, bestScore);

	F32 queuePriority = 1.0f;
	VkDeviceQueueCreateInfo const vkDeviceQueueCreateInfo = {
		.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
		.pNext            = 0,
		.flags            = 0,
		.queueFamilyIndex = gpu_physicalDevice->queueFamily,
		.queueCount       = 1,
		.pQueuePriorities = &queuePriority,
	};

	// TODO: possibly create more queues? one per type?
	VkDeviceCreateInfo const vkDeviceCreateInfo = {
		.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
		.pNext                   = &gpu_physicalDevice->vkPhysicalDeviceFeatures2,
		.flags                   = 0,
		.queueCreateInfoCount    = 1,
		.pQueueCreateInfos       = &vkDeviceQueueCreateInfo,
		.enabledLayerCount       = 0,
		.ppEnabledLayerNames     = 0,
		.enabledExtensionCount   = LenOf(RequiredDeviceExts),
		.ppEnabledExtensionNames = RequiredDeviceExts,
		.pEnabledFeatures        = 0,
	};

	Gpu_CheckVk(vkCreateDevice(gpu_physicalDevice->vkPhysicalDevice, &vkDeviceCreateInfo, gpu_vkAllocationCallbacks, &gpu_vkDevice));
	Gpu_VkName(gpu_vkDevice);

	Gpu_LoadDeviceFns(gpu_vkDevice);

	vkGetDeviceQueue(gpu_vkDevice, gpu_physicalDevice->queueFamily, 0, &gpu_vkQueue);

	return Ok();
}

//-------------------------------------------------------------------------------------------------

static Res<> Gpu_InitSwapchain(U32 width, U32 height) {
	VkSurfaceCapabilitiesKHR vkSurfaceCapabilities;
	Gpu_CheckVk(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(gpu_physicalDevice->vkPhysicalDevice, gpu_vkSurface, &vkSurfaceCapabilities));
	
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
	for (U64 i = 0; i < gpu_physicalDevice->vkPresentModes.len; i++) {
		if (gpu_physicalDevice->vkPresentModes[i] == VK_PRESENT_MODE_MAILBOX_KHR) {
			presentMode = VK_PRESENT_MODE_MAILBOX_KHR;
		}
	}
	Logf("Selected swapchain present mode: %s", Gpu_PresentModeStr(presentMode));

	VkSwapchainCreateInfoKHR const vkSwapchainCreateInfoKHR = {
		.sType                 = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
		.pNext                 = 0,
		.flags                 = 0,
		.surface               = gpu_vkSurface,
		.minImageCount         = imageCount,
		.imageFormat           = gpu_physicalDevice->vkSwapchainFormat,
		.imageColorSpace       = gpu_physicalDevice->vkSwapchainColorSpace,
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
	Gpu_CheckVk(vkCreateSwapchainKHR(gpu_vkDevice, &vkSwapchainCreateInfoKHR, gpu_vkAllocationCallbacks, &gpu_vkSwapchain));

	U32 n = 0;
	Gpu_CheckVk(vkGetSwapchainImagesKHR(gpu_vkDevice, gpu_vkSwapchain, &n, 0));
	VkImage* const vkSwapchainImages = (VkImage*)Mem_Alloc(gpu_tempMem, n * sizeof(VkImage));
	Gpu_CheckVk(vkGetSwapchainImagesKHR(gpu_vkDevice, gpu_vkSwapchain, &n, vkSwapchainImages));

	gpu_swapchainImages = Span_Make<Gpu_Image>(gpu_permMem, n);
	for (U64 i = 0; i < n; i++) {
		Gpu_VkNamef(vkSwapchainImages[i], "vkSwapchainImages[{}]", i);

		VkImageViewCreateInfo const vkImageViewCreateInfo = {
			.sType              = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			.pNext              = 0,
			.flags              = 0,
			.image              = vkSwapchainImages[i],
			.viewType           = VK_IMAGE_VIEW_TYPE_2D,
			.format             = gpu_physicalDevice->vkSwapchainFormat,
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
		Gpu_CheckVk(vkCreateImageView(gpu_vkDevice, &vkImageViewCreateInfo, gpu_vkAllocationCallbacks, &vkSwapchainImageView));
		Gpu_VkNamef(vkSwapchainImageView, "vkSwapchainImageView#{}", i);

		auto* const entry = gpu_imageObjs.Alloc();
		gpu_swapchainImages[i] = entry->Handle();
		entry->obj.vkImage       = vkSwapchainImages[i];
		entry->obj.vkImageView   = vkSwapchainImageView;
		entry->obj.allocation    = {};
		entry->obj.width         = vkSwapchainExtent.width;
		entry->obj.height        = vkSwapchainExtent.height;
		entry->obj.vkFormat      = gpu_physicalDevice->vkSwapchainFormat;
	}

	vkDeviceWaitIdle(gpu_vkDevice);

	return Ok();
}

//----------------------------------------------------------------------------------------------

static Res<> Gpu_InitFrame() {
	VkCommandPoolCreateInfo const vkCommandPoolCreateInfo = {
		.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
		.pNext            = 0,
		.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
		.queueFamilyIndex = gpu_physicalDevice->queueFamily,
	};
	Gpu_CheckVk(vkCreateCommandPool(gpu_vkDevice, &vkCommandPoolCreateInfo, gpu_vkAllocationCallbacks, &gpu_vkFrameCommandPool));
	Gpu_VkName(gpu_vkFrameCommandPool);

	VkCommandBufferAllocateInfo const vkCommandBufferAllocateInfo = {
		.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.pNext              = 0,
		.commandPool        = gpu_vkFrameCommandPool,
		.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		.commandBufferCount = Gpu_MaxFrames,
	};
	Gpu_CheckVk(vkAllocateCommandBuffers(gpu_vkDevice, &vkCommandBufferAllocateInfo, gpu_vkFrameCommandBuffers));
	for (U64 i = 0; i < Gpu_MaxFrames; i++) {
		Gpu_VkNamef(gpu_vkFrameCommandBuffers[i], "gpu_vkFrameCommandBuffers[%u]", i);
	}

	VkSemaphoreTypeCreateInfo const vkSemaphoreTypeCreateInfo = {
		.sType         = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
		.pNext         = 0,
		.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE,
		.initialValue  = Gpu_MaxFrames - 1,
	};
	{
		VkSemaphoreCreateInfo const vkSemaphoreCreateInfo = {
			.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
			.pNext = &vkSemaphoreTypeCreateInfo,
			.flags = 0,
		};
		Gpu_CheckVk(vkCreateSemaphore(gpu_vkDevice, &vkSemaphoreCreateInfo, gpu_vkAllocationCallbacks, &gpu_vkFrameTimelineSemaphore));
		Gpu_VkName(gpu_vkFrameTimelineSemaphore);
	}

	for (U64 i = 0; i < Gpu_MaxFrames; i++) {
		VkSemaphoreCreateInfo const vkSemaphoreCreateInfo = {
			.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
			.pNext = 0,
			.flags = 0,
		};
		Gpu_CheckVk(vkCreateSemaphore(gpu_vkDevice, &vkSemaphoreCreateInfo, gpu_vkAllocationCallbacks, &gpu_vkFrameImageAcquiredSemaphores[i]));
		Gpu_CheckVk(vkCreateSemaphore(gpu_vkDevice, &vkSemaphoreCreateInfo, gpu_vkAllocationCallbacks, &gpu_vkFrameSubmitCompleteSemaphores[i]));
		Gpu_VkNamef(gpu_vkFrameImageAcquiredSemaphores[i],  "gpu_vkFrameImageAcquiredSemaphores[%u]",  i);
		Gpu_VkNamef(gpu_vkFrameSubmitCompleteSemaphores[i], "gpu_vkFrameSubmitCompleteSemaphores[%u]", i);
	}

	if (Res<> r = Gpu_CreateBufferImpl(
		Gpu_PerFrameStagingBufferSize * Gpu_MaxFrames,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		0,
		VK_MEMORY_PROPERTY_HOST_CACHED_BIT | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		0
	).To(gpu_frameStagingBuffer); !r) {
		return r.err;
	}
	Gpu_VkName(gpu_frameStagingBuffer.vkBuffer);

	U8* ptr = 0;
	Gpu_CheckVk(vkMapMemory(gpu_vkDevice, gpu_frameStagingBuffer.allocation.vkDeviceMemory, 0, Gpu_PerFrameStagingBufferSize * Gpu_MaxFrames, 0, (void**)&ptr));

	for (U32 i = 0; i < Gpu_MaxFrames; i++) {
		gpu_frameStagingBufferPtrs[i] = ptr + (i * Gpu_PerFrameStagingBufferSize);
		gpu_frameStagingBufferUsed[i] = 0;
	}

	gpu_frame    = 0;
	gpu_frameIdx = 0;

	return Ok();
}

//----------------------------------------------------------------------------------------------

static Res<> Gpu_InitImmediate() {
	VkCommandPoolCreateInfo const vkCommandPoolCreateInfo = {
		.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
		.pNext            = 0,
		.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
		.queueFamilyIndex = gpu_physicalDevice->queueFamily,
	};
	Gpu_CheckVk(vkCreateCommandPool(gpu_vkDevice, &vkCommandPoolCreateInfo, gpu_vkAllocationCallbacks, &gpu_vkImmediateCommandPool));
	Gpu_VkName(gpu_vkImmediateCommandPool);

	VkCommandBufferAllocateInfo const vkCommandBufferAllocateInfo = {
		.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.pNext              = 0,
		.commandPool        = gpu_vkImmediateCommandPool,
		.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		.commandBufferCount = 1,
	};
	Gpu_CheckVk(vkAllocateCommandBuffers(gpu_vkDevice, &vkCommandBufferAllocateInfo, &gpu_vkImmediateCommandBuffer));
	Gpu_VkName(gpu_vkImmediateCommandBuffer);
	gpu_vkImmediateCommandBufferBegun = false;

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
	Gpu_CheckVk(vkCreateSemaphore(gpu_vkDevice, &vkSemaphoreCreateInfo, gpu_vkAllocationCallbacks, &gpu_vkImmediateTimelineSemaphore));
	Gpu_VkName(gpu_vkImmediateTimelineSemaphore);
	gpu_immediateTimeline = 0;

	if (Res<> r = Gpu_CreateBufferImpl(
		Gpu_ImmediateStagingBufferSize,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		0,
		VK_MEMORY_PROPERTY_HOST_CACHED_BIT | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		0
	).To(gpu_immediateStagingBuffer); !r) {
		return r.err;
	}
	Gpu_VkName(gpu_immediateStagingBuffer.vkBuffer);

	Gpu_CheckVk(vkMapMemory(gpu_vkDevice, gpu_immediateStagingBuffer.allocation.vkDeviceMemory, 0, Gpu_ImmediateStagingBufferSize, 0, (void**)&gpu_immediateStagingBufferPtr));

	gpu_immediateStagingBufferUsed = 0;

	return Ok();
}

//-------------------------------------------------------------------------------------------------

static void Gpu_AddBindlessSampler(VkSampler vkSampler) {
	Assert(gpu_vkBindlessSamplersLen < Gpu_MaxBindlessSamplers);
	VkDescriptorImageInfo const vkDescriptorImageInfo = {
		.sampler     = vkSampler,
		.imageView   = 0,
		.imageLayout = VK_IMAGE_LAYOUT_GENERAL,
	};
	VkWriteDescriptorSet const vkWriteDescriptorSet = {
		.sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		.pNext            = 0,
		.dstSet           = gpu_vkBindlessDescriptorSet,
		.dstBinding       = 1,
		.dstArrayElement  = gpu_vkBindlessSamplersLen,
		.descriptorCount  = 1,
		.descriptorType   = VK_DESCRIPTOR_TYPE_SAMPLER,
		.pImageInfo       = &vkDescriptorImageInfo,
		.pBufferInfo      = 0,
		.pTexelBufferView = 0,
	};
	vkUpdateDescriptorSets(gpu_vkDevice, 1, &vkWriteDescriptorSet, 0, 0);
	gpu_vkBindlessSamplers[gpu_vkBindlessSamplersLen] = vkSampler;
	gpu_vkBindlessSamplersLen++;
}

static Res<> Gpu_InitBindless() {
	constexpr VkDescriptorPoolSize vkDescriptorPoolSizes[] = {
		{ .type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, .descriptorCount = Gpu_MaxBindlessSampledImages },
		{ .type = VK_DESCRIPTOR_TYPE_SAMPLER,       .descriptorCount = Gpu_MaxBindlessSamplers },
	};
	VkDescriptorPoolCreateInfo const vkDescriptorPoolCreateInfo = {
		.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
		.pNext         = 0,
		.flags         = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT,
		.maxSets       = Gpu_MaxBindlessDescriptorSets,
		.poolSizeCount = LenOf(vkDescriptorPoolSizes),
		.pPoolSizes    = vkDescriptorPoolSizes,
	};
	Gpu_CheckVk(vkCreateDescriptorPool(gpu_vkDevice, &vkDescriptorPoolCreateInfo, gpu_vkAllocationCallbacks, &gpu_vkDescriptorPool));
	Gpu_VkName(gpu_vkDescriptorPool);

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
			.descriptorCount    = Gpu_MaxBindlessSampledImages,
			.stageFlags         = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
			.pImmutableSamplers = 0,
		},
		{
			.binding            = 1,
			.descriptorType     = VK_DESCRIPTOR_TYPE_SAMPLER,
			.descriptorCount    = Gpu_MaxBindlessSamplers,
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
	Gpu_CheckVk(vkCreateDescriptorSetLayout(gpu_vkDevice, &vkDescriptorSetLayoutCreateInfo, gpu_vkAllocationCallbacks, &gpu_vkBindlessDescriptorSetLayout));
	Gpu_VkName(gpu_vkBindlessDescriptorSetLayout);

	VkDescriptorSetAllocateInfo const vkDescriptorSetAllocateInfo = {
		.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
		.pNext              = 0,
		.descriptorPool     = gpu_vkDescriptorPool,
		.descriptorSetCount = 1,
		.pSetLayouts        = &gpu_vkBindlessDescriptorSetLayout,
	};
	Gpu_CheckVk(vkAllocateDescriptorSets(gpu_vkDevice, &vkDescriptorSetAllocateInfo, &gpu_vkBindlessDescriptorSet));
	Gpu_VkName(gpu_vkBindlessDescriptorSet);

	gpu_nextBindlessDescriptorIdx = 1;	// reserve index 0 for invalid

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
	Gpu_CheckVk(vkCreateSampler(gpu_vkDevice, &vkSamplerCreateInfo, gpu_vkAllocationCallbacks, &vkNearestSampler));
	Gpu_VkName(vkNearestSampler);
	Gpu_AddBindlessSampler(vkNearestSampler);

	vkSamplerCreateInfo.magFilter        = VK_FILTER_LINEAR;
	vkSamplerCreateInfo.minFilter        = VK_FILTER_LINEAR;
	vkSamplerCreateInfo.mipmapMode       = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	vkSamplerCreateInfo.anisotropyEnable = VK_TRUE;
	vkSamplerCreateInfo.maxAnisotropy    = Gpu_MaxAnisotropy;
	VkSampler vkLinearSampler = VK_NULL_HANDLE;
	Gpu_CheckVk(vkCreateSampler(gpu_vkDevice, &vkSamplerCreateInfo, gpu_vkAllocationCallbacks, &vkLinearSampler));
	Gpu_VkName(vkLinearSampler);
	Gpu_AddBindlessSampler(vkLinearSampler);

	return Ok();
}

//-------------------------------------------------------------------------------------------------

Res<> Gpu_Init(Gpu_InitDesc const* initDesc) {
	gpu_permMem = initDesc->permMem;
	gpu_tempMem = initDesc->tempMem;
	Sys_InitMutex(&gpu_mutex);

	Try(Gpu_InitInstance());
	Try(Gpu_InitSurface(initDesc->wndPlatformDesc));
	Try(Gpu_InitDevice());
	Try(Gpu_InitSwapchain(initDesc->windowWidth, initDesc->windowHeight));
	Try(Gpu_InitFrame());
	Try(Gpu_InitImmediate());
	Try(Gpu_InitBindless());

	VkCommandBufferBeginInfo const vkCommandBufferBeginInfo = {
		.sType            = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.pNext            = 0,
		.flags            = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
		.pInheritanceInfo = 0,
	};
	vkBeginCommandBuffer(gpu_vkImmediateCommandBuffer, &vkCommandBufferBeginInfo);
	gpu_vkImmediateCommandBufferBegun = true;

	for (U64 i = 0; i < gpu_swapchainImages.len; i++) {
		Gpu_ImageMemoryBarrier(
			gpu_vkImmediateCommandBuffer,
			gpu_imageObjs.Get(gpu_swapchainImages[i])->vkImage,
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

#define Gpu_DestroyVk(h, DestroyFn) \
	if (h != VK_NULL_HANDLE) { \
		DestroyFn(gpu_vkDevice, h, gpu_vkAllocationCallbacks); \
		h = VK_NULL_HANDLE; \
	}

#define Gpu_DestroyVkSpan(a, DestroyFn) \
	for (U64 i = 0; i < a.len; i++) { \
		if (a[i] != VK_NULL_HANDLE) { \
			DestroyFn(gpu_vkDevice, a[i], gpu_vkAllocationCallbacks); \
		} \
	} \
	a.len = 0;

#define DestroyVkCArray(a, DestroyFn) \
	for (U64 i = 0; i < LenOf(a); i++) { \
		if (a[i] != VK_NULL_HANDLE) { \
			DestroyFn(gpu_vkDevice, a[i], gpu_vkAllocationCallbacks); \
		} \
	} \

//--------------------------------------------------------------------------------------------------

void Shutdown() {

	for (U64 i = 0; i < gpu_swapchainImages.len; i++) {
		Gpu_ImageObj* const imageObj = gpu_imageObjs.Get(gpu_swapchainImages[i]);
		Gpu_DestroyVk(imageObj->vkImageView, vkDestroyImageView);
		gpu_imageObjs.Free(gpu_swapchainImages[i]);
	}
	gpu_swapchainImages.len = 0;
	Gpu_DestroyVk(gpu_vkSwapchain, vkDestroySwapchainKHR);

	if (gpu_vkFrameCommandPool != VK_NULL_HANDLE) {
		vkFreeCommandBuffers(gpu_vkDevice, gpu_vkFrameCommandPool, Gpu_MaxFrames, gpu_vkFrameCommandBuffers);
	}
	Gpu_DestroyVk(gpu_vkFrameCommandPool, vkDestroyCommandPool);
	Gpu_DestroyVk(gpu_frameStagingBuffer.vkBuffer, vkDestroyBuffer);
	Gpu_DestroyVk(gpu_frameStagingBuffer.allocation.vkDeviceMemory, vkFreeMemory);
	Gpu_DestroyVk(gpu_vkFrameTimelineSemaphore, vkDestroySemaphore);
	DestroyVkCArray(gpu_vkFrameImageAcquiredSemaphores, vkDestroySemaphore);
	DestroyVkCArray(gpu_vkFrameSubmitCompleteSemaphores, vkDestroySemaphore);

	if (gpu_vkImmediateCommandPool != VK_NULL_HANDLE) {
		vkFreeCommandBuffers(gpu_vkDevice, gpu_vkImmediateCommandPool, 1, &gpu_vkImmediateCommandBuffer);
	}
	Gpu_DestroyVk(gpu_vkImmediateCommandPool, vkDestroyCommandPool);
	Gpu_DestroyVk(gpu_vkImmediateTimelineSemaphore, vkDestroySemaphore);
	Gpu_DestroyVk(gpu_immediateStagingBuffer.vkBuffer, vkDestroyBuffer);
	Gpu_DestroyVk(gpu_immediateStagingBuffer.allocation.vkDeviceMemory, vkFreeMemory);

	DestroyVkCArray(gpu_vkBindlessSamplers, vkDestroySampler);
	gpu_vkBindlessSamplersLen = 0;
	Gpu_DestroyVk(gpu_vkBindlessDescriptorSetLayout, vkDestroyDescriptorSetLayout);
	Gpu_DestroyVk(gpu_vkDescriptorPool, vkDestroyDescriptorPool);

	if (gpu_vkDevice != VK_NULL_HANDLE) { vkDestroyDevice(gpu_vkDevice, gpu_vkAllocationCallbacks); gpu_vkDevice = VK_NULL_HANDLE; }
	if (gpu_vkSurface != VK_NULL_HANDLE) { vkDestroySurfaceKHR(gpu_vkInstance, gpu_vkSurface, gpu_vkAllocationCallbacks); gpu_vkSurface = VK_NULL_HANDLE; }
	if (gpu_vkDebugUtilsMessenger != VK_NULL_HANDLE) { vkDestroyDebugUtilsMessengerEXT(gpu_vkInstance, gpu_vkDebugUtilsMessenger, gpu_vkAllocationCallbacks); vkDebugUtilsMessenger = VK_NULL_HANDLE; }
	if (gpu_vkInstance != VK_NULL_HANDLE) { vkDestroyInstance(gpu_vkInstance, gpu_vkAllocationCallbacks); gpu_vkInstance = VK_NULL_HANDLE; }
	Sys_ShutdownMutex(&gpu_mutex);
}

//----------------------------------------------------------------------------------------------

Gpu_ImageFormat GetSwapchainImageFormat() {
	return Gpu_VkFormatToImageFormat(gpu_physicalDevice->vkSwapchainFormat);
}

//----------------------------------------------------------------------------------------------

Res<> Gpu_RecreateSwapchain(U32 width, U32 height) {
	vkDeviceWaitIdle(gpu_vkDevice);

	for (U64 i = 0; i < gpu_swapchainImages.len; i++) {
		Gpu_ImageObj* const imageObj = gpu_imageObjs.Get(gpu_swapchainImages[i]);
		Gpu_DestroyVk(imageObj->vkImageView, vkDestroyImageView);
		gpu_imageObjs.Free(gpu_swapchainImages[i]);
	}
	gpu_swapchainImages.len = 0;
	Gpu_DestroyVk(gpu_vkSwapchain, vkDestroySwapchainKHR);

	Res<> r = Gpu_InitSwapchain(width, height);

	vkDeviceWaitIdle(gpu_vkDevice);

	return r;
}

//----------------------------------------------------------------------------------------------

static U32 Gpu_SelectMemTypeIdx(
	U32                   memTypeBits,
	VkMemoryPropertyFlags vkMemoryPropertyFlagsNeed,
	VkMemoryPropertyFlags vkMemoryPropertyFlagsWant,
	VkMemoryPropertyFlags vkMemoryPropertyFlagsAvoid
) {
	U32 minCost = U32Max;
	U32 selectedMemTypeIdx = VK_MAX_MEMORY_TYPES;
	for (U32 i = 0, memTypeIdx = 1; i < gpu_physicalDevice->vkPhysicalDeviceMemoryProperties.memoryTypeCount; i++, memTypeIdx <<= 1) {
		if (!(memTypeBits & memTypeIdx)) {
			continue;
		}
		VkMemoryPropertyFlags const typeFlags = gpu_physicalDevice->vkPhysicalDeviceMemoryProperties.memoryTypes[i].propertyFlags;
		if ((typeFlags & vkMemoryPropertyFlagsNeed) != vkMemoryPropertyFlagsNeed) {
			continue;
		}
		U32 const wantCost  = PopCount32(vkMemoryPropertyFlagsWant  & ~typeFlags);
		U32 const avoidCost = PopCount32(vkMemoryPropertyFlagsAvoid & typeFlags);
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
static Res<Gpu_Allocation> Gpu_AllocVkMem(
	VkMemoryRequirements  vkMemoryRequirements,
	VkMemoryPropertyFlags vkMemoryPropertyFlagsNeed,
	VkMemoryPropertyFlags vkMemoryPropertyFlagsWant,
	VkMemoryPropertyFlags vkMemoryPropertyFlagsAvoid,
	VkMemoryAllocateFlags vkMemoryAllocateFlags
) {
	U32 const memTypeIdx = Gpu_SelectMemTypeIdx(vkMemoryRequirements.memoryTypeBits, vkMemoryPropertyFlagsNeed, vkMemoryPropertyFlagsWant, vkMemoryPropertyFlagsAvoid);
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
	if (VkResult vkResult = vkAllocateMemory(gpu_vkDevice, &vkMemoryAllocateInfo, gpu_vkAllocationCallbacks, &vkDeviceMemory); vkResult != VK_SUCCESS) {
		return Err_Vk(vkResult, "vkAllocateMemory", "memTypeIdx", memTypeIdx, "size", vkMemoryRequirements.size);
	}

	return Gpu_Allocation {
		.vkDeviceMemory = vkDeviceMemory,
		.memTypeIdx     = memTypeIdx,
		.size           = vkMemoryRequirements.size,
	};
}

//--------------------------------------------------------------------------------------------------

#pragma warning (disable: 4505)
static Res<Gpu_BufferObj> Gpu_CreateBufferImpl(
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
		.pQueueFamilyIndices   = &gpu_physicalDevice->queueFamily,
	};
	VkBuffer vkBuffer = VK_NULL_HANDLE;
	Gpu_CheckVk(vkCreateBuffer(gpu_vkDevice, &vkBufferCreateInfo, gpu_vkAllocationCallbacks, &vkBuffer));

	VkMemoryRequirements vkMemoryRequirements;
	vkGetBufferMemoryRequirements(gpu_vkDevice, vkBuffer, &vkMemoryRequirements);

	Gpu_Allocation allocation;
	if (Res<> r = Gpu_AllocVkMem(
		vkMemoryRequirements,
		vkMemoryPropertyFlagsNeed,
		vkMemoryPropertyFlagsWant,
		vkMemoryPropertyFlagsAvoid,
		vkMemoryAllocateFlags
	  ).To(allocation); !r) {
		vkDestroyBuffer(gpu_vkDevice, vkBuffer, gpu_vkAllocationCallbacks);
		return r.err;
	  }

	if (VkResult const r = vkBindBufferMemory(gpu_vkDevice, vkBuffer, allocation.vkDeviceMemory, 0); r != VK_SUCCESS) {
		vkDestroyBuffer(gpu_vkDevice, vkBuffer, gpu_vkAllocationCallbacks);
		vkFreeMemory(gpu_vkDevice, allocation.vkDeviceMemory, gpu_vkAllocationCallbacks);
		return Err_Vk(r, "vkBindBufferMemory");
	}

	U64 addr = 0;
	if (vkBufferUsageFlags & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT) {
		VkBufferDeviceAddressInfo const vkBufferDeviceAddressInfo = {
			.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
			.pNext = 0,
			.buffer = vkBuffer,
		};
		addr = vkGetBufferDeviceAddress(gpu_vkDevice, &vkBufferDeviceAddressInfo);
	}

	return Gpu_BufferObj {
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

Res<Gpu_Buffer> Gpu_CreateBuffer(U64 size, Gpu_BufferUsage::Flags bufferUsageFlags) {
	VkBufferUsageFlags vkBufferUsageFlags = 0;
	if (bufferUsageFlags & Gpu_BufferUsage::Storage)      { vkBufferUsageFlags |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT; }
	if (bufferUsageFlags & Gpu_BufferUsage::Index)        { vkBufferUsageFlags |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT; }
	if (bufferUsageFlags & Gpu_BufferUsage::DrawIndirect) { vkBufferUsageFlags |= VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT; }
	if (bufferUsageFlags & Gpu_BufferUsage::Copy)         { vkBufferUsageFlags |= VK_BUFFER_USAGE_TRANSFER_DST_BIT; }

	VkMemoryAllocateFlags vkMemoryAllocateFlags = 0;
	if (bufferUsageFlags & Gpu_BufferUsage::Addr) {
		vkBufferUsageFlags    |= VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
		vkMemoryAllocateFlags |= VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;
	}

	Gpu_BufferObj bufferObj;
	Try(Gpu_CreateBufferImpl(
		size,
		vkBufferUsageFlags,
		0,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		0,
		vkMemoryAllocateFlags
	).To(bufferObj));

	Gpu_BufferPool::Entry* const entry = gpu_bufferObjs.Alloc();
	entry->obj = bufferObj;
	return entry->Handle();
}

//----------------------------------------------------------------------------------------------

void Gpu_DestroyBuffer(Gpu_Buffer buffer) {
	if (buffer.handle) {
		Gpu_BufferObj* const bufferObj = gpu_bufferObjs.Get(buffer);
		Gpu_DestroyVk(bufferObj->vkBuffer, vkDestroyBuffer);
		Gpu_DestroyVk(bufferObj->allocation.vkDeviceMemory, vkFreeMemory);
		memset(bufferObj, 0, sizeof(*bufferObj));
		gpu_bufferObjs.Free(buffer);
	}
}

//----------------------------------------------------------------------------------------------

U64 Gpu_GetBufferAddr(Gpu_Buffer buffer) {
	Gpu_BufferObj* const bufferObj = gpu_bufferObjs.Get(buffer);
	Assert(bufferObj->addr);
	return bufferObj->addr;
}

//----------------------------------------------------------------------------------------------

static void Gpu_BufferBarrierImpl(
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

Res<Gpu_Image> Gpu_CreateImage(U32 width, U32 height, Gpu_ImageFormat format, Gpu_ImageUsage::Flags imageUsageFlags) {
	// TODO: check format properties optimal tiling supports the requested format/feature
	VkFormat const vkFormat = Gpu_ImageFormatToVkFormat(format);

	VkImageUsageFlags vkImageUsageFlags = 0;
	if (imageUsageFlags & Gpu_ImageUsage::Sampled) { vkImageUsageFlags |= VK_IMAGE_USAGE_SAMPLED_BIT; }
	if (imageUsageFlags & Gpu_ImageUsage::Color)   { vkImageUsageFlags |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT; }
	if (imageUsageFlags & Gpu_ImageUsage::Depth)   { vkImageUsageFlags |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT; }
	if (imageUsageFlags & Gpu_ImageUsage::Copy)    { vkImageUsageFlags |= VK_IMAGE_USAGE_TRANSFER_DST_BIT; }

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
	Gpu_CheckVk(vkCreateImage(gpu_vkDevice, &vkImageCreateInfo, gpu_vkAllocationCallbacks, &vkImage));

	VkMemoryRequirements vkMemoryRequirements;
	vkGetImageMemoryRequirements(gpu_vkDevice, vkImage, &vkMemoryRequirements);

	Gpu_Allocation allocation;
	if (Res<> r = Gpu_AllocVkMem(vkMemoryRequirements, 0, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 0, 0).To(allocation); !r) {
		vkDestroyImage(gpu_vkDevice, vkImage, gpu_vkAllocationCallbacks);
		return r.err;
	}
	
	if (VkResult const r = vkBindImageMemory(gpu_vkDevice, vkImage, allocation.vkDeviceMemory, 0); r != VK_SUCCESS) {
		vkDestroyImage(gpu_vkDevice, vkImage, gpu_vkAllocationCallbacks);
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
			.aspectMask     = (VkImageAspectFlags)(Gpu_IsDepthFormat(vkFormat) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT),
			.baseMipLevel   = 0,
			.levelCount     = VK_REMAINING_MIP_LEVELS,
			.baseArrayLayer = 0,
			.layerCount     = VK_REMAINING_ARRAY_LAYERS,
		},
	};
	VkImageView vkImageView = VK_NULL_HANDLE;
	if (VkResult r = vkCreateImageView(gpu_vkDevice, &vkImageViewCreateInfo, gpu_vkAllocationCallbacks, &vkImageView); r != VK_SUCCESS) {
		vkDestroyImage(gpu_vkDevice, vkImage, gpu_vkAllocationCallbacks);
		return Err_Vk(r, "vkCreateImageView");
	}

	U32 bindlessIdx = 0;
	if (vkImageUsageFlags & VK_IMAGE_USAGE_SAMPLED_BIT) {
		bindlessIdx = gpu_nextBindlessDescriptorIdx++;
		VkDescriptorImageInfo const vkDescriptorImageInfo = {
			.sampler     = VK_NULL_HANDLE,
			.imageView   = vkImageView,
			.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		};
		VkWriteDescriptorSet const vkWriteDescriptorSet = {
			.sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.pNext            = 0,
			.dstSet           = gpu_vkBindlessDescriptorSet,
			.dstBinding       = 0,
			.dstArrayElement  = bindlessIdx,
			.descriptorCount  = 1,
			.descriptorType   = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
			.pImageInfo       = &vkDescriptorImageInfo,
			.pBufferInfo      = 0,
			.pTexelBufferView = 0,
		};
		vkUpdateDescriptorSets(gpu_vkDevice, 1, &vkWriteDescriptorSet, 0, 0);
	}

	Gpu_ImagePool::Entry* const entry = gpu_imageObjs.Alloc();
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

void Gpu_DestroyImage(Gpu_Image image) {
	if (image.handle) {
		Gpu_ImageObj* const imageObj = gpu_imageObjs.Get(image);
		Gpu_DestroyVk(imageObj->vkImageView, vkDestroyImageView);
		Gpu_DestroyVk(imageObj->vkImage, vkDestroyImage);
		Gpu_DestroyVk(imageObj->allocation.vkDeviceMemory, vkFreeMemory);
		memset(imageObj, 0, sizeof(*imageObj));
		gpu_imageObjs.Free(image);
	}
}

//-------------------------------------------------------------------------------------------------

U32             Gpu_GetImageWidth (Gpu_Image image)  { return gpu_imageObjs.Get(image)->width;  }
U32             Gpu_GetImageHeight(Gpu_Image image)  { return gpu_imageObjs.Get(image)->height; }
Gpu_ImageFormat Gpu_GetImageFormat(Gpu_Image image)  { return Gpu_VkFormatToImageFormat(gpu_imageObjs.Get(image)->vkFormat); }
U32             Gpu_GetImageBindIdx(Gpu_Image image) { return gpu_imageObjs.Get(image)->bindlessIdx; }

//-------------------------------------------------------------------------------------------------

Res<Gpu_Shader> Gpu_CreateShader(void const* data, U64 len) {
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
	if (VkResult const r = vkCreateShaderModule(gpu_vkDevice, &vkShaderModuleCreateInfo, gpu_vkAllocationCallbacks, &vkShaderModule); r != VK_SUCCESS) {
		spvReflectDestroyShaderModule(&spvReflectShaderModule);
		return Err_Vk(r, "vkCreateShaderModule");
	}

	Gpu_ShaderPool::Entry* const entry = gpu_shaderObjs.Alloc();
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

void Gpu_DestroyShader(Gpu_Shader shader) {
	if (shader.handle) {
		Gpu_ShaderObj* const shaderObj = gpu_shaderObjs.Get(shader);
		Gpu_DestroyVk(shaderObj->vkShaderModule, vkDestroyShaderModule);
		gpu_shaderObjs.Free(shader);
	}
}

//----------------------------------------------------------------------------------------------

Res<Gpu_Pipeline> Gpu_CreateGraphicsPipeline(Span<Gpu_Shader> shaders, Span<Gpu_ImageFormat> colorAttachmentFormats, Gpu_ImageFormat depthFormat) {
	Assert(shaders.len > 0);

	VkShaderStageFlags vkShaderStageFlags = 0;
	VkPushConstantRange vkPushConstantRange = {};
	VkPipelineShaderStageCreateInfo* const vkPipelineShaderStageCreateInfos = (VkPipelineShaderStageCreateInfo*)Mem_Alloc(gpu_tempMem, shaders.len * sizeof(VkPipelineShaderStageCreateInfo));
	for (U64 i = 0; i < shaders.len; i++) {
		Gpu_ShaderObj* const shaderObj = gpu_shaderObjs.Get(shaders[i]);

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
		.pSetLayouts            = &gpu_vkBindlessDescriptorSetLayout,
		.pushConstantRangeCount = 1,
		.pPushConstantRanges    = &vkPushConstantRange,
	};
	VkPipelineLayout vkPipelineLayout;
	if (VkResult const r = vkCreatePipelineLayout(gpu_vkDevice, &vkPipelineLayoutCreateInfo, gpu_vkAllocationCallbacks, &vkPipelineLayout); r != VK_SUCCESS) {
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
	Gpu_ImageObj const* swapchainImageObj = gpu_imageObjs.Get(gpu_swapchainImages[0]);
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
	VkPipelineColorBlendAttachmentState* const vkPipelineColorBlendAttachmentStates = (VkPipelineColorBlendAttachmentState*)Mem_Alloc(gpu_tempMem, colorAttachmentFormats.len * sizeof(VkPipelineColorBlendAttachmentState));
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
	VkFormat* const vkColorAttachmentFormats = (VkFormat*)Mem_Alloc(gpu_tempMem, colorAttachmentFormats.len * sizeof(VkFormat));
	for (U64 i = 0; i < colorAttachmentFormats.len; i++) {
		vkColorAttachmentFormats[i] = Gpu_ImageFormatToVkFormat(colorAttachmentFormats[i]);
	}
	VkPipelineRenderingCreateInfo const vkPipelineRenderingCreateInfo = {
		.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
		.pNext                   = 0,
		.viewMask                = 0,
		.colorAttachmentCount    = (U32)colorAttachmentFormats.len,
		.pColorAttachmentFormats = vkColorAttachmentFormats,
		.depthAttachmentFormat   = Gpu_ImageFormatToVkFormat(depthFormat),
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
	if (VkResult const r = vkCreateGraphicsPipelines(gpu_vkDevice, 0, 1, &vkGraphicsPipelineCreateInfo, gpu_vkAllocationCallbacks, &vkPipeline); r != VK_SUCCESS) {
		vkDestroyPipelineLayout(gpu_vkDevice, vkPipelineLayout, gpu_vkAllocationCallbacks);
		return Err_Vk(r, "vkCreateGraphicsPipelines");
	}

	Gpu_PipelinePool::Entry* const entry = gpu_pipelineObjs.Alloc();
	entry->obj.vkPipelineLayout    = vkPipelineLayout;
	entry->obj.vkPipeline          = vkPipeline;
	entry->obj.vkPipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	entry->obj.vkPushConstantRange = vkPushConstantRange;

	return entry->Handle();
}

//-------------------------------------------------------------------------------------------------

void Gpu_DestroyPipeline(Gpu_Pipeline pipeline) {
	if (pipeline.handle) {
		Gpu_PipelineObj* const pipelineObj = gpu_pipelineObjs.Get(pipeline);
		Gpu_DestroyVk(pipelineObj->vkPipeline, vkDestroyPipeline);
		Gpu_DestroyVk(pipelineObj->vkPipelineLayout, vkDestroyPipelineLayout);
		gpu_pipelineObjs.Free(pipeline);
	}
}

//-------------------------------------------------------------------------------------------------

Res<> Gpu_ImmediateWait() {
	if (!gpu_vkImmediateCommandBufferBegun) {
		return Ok();
	}

	vkEndCommandBuffer(gpu_vkImmediateCommandBuffer);
	gpu_vkImmediateCommandBufferBegun = false;

	gpu_immediateTimeline++;

	VkCommandBufferSubmitInfo const vkCommandBufferSubmitInfo = {
		.sType         = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
		.pNext         = 0,
		.commandBuffer = gpu_vkImmediateCommandBuffer,
		.deviceMask    = 0,
	};
	VkSemaphoreSubmitInfo const vkSignalSemaphoreSubmitInfo = {
		.sType       = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
		.pNext       = 0,
		.semaphore   = gpu_vkImmediateTimelineSemaphore,
		.value       = gpu_immediateTimeline,
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
	Gpu_CheckVk(vkQueueSubmit2(gpu_vkQueue, 1, &vkSubmitInfo2, 0));

	VkSemaphoreWaitInfo const vkSemaphoreWaitInfo = {
		.sType          = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO,
		.pNext          = 0,
		.flags          = 0,
		.semaphoreCount = 1,
		.pSemaphores    = &gpu_vkImmediateTimelineSemaphore,
		.pValues        = &gpu_immediateTimeline,

	};
	Gpu_CheckVk(vkWaitSemaphores(gpu_vkDevice, &vkSemaphoreWaitInfo, U64Max));

	vkResetCommandPool(gpu_vkDevice, gpu_vkImmediateCommandPool, 0);

	gpu_immediateStagingBufferUsed = 0;

	return Ok();
}

//-------------------------------------------------------------------------------------------------

Res<> Gpu_ImmediateCopyToBuffer(
	void const* data,
	U64         len,
	Gpu_Buffer  buffer,
	U64         offset
) {
	Gpu_BufferObj const* const bufferObj = gpu_bufferObjs.Get(buffer);
	while (len > 0) {
		U64 const amt = Min(len, Gpu_ImmediateStagingBufferSize - gpu_immediateStagingBufferUsed);
		memcpy(gpu_immediateStagingBufferPtr + gpu_immediateStagingBufferUsed, data, amt);

		if (!gpu_vkImmediateCommandBufferBegun) {
			VkCommandBufferBeginInfo const vkCommandBufferBeginInfo = {
				.sType            = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
				.pNext            = 0,
				.flags            = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
				.pInheritanceInfo = 0,
			};
			Gpu_CheckVk(vkBeginCommandBuffer(gpu_vkImmediateCommandBuffer, &vkCommandBufferBeginInfo));
			gpu_vkImmediateCommandBufferBegun = true;
		}

		VkBufferCopy2 const vkBufferCopy2 = {
			.sType     = VK_STRUCTURE_TYPE_BUFFER_COPY_2,
			.pNext     = 0,
			.srcOffset = gpu_immediateStagingBufferUsed,
			.dstOffset = offset,
			.size      = amt,
		};
		VkCopyBufferInfo2 const vkCopyBufferInfo2 = {
			.sType       = VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2,
			.pNext       = 0,
			.srcBuffer   = gpu_immediateStagingBuffer.vkBuffer,
			.dstBuffer   = bufferObj->vkBuffer,
			.regionCount = 1,
			.pRegions    = &vkBufferCopy2,
		};
		vkCmdCopyBuffer2(gpu_vkImmediateCommandBuffer, &vkCopyBufferInfo2);

		gpu_immediateStagingBufferUsed += amt;

		if (amt == len) {
			return Ok();	// do not submit
		}

		len -= amt;
		data = (U8*)data + amt;
		offset += amt;

		Try(Gpu_ImmediateWait());
	}

	return Ok();
}

//-------------------------------------------------------------------------------------------------

Res<> Gpu_ImmediateCopyToImage(void const* data, Gpu_Image image, Gpu_BarrierStage::Flags finalBarrierStageFlags, Gpu_ImageLayout finalImageLayout) {
	Gpu_ImageObj const* const imageObj = gpu_imageObjs.Get(image);
	U64 len = imageObj->width * imageObj->height * Gpu_FormatSize(imageObj->vkFormat);
	Assert(len <= Gpu_ImmediateStagingBufferSize);
	if (len > Gpu_ImmediateStagingBufferSize - gpu_immediateStagingBufferUsed) {
		Try(Gpu_ImmediateWait());
	}

	memcpy(gpu_immediateStagingBufferPtr + gpu_immediateStagingBufferUsed, data, len);

	if (!gpu_vkImmediateCommandBufferBegun) {
		VkCommandBufferBeginInfo const vkCommandBufferBeginInfo = {
			.sType            = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
			.pNext            = 0,
			.flags            = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
			.pInheritanceInfo = 0,
		};
		Gpu_CheckVk(vkBeginCommandBuffer(gpu_vkImmediateCommandBuffer, &vkCommandBufferBeginInfo));
		gpu_vkImmediateCommandBufferBegun = true;
	}

	Gpu_ImageMemoryBarrier(
		gpu_vkImmediateCommandBuffer,
		imageObj->vkImage,
		VK_PIPELINE_STAGE_2_NONE,
		VK_ACCESS_2_NONE,
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_PIPELINE_STAGE_2_COPY_BIT,
		VK_ACCESS_2_TRANSFER_WRITE_BIT,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		Gpu_IsDepthFormat(imageObj->vkFormat) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT 
	);

	VkBufferImageCopy2 const vkBufferImageCopy2 = {
		.sType             = VK_STRUCTURE_TYPE_BUFFER_IMAGE_COPY_2,
		.pNext             = 0,
		.bufferOffset      = gpu_immediateStagingBufferUsed,
		.bufferRowLength   = 0,
		.bufferImageHeight = 0,
		.imageSubresource  = {
			.aspectMask      = (VkImageAspectFlags)(Gpu_IsDepthFormat(imageObj->vkFormat) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT),
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
		.srcBuffer      = gpu_immediateStagingBuffer.vkBuffer,
		.dstImage       = imageObj->vkImage,
		.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		.regionCount    = 1,
		.pRegions       = &vkBufferImageCopy2,
	};
	vkCmdCopyBufferToImage2(gpu_vkImmediateCommandBuffer, &vkCopyBufferToImageInfo2);

	gpu_immediateStagingBufferUsed += len;

	Gpu_ImageMemoryBarrier(
		gpu_vkImmediateCommandBuffer,
		imageObj->vkImage,
		VK_PIPELINE_STAGE_2_COPY_BIT,
		VK_ACCESS_2_TRANSFER_WRITE_BIT,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		Gpu_BarrierStageFlagsToVkPipelineStageFlags2(finalBarrierStageFlags),
		Gpu_BarrierStageFlagsToVkAccessFlags2(finalBarrierStageFlags),
		Gpu_ImageLayoutToVkImageLayout(finalImageLayout),
		Gpu_IsDepthFormat(imageObj->vkFormat) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT 
	);

	return Ok();
}

//-------------------------------------------------------------------------------------------------

Res<Gpu_Frame> BeginFrame() {
	VkSemaphoreWaitInfo const vkSemaphoreWaitInfo = {
		.sType          = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO,
		.pNext          = 0,
		.flags          = 0,
		.semaphoreCount = 1,
		.pSemaphores    = &gpu_vkFrameTimelineSemaphore,
		.pValues        = &gpu_frame,

	};
	Gpu_CheckVk(vkWaitSemaphores(gpu_vkDevice, &vkSemaphoreWaitInfo, U64Max));

	gpu_frameStagingBufferUsed[gpu_frameIdx] = 0;

	if (VkResult r = vkAcquireNextImageKHR(gpu_vkDevice, gpu_vkSwapchain, U64Max, gpu_vkFrameImageAcquiredSemaphores[gpu_frameIdx], 0, &gpu_swapchainImageIdx); r != VK_SUCCESS) {
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
	Gpu_CheckVk(vkBeginCommandBuffer(gpu_vkFrameCommandBuffers[gpu_frameIdx], &vkCommandBufferBeginInfo));

	Gpu_ImageMemoryBarrier(
		gpu_vkFrameCommandBuffers[gpu_frameIdx],
		gpu_imageObjs.Get(gpu_swapchainImages[gpu_swapchainImageIdx])->vkImage,
		VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
		VK_ACCESS_2_NONE,
		VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
		VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
		VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		VK_IMAGE_ASPECT_COLOR_BIT
	);

	return Gpu_Frame {
		.frame          = gpu_frame,
		.swapchainImage = gpu_swapchainImages[gpu_swapchainImageIdx],
	};
};

//----------------------------------------------------------------------------------------------

Res<> Gpu_EndFrame() {
	Gpu_ImageMemoryBarrier(
		gpu_vkFrameCommandBuffers[gpu_frameIdx],
		gpu_imageObjs.Get(gpu_swapchainImages[gpu_swapchainImageIdx])->vkImage,
		VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
		VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
		VK_ACCESS_2_NONE,
		VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
		VK_IMAGE_ASPECT_COLOR_BIT
	);

	Gpu_CheckVk(vkEndCommandBuffer(gpu_vkFrameCommandBuffers[gpu_frameIdx]));

	VkSemaphoreSubmitInfo const vkWaitSemaphoreSubmitInfo = {
		.sType       = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
		.pNext       = 0,
		.semaphore   = gpu_vkFrameImageAcquiredSemaphores[gpu_frameIdx],
		.value       = 0,
		.stageMask   = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
		.deviceIndex = 0,
	};
	VkCommandBufferSubmitInfo const vkCommandBufferSubmitInfo = {
		.sType         = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
		.pNext         = 0,
		.commandBuffer = gpu_vkFrameCommandBuffers[gpu_frameIdx],
		.deviceMask    = 0,
	};
	VkSemaphoreSubmitInfo const vkSignalSemaphoreSubmitInfos[2] = {
		{
			.sType       = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
			.pNext       = 0,
			.semaphore   = gpu_vkFrameSubmitCompleteSemaphores[gpu_frameIdx],
			.value       = 0,
			.stageMask   = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
			.deviceIndex = 0,
		},
		{
			.sType       = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
			.pNext       = 0,
			.semaphore   = gpu_vkFrameTimelineSemaphore,
			.value       = gpu_frame + Gpu_MaxFrames,
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
	Gpu_CheckVk(vkQueueSubmit2(gpu_vkQueue, 1, &vkSubmitInfo2, 0));

	VkPresentInfoKHR vkPresentInfoKHR = {
		.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
		.pNext              = 0,
		.waitSemaphoreCount = 1,
		.pWaitSemaphores    = &gpu_vkFrameSubmitCompleteSemaphores[gpu_frameIdx],
		.swapchainCount     = 1,
		.pSwapchains        = &gpu_vkSwapchain,
		.pImageIndices      = &gpu_swapchainImageIdx,
		.pResults           = 0,
	};
	if (VkResult r = vkQueuePresentKHR(gpu_vkQueue, &vkPresentInfoKHR); r != VK_SUCCESS) {
		if (r == VK_SUBOPTIMAL_KHR || r == VK_ERROR_OUT_OF_DATE_KHR) {
			return Err_RecreateSwapchain();
		} else {
			return Err_Vk(r, "vkAcquireNextImageKHR");
		}
	}

	gpu_frame++;
	gpu_frameIdx = frame % Gpu_MaxFrames;

	return Ok();
}

//-------------------------------------------------------------------------------------------------

void Gpu_WaitIdle() {
	if (gpu_vkDevice) {
		vkDeviceWaitIdle(gpu_vkDevice);
	}
}

//----------------------------------------------------------------------------------------------

void* Gpu_AllocStaging(U64 len) {
	Assert(gpu_frameStagingBufferUsed[gpu_frameIdx] + len <= Gpu_PerFrameStagingBufferSize);
	U8* const ptr = gpu_frameStagingBufferPtrs[gpu_frameIdx] + gpu_frameStagingBufferUsed[gpu_frameIdx];
	gpu_frameStagingBufferUsed[gpu_frameIdx] += len;
	return ptr;
}

//----------------------------------------------------------------------------------------------

void Gpu_CopyStagingToBuffer(void* staging, U64 len, Gpu_Buffer buffer, U64 offset) {
	Assert((U8*)staging >= gpu_frameStagingBufferPtrs[gpu_frameIdx]);
	Assert((U8*)staging + len < gpu_frameStagingBufferPtrs[gpu_frameIdx] + Gpu_PerFrameStagingBufferSize);

	Gpu_BufferObj const* const bufferObj = gpu_bufferObjs.Get(buffer);
	VkDeviceSize const srcOffset = 
		(gpu_frameIdx * Gpu_PerFrameStagingBufferSize) + 
		(VkDeviceSize)((U8*)staging - gpu_frameStagingBufferPtrs[gpu_frameIdx]);
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
		.srcBuffer   = gpu_frameStagingBuffer.vkBuffer,
		.dstBuffer   = bufferObj->vkBuffer,
		.regionCount = 1,
		.pRegions    = &vkBufferCopy2,
	};
	vkCmdCopyBuffer2(gpu_vkFrameCommandBuffers[gpu_frameIdx], &vkCopyBufferInfo2);
}

//-------------------------------------------------------------------------------------------------

void Gpu_CopyStagingToImage(void* staging, Gpu_Image image) {
	Gpu_ImageObj const* const imageObj = gpu_imageObjs.Get(image);
	U64 const len = imageObj->width * imageObj->height * Gpu_FormatSize(imageObj->vkFormat);
	Assert((U8*)staging>= gpu_frameStagingBufferPtrs[gpu_frameIdx]);
	Assert((U8*)staging + len < gpu_frameStagingBufferPtrs[gpu_frameIdx] + Gpu_PerFrameStagingBufferSize);

	VkDeviceSize const srcOffset = 
		(gpu_frameIdx * Gpu_PerFrameStagingBufferSize) + 
		(VkDeviceSize)((U8*)staging - gpu_frameStagingBufferPtrs[gpu_frameIdx]);

	VkBufferImageCopy2 const vkBufferImageCopy2 = {
		.sType             = VK_STRUCTURE_TYPE_BUFFER_IMAGE_COPY_2,
		.pNext             = 0,
		.bufferOffset      = srcOffset,
		.bufferRowLength   = 0,
		.bufferImageHeight = 0,
		.imageSubresource  = {
			.aspectMask      = (VkImageAspectFlags)(Gpu_IsDepthFormat(imageObj->vkFormat) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT),
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
		.srcBuffer      = gpu_frameStagingBuffer.vkBuffer,
		.dstImage       = imageObj->vkImage,
		.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		.regionCount    = 1,
		.pRegions       = &vkBufferImageCopy2,
	};
	vkCmdCopyBufferToImage2(gpu_vkFrameCommandBuffers[gpu_frameIdx], &vkCopyBufferToImageInfo2);
}

//-------------------------------------------------------------------------------------------------

void Gpu_BufferBarrier(Gpu_Buffer buffer, U64 offset, U64 size, Gpu_BarrierStage::Flags srcBarrierStageFlags, Gpu_BarrierStage::Flags dstBarrierStageFlags) {
	VkBufferMemoryBarrier2 const vkBufferMemoryBarrier2 = {
		.sType               = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
		.pNext               = 0,
		.srcStageMask        = Gpu_BarrierStageFlagsToVkPipelineStageFlags2(srcBarrierStageFlags),
		.srcAccessMask       = Gpu_BarrierStageFlagsToVkAccessFlags2(srcBarrierStageFlags),
		.dstStageMask        = Gpu_BarrierStageFlagsToVkPipelineStageFlags2(dstBarrierStageFlags),
		.dstAccessMask       = Gpu_BarrierStageFlagsToVkAccessFlags2(dstBarrierStageFlags),
		.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.buffer              = gpu_bufferObjs.Get(buffer)->vkBuffer,
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
	vkCmdPipelineBarrier2(gpu_vkFrameCommandBuffers[gpu_frameIdx], &vkDependencyInfo);
}

//-------------------------------------------------------------------------------------------------

void ImageBarrier(Gpu_Image image, Gpu_BarrierStage::Flags srcBarrierStageFlags, Gpu_ImageLayout srcImageLayout, Gpu_BarrierStage::Flags dstBarrierStageFlags, Gpu_ImageLayout dstImageLayout) {
	Gpu_ImageObj* const imageObj = gpu_imageObjs.Get(image);
	Gpu_ImageMemoryBarrier(
		gpu_vkFrameCommandBuffers[gpu_frameIdx],
		imageObj->vkImage,
		Gpu_BarrierStageFlagsToVkPipelineStageFlags2(srcBarrierStageFlags),
		Gpu_BarrierStageFlagsToVkAccessFlags2(srcBarrierStageFlags),
		Gpu_ImageLayoutToVkImageLayout(srcImageLayout),
		Gpu_BarrierStageFlagsToVkPipelineStageFlags2(dstBarrierStageFlags),
		Gpu_BarrierStageFlagsToVkAccessFlags2(dstBarrierStageFlags),
		Gpu_ImageLayoutToVkImageLayout(dstImageLayout),
		Gpu_IsDepthFormat(imageObj->vkFormat) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT
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
	vkCmdPipelineBarrier2(gpu_vkFrameCommandBuffers[gpu_frameIdx], &vkDependencyInfo);
}

//-------------------------------------------------------------------------------------------------

void Gpu_BeginPass(Gpu_Pass const* pass) {
	VkRenderingAttachmentInfo* const vkColorRenderingAttachmentInfos = (VkRenderingAttachmentInfo*)Mem_Alloc(gpu_tempMem, pass->colorAttachments.len * sizeof(VkRenderingAttachmentInfo));
	for (U64 i = 0; i < pass->colorAttachments.len; i++) {
		Gpu_ImageObj* const imageObj = gpu_imageObjs.Get(pass->colorAttachments[i]);
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

	Gpu_ImageObj* depthImageObj = 0;
	VkRenderingAttachmentInfo vkDepthRenderingAttachmentInfo;
	if (pass->depthAttachment.handle) {
		depthImageObj = gpu_imageObjs.Get(pass->depthAttachment);
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
		.renderArea           = { .offset = { 0, 0 }, .extent = { .width = (U32)pass->viewport.w, .height = (U32)pass->viewport.h} },
		.layerCount           = 1,
		.viewMask             = 0,
		.colorAttachmentCount = (U32)pass->colorAttachments.len,
		.pColorAttachments    = vkColorRenderingAttachmentInfos,
		.pDepthAttachment     = depthImageObj ? &vkDepthRenderingAttachmentInfo : 0,
		.pStencilAttachment   = 0,
	};
	vkCmdBeginRendering(gpu_vkFrameCommandBuffers[gpu_frameIdx], &vkRenderingInfo);

	VkViewport const vkViewport = {
		.x        = pass->viewport.x,
		.y        = pass->viewport.y,
		.width    = pass->viewport.w,
		.height   = pass->viewport.h,
		.minDepth = 0.0f,
		.maxDepth = 1.0f,
	};
	vkCmdSetViewport(gpu_vkFrameCommandBuffers[gpu_frameIdx], 0, 1, &vkViewport);

	VkRect2D const vkScissorRect2D = {
		.offset     = {
			.x      = pass->scissor.x,
			.y      = pass->scissor.y,
		},
		.extent     = {
			.width  = (U32)pass->scissor.w,
			.height = (U32)pass->scissor.h
		},
	};
	vkCmdSetScissor(gpu_vkFrameCommandBuffers[gpu_frameIdx], 0, 1, &vkScissorRect2D);

	Gpu_PipelineObj* const pipelineObj = gpu_pipelineObjs.Get(pass->pipeline);
	vkCmdBindPipeline(gpu_vkFrameCommandBuffers[gpu_frameIdx], pipelineObj->vkPipelineBindPoint, pipelineObj->vkPipeline);

	vkCmdBindDescriptorSets(gpu_vkFrameCommandBuffers[gpu_frameIdx], pipelineObj->vkPipelineBindPoint, pipelineObj->vkPipelineLayout, 0, 1, &gpu_vkBindlessDescriptorSet, 0, 0);
}

//----------------------------------------------------------------------------------------------

void Gpu_EndPass() {
	vkCmdEndRendering(gpu_vkFrameCommandBuffers[gpu_frameIdx]);
}

//----------------------------------------------------------------------------------------------

void Gpu_BindIndexBuffer(Gpu_Buffer buffer) {
	vkCmdBindIndexBuffer(gpu_vkFrameCommandBuffers[gpu_frameIdx], gpu_bufferObjs.Get(buffer)->vkBuffer, 0, VK_INDEX_TYPE_UINT32);
}

//----------------------------------------------------------------------------------------------

void Gpu_PushConstants(Gpu_Pipeline pipeline, void const* ptr, U32 len) {
	Gpu_PipelineObj* const pipelineObj = gpu_pipelineObjs.Get(pipeline);
	vkCmdPushConstants(gpu_vkFrameCommandBuffers[gpu_frameIdx], pipelineObj->vkPipelineLayout, pipelineObj->vkPushConstantRange.stageFlags, 0, len, ptr);
}

//----------------------------------------------------------------------------------------------

void Gpu_Draw(U32 vertexCount, U32 instanceCount) {
	vkCmdDraw(gpu_vkFrameCommandBuffers[gpu_frameIdx], vertexCount, instanceCount, 0, 0);
}

void Gpu_DrawIndexed(U32 indexCount) {
	vkCmdDrawIndexed(gpu_vkFrameCommandBuffers[gpu_frameIdx], indexCount, 1, 0, 0, 0);
}

void Gpu_DrawIndexedIndirect(Gpu_Buffer indirectBuffer, U32 drawCount) {
	Gpu_BufferObj* const bufferObj = gpu_bufferObjs.Get(indirectBuffer);
	vkCmdDrawIndexedIndirect(gpu_vkFrameCommandBuffers[gpu_frameIdx], bufferObj->vkBuffer, 0, drawCount, 0);
}