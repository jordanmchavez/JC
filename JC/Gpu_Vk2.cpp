#include "JC/Gpu_Vk.h"

#include "JC/Array.h"
#include "JC/Log.h"

namespace JC::Gpu2 {
using namespace JC::Gpu;

//--------------------------------------------------------------------------------------------------

DefErr(Gpu, VkApiVersion);

//--------------------------------------------------------------------------------------------------

static constexpr U32 MinVkApiVersion = VK_MAKE_API_VERSION(0, 1, 4, 0);

static Mem::Allocator*     allocator;
static Mem::TempAllocator* tempAllocator;
static Log::Logger*        logger;

//--------------------------------------------------------------------------------------------------

Res<> InitSwapchain() {
	vkGetPhysicalDeviceSurfaceCapabilities2KHR;
	vkGetPhysicalDeviceSurfaceFormats2KHR;
	vkGetPhysicalDeviceSurfacePresentModesKHR;
	if #format == 1 && format == VK_FORMAT_UNDEFINED {VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};
	else find first matching {
		{VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR},
		{VK_FORMAT_R8G8B8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR},
	};
	select present mode:
		if cfg.vsync: VK_PRESENT_MODE_FIFO_KHR;
		else prefer mailbox thenimmediate;
	}
	framesInFlight = clamp(minImageCount, 3, maxImageCount == 0 ? inf : maxImageCount);
	vkCreateSwapchainKHR(currentExtent, currentTransform, exclusive, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);
	vkGetSwapchainImagesKHR
	m_maxFramesInFlight = imageCount;  // can this fail to be different?
	for each image:
		vkCreateImageView;

