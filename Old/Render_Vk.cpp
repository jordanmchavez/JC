#include "JC/Array.h"
#include "JC/File.h"
#include "JC/Log.h"
#include "JC/Math.h"
#include "JC/Mem.h"
#include "JC/Render.h"
#include "JC/Render_Vk.h"

//--------------------------------------------------------------------------------------------------

static constexpr u32 Render_MaxFrames = 2;

struct Render_FrameData {
	VkSemaphore     swapchainSemaphore;
	VkSemaphore     renderSemaphore;
	VkFence         renderFence;
	VkCommandBuffer commandBuffer;
};

struct Vertex {
	Vec3 pos;
	Vec3 color;
};

static constexpr Vertex verts[] = {
	{ { -0.5f, -0.5f, 0.0f }, { 1.0f, 0.0f, 0.0f } },
	{ {  0.5f, -0.5f, 0.0f }, { 0.0f, 1.0f, 0.0f } },
	{ {  0.5f,  0.5f, 0.0f }, { 0.0f, 0.0f, 1.0f } },
	{ { -0.5f,  0.5f, 0.0f }, { 1.0f, 1.0f, 1.0f } },
};

static constexpr u16 indices[] = {
	0, 1, 2, 0, 2, 3,
};

struct Uniform {
	Mat4 model;
	Mat4 view;
	Mat4 proj;
};

struct Render_PhysicalDevice {
	VkPhysicalDevice                 vkPhysicalDevice                 = { 0 };
	VkPhysicalDeviceProperties       vkPhysicalDeviceProperties       = { 0 };
	VkPhysicalDeviceFeatures         vkPhysicalDeviceFeatures         = { 0 };
	VkQueueFamilyProperties*         vkQueueFamilyProperties          = nullptr;
	u32                              vkQueueFamilyPropertiesLen       = 0;
	u32                              graphicsQueueFamily              = VK_QUEUE_FAMILY_IGNORED;
	u32                              presentQueueFamily               = VK_QUEUE_FAMILY_IGNORED;
	VkExtensionProperties*           vkExtensionProperties            = nullptr;
	u32                              vkExtensionPropertiesLen         = 0;
	VkSurfaceCapabilitiesKHR         vkSurfaceCapabilities            = { 0 };
	u32                              vkSurfaceFormatsLen              = 0;
	VkSurfaceFormatKHR*              vkSurfaceFormats                 = nullptr;
	VkFormat                         vkSwapchainFormat                = VK_FORMAT_UNDEFINED;
	VkColorSpaceKHR                  vkSwapchainColorSpace            = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
	u32                              vkPresentModesLen                = 0;
	VkPresentModeKHR*                vkPresentModes                   = nullptr;
	VkPresentModeKHR                 vkSwapchainPresentMode           = VK_PRESENT_MODE_FIFO_KHR;	// guaranteed to be supported
	VkPhysicalDeviceMemoryProperties vkPhysicalDeviceMemoryProperties = { 0 };
};

static Allocator*               render_allocator                               = nullptr;
static VkAllocationCallbacks    render_vkAllocationCallbacks                   = { 0 };
static VkAllocationCallbacks*   render_vkAllocationCallbacksTODOFIXME          = nullptr;
static VkInstance               render_vkInstance                              = VK_NULL_HANDLE;
static VkSurfaceKHR             render_vkSurface                               = VK_NULL_HANDLE;
static Render_PhysicalDevice*   render_physicalDevices                         = nullptr;
static u32                      render_physicalDevicesLen                      = 0;
static Render_PhysicalDevice*   render_physicalDevice                          = nullptr;
static VkDevice                 render_vkDevice                                = VK_NULL_HANDLE;
static VkDebugUtilsMessengerEXT render_vkDebugUtilsMessenger                   = VK_NULL_HANDLE;
static VkQueue                  render_vkGraphicsQueue                         = VK_NULL_HANDLE;
static VkQueue                  render_vkPresentQueue                          = VK_NULL_HANDLE;
static VkQueue                  render_vkTransferQueue                         = VK_NULL_HANDLE;
static VkSwapchainKHR           render_vkSwapchain                             = VK_NULL_HANDLE;
static VkImage*                 render_vkSwapchainImages                       = nullptr;
static u32                      render_vkSwapchainImagesLen                    = 0;
static VkImageView*             render_vkSwapchainImageViews                   = nullptr;
static u32                      render_vkSwapchainImageViewsLen                = 0;
static VkPipeline               render_vkPipeline                              = VK_NULL_HANDLE;
static VkShaderModule           render_vkVertShaderModule                      = VK_NULL_HANDLE;
static VkShaderModule           render_vkFragShaderModule                      = VK_NULL_HANDLE;
static VkDescriptorSetLayout    render_vkDescriptorSetLayout                   = VK_NULL_HANDLE;
static VkPipelineLayout         render_vkPipelineLayout                        = VK_NULL_HANDLE;
static VkRenderPass             render_vkRenderPass                            = VK_NULL_HANDLE;
static VkFramebuffer*           render_vkFramebuffers                          = nullptr;
static u32                      render_vkFramebuffersLen                       = 0;
static VkCommandPool            render_vkCommandPool                           = VK_NULL_HANDLE;
static u32                      render_frame                                   = 0;
static VkBuffer                 render_vkVertexBuffer                          = VK_NULL_HANDLE;
static VkDeviceMemory           render_vkVertexBufferDeviceMemory              = VK_NULL_HANDLE;
static VkBuffer                 render_vkIndexBuffer                           = VK_NULL_HANDLE;
static VkDeviceMemory           render_vkIndexBufferDeviceMemory               = VK_NULL_HANDLE;
static VkBuffer*                render_vkUniformBuffers                        = nullptr;
static VkDeviceMemory*          render_vkUniformBufferDeviceMemories           = nullptr;
static void**                   render_vkUniformBufferPtrs                     = nullptr;
static u32                      render_vkUniformBuffersLen                     = 0;
static float                    render_time                                    = 0.0f;
static VkDescriptorPool         render_vkDescriptorPool                        = VK_NULL_HANDLE;
static VkDescriptorSet*         render_vkDescriptorSets                        = nullptr;

static constexpr const char* Render_WantLayers[] = {
	"VK_LAYER_KHRONOS_validation"
};
static constexpr u32 Render_WantLayersLen = sizeof(Render_WantLayers) / sizeof(Render_WantLayers[0]);

static constexpr const char* Render_WantInstanceExts[] = {
	"VK_KHR_surface",
	"VK_KHR_win32_surface",
	"VK_EXT_debug_utils",
};
static constexpr u32 Render_WantInstanceExtsLen = sizeof(Render_WantInstanceExts) / sizeof(Render_WantInstanceExts[0]);

static constexpr const char* Render_WantDeviceExts[] = {
	"VK_KHR_swapchain",
};
static constexpr u32 Render_WantDeviceExtsLen = sizeof(Render_WantDeviceExts) / sizeof(Render_WantDeviceExts[0]);