	struct FrameResources {
		VkSemaphore imageAvailableSemaphore;
		VkSemaphore renderFinishedSemaphore;
	};
	static FrameResources m_frameResources[framesInFlight];
	VkCommandBuffer cmd = beginSingleTimeCommands;
	for each frame:
		m_frameResources[i].sems = vkCreateSemaphore;
		cmdTransitionImageLayout(VK_IMAGE_LAYOUT_UNDEFINED -> VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
	endSingleTimeCommands;
}

Res<> InitGBuffer() {
	static Image gBufferColor =  CreateImage(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);
	vkCreateImageView;
	static Image gBufferDepth = CreateImage(VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
	beginSingleTimeCommands;
		vkCreateImageView;
		cmdTransitionImageLayout(gBufferColor, VK_IMAGE_LAYOUT_UNDEFINED -> VK_IMAGE_LAYOUT_GENERAL);
		gBufferColor.layout = VK_IMAGE_LAYOUT_GENERAL;
		vkCmdClearColorImage;	// unsure why this is needed
	endSingleTimeCommands;
}

Res<> Init() {
	U32 apiVersion;
	vkEnumerateInstanceVersion(&apiVersion);
	if (apiVersion <= MinVkApiVersion) {
		return Err_VkApiVersion("version", apiVersion);
	}

	vkEnumerateInstanceExtensionProperties;
	const char* requiredInstanceExtensions[] = {
		VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
		VK_EXT_SURFACE_MAINTENANCE_1_EXTENSION_NAME,
		"VK_LAYER_KHRONOS_validation",
	};

	constexpr const char* ValidationDebugActions[] = { "VK_DBG_LAYER_ACTION_LOG_MSG" };  // "VK_DBG_LAYER_ACTION_DEBUG_OUTPUT", "VK_DBG_LAYER_ACTION_BREAK"
	constexpr const char* ValidationReportFlags[]  = { "error" };
	constexpr U32 VkTrue = VK_TRUE;
	VkLayerSettingEXT vkLayerSettings[] = {
		{ "VK_LAYER_KHRONOS_validation", "fine_grained_locking",  VK_LAYER_SETTING_TYPE_BOOL32_EXT, 1,                             &VkTrue },
		{ "VK_LAYER_KHRONOS_validation", "validate_core",         VK_LAYER_SETTING_TYPE_BOOL32_EXT, 1,                             &VkTrue },
		{ "VK_LAYER_KHRONOS_validation", "check_image_layout",    VK_LAYER_SETTING_TYPE_BOOL32_EXT, 1,                             &VkTrue },
		{ "VK_LAYER_KHRONOS_validation", "check_command_buffer",  VK_LAYER_SETTING_TYPE_BOOL32_EXT, 1,                             &VkTrue },
		{ "VK_LAYER_KHRONOS_validation", "check_object_in_use",   VK_LAYER_SETTING_TYPE_BOOL32_EXT, 1,                             &VkTrue },
		{ "VK_LAYER_KHRONOS_validation", "check_query",           VK_LAYER_SETTING_TYPE_BOOL32_EXT, 1,                             &VkTrue },
		{ "VK_LAYER_KHRONOS_validation", "check_shaders",         VK_LAYER_SETTING_TYPE_BOOL32_EXT, 1,                             &VkTrue },
		{ "VK_LAYER_KHRONOS_validation", "check_shaders_caching", VK_LAYER_SETTING_TYPE_BOOL32_EXT, 1,                             &VkTrue },
		{ "VK_LAYER_KHRONOS_validation", "unique_handles",        VK_LAYER_SETTING_TYPE_BOOL32_EXT, 1,                             &VkTrue },
		{ "VK_LAYER_KHRONOS_validation", "object_lifetime",       VK_LAYER_SETTING_TYPE_BOOL32_EXT, 1,                             &VkTrue },
		{ "VK_LAYER_KHRONOS_validation", "stateless_param",       VK_LAYER_SETTING_TYPE_BOOL32_EXT, 1,                             &VkTrue },
		{ "VK_LAYER_KHRONOS_validation", "debug_action",          VK_LAYER_SETTING_TYPE_STRING_EXT, LenOf(ValidationDebugActions), ValidationDebugActions },
		{ "VK_LAYER_KHRONOS_validation", "report_flags",          VK_LAYER_SETTING_TYPE_STRING_EXT, LenOf(ValidationReportFlags),  ValidationReportFlags },
	};
	VkLayerSettingsCreateInfoEXT vkLayerSettingsCreateInfo;
	VkInstanceCreateInfo vkInstanceCreateInfo = {
		.pNext = &vkLayerSettingsCreateInfo,
	};
	vkCreateInstance;
	volkLoadInstance;
	vkCreateDebugUtilsMessengerEXT;

	// select physical device
	vkEnumeratePhysicalDevices;
	for each physical device {
		vkGetPhysicalDeviceProperties2
		select if deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;
	}

	vkGetPhysicalDeviceQueueFamilyProperties2;
	for each queue family {
		select if queueFlags & VK_QUEUE_GRAPHICS_BIT
	}

	vkEnumerateDeviceExtensionProperties;
	device extensions = {
		VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME,
		VK_EXT_EXTENDED_DYNAMIC_STATE_EXTENSION_NAME,
		VK_EXT_EXTENDED_DYNAMIC_STATE_2_EXTENSION_NAME,
		VK_EXT_EXTENDED_DYNAMIC_STATE_3_EXTENSION_NAME,
		VK_EXT_SWAPCHAIN_MAINTENANCE_1_EXTENSION_NAME,
		VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME,
	};


	VkDeviceQueueCreateInfo using selected queue;
	VkPhysicalDeviceVulkan11Features ->
	VkPhysicalDeviceVulkan12Features ->
	VkPhysicalDeviceVulkan13Features ->
	VkPhysicalDeviceVulkan14Features ->
	VkPhysicalDeviceExtendedDynamicStateFeaturesEXT ->
	VkPhysicalDeviceExtendedDynamicState2FeaturesEXT ->
	VkPhysicalDeviceExtendedDynamicState3FeaturesEXT ->
	VkPhysicalDeviceSwapchainMaintenance1FeaturesEXT
	;
	vkGetPhysicalDeviceFeatures2;
		
	VkPhysicalDeviceProperties2 -> VkPhysicalDevicePushDescriptorPropertiesKHR
	vkGetPhysicalDeviceProperties2;
	VkDeviceCreateInfo
		next = features;
	vkCreateDevice;
	volkLoadDevice(m_device);
	debugUtilInitialize(m_device);
	vkGetDeviceQueue
	initVma();
	initSurface();
	static VkCommandPool m_transientCmdPool = vkCreateCommandPool(VK_COMMAND_POOL_CREATE_TRANSIENT_BIT);
	InitSwapchain();
	static VkSemaphore m_frameTimelineSemaphore = vkCreateSemaphore(VK_SEMAPHORE_TYPE_TIMELINE, initialValue = framesInFlight - 1);
	struct FrameData
	{
		VkCommandPool   cmdPool;      // Command pool for recording commands for this frame
		VkCommandBuffer cmdBuffer;    // Command buffer containing the frame's rendering commands
		uint64_t        frameNumber;  // Timeline value for synchronization (increases each frame)
	};
	static FrameData frameData[framesInFlight];
	for each frame:
		frameData[i] = {vkCreateCommandPool, vkAllocateCommandBuffers(1), i}
	static VkDescriptorPool m_descriptorPool = vkCreateDescriptorPool(maxSets=32, poolSizes={VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, min(10000, maxDescriptorSetSampledImages-2});
	static VkSampler m_sampler = vkCreateSampler(VK_FILTER_LINEAR);

	for format in {VK_FORMAT_D16_UNORM, VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT}
		vkGetPhysicalDeviceFormatProperties2;
		check optimalTilingFeatures.features & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
	InitGBuffer();

	static VkDescriptorSetLayout m_textureDescriptorSetLayout = vkCreateDescriptorSetLayout {
		VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT,
		{
			{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, numTextures, VK_SHADER_STAGE_ALL_GRAPHICS}
		}
		VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_UNUSED_WHILE_PENDING_BIT | VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT
	};
	static VkDescriptorSet m_textureDescriptorSet = vkAllocateDescriptorSets(1);
	static VkDescriptorSetLayout m_graphicDescriptorSetLayout = vkCreateDescriptorSetLayout {
		VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR
		{{.binding = 0, .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS}}};
	};
	InitShaders();
	InitGraphicsPipeline();	// usual suspects
	createComputeShaderPipeline();

	vb and upload;
	images and upload;
	free staging buffers;
	create scene buffer;
	vkUpdateDescriptorSets for the new images;
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Gpu2