#define Err_CheckVk(expr) \
	do { if (VkResult r = (expr); r != VK_SUCCESS) { \
		Err* e = Err_Make({ .ns = "vk", .code = (u64)r }, "expr", #expr, "msg", Render_VkResultStr(r)); \
		return Err_Push(e, Render_Err); \
	} } while (false);

LOG_DEFINE_FNS(Render)
LOG_DEFINE_FNS(VK)

VkResult Render_CreateSurface(VkInstance vkInstance, const VkAllocationCallbacks* vkAllocationCallbacks, VkSurfaceKHR* vkSurface);

//--------------------------------------------------------------------------------------------------

static s8 SizeStr(u64 size) {
	if (size > 1024 * 1024 * 1024) {
		return Fmt("{.1}gb", (double)size / (1024.0 * 1024.0 * 1024.0));
	}
	if (size > 1024 * 1024) {
		return Fmt("{.1}mb", (double)size / (1024.0 * 1024.0));
	}
	if (size > 1024) {
		return Fmt("{.1}kb", (double)size / 1024.0);
	}
	return Fmt("{}", size);
}

//--------------------------------------------------------------------------------------------------

static VkBool32 Render_VkDebugUtilsMessengerCallback(VkDebugUtilsMessageSeverityFlagBitsEXT sev, VkDebugUtilsMessageTypeFlagsEXT type, const VkDebugUtilsMessengerCallbackDataEXT* data, void*) {
	char sevCh = '?';
	     if (sev & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT) { sevCh = 'V'; }
	else if (sev & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT   ) { sevCh = 'I'; }
	else if (sev & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) { sevCh = 'W'; }
	else if (sev & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT  ) { sevCh = '!'; }

	s8 typeStr = "???";
         if (type & VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT    ) { typeStr = "Gen"; }
    else if (type & VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT ) { typeStr = "Val"; }
    else if (type & VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT) { typeStr = "Prf"; }

	VK_Log("[{}][{}] {}({}): {}", sevCh, typeStr, data->pMessageIdName, data->messageIdNumber, data->pMessage);

	for (u32 i = 0; i < data->queueLabelCount; ++i) {
		VK_Log("  Label: {}", data->pQueueLabels[i].pLabelName);
	}
    
	for (u32 i = 0; i < data->cmdBufLabelCount; ++i) {
		VK_Log("  Cmd: {}", data->pCmdBufLabels[i].pLabelName);
    }

	for (u32 i = 0; i < data->objectCount; ++i) {
		VK_Log("  Obj: [{}] {} (handle={})", Render_VkObjectTypeStr(data->pObjects[i].objectType), data->pObjects[i].pObjectName, data->pObjects[i].objectHandle);
	}

	return VK_FALSE;
}

//--------------------------------------------------------------------------------------------------

static void* VKAPI_PTR Render_VkAllocation(void*, u64 size, u64 align, VkSystemAllocationScope scope) {
	size;align;scope;
	return nullptr;
}

static void* VKAPI_PTR Render_VkReallocation(void*, void* ptr, u64 size, u64 align, VkSystemAllocationScope scope) {
	ptr;size;align;scope;
	return nullptr;
}

static void VKAPI_PTR Render_VkFree(void*, void* ptr) {
	ptr;
}

static void VKAPI_PTR Render_VkInternalAllocation(void*, u64 size, VkInternalAllocationType type, VkSystemAllocationScope scope) {
	size;type;scope;
}

static void VKAPI_PTR Render_VkInternalFree(void*, u64 size, VkInternalAllocationType type, VkSystemAllocationScope scope) {
	size;type;scope;
}

//--------------------------------------------------------------------------------------------------

Res<> Render_Init() {
	render_allocator = Mem_CreateAllocator("Render");

	render_vkAllocationCallbacks = {
		.pfnAllocation         = Render_VkAllocation,
		.pfnReallocation       = Render_VkReallocation,
		.pfnFree               = Render_VkFree,
		.pfnInternalAllocation = Render_VkInternalAllocation,
		.pfnInternalFree       = Render_VkInternalFree,
	};

	Err_Try(Render_LoadVkRootFns());

	//-------------------------------------------------------------------------------------------------

	u32 vkLayerPropertiesLen = 0;
	Err_CheckVk(vkEnumerateInstanceLayerProperties(&vkLayerPropertiesLen, nullptr));
	VkLayerProperties* vkLayerProperties = (VkLayerProperties*)mem_scratch->Alloc(vkLayerPropertiesLen * sizeof(VkLayerProperties));
	Err_CheckVk(vkEnumerateInstanceLayerProperties(&vkLayerPropertiesLen, vkLayerProperties));
	Render_Log("Vulkan layers:");
	for (u32 i = 0; i < vkLayerPropertiesLen; ++i) {
		Render_Log("  [{}] {} v{}: {}", i, vkLayerProperties[i].layerName, vkLayerProperties[i].implementationVersion, vkLayerProperties[i].description);
	}

	u32 vkExtensionPropertiesLen = 0;
	Err_CheckVk(vkEnumerateInstanceExtensionProperties(nullptr, &vkExtensionPropertiesLen, nullptr));
	VkExtensionProperties* vkExtensionProperties = (VkExtensionProperties*)mem_scratch->Alloc(vkExtensionPropertiesLen * sizeof(VkExtensionProperties));
	Err_CheckVk(vkEnumerateInstanceExtensionProperties(nullptr, &vkExtensionPropertiesLen, vkExtensionProperties));
	Render_Log("Vulkan extensions:");
	for (u32 i = 0; i < vkExtensionPropertiesLen; ++i) {
		Render_Log("  [{}] {}", i, vkExtensionProperties[i].extensionName);
	}

	//-------------------------------------------------------------------------------------------------

	for (u32 i = 0; i < Render_WantLayersLen; ++i) {
		bool found = false;
		for (u32 j = 0; j < vkLayerPropertiesLen; ++j) {
			if (strcmp(Render_WantLayers[i], vkLayerProperties[j].layerName) == 0) {
				Render_Log("Found wanted layer {}", Render_WantLayers[i]);
				found = true;
				break;
			}
		}
		Err_Check(found, Render_ErrLayerNotFound, "layer", Render_WantLayers[i]);
	}

	for (u32 i = 0; i < Render_WantInstanceExtsLen; ++i) {
		bool found = false;
		for (u32 j = 0; j < vkLayerPropertiesLen; ++j) {
			if (strcmp(Render_WantInstanceExts[i], vkExtensionProperties[j].extensionName) == 0) {
				Render_Log("Found wanted instance extension {}", Render_WantInstanceExts[i]);
				found = true;
				break;
			}
		}
		Err_Check(found, Render_ErrInstanceExtNotFound, "ext", Render_WantInstanceExts[i]);
	}

	//-------------------------------------------------------------------------------------------------

	VkApplicationInfo vkApplicationInfo = {
		.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO,
		.pNext              = nullptr,
		.pApplicationName   = nullptr,
		.applicationVersion = 0,
		.pEngineName        = nullptr,
		.engineVersion      = 0,
		.apiVersion         = VK_API_VERSION_1_3,
	};

	VkDebugUtilsMessengerCreateInfoEXT vkDebugUtilsMessengerCreateInfo = {
		.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
		.pNext = nullptr,
		.flags = 0,
		.messageSeverity = 
			VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
		.messageType = 
			VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_TYPE_DEVICE_ADDRESS_BINDING_BIT_EXT,
		.pfnUserCallback = Render_VkDebugUtilsMessengerCallback,
		.pUserData = nullptr,
	};

	VkInstanceCreateInfo vkInstanceCreateInfo = {
		.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
		.pNext                   = &vkDebugUtilsMessengerCreateInfo,
		.flags                   = 0,
		.pApplicationInfo        = &vkApplicationInfo,
		.enabledLayerCount       = Render_WantLayersLen,
		.ppEnabledLayerNames     = Render_WantLayers,
		.enabledExtensionCount   = Render_WantInstanceExtsLen,
		.ppEnabledExtensionNames = Render_WantInstanceExts,
    };

	Err_CheckVk(vkCreateInstance(&vkInstanceCreateInfo, render_vkAllocationCallbacksTODOFIXME, &render_vkInstance));

	Render_LoadVkInstanceFns(render_vkInstance);

	//----------------------------------------------------------------------------------------------

	Err_CheckVk(Render_CreateSurface(render_vkInstance, render_vkAllocationCallbacksTODOFIXME, &render_vkSurface));

	//----------------------------------------------------------------------------------------------

	Err_CheckVk(vkEnumeratePhysicalDevices(render_vkInstance, &render_physicalDevicesLen, nullptr));
	VkPhysicalDevice* vkPhysicalDevices = (VkPhysicalDevice*)mem_scratch->Alloc(render_physicalDevicesLen * sizeof(VkPhysicalDevice));
	Err_CheckVk(vkEnumeratePhysicalDevices(render_vkInstance, &render_physicalDevicesLen, vkPhysicalDevices));

	render_physicalDevices = (Render_PhysicalDevice*)render_allocator->Alloc(render_physicalDevicesLen * sizeof(Render_PhysicalDevice));

	Render_PhysicalDevice* discrete = nullptr;
	Render_PhysicalDevice* integrated = nullptr;
	Render_PhysicalDevice* backup = nullptr;

	Render_Log("{} Vulkan physical device(s):", render_physicalDevicesLen);
	for (u32 i = 0; i < render_physicalDevicesLen; ++i) {
		Render_PhysicalDevice* pd = &render_physicalDevices[i];
		pd->vkPhysicalDevice = vkPhysicalDevices[i];

		vkGetPhysicalDeviceProperties(pd->vkPhysicalDevice, &pd->vkPhysicalDeviceProperties);
		Render_Log(
			"  [{}] {}: apiVersion={}, driverVersion={}, vendorId={}, deviceId={}, type={}",
			i,
			pd->vkPhysicalDeviceProperties.deviceName,
			pd->vkPhysicalDeviceProperties.apiVersion,
			pd->vkPhysicalDeviceProperties.driverVersion,
			pd->vkPhysicalDeviceProperties.vendorID,
			pd->vkPhysicalDeviceProperties.deviceID,
			pd->vkPhysicalDeviceProperties.deviceType
		);

		vkGetPhysicalDeviceFeatures(pd->vkPhysicalDevice, &pd->vkPhysicalDeviceFeatures);

		vkGetPhysicalDeviceQueueFamilyProperties(pd->vkPhysicalDevice, &pd->vkQueueFamilyPropertiesLen, nullptr);
		pd->vkQueueFamilyProperties = (VkQueueFamilyProperties*)render_allocator->Alloc(pd->vkQueueFamilyPropertiesLen * sizeof(VkQueueFamilyProperties));
		vkGetPhysicalDeviceQueueFamilyProperties(pd->vkPhysicalDevice, &pd->vkQueueFamilyPropertiesLen, pd->vkQueueFamilyProperties);
		Render_Log("  Queues:");
		for (u32 j = 0; j < pd->vkQueueFamilyPropertiesLen; ++j) {
			Render_Log("    [{}] {} x {}", j, Render_VkQueueFlagsStr(pd->vkQueueFamilyProperties[j].queueFlags), pd->vkQueueFamilyProperties[j].queueCount);
			if (pd->graphicsQueueFamily != VK_QUEUE_FAMILY_IGNORED && pd->vkQueueFamilyProperties[j].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
				pd->graphicsQueueFamily = j;
			}

			VkBool32 supportsPresent = VK_FALSE;
			Err_CheckVk(vkGetPhysicalDeviceSurfaceSupportKHR(pd->vkPhysicalDevice, j, render_vkSurface, &supportsPresent));
			if (pd->presentQueueFamily != VK_QUEUE_FAMILY_IGNORED && supportsPresent == VK_TRUE) {
				pd->presentQueueFamily = j;
				Render_Log("      Supports present");
			}
		}

		Err_CheckVk(vkEnumerateDeviceExtensionProperties(pd->vkPhysicalDevice, nullptr, &pd->vkExtensionPropertiesLen, nullptr));
		pd->vkExtensionProperties = (VkExtensionProperties*)render_allocator->Alloc(pd->vkExtensionPropertiesLen * sizeof(VkExtensionProperties));
		Err_CheckVk(vkEnumerateDeviceExtensionProperties(pd->vkPhysicalDevice, nullptr, &pd->vkExtensionPropertiesLen, pd->vkExtensionProperties));
		Render_Log("  Extensions:");
		for (u32 j = 0; j < pd->vkExtensionPropertiesLen; ++j) {
			Render_Log("    {} v{}", pd->vkExtensionProperties[j].extensionName, pd->vkExtensionProperties[j].specVersion);
		}

		bool missingExts = false;
		for (u32 j = 0; j < Render_WantDeviceExtsLen; ++j) {
			bool found = false;
			for (u32 k = 0; k < pd->vkExtensionPropertiesLen; ++k) {
				if (strcmp(Render_WantDeviceExts[j], pd->vkExtensionProperties[k].extensionName) == 0) {
					Render_Log("  Found wanted device extension {}", Render_WantDeviceExts[j]);
					found = true;
					break;
				}
			}
			if (!found) {
				Render_Log("  Missing wanted device extension {}", Render_WantDeviceExts[j]);
				missingExts = true;
			}
		}

		Err_CheckVk(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(pd->vkPhysicalDevice, render_vkSurface, &pd->vkSurfaceCapabilities));

		Err_CheckVk(vkGetPhysicalDeviceSurfaceFormatsKHR(pd->vkPhysicalDevice, render_vkSurface, &pd->vkSurfaceFormatsLen, nullptr));
		pd->vkSurfaceFormats = (VkSurfaceFormatKHR*)render_allocator->Alloc(pd->vkSurfaceFormatsLen * sizeof(VkSurfaceFormatKHR));
		Err_CheckVk(vkGetPhysicalDeviceSurfaceFormatsKHR(pd->vkPhysicalDevice, render_vkSurface, &pd->vkSurfaceFormatsLen, pd->vkSurfaceFormats));
		Render_Log("  Surface formats:");
		for (u32 j = 0; j < pd->vkSurfaceFormatsLen; ++j) {
			Render_Log("    {} ({})", Render_VkFormatStr(pd->vkSurfaceFormats[j].format), Render_VkColorSpaceStr(pd->vkSurfaceFormats[j].colorSpace));
			if (
				(pd->vkSurfaceFormats[j].format == VK_FORMAT_R8G8B8A8_SRGB || pd->vkSurfaceFormats[j].format == VK_FORMAT_B8G8R8A8_SRGB) &&
				pd->vkSurfaceFormats[j].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR
			) {
				pd->vkSwapchainFormat = pd->vkSurfaceFormats[j].format;
				pd->vkSwapchainColorSpace = pd->vkSurfaceFormats[j].colorSpace;
			}
		}
		if (pd->vkSwapchainFormat == VK_FORMAT_UNDEFINED) {
			pd->vkSwapchainFormat = pd->vkSurfaceFormats[0].format;
		}
		Render_Log("  Selected {} as swap chain format", Render_VkFormatStr(pd->vkSwapchainFormat));

		Err_CheckVk(vkGetPhysicalDeviceSurfacePresentModesKHR(pd->vkPhysicalDevice, render_vkSurface, &pd->vkPresentModesLen, nullptr));
		pd->vkPresentModes = (VkPresentModeKHR*)render_allocator->Alloc(pd->vkPresentModesLen * sizeof(VkPresentModeKHR));
		Err_CheckVk(vkGetPhysicalDeviceSurfacePresentModesKHR(pd->vkPhysicalDevice, render_vkSurface, &pd->vkPresentModesLen, pd->vkPresentModes));
		Render_Log("  Present modes:");
		for (u32 j = 0; j < pd->vkPresentModesLen; ++j) {
			Render_Log("    {}", Render_VkPresentModeStr(pd->vkPresentModes[j]));
			if (pd->vkPresentModes[j] == VK_PRESENT_MODE_MAILBOX_KHR) {
				pd->vkSwapchainPresentMode = VK_PRESENT_MODE_MAILBOX_KHR;
			}
		}
		Render_Log("  Selected {} as swap chain present mode", Render_VkPresentModeStr(pd->vkSwapchainPresentMode));

		if (pd->graphicsQueueFamily != VK_QUEUE_FAMILY_IGNORED && pd->presentQueueFamily != VK_QUEUE_FAMILY_IGNORED && !missingExts) {
			if (discrete == nullptr && pd->vkPhysicalDeviceProperties.deviceType & VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
				discrete = pd;
			}
			if (integrated == nullptr && pd->vkPhysicalDeviceProperties.deviceType & VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) {
				integrated = pd;
			}
			if (backup == nullptr) {
				backup = pd;
			}
		}

		vkGetPhysicalDeviceMemoryProperties(pd->vkPhysicalDevice, &pd->vkPhysicalDeviceMemoryProperties);
		Render_Log("  Memory Types:");
		for (u32 j = 0; j < pd->vkPhysicalDeviceMemoryProperties.memoryTypeCount; ++j) {
			Render_Log("    [{}] heapIndex={}, flags={}", j, pd->vkPhysicalDeviceMemoryProperties.memoryTypes[j].heapIndex, Render_VkMemoryPropertyFlagsStr(pd->vkPhysicalDeviceMemoryProperties.memoryTypes[j].propertyFlags));
		}
		Render_Log("  Heaps:");
		for (u32 j = 0; j < pd->vkPhysicalDeviceMemoryProperties.memoryHeapCount; ++j) {
			Render_Log("    [{}] size={}, flags={}", j, SizeStr(pd->vkPhysicalDeviceMemoryProperties.memoryHeaps[j].size), Render_VkMemoryHeapFlagsStr(pd->vkPhysicalDeviceMemoryProperties.memoryHeaps[j].flags));
		}
	}

	Err_Check(discrete || integrated || backup, Render_ErrNoDevices);
	if (backup)     { render_physicalDevice = backup; }
	if (integrated) { render_physicalDevice = integrated; }
	if (discrete)   { render_physicalDevice = discrete; }

	Render_Log("Selected physical device: {}", render_physicalDevice->vkPhysicalDeviceProperties.deviceName);

	//----------------------------------------------------------------------------------------------

	const float queuePriority = 1.0f;
	VkDeviceQueueCreateInfo vkDeviceQueueCreateInfos[] = {
		{
			.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
			.pNext            = nullptr,
			.flags            = 0,
			.queueFamilyIndex = render_physicalDevice->graphicsQueueFamily,
			.queueCount       = 1,
			.pQueuePriorities = &queuePriority,
		},
		{
			.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
			.pNext            = nullptr,
			.flags            = 0,
			.queueFamilyIndex = render_physicalDevice->presentQueueFamily,
			.queueCount       = 1,
			.pQueuePriorities = &queuePriority,
		},
	};
	VkDeviceCreateInfo vkDeviceCreateInfo = {
		.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
		.pNext                   = nullptr,
		.flags                   = 0,
		.queueCreateInfoCount    = 2,
		.pQueueCreateInfos       = vkDeviceQueueCreateInfos,
		.enabledLayerCount       = Render_WantLayersLen,
		.ppEnabledLayerNames     = Render_WantLayers,
		.enabledExtensionCount   = Render_WantDeviceExtsLen,
		.ppEnabledExtensionNames = Render_WantDeviceExts,
		.pEnabledFeatures        = nullptr,
	};
	Err_CheckVk(vkCreateDevice(render_physicalDevice->vkPhysicalDevice, &vkDeviceCreateInfo, render_vkAllocationCallbacksTODOFIXME, &render_vkDevice));
	
	Render_LoadVkDeviceFns(render_vkDevice);
	
	Err_CheckVk(vkCreateDebugUtilsMessengerEXT(render_vkInstance, &vkDebugUtilsMessengerCreateInfo, nullptr, &render_vkDebugUtilsMessenger));
	
	vkGetDeviceQueue(render_vkDevice, render_physicalDevice->graphicsQueueFamily, 0, &render_vkGraphicsQueue);
	vkGetDeviceQueue(render_vkDevice, render_physicalDevice->presentQueueFamily, 0, &render_vkPresentQueue);

	//----------------------------------------------------------------------------------------------

	const Rect windowRect = Window_GetRect();
	VkSwapchainCreateInfoKHR vkSwapchainCreateInfo = {
		.sType                 = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
		.pNext                 = nullptr,
		.flags                 = 0,
		.surface               = render_vkSurface,
		.minImageCount         = Clamp(render_physicalDevice->vkSurfaceCapabilities.minImageCount, 2u, render_physicalDevice->vkSurfaceCapabilities.maxImageCount),
		.imageFormat           = render_physicalDevice->vkSwapchainFormat,
		.imageColorSpace       = render_physicalDevice->vkSwapchainColorSpace,
		.imageExtent           = { .width = windowRect.w, .height = windowRect.h },
		.imageArrayLayers      = 1,
		.imageUsage            = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT ,
		.imageSharingMode      = VK_SHARING_MODE_EXCLUSIVE,
		.queueFamilyIndexCount = 0,
		.pQueueFamilyIndices   = nullptr,
		.preTransform          = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
		.compositeAlpha        = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
		.presentMode           = render_physicalDevice->vkSwapchainPresentMode,
		.clipped               = VK_TRUE,
		.oldSwapchain          = VK_NULL_HANDLE,
	};
	const u32 queueFamilyIndices[] = { render_physicalDevice->graphicsQueueFamily, render_physicalDevice->presentQueueFamily };
	if (render_physicalDevice->graphicsQueueFamily != render_physicalDevice->presentQueueFamily) {
		vkSwapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
		vkSwapchainCreateInfo.queueFamilyIndexCount = 2;
		vkSwapchainCreateInfo.pQueueFamilyIndices = queueFamilyIndices;
	}
	Err_CheckVk(vkCreateSwapchainKHR(render_vkDevice, &vkSwapchainCreateInfo, render_vkAllocationCallbacksTODOFIXME, &render_vkSwapchain));

	//----------------------------------------------------------------------------------------------

	Err_CheckVk(vkGetSwapchainImagesKHR(render_vkDevice, render_vkSwapchain, &render_vkSwapchainImagesLen, nullptr));
	render_vkSwapchainImages = (VkImage*)render_allocator->Alloc(render_vkSwapchainImagesLen * sizeof(VkImage));
	Err_CheckVk(vkGetSwapchainImagesKHR(render_vkDevice, render_vkSwapchain, &render_vkSwapchainImagesLen, render_vkSwapchainImages));

	//----------------------------------------------------------------------------------------------

	render_vkSwapchainImageViewsLen = render_vkSwapchainImagesLen;
	render_vkSwapchainImageViews = (VkImageView*)render_allocator->Alloc(render_vkSwapchainImageViewsLen * sizeof(VkImageView));
	VkImageViewCreateInfo vkImageViewCreateInfo = {
		.sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.pNext            = nullptr,
		.flags            = 0,
		.viewType         = VK_IMAGE_VIEW_TYPE_2D,
		.format           = render_physicalDevice->vkSwapchainFormat,
		.components       = { .r = VK_COMPONENT_SWIZZLE_IDENTITY, .g = VK_COMPONENT_SWIZZLE_IDENTITY, .b = VK_COMPONENT_SWIZZLE_IDENTITY, .a = VK_COMPONENT_SWIZZLE_IDENTITY },
		.subresourceRange = { .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .baseMipLevel = 0, .levelCount = 1, .baseArrayLayer = 0, .layerCount = 1 },
	};
	for (u32 i = 0; i < render_vkSwapchainImagesLen; ++i) {
		vkImageViewCreateInfo.image = render_vkSwapchainImages[i];
		Err_CheckVk(vkCreateImageView(render_vkDevice, &vkImageViewCreateInfo, render_vkAllocationCallbacksTODOFIXME, &render_vkSwapchainImageViews[i]));
	}

	//----------------------------------------------------------------------------------------------

	Res<File> vf = File_Open("C:/Code/JC/Shaders/vert.spirv"); if (!vf) { return vf.err; }
	Res<File_Attrs> vfa = File_GetAttrs(vf.val); if (!vfa) { File_Close(vf.val); return vfa.err; }
	void* vbuf = mem_scratch->Alloc(vfa.val.len);
	Res<u64> vfr = File_Read(vf.val, vbuf, vfa.val.len); if (!vfr) { File_Close(vf.val); return vfr.err; }
	File_Close(vf.val);
	VkShaderModuleCreateInfo vkVertShaderModuleCreateInfo = {
		.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		.pNext    = nullptr,
		.flags    = 0,
		.codeSize = vfa.val.len,
		.pCode    = (const uint32_t*)vbuf,
	};
	Err_CheckVk(vkCreateShaderModule(render_vkDevice, &vkVertShaderModuleCreateInfo, render_vkAllocationCallbacksTODOFIXME, &render_vkVertShaderModule));

	//----------------------------------------------------------------------------------------------

	Res<File> ff = File_Open("C:/Code/JC/Shaders/frag.spirv"); if (!ff) { return ff.err; }
	Res<File_Attrs> ffa = File_GetAttrs(ff.val); if (!ffa) { File_Close(ff.val); return ffa.err; }
	void* fbuf = mem_scratch->Alloc(ffa.val.len);
	Res<u64> ffr = File_Read(ff.val, fbuf, ffa.val.len); if (!ffr) { File_Close(ff.val); return ffr.err; }
	File_Close(ff.val);
	VkShaderModuleCreateInfo vkFragShaderModuleCreateInfo = {
		.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		.pNext    = nullptr,
		.flags    = 0,
		.codeSize = ffa.val.len,
		.pCode    = (const uint32_t*)fbuf,
	};
	Err_CheckVk(vkCreateShaderModule(render_vkDevice, &vkFragShaderModuleCreateInfo, render_vkAllocationCallbacksTODOFIXME, &render_vkFragShaderModule));

	//----------------------------------------------------------------------------------------------

	VkDescriptorSetLayoutBinding vkDescriptorSetLayoutBinding = {
		.binding            = 0,
		.descriptorType     = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		.descriptorCount    = 1,
		.stageFlags         = VK_SHADER_STAGE_VERTEX_BIT,
		.pImmutableSamplers = nullptr,
	};
	VkDescriptorSetLayoutCreateInfo vkDescriptorSetLayoutCreateInfo = {
		.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		.pNext        = nullptr,
		.flags        = 0,
		.bindingCount = 1,
		.pBindings    = &vkDescriptorSetLayoutBinding,
	};
	Err_CheckVk(vkCreateDescriptorSetLayout(render_vkDevice, &vkDescriptorSetLayoutCreateInfo, render_vkAllocationCallbacksTODOFIXME, &render_vkDescriptorSetLayout));

	//----------------------------------------------------------------------------------------------

	VkPipelineLayoutCreateInfo vkPipelineLayoutCreateInfo = {
		.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.pNext                  = nullptr,
		.flags                  = 0,
		.setLayoutCount         = 1,
		.pSetLayouts            = &render_vkDescriptorSetLayout,
		.pushConstantRangeCount = 0,
		.pPushConstantRanges    = nullptr,
	};
	Err_CheckVk(vkCreatePipelineLayout(render_vkDevice, &vkPipelineLayoutCreateInfo, render_vkAllocationCallbacksTODOFIXME, &render_vkPipelineLayout));

	//----------------------------------------------------------------------------------------------

	VkAttachmentDescription vkAttachmentDescription = {
		.flags          = 0,
		.format         = render_physicalDevice->vkSwapchainFormat,
		.samples        = VK_SAMPLE_COUNT_1_BIT,
		.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR,
		.storeOp        = VK_ATTACHMENT_STORE_OP_STORE,
		.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
		.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
		.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED,
		.finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
	};

	VkAttachmentReference vkColorAttachmentReference = {
		.attachment = 0,
		.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
	};

	VkSubpassDescription vkSubpassDescription = {
		.flags                   = 0,
		.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS,
		.inputAttachmentCount    = 0,
		.pInputAttachments       = nullptr,
		.colorAttachmentCount    = 1,
		.pColorAttachments       = &vkColorAttachmentReference,
		.pResolveAttachments     = nullptr,
		.pDepthStencilAttachment = nullptr,
		.preserveAttachmentCount = 0,
		.pPreserveAttachments    = nullptr,
	};

	VkSubpassDependency vkSubpassDependency = {
		.srcSubpass      = VK_SUBPASS_EXTERNAL,
		.dstSubpass      = 0,
		.srcStageMask    = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		.dstStageMask    = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		.srcAccessMask   = 0,
		.dstAccessMask   = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
		.dependencyFlags = 0,
	};

	VkRenderPassCreateInfo vkRenderPassCreateInfo = {
		.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
		.pNext           = nullptr,
		.flags           = 0,
		.attachmentCount = 1,
		.pAttachments    = &vkAttachmentDescription,
		.subpassCount    = 1,
		.pSubpasses      = &vkSubpassDescription,
		.dependencyCount = 0,
		.pDependencies   = nullptr,
	};
	Err_CheckVk(vkCreateRenderPass(render_vkDevice, &vkRenderPassCreateInfo, render_vkAllocationCallbacksTODOFIXME, &render_vkRenderPass));
	
	//----------------------------------------------------------------------------------------------

	VkPipelineShaderStageCreateInfo vkPipelineShaderStageCreateInfo[2] = {
		{
			.sType               = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.pNext               = nullptr,
			.flags               = 0,
			.stage               = VK_SHADER_STAGE_VERTEX_BIT,
			.module              = render_vkVertShaderModule,
			.pName               = "main",
			.pSpecializationInfo = nullptr,
		},
		{
			.sType               = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.pNext               = nullptr,
			.flags               = 0,
			.stage               = VK_SHADER_STAGE_FRAGMENT_BIT,
			.module              = render_vkFragShaderModule,
			.pName               = "main",
			.pSpecializationInfo = nullptr,
		},
	};

	VkVertexInputBindingDescription vkVertexInputBindingDescription = {
		.binding   = 0,
		.stride    = sizeof(Vertex),
		.inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
	};

	VkVertexInputAttributeDescription vkVertexInputAttributeDescriptions[2] = {
		{
			.location = 0,
			.binding  = 0,
			.format   = VK_FORMAT_R32G32B32_SFLOAT,
			.offset   = 0,
		},
		{
			.location = 1,
			.binding  = 0,
			.format   = VK_FORMAT_R32G32B32_SFLOAT,
			.offset   = offsetof(Vertex, color),	// TODO: define this in common
		},
	};
	
	VkPipelineVertexInputStateCreateInfo vkPipelineVertexInputStateCreateInfo {
		.sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
		.pNext                           = nullptr,
		.flags                           = 0,
		.vertexBindingDescriptionCount   = 1,
		.pVertexBindingDescriptions      = &vkVertexInputBindingDescription,
		.vertexAttributeDescriptionCount = 2,
		.pVertexAttributeDescriptions    = vkVertexInputAttributeDescriptions,
	};

	VkPipelineInputAssemblyStateCreateInfo vkPipelineInputAssemblyStateCreateInfo= {
		.sType                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
		.pNext                  = nullptr,
		.flags                  = 0,
		.topology               = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
		.primitiveRestartEnable = VK_FALSE,
	};

	VkPipelineTessellationStateCreateInfo vkPipelineTessellationStateCreateInfo = {
		.sType              = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO,
		.pNext              = nullptr,
		.flags              = 0,
		.patchControlPoints = 0,
	};

	VkViewport vkViewport = {
		.x        = 0.0f,
		.y        = 0.0f,
		.width    = (float)windowRect.w,
		.height   = (float)windowRect.h,
		.minDepth = 0.0f,
		.maxDepth = 1.0f,
	};

	VkRect2D vkScissorRect = {
		.offset = { .x = 0, .y = 0 },
		.extent = { .width = windowRect.w, .height = windowRect.h },
	};

	VkPipelineViewportStateCreateInfo vkPipelineViewportStateCreateInfo = {
		.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
		.pNext         = nullptr,
		.flags         = 0,
		.viewportCount = 1,
		.pViewports    = &vkViewport,
		.scissorCount  = 1,
		.pScissors     = &vkScissorRect,
	};

	VkPipelineRasterizationStateCreateInfo vkPipelineRasterizationStateCreateInfo = {
		.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
		.pNext                   = nullptr,
		.flags                   = 0,
		.depthClampEnable        = VK_FALSE,
		.rasterizerDiscardEnable = VK_FALSE,
		.polygonMode             = VK_POLYGON_MODE_FILL,
		.cullMode                = VK_CULL_MODE_NONE,//VK_CULL_MODE_BACK_BIT,
		.frontFace               = VK_FRONT_FACE_CLOCKWISE,
		.depthBiasEnable         = VK_FALSE,
		.depthBiasConstantFactor = 0.0f,
		.depthBiasClamp          = 0.0f,
		.depthBiasSlopeFactor    = 0.0f,
		.lineWidth               = 1.0f,
	};

	VkPipelineMultisampleStateCreateInfo vkPipelineMultisampleStateCreateInfo = {
		.sType                 = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
		.pNext                 = nullptr,
		.flags                 = 0,
		.rasterizationSamples  = VK_SAMPLE_COUNT_1_BIT,
		.sampleShadingEnable   = VK_FALSE,
		.minSampleShading      = 1.0f,
		.pSampleMask           = nullptr,
		.alphaToCoverageEnable = VK_FALSE,
		.alphaToOneEnable      = VK_FALSE,
	};

	/*
	VkPipelineDepthStencilStateCreateInfo vkPipelineDepthStencilStateCreateInfo = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
		.pNext = nullptr,
		VkPipelineDepthStencilStateCreateFlags    .flags = ,
		VkBool32                                  .depthTestEnable = ,
		VkBool32                                  .depthWriteEnable = ,
		VkCompareOp                               .depthCompareOp = ,
		VkBool32                                  .depthBoundsTestEnable = ,
		VkBool32                                  .stencilTestEnable = ,
		VkStencilOpState                          .front = ,
		VkStencilOpState                          .back = ,
		float                                     .minDepthBounds = ,
		float                                     .maxDepthBounds = ,
	};
	*/

	VkPipelineColorBlendAttachmentState vkPipelineColorBlendAttachmentState = {
		.blendEnable = VK_TRUE,
		.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
		.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
		.colorBlendOp = VK_BLEND_OP_ADD,
		.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
		.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
		.alphaBlendOp        = VK_BLEND_OP_ADD,
		.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
	};

	VkPipelineColorBlendStateCreateInfo vkPipelineColorBlendStateCreateInfo = {
		.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
		.pNext           = nullptr,
		.flags           = 0,
		.logicOpEnable   = VK_FALSE,
		.logicOp         = VK_LOGIC_OP_COPY,
		.attachmentCount = 1,
		.pAttachments    = &vkPipelineColorBlendAttachmentState,
		.blendConstants  = { 0.0f, 0.0f, 0.0f, 0.0f },
	};

	VkDynamicState vkDynamicStates[2] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
	VkPipelineDynamicStateCreateInfo vkPipelineDynamicStateCreateInfo = {
		.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
		.pNext             = nullptr,
		.flags             = 0,
		.dynamicStateCount = 2,
		.pDynamicStates    = vkDynamicStates,
	};

	VkGraphicsPipelineCreateInfo vkGraphicsPipelineCreateInfo = {
		.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
		.pNext               = nullptr,
		.flags               = 0,
		.stageCount          = 2,
		.pStages             = vkPipelineShaderStageCreateInfo,
		.pVertexInputState   = &vkPipelineVertexInputStateCreateInfo,
		.pInputAssemblyState = &vkPipelineInputAssemblyStateCreateInfo,
		.pTessellationState  = &vkPipelineTessellationStateCreateInfo,
		.pViewportState      = &vkPipelineViewportStateCreateInfo,
		.pRasterizationState = &vkPipelineRasterizationStateCreateInfo,
		.pMultisampleState   = &vkPipelineMultisampleStateCreateInfo,
		.pDepthStencilState  = nullptr,
		.pColorBlendState    = &vkPipelineColorBlendStateCreateInfo,
		.pDynamicState       = &vkPipelineDynamicStateCreateInfo,
		.layout              = render_vkPipelineLayout,
		.renderPass          = render_vkRenderPass,
		.subpass             = 0,
		.basePipelineHandle  = VK_NULL_HANDLE,
		.basePipelineIndex   = 0,
	};
	Err_CheckVk(vkCreateGraphicsPipelines(render_vkDevice, VK_NULL_HANDLE, 1, &vkGraphicsPipelineCreateInfo, render_vkAllocationCallbacksTODOFIXME, &render_vkPipeline));

	//----------------------------------------------------------------------------------------------

	VkSemaphoreCreateInfo vkSemaphoreCreateInfo = {
		.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
	};
	VkFenceCreateInfo vkFenceCreateInfo = {
		.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
		.pNext = nullptr,
		.flags = VK_FENCE_CREATE_SIGNALED_BIT,
	};
	for (u32 i = 0; i < Render_MaxFrames; ++i) {
		Err_CheckVk(vkCreateSemaphore(render_vkDevice, &vkSemaphoreCreateInfo, render_vkAllocationCallbacksTODOFIXME, &render_vkSwapchainSemaphores[i]));
		Err_CheckVk(vkCreateSemaphore(render_vkDevice, &vkSemaphoreCreateInfo, render_vkAllocationCallbacksTODOFIXME, &render_vkPresentSemaphores[i]));
		Err_CheckVk(vkCreateFence(render_vkDevice, &vkFenceCreateInfo, render_vkAllocationCallbacksTODOFIXME, &render_vkFrameFences[i]));
	}

	//----------------------------------------------------------------------------------------------

	render_vkFramebuffersLen = render_vkSwapchainImagesLen;
	render_vkFramebuffers = (VkFramebuffer*)render_allocator->Alloc(render_vkFramebuffersLen * sizeof(VkFramebuffer));
	for (u32 i = 0; i < render_vkFramebuffersLen; ++i) {
		VkFramebufferCreateInfo vkFramebufferCreateInfo = {
			.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
			.pNext           = 0,
			.flags           = 0,
			.renderPass      = render_vkRenderPass,
			.attachmentCount = 1,
			.pAttachments    = &render_vkSwapchainImageViews[i],
			.width           = windowRect.w,
			.height          = windowRect.h,
			.layers          = 1,
		};
		Err_CheckVk(vkCreateFramebuffer(render_vkDevice, &vkFramebufferCreateInfo, render_vkAllocationCallbacksTODOFIXME, &render_vkFramebuffers[i]));
	}

	//----------------------------------------------------------------------------------------------

	VkCommandPoolCreateInfo vkCommandPoolCreateInfo = {
		.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
		.pNext            = nullptr,
		.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
		.queueFamilyIndex = render_physicalDevice->graphicsQueueFamily,
	};
	Err_CheckVk(vkCreateCommandPool(render_vkDevice, &vkCommandPoolCreateInfo, render_vkAllocationCallbacksTODOFIXME, &render_vkCommandPool));

	//----------------------------------------------------------------------------------------------

	VkCommandBufferAllocateInfo vkCommandBufferAllocateInfo = {
		.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.pNext              = nullptr,
		.commandPool        = render_vkCommandPool,
		.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		.commandBufferCount = (u32)Render_MaxFrames,
	};
	Err_CheckVk(vkAllocateCommandBuffers(render_vkDevice, &vkCommandBufferAllocateInfo, render_vkCommandBuffers));

	//----------------------------------------------------------------------------------------------

	auto CreateBuffer = [](u64 size, VkBufferUsageFlags vkBufferUsageFlags, VkMemoryPropertyFlags vkMemoryPropertyFlags, VkBuffer* vkBuffer, VkDeviceMemory* vkDeviceMemory) -> Res<> {
		VkBufferCreateInfo vkBufferCreateInfo = {
			.sType                 = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
			.pNext                 = nullptr,
			.flags                 = 0,
			.size                  = size,
			.usage                 = vkBufferUsageFlags,
			.sharingMode           = VK_SHARING_MODE_EXCLUSIVE,
			.queueFamilyIndexCount = 0,
			.pQueueFamilyIndices   = nullptr,
		};
		Err_CheckVk(vkCreateBuffer(render_vkDevice, &vkBufferCreateInfo, render_vkAllocationCallbacksTODOFIXME, vkBuffer));

		VkMemoryRequirements vkMemoryRequirements;
		vkGetBufferMemoryRequirements(render_vkDevice, *vkBuffer, &vkMemoryRequirements);

		u32 memoryTypeIndex = 0xffffffff;
		for (u32 i = 0; i < render_physicalDevice->vkPhysicalDeviceMemoryProperties.memoryTypeCount; ++i) {
			if (
				(vkMemoryRequirements.memoryTypeBits & (1 << i)) &&
				(render_physicalDevice->vkPhysicalDeviceMemoryProperties.memoryTypes[i].propertyFlags & vkMemoryPropertyFlags)
			) {
				memoryTypeIndex = i;
				break;
			}
		}
		Err_Check(memoryTypeIndex != 0xffffffff, Render_ErrMem);

		VkMemoryAllocateInfo vkMemoryAllocateInfo = {
			.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
			.pNext           = nullptr,
			.allocationSize  = vkMemoryRequirements.size,
			.memoryTypeIndex = memoryTypeIndex,
		};
		Err_CheckVk(vkAllocateMemory(render_vkDevice, &vkMemoryAllocateInfo, render_vkAllocationCallbacksTODOFIXME, vkDeviceMemory));

		Err_CheckVk(vkBindBufferMemory(render_vkDevice, *vkBuffer, *vkDeviceMemory, 0));

		return Ok();
	};
	
	//----------------------------------------------------------------------------------------------

	auto CopyBuffer = [](VkBuffer src, VkBuffer dst, u64 size) -> Res<> {
		VkCommandBufferAllocateInfo vkCommandBufferAllocateInfo = {
			.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
			.pNext              = nullptr,
			.commandPool        = render_vkCommandPool,
			.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
			.commandBufferCount = 1,
		};
		VkCommandBuffer vkStagingCommandBuffer = VK_NULL_HANDLE;
		Err_CheckVk(vkAllocateCommandBuffers(render_vkDevice, &vkCommandBufferAllocateInfo, &vkStagingCommandBuffer));

		VkCommandBufferBeginInfo vkCommandBufferBeginInfo = {
			.sType            = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
			.pNext            = nullptr,
			.flags            = 0,
			.pInheritanceInfo = nullptr,
		};
		Err_CheckVk(vkBeginCommandBuffer(vkStagingCommandBuffer, &vkCommandBufferBeginInfo));
		VkBufferCopy vkBufferCopy = {
			.srcOffset = 0,
			.dstOffset = 0,
			.size      = size,
		};
		vkCmdCopyBuffer(vkStagingCommandBuffer, src, dst, 1, &vkBufferCopy);
		vkEndCommandBuffer(vkStagingCommandBuffer);
		VkSubmitInfo vkSubmitInfo = {
			.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO,
			.pNext                = nullptr,
			.waitSemaphoreCount   = 0,
			.pWaitSemaphores      = nullptr,
			.pWaitDstStageMask    = nullptr,
			.commandBufferCount   = 1,
			.pCommandBuffers      = &vkStagingCommandBuffer,
			.signalSemaphoreCount = 0,
			.pSignalSemaphores    = nullptr,
		};
		Err_CheckVk(vkQueueSubmit(render_vkGraphicsQueue, 1, &vkSubmitInfo, VK_NULL_HANDLE));

		vkQueueWaitIdle(render_vkGraphicsQueue);

		vkFreeCommandBuffers(render_vkDevice, render_vkCommandPool, 1, &vkStagingCommandBuffer);

		return Ok();
	};

	//----------------------------------------------------------------------------------------------

	VkBuffer vkStagingBuffer = VK_NULL_HANDLE;
	VkDeviceMemory vkStagingBufferDeviceMemory = VK_NULL_HANDLE;
	Err_Try(CreateBuffer(
		sizeof(verts),
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		&vkStagingBuffer,
		&vkStagingBufferDeviceMemory
	));
	void* vertsBuf = nullptr;
	Err_CheckVk(vkMapMemory(render_vkDevice, vkStagingBufferDeviceMemory, 0, sizeof(verts), 0, &vertsBuf));
	memcpy(vertsBuf, verts, sizeof(verts));
	vkUnmapMemory(render_vkDevice, vkStagingBufferDeviceMemory);

	Err_Try(CreateBuffer(
		sizeof(verts),
		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT ,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		&render_vkVertexBuffer,
		&render_vkVertexBufferDeviceMemory
	));

	Err_Try(CopyBuffer(vkStagingBuffer, render_vkVertexBuffer, sizeof(verts)));

	vkDestroyBuffer(render_vkDevice, vkStagingBuffer, render_vkAllocationCallbacksTODOFIXME);
	vkFreeMemory(render_vkDevice, vkStagingBufferDeviceMemory, render_vkAllocationCallbacksTODOFIXME);

	//----------------------------------------------------------------------------------------------

	Err_Try(CreateBuffer(
		sizeof(indices),
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		&vkStagingBuffer,
		&vkStagingBufferDeviceMemory
	));
	void* indBuf = nullptr;
	Err_CheckVk(vkMapMemory(render_vkDevice, vkStagingBufferDeviceMemory, 0, sizeof(indices), 0, &indBuf));
	memcpy(indBuf, indices, sizeof(indices));
	vkUnmapMemory(render_vkDevice, vkStagingBufferDeviceMemory);

	Err_Try(CreateBuffer(
		sizeof(indices),
		VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT ,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		&render_vkIndexBuffer,
		&render_vkIndexBufferDeviceMemory
	));

	Err_Try(CopyBuffer(vkStagingBuffer, render_vkIndexBuffer, sizeof(indices)));

	vkDestroyBuffer(render_vkDevice, vkStagingBuffer, render_vkAllocationCallbacksTODOFIXME);
	vkFreeMemory(render_vkDevice, vkStagingBufferDeviceMemory, render_vkAllocationCallbacksTODOFIXME);

	//----------------------------------------------------------------------------------------------

	render_vkUniformBuffersLen = Render_MaxFrames;
	render_vkUniformBuffers = (VkBuffer*)render_allocator->Alloc(Render_MaxFrames * sizeof(render_vkUniformBuffers[0]));
	render_vkUniformBufferDeviceMemories = (VkDeviceMemory*)render_allocator->Alloc(Render_MaxFrames * sizeof(render_vkUniformBufferDeviceMemories[0]));
	render_vkUniformBufferPtrs = (void**)render_allocator->Alloc(Render_MaxFrames * sizeof(render_vkUniformBufferPtrs[0]));

	for (u32 i = 0; i < Render_MaxFrames; ++i) {
		Err_Try(CreateBuffer(
			sizeof(Uniform),
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			&render_vkUniformBuffers[i],
			&render_vkUniformBufferDeviceMemories[i]
		));
		Err_CheckVk(vkMapMemory(render_vkDevice, render_vkUniformBufferDeviceMemories[i], 0, sizeof(Uniform), 0, &render_vkUniformBufferPtrs[i]));
	}

	//----------------------------------------------------------------------------------------------

	VkDescriptorPoolSize vkDescriptorPoolSize = {
		.type            = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		.descriptorCount = Render_MaxFrames,
	};
	VkDescriptorPoolCreateInfo vkDescriptorPoolCreateInfo =  {
		.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
		.pNext         = nullptr,
		.flags         = 0,
		.maxSets       = Render_MaxFrames,
		.poolSizeCount = 1,
		.pPoolSizes    = &vkDescriptorPoolSize,

	};
	Err_CheckVk(vkCreateDescriptorPool(render_vkDevice, &vkDescriptorPoolCreateInfo, render_vkAllocationCallbacksTODOFIXME, &render_vkDescriptorPool));

	render_vkDescriptorSets = (VkDescriptorSet*)render_allocator->Alloc(Render_MaxFrames * sizeof(render_vkDescriptorSets[0]));
	VkDescriptorSetLayout* vkDescriptorSetLayouts = (VkDescriptorSetLayout*)mem_scratch->Alloc(Render_MaxFrames * sizeof(VkDescriptorSetLayout));
	for (u32 i = 0; i < Render_MaxFrames; ++i) {
		vkDescriptorSetLayouts[i] = render_vkDescriptorSetLayout;
	}
	VkDescriptorSetAllocateInfo vkDescriptorSetAllocateInfo = {
		.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
		.pNext              = nullptr,
		.descriptorPool     = render_vkDescriptorPool,
		.descriptorSetCount = Render_MaxFrames,
		.pSetLayouts        = vkDescriptorSetLayouts,
	};
	Err_CheckVk(vkAllocateDescriptorSets(render_vkDevice, &vkDescriptorSetAllocateInfo, render_vkDescriptorSets));

	//----------------------------------------------------------------------------------------------

	for (u32 i = 0; i < Render_MaxFrames; ++i) {
		VkDescriptorBufferInfo vkDescriptorBufferInfo = {
			.buffer = render_vkUniformBuffers[i],
			.offset = 0,
			.range  = sizeof(Uniform),
		};
		VkWriteDescriptorSet vkWriteDescriptorSet = {
			.sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.pNext            = nullptr,
			.dstSet           = render_vkDescriptorSets[i],
			.dstBinding       = 0,
			.dstArrayElement  = 0,
			.descriptorCount  = 1,
			.descriptorType   = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			.pImageInfo       = nullptr,
			.pBufferInfo      = &vkDescriptorBufferInfo,
			.pTexelBufferView = nullptr,
		};
		vkUpdateDescriptorSets(render_vkDevice, 1, &vkWriteDescriptorSet, 0, nullptr);

	}

	//----------------------------------------------------------------------------------------------
	//----------------------------------------------------------------------------------------------
	//----------------------------------------------------------------------------------------------
	//----------------------------------------------------------------------------------------------

	return Ok();
}

//----------------------------------------------------------------------------------------------

void Render_Shutdown() {
	if (render_vkDevice != VK_NULL_HANDLE) {
		vkDeviceWaitIdle(render_vkDevice);
	}

	render_allocator->Free(render_vkDescriptorSets, Render_MaxFrames * sizeof(render_vkDescriptorSets[0]));
	render_vkDescriptorSets = nullptr;

	if (render_vkDescriptorPool != VK_NULL_HANDLE) {
		vkDestroyDescriptorPool(render_vkDevice, render_vkDescriptorPool, render_vkAllocationCallbacksTODOFIXME);
		render_vkDescriptorPool = VK_NULL_HANDLE;
	}

	if (render_vkDescriptorSetLayout != VK_NULL_HANDLE) {
		vkDestroyDescriptorSetLayout(render_vkDevice, render_vkDescriptorSetLayout, render_vkAllocationCallbacksTODOFIXME);
		render_vkDescriptorSetLayout = VK_NULL_HANDLE;
	}

	for (u32 i = 0; i < render_vkUniformBuffersLen; ++i) {
		vkUnmapMemory(render_vkDevice, render_vkUniformBufferDeviceMemories[i]);
		vkDestroyBuffer(render_vkDevice, render_vkUniformBuffers[i], render_vkAllocationCallbacksTODOFIXME);
		vkFreeMemory(render_vkDevice, render_vkUniformBufferDeviceMemories[i], render_vkAllocationCallbacksTODOFIXME);
	}
	render_allocator->Free(render_vkUniformBuffers, render_vkUniformBuffersLen * sizeof(render_vkUniformBuffers[0]));
	render_allocator->Free(render_vkUniformBufferDeviceMemories, render_vkUniformBuffersLen * sizeof(render_vkUniformBufferDeviceMemories[0]));
	render_allocator->Free(render_vkUniformBufferPtrs, render_vkUniformBuffersLen * sizeof(render_vkUniformBufferPtrs[0]));
	render_vkUniformBuffers = nullptr;
	render_vkUniformBufferDeviceMemories = nullptr;
	render_vkUniformBufferPtrs = nullptr;
	render_vkUniformBuffersLen = 0;

	if (render_vkIndexBuffer != VK_NULL_HANDLE) {
		vkDestroyBuffer(render_vkDevice, render_vkIndexBuffer, render_vkAllocationCallbacksTODOFIXME);
		render_vkIndexBuffer = VK_NULL_HANDLE;
	}

	if (render_vkIndexBufferDeviceMemory != VK_NULL_HANDLE) {
		vkFreeMemory(render_vkDevice, render_vkIndexBufferDeviceMemory, render_vkAllocationCallbacksTODOFIXME);
		render_vkIndexBufferDeviceMemory = VK_NULL_HANDLE;
	}

	if (render_vkVertexBuffer != VK_NULL_HANDLE) {
		vkDestroyBuffer(render_vkDevice, render_vkVertexBuffer, render_vkAllocationCallbacksTODOFIXME);
		render_vkVertexBuffer = VK_NULL_HANDLE;
	}

	if (render_vkVertexBufferDeviceMemory != VK_NULL_HANDLE) {
		vkFreeMemory(render_vkDevice, render_vkVertexBufferDeviceMemory, render_vkAllocationCallbacksTODOFIXME);
		render_vkVertexBufferDeviceMemory = VK_NULL_HANDLE;
	}

	for (u32 i = 0; i < Render_MaxFrames; ++i) {
		if (render_vkFrameFences[i] != VK_NULL_HANDLE) {
			vkDestroyFence(render_vkDevice, render_vkFrameFences[i], render_vkAllocationCallbacksTODOFIXME);
			render_vkFrameFences[i] = VK_NULL_HANDLE;
		}
		if (render_vkPresentSemaphores[i] != VK_NULL_HANDLE) {
			vkDestroySemaphore(render_vkDevice, render_vkPresentSemaphores[i], render_vkAllocationCallbacksTODOFIXME);
			render_vkPresentSemaphores[i] = VK_NULL_HANDLE;
		}
	}

	for (u32 i = 0; i < Render_MaxFrames; ++i) {
		if (render_vkSwapchainSemaphores[i] != VK_NULL_HANDLE) {
			vkDestroySemaphore(render_vkDevice, render_vkSwapchainSemaphores[i], render_vkAllocationCallbacksTODOFIXME);
			render_vkSwapchainSemaphores[i] = VK_NULL_HANDLE;
		}
	}

	if (render_vkCommandPool != VK_NULL_HANDLE) {
		vkDestroyCommandPool(render_vkDevice, render_vkCommandPool , render_vkAllocationCallbacksTODOFIXME);
		render_vkCommandPool  = VK_NULL_HANDLE;
	}

	for (u32 i = 0; i < render_vkFramebuffersLen; ++i) {
		vkDestroyFramebuffer(render_vkDevice, render_vkFramebuffers[i], render_vkAllocationCallbacksTODOFIXME);
	}
	render_allocator->Free(render_vkFramebuffers, render_vkFramebuffersLen * sizeof(VkFramebuffer));
	render_vkFramebuffers = nullptr;
	render_vkFramebuffersLen = 0;

	if (render_vkPipeline != VK_NULL_HANDLE) {
		vkDestroyPipeline(render_vkDevice, render_vkPipeline, render_vkAllocationCallbacksTODOFIXME);
		render_vkPipeline = VK_NULL_HANDLE;
	}

	if (render_vkRenderPass != VK_NULL_HANDLE) {
		vkDestroyRenderPass(render_vkDevice, render_vkRenderPass, render_vkAllocationCallbacksTODOFIXME);
		render_vkRenderPass = VK_NULL_HANDLE;
	}

	if (render_vkPipelineLayout != VK_NULL_HANDLE) {
		vkDestroyPipelineLayout(render_vkDevice, render_vkPipelineLayout, render_vkAllocationCallbacksTODOFIXME);
		render_vkPipelineLayout = VK_NULL_HANDLE;
	}

	if (render_vkDescriptorSetLayout != VK_NULL_HANDLE) {
		vkDestroyDescriptorSetLayout(render_vkDevice, render_vkDescriptorSetLayout, render_vkAllocationCallbacksTODOFIXME);
		render_vkDescriptorSetLayout = VK_NULL_HANDLE;
	}

	if (render_vkFragShaderModule != VK_NULL_HANDLE) {
		vkDestroyShaderModule(render_vkDevice, render_vkFragShaderModule, render_vkAllocationCallbacksTODOFIXME);
		render_vkFragShaderModule = VK_NULL_HANDLE;
	}
	if (render_vkVertShaderModule != VK_NULL_HANDLE) {
		vkDestroyShaderModule(render_vkDevice, render_vkVertShaderModule, render_vkAllocationCallbacksTODOFIXME);
		render_vkVertShaderModule = VK_NULL_HANDLE;
	}

	for (u32 i = 0; i < render_vkSwapchainImageViewsLen; ++i) {
		vkDestroyImageView(render_vkDevice, render_vkSwapchainImageViews[i], render_vkAllocationCallbacksTODOFIXME);
	}
	render_allocator->Free(render_vkSwapchainImageViews, render_vkSwapchainImageViewsLen * sizeof(VkImageView));
	render_vkSwapchainImageViews = nullptr;
	render_vkSwapchainImageViewsLen = 0;

	render_allocator->Free(render_vkSwapchainImages, render_vkSwapchainImagesLen * sizeof(VkImage));
	render_vkSwapchainImages = nullptr;
	render_vkSwapchainImagesLen = 0;

	if (render_vkSwapchain  != VK_NULL_HANDLE) {
		vkDestroySwapchainKHR(render_vkDevice, render_vkSwapchain, render_vkAllocationCallbacksTODOFIXME);
		render_vkSwapchain = VK_NULL_HANDLE;
	}
	// TODO: free render_physicalDevices
	if (render_vkDebugUtilsMessenger != VK_NULL_HANDLE) {
		vkDestroyDebugUtilsMessengerEXT(render_vkInstance, render_vkDebugUtilsMessenger, render_vkAllocationCallbacksTODOFIXME);
		render_vkDebugUtilsMessenger = VK_NULL_HANDLE;
	}
	if (render_vkDevice != VK_NULL_HANDLE) {
		vkDestroyDevice(render_vkDevice, render_vkAllocationCallbacksTODOFIXME);
		render_vkDevice = VK_NULL_HANDLE;
	}
	if (render_vkSurface != VK_NULL_HANDLE) {
		vkDestroySurfaceKHR(render_vkInstance, render_vkSurface, render_vkAllocationCallbacksTODOFIXME);
		render_vkSurface = VK_NULL_HANDLE;
	}
	if (render_vkInstance != VK_NULL_HANDLE) {
		vkDestroyInstance(render_vkInstance, render_vkAllocationCallbacksTODOFIXME);
		render_vkInstance = VK_NULL_HANDLE;
	}
	Render_FreeVkFns();
}

//----------------------------------------------------------------------------------------------

Res<> Render_Frame() {
	Err_CheckVk(vkWaitForFences(render_vkDevice, 1, &render_vkFrameFences[render_frame], VK_TRUE, 5000000000ull));	// 5 sec
	Err_CheckVk(vkResetFences(render_vkDevice, 1, &render_vkFrameFences[render_frame]));

	const Rect windowRect = Window_GetRect();

	Uniform u;
	u.model = Mat4_AngleAxis(render_time, Vec3_Make(0.0f, 0.0f, 1.0f));
	u.view = Mat4_LookAt(Vec3_Make(2.0f, 2.0f, 5.0f), Vec3_Make(0.0f, 0.0f, 0.0f), Vec3_Make(0.0f, 1.0f, 0.0f));
	u.proj = Mat4_Perspective(Math_Radians(45.0f), (float)windowRect.w / (float)windowRect.h, 0.1f, 10.0f);
	memcpy(render_vkUniformBufferPtrs[render_frame], &u, sizeof(u));

	u32 imageIndex = 0;
	Err_CheckVk(vkAcquireNextImageKHR(render_vkDevice, render_vkSwapchain, U64Max, render_vkSwapchainSemaphores[render_frame], VK_NULL_HANDLE, &imageIndex));
	// TODO: check returncode: VK_ERROR_OUT_OF_DATE?

	Err_CheckVk(vkResetCommandBuffer(render_vkCommandBuffers[render_frame], 0));

	VkCommandBufferBeginInfo vkCommandBufferBeginInfo = {
		.sType            = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.pNext            = nullptr,
		.flags            = 0,
		.pInheritanceInfo = nullptr,
	};
	Err_CheckVk(vkBeginCommandBuffer(render_vkCommandBuffers[render_frame], &vkCommandBufferBeginInfo));

	VkClearValue vkClearValue = {
		.color = { .float32 = { 0.0f, 0.0f, 0.0f, 1.0f } },
		// TODO: d/s
		//.depthStencil = { .depth = }
	};

	VkRenderPassBeginInfo vkRenderPassBeginInfo = {
		.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
		.pNext           = nullptr,
		.renderPass      = render_vkRenderPass,
		.framebuffer     = render_vkFramebuffers[imageIndex],
		.renderArea      = { .offset = { .x = 0, .y = 0 }, .extent = { .width = windowRect.w, .height = windowRect.h } },
		.clearValueCount = 1,
		.pClearValues    = &vkClearValue,
	};
	vkCmdBeginRenderPass(render_vkCommandBuffers[render_frame], &vkRenderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

	vkCmdBindPipeline(render_vkCommandBuffers[render_frame], VK_PIPELINE_BIND_POINT_GRAPHICS, render_vkPipeline);

	VkViewport vkViewport = {
		.x        = 0.0f,
		.y        = 0.0f,
		.width    = (float)windowRect.w,
		.height   = (float)windowRect.h,
		.minDepth = 0.0f,
		.maxDepth = 1.0f,
	};
	vkCmdSetViewport(render_vkCommandBuffers[render_frame], 0, 1, &vkViewport);

	VkRect2D vkScissorRect = {
		.offset = { .x = 0, .y = 0 },
		.extent = { .width = windowRect.w, .height = windowRect.h },
	};
	vkCmdSetScissor(render_vkCommandBuffers[render_frame], 0, 1, &vkScissorRect);

	VkDeviceSize offset = 0;
	vkCmdBindVertexBuffers(render_vkCommandBuffers[render_frame], 0, 1, &render_vkVertexBuffer, &offset);

	vkCmdBindIndexBuffer(render_vkCommandBuffers[render_frame], render_vkIndexBuffer, offset, VK_INDEX_TYPE_UINT16);

	vkCmdBindDescriptorSets(render_vkCommandBuffers[render_frame], VK_PIPELINE_BIND_POINT_GRAPHICS, render_vkPipelineLayout, 0, 1, &render_vkDescriptorSets[render_frame], 0, nullptr);

	vkCmdDrawIndexed(render_vkCommandBuffers[render_frame], sizeof(indices) / sizeof(indices[0]), 1, 0, 0, 0);

	vkCmdEndRenderPass(render_vkCommandBuffers[render_frame]);

	Err_CheckVk(vkEndCommandBuffer(render_vkCommandBuffers[render_frame]));

	VkPipelineStageFlags vkPipelineStageFlag = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	VkSubmitInfo vkSubmitInfo = {
		.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.pNext                = nullptr,
		.waitSemaphoreCount   = 1,
		.pWaitSemaphores      = &render_vkSwapchainSemaphores[render_frame],
		.pWaitDstStageMask    = &vkPipelineStageFlag,
		.commandBufferCount   = 1,
		.pCommandBuffers      = &render_vkCommandBuffers[render_frame],
		.signalSemaphoreCount = 1,
		.pSignalSemaphores    = &render_vkPresentSemaphores[render_frame],
	};
	Err_CheckVk(vkQueueSubmit(render_vkGraphicsQueue, 1, &vkSubmitInfo, render_vkFrameFences[render_frame]));

	VkPresentInfoKHR vkPresentInfo = {
		.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
		.pNext              = nullptr,
		.waitSemaphoreCount = 1,
		.pWaitSemaphores    = &render_vkPresentSemaphores[render_frame],
		.swapchainCount     = 1,
		.pSwapchains        = &render_vkSwapchain,
		.pImageIndices      = &imageIndex,
		.pResults           = nullptr,
	};
	Err_CheckVk(vkQueuePresentKHR(render_vkGraphicsQueue, &vkPresentInfo));

	render_frame = (render_frame + 1) % Render_MaxFrames;

	return Ok();
}