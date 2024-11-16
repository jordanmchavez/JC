#pragma once

#include "JC/Render.h"
#include "JC/Render_Vk.h"

#include "JC/Allocator.h"
#include "JC/Array.h"
#include "JC/Log.h"

namespace JC {

//--------------------------------------------------------------------------------------------------

VkBool32 VKAPI_PTR VkDebugUtilsCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT      severity,
    VkDebugUtilsMessageTypeFlagsEXT             types,
    const VkDebugUtilsMessengerCallbackDataEXT* data,
    void*                                       userData
);

struct Seg {
	u64 offset = 0;
	u64 len    = 0;
};

template <class T>
struct Buf {
	Allocator* allocator = nullptr;
	T*         data = nullptr;
	u64        len = 0;

	Buf() = default;

	Buf(Allocator* a) {
		allocator = a;
	}

	Buf(Allocator* a, u64 n, SrcLoc srcLoc = SrcLoc::Here()) {
		allocator = a;
		data = (T*)a->Alloc(n * sizeof(T), srcLoc);
		len = n;
	}

	void Init(Allocator* a) {
		allocator = a;
	}

	void Init(Allocator* a, u64 n, SrcLoc srcLoc = SrcLoc::Here()) {
		allocator = a;
		data = (T*)a->Alloc(n * sizeof(T), srcLoc);
		len = n;
	}

	void Shutdown() {
		if (allocator) {
			allocator->Free(data, len * sizeof(T));
		}
	}

	T* Resize(u64 n, SrcLoc srcLoc = SrcLoc::Here()) {
		data = (T*)allocator->Realloc(data, len * sizeof(T), n * sizeof(T), srcLoc);
		len = n;
		return data;
	}

	T  operator[](u64 i) const { JC_ASSERT(i < len); return data[i]; }
	T& operator[](u64 i)       { JC_ASSERT(i < len); return data[i]; }
};

struct Ext {
	Str name;
	u32 specVer = 0;
};

struct Layer {
	Str      name;
	u32      specVer = 0;
	u32      implVer = 0;
	Str      desc;
	Buf<Ext> exts;
};

static constexpr u32 InvalidQueueFamily = 0xffffffff;

struct PhysicalDevice {
	VkPhysicalDevice                                   vkPhysicalDevice;

	Buf<VkLayerProperties>                             vkLayerProperties;

	Buf<VkExtensionProperties>                         vkExtensionProperties;

	VkPhysicalDeviceProperties                         vkPhysicalDeviceProperties;
	VkPhysicalDeviceDescriptorIndexingProperties       vkPhysicalDeviceDescriptorIndexingProperties;
	VkPhysicalDeviceSubgroupProperties                 vkPhysicalDeviceSubgroupProperties;
	VkPhysicalDeviceAccelerationStructurePropertiesKHR vkPhysicalDeviceAccelerationStructurePropertiesKHR;
	VkPhysicalDeviceRayTracingPipelinePropertiesKHR    vkPhysicalDeviceRayTracingPipelinePropertiesKHR;
	VkPhysicalDeviceMeshShaderPropertiesNV             vkPhysicalDeviceMeshShaderPropertiesNV;

	VkPhysicalDeviceFeatures                           vkPhysicalDeviceFeatures;
	VkPhysicalDeviceVulkan12Features                   vkPhysicalDeviceVulkan12Features;
	VkPhysicalDeviceVulkan13Features                   vkPhysicalDeviceVulkan13Features;
	VkPhysicalDeviceAccelerationStructureFeaturesKHR   vkPhysicalDeviceAccelerationStructureFeaturesKHR;
	VkPhysicalDeviceRayTracingPipelineFeaturesKHR      vkPhysicalDeviceRayTracingPipelineFeaturesKHR;
	VkPhysicalDeviceMeshShaderFeaturesNV               vkPhysicalDeviceMeshShaderFeaturesNV;

	VkPhysicalDeviceMemoryProperties                   vkPhysicalDeviceMemoryProperties;

	Buf<VkQueueFamilyProperties>                       vkQueueFamilyProperties;
	u32                                                graphicsQueueFamily;
	u32                                                computeQueueFamily;
	u32                                                transferQueueFamily;
	u32                                                presentQueueFamily;
};

struct RenderApiImpl : RenderApi {
	LogApi*                  logApi                   = nullptr;
	Allocator*               allocator                = nullptr;
	TempAllocator*           tempAllocator            = nullptr;
	Buf<Ext>                 globalInstanceExts;
	Buf<Layer>               layers;
	bool                     haveVkExtDebugUtils      = false;
	VkAllocationCallbacks*   vkAllocationCallbacks    = nullptr;
	VkInstance               vkInstance               = VK_NULL_HANDLE;
	VkDebugUtilsMessengerEXT vkDebugUtilsMessengerEXT = VK_NULL_HANDLE;
	Buf<PhysicalDevice>      physicalDevices;

	//----------------------------------------------------------------------------------------------

	#define JC_MAKE_VK_ERR(Fn, vkResult) \
		Err::Make(tempAllocator, nullptr, __FILE__, __LINE__, ErrCode { .ns = "vk", .code = (u64)vkResult }, "fn", #Fn, "desc", RenderVk::ResultStr(vkResult))

	#define JC_LOG_VK_ERR(Fn, vkResult) \
		JC_LOG_ERR(JC_MAKE_VK_ERR(Fn, vkResult))

	//----------------------------------------------------------------------------------------------

	VkBool32 VkDebugUtilsCallback(VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT, const VkDebugUtilsMessengerCallbackDataEXT* data) {
		logApi->Log(
			__FILE__,
			__LINE__,
			(severity & (VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)) ? LogCategory::Error : LogCategory::Info,
			"[{}] {}: {}",
			data->messageIdNumber,
			data->pMessageIdName,
			data->pMessage ? data->pMessage : ""
		);
		return VK_FALSE;
	}

	//----------------------------------------------------------------------------------------------

	void* scratch = nullptr;
	u64 scratchSize = 0;
	void* Scratch() {
		return scratch;
	}
	template <class T> T* Scratch(u64 n) {
		u64 newSize = n * sizeof(T);
		scratch = tempAllocator->Realloc(scratch, scratchSize, newSize);
		scratchSize = newSize;
		return (T*)scratch;
	}

	//----------------------------------------------------------------------------------------------

	void EnumerateLayers() {
		VkResult r = VK_SUCCESS;
		u32 n = 0;
		VkLayerProperties* props = nullptr;
		do {
			if (r = vkEnumerateInstanceLayerProperties(&n, nullptr); r == VK_SUCCESS) {
				props = Scratch<VkLayerProperties>(n);
				r = vkEnumerateInstanceLayerProperties(&n, props);
			}
		} while (r == VK_INCOMPLETE);
		if (r) {
			JC_LOG_VK_ERR(vkEnumerateInstanceLayerProperties, r);
			return;
		}
		layers.Init(allocator, n);
		for (u32 i = 0; i < layers.len; i++) {
			layers[i].name    = Str::Make(props[i].layerName);
			layers[i].specVer = props[i].specVersion;
			layers[i].implVer = props[i].implementationVersion;
			layers[i].desc    = Str::Make(props[i].description);
			layers[i].exts    = EnumInstanceExts(layers[i].name);
		}
	}

	//----------------------------------------------------------------------------------------------

	Buf<Ext> EnumInstanceExts(Str layerName) {
		VkResult r = VK_SUCCESS;
		u32 n = 0;
		VkExtensionProperties* props = nullptr;
		do {
			if (r = vkEnumerateInstanceExtensionProperties(layerName.data, &n, nullptr); r == VK_SUCCESS) {
				props = Scratch<VkExtensionProperties>(n);
				r = vkEnumerateInstanceExtensionProperties(layerName.data, &n, props);
			}
		} while (r == VK_INCOMPLETE);
		if (r) {
			JC_LOG_VK_ERR(vkEnumerateInstanceExtensionProperties, r);
			return Buf<Ext>{};
		}
		Buf<Ext> exts(allocator, n);
		for (u32 i = 0; i < n; i++) {
			exts[i].name    = Str::Make(props[i].extensionName);
			exts[i].specVer = props[i].specVersion;
		}
		return exts;
	}
	//----------------------------------------------------------------------------------------------

	Res<> AddInstanceLayer(Str name, Array<Str>* out) {
		for (u64 i = 0; i < layers.len; i++) {
			if (name == layers[i].name) {
				out->Add(name);
				return Ok();
			}
		}
		return JC_ERR(tempAllocator, Err_LayerNotFound, "name", name);
	}

	//----------------------------------------------------------------------------------------------

	Res<> AddInstanceExtension(Str name, Span<Str> layerNames, Array<Str>* out) {
		for (u64 i = 0; i < globalInstanceExts.len; i++) {
			if (name == globalInstanceExts[i].name) {
				JC_LOG("Found requested instance extension {} in global instance extensions", name);
				out->Add(name);
				return Ok();
			}
		}
		for (u64 i = 0; i < layerNames.len; i++) {
			for (u64 j = 0; j < layers.len; j++) {
				if (layerNames[i] == layers[j].name) {
					for (u64 k = 0; k < layers[j].exts.len; k++) {
						if (name == layers[j].exts[k].name) {
							JC_LOG("Found requested instance extension {} in layer {}", name, layers[j].name);
							out->Add(name);
							return Ok();
						}
					}
				}
			}
		}
		return JC_ERR(tempAllocator, Err_ExtensionNotFound, "name", name);
	}

	//----------------------------------------------------------------------------------------------

	static Res<Buf<VkLayerProperties>> EnumerateDeviceLayers(VkPhysicalDevice vkPhysicalDevice, Allocator* allocator) {
		VkResult vkResult = VK_SUCCESS;
		u32 n = 0;
		Buf<VkLayerProperties> vkLayerProperties(allocator);
		do {
			if (vkResult = vkEnumerateDeviceLayerProperties(vkPhysicalDevice, &n, nullptr); vkResult == VK_SUCCESS) {
				vkResult = vkEnumerateDeviceLayerProperties(vkPhysicalDevice, &n, vkLayerProperties.Resize(n));
			}
		} while (vkResult == VK_INCOMPLETE);
		if (vkResult) {
			return JC_MAKE_VK_ERR(vkEnumerateDeviceLayerProperties, vkResult);
		}
		return vkLayerProperties;
	}

	Res<Buf<VkExtensionProperties>> EnumerateDeviceExtensions(VkPhysicalDevice vkPhysicalDevice) {
		VkResult vkResult = VK_SUCCESS;
		u32 n = 0;
		Buf<VkExtensionProperties> vkExtensionProperties(allocator);
		do {
			if (vkResult = vkEnumerateDeviceExtensionProperties(vkPhysicalDevice, nullptr, &n, nullptr); vkResult == VK_SUCCESS) {
				vkResult = vkEnumerateDeviceExtensionProperties(vkPhysicalDevice, nullptr, &n, vkExtensionProperties.Resize(n));
			}
		} while (vkResult == VK_INCOMPLETE);
		if (vkResult) {
			return JC_MAKE_VK_ERR(vkEnumerateDeviceExtensionProperties, vkResult);
		}
		return vkExtensionProperties;
	}

	bool HasExtension(Buf<VkExtensionProperties> vkExtensionProperties, s8 extensionName) {
		for (u64 i = 0; i < vkExtensionProperties.len; i++) {
			if (extensionName == vkExtensionProperties[i].extensionName) {
				return true;
			}
		}
		return false;
	}

	VkBaseInStructure* Link(VkBaseInStructure* base, void* next) {
		base->pNext = (VkBaseInStructure*)next;
		return (VkBaseInStructure*)next;
	}

	VkBaseInStructure* LinkIfHasExtension(VkBaseInStructure* base, void* next, Buf<VkExtensionProperties> vkExtensionProperties, s8 extensionName) {
		if (HasExtension(vkExtensionProperties, extensionName)) {
			base->pNext = (VkBaseInStructure*)next;
			return (VkBaseInStructure*)next;
		}
		return base;
	}

	static Res<Buf<PhysicalDevice>> EnumeratePhysicalDevices(VkInstance vkInstance, Allocator* allocator, TempAllocator* tempAllocator) {
		VkResult vkResult = VK_SUCCESS;
		u32 n = 0;
		VkPhysicalDevice* vkPhysicalDevices = nullptr;
		do {
			if (vkResult = vkEnumeratePhysicalDevices(vkInstance, &n, nullptr); vkResult == VK_SUCCESS) {
				vkPhysicalDevices = Scratch<VkPhysicalDevice>(n);
				vkResult = vkEnumeratePhysicalDevices(vkInstance, &n, vkPhysicalDevices);
			}
		} while (vkResult == VK_INCOMPLETE);
		if (vkResult) {
			return JC_MAKE_VK_ERR(vkEnumeratePhysicalDevices, vkResult);
		}

		Buf<PhysicalDevice> physicalDevices(allocator, n);
		for (u32 i = 0; i < physicalDevices.len; i++) {
			PhysicalDevice* const pd = &physicalDevices[i];

			pd->vkPhysicalDevice = vkPhysicalDevices[i];

			if (Err* err = EnumerateDeviceLayers(pd->vkPhysicalDevice).To(pd->vkLayerProperties)) {
				return err;
			}

			if (Err* err = EnumerateDeviceExtensions(pd->vkPhysicalDevice).To(pd->vkExtensionProperties)) {
				return err;
			}

			VkPhysicalDeviceProperties2 vkPhysicalDeviceProperties2 = { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2 };
			pd->vkPhysicalDeviceDescriptorIndexingProperties        = { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_PROPERTIES };
			pd->vkPhysicalDeviceSubgroupProperties                  = { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES };
			pd->vkPhysicalDeviceAccelerationStructurePropertiesKHR  = { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR };
			pd->vkPhysicalDeviceRayTracingPipelinePropertiesKHR     = { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR };
			pd->vkPhysicalDeviceMeshShaderPropertiesNV              = { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_PROPERTIES_NV };
			VkBaseInStructure* next = (VkBaseInStructure*)&vkPhysicalDeviceProperties2;
			next = Link              (next, &pd->vkPhysicalDeviceSubgroupProperties);
			next = Link              (next, &pd->vkPhysicalDeviceDescriptorIndexingProperties);
			next = LinkIfHasExtension(next, &pd->vkPhysicalDeviceAccelerationStructurePropertiesKHR, pd->vkExtensionProperties, VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME);
			next = LinkIfHasExtension(next, &pd->vkPhysicalDeviceRayTracingPipelinePropertiesKHR,    pd->vkExtensionProperties, VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME);
			next = LinkIfHasExtension(next, &pd->vkPhysicalDeviceMeshShaderPropertiesNV,             pd->vkExtensionProperties, VK_NV_MESH_SHADER_EXTENSION_NAME);
			vkGetPhysicalDeviceProperties2(pd->vkPhysicalDevice, &vkPhysicalDeviceProperties2);
			pd->vkPhysicalDeviceProperties = vkPhysicalDeviceProperties2.properties;

			VkPhysicalDeviceFeatures2 vkPhysicalDeviceFeatures2  = { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
			pd->vkPhysicalDeviceVulkan12Features                 = { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES };
			pd->vkPhysicalDeviceVulkan13Features                 = { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES };
			pd->vkPhysicalDeviceAccelerationStructureFeaturesKHR = { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR };
			pd->vkPhysicalDeviceRayTracingPipelineFeaturesKHR    = { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR };
			pd->vkPhysicalDeviceMeshShaderFeaturesNV             = { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_NV };
			next = (VkBaseInStructure*)&vkPhysicalDeviceFeatures2;
			next = Link              (next, &pd->vkPhysicalDeviceVulkan12Features);
			next = Link              (next, &pd->vkPhysicalDeviceVulkan13Features);
			next = LinkIfHasExtension(next, &pd->vkPhysicalDeviceAccelerationStructureFeaturesKHR, pd->vkExtensionProperties, VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME);
			next = LinkIfHasExtension(next, &pd->vkPhysicalDeviceRayTracingPipelineFeaturesKHR,    pd->vkExtensionProperties, VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME);
			next = LinkIfHasExtension(next, &pd->vkPhysicalDeviceMeshShaderFeaturesNV,             pd->vkExtensionProperties, VK_NV_MESH_SHADER_EXTENSION_NAME);
			vkGetPhysicalDeviceFeatures2(pd->vkPhysicalDevice, &vkPhysicalDeviceFeatures2);
			pd->vkPhysicalDeviceFeatures = vkPhysicalDeviceFeatures2.features;

			// TODO: if no validation, then device->enabled_features.features.robustBufferAccess = false;

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

		return physicalDevices;
	}

	//----------------------------------------------------------------------------------------------

	Res<> Init(Allocator* allocatorIn, LogApi* logApiIn, TempAllocator* tempAllocatorIn) override {
		allocator     = allocatorIn;
		logApi        = logApiIn;
		tempAllocator = tempAllocatorIn;

		globalInstanceExts.Init(allocator);
		layers.Init(allocator);
		physicalDevices.Init(allocator);

		if (Res<> r = RenderVk::LoadRootFns(tempAllocator); !r) {
			return r;
		}

		globalInstanceExts = EnumInstanceExts(Str());
		JC_LOG("{} global instance extensions", globalInstanceExts.len);
		for (u64 i = 0; i < globalInstanceExts.len; i++) {
			JC_LOG("    {}: specVer={}", globalInstanceExts[i].name, globalInstanceExts[i].specVer);
		}

		EnumerateLayers();
		JC_LOG("{} layers", layers.len);
		for (u32 i = 0; i < layers.len; i++) {
			JC_LOG("    {}: specVer={}, implVer={}, desc={}", layers[i].name, layers[i].specVer, layers[i].implVer, layers[i].desc);
			for (u64 j = 0; j < layers[i].exts.len; j++) {
				JC_LOG("        {}: specVer={}", layers[i].exts[j].name, layers[i].exts[j].specVer);
			}
		}

		VkApplicationInfo vkApplicationInfo = {
			.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO,
			.pNext              = nullptr,
			.pApplicationName   = "JC",
			.applicationVersion = 1,
			.pEngineName        = "JC",
			.engineVersion      = 1,
			.apiVersion         = VK_API_VERSION_1_3,
		};

		Array<Str> layerNames(tempAllocator);
		if (Res<> r = AddInstanceLayer(Str::Make("VK_LAYER_KHRONOS_validation"), &layerNames); !r) {
			JC_LOG("Couldn't find optional instance extension 'VK_LAYER_KHRONOS_validation'");
		}

		Array<Str> instanceExtensionNames;
		instanceExtensionNames.Init(tempAllocator);
		if (Res<> r = AddInstanceExtension(Str::Make(VK_KHR_SURFACE_EXTENSION_NAME), layerNames, &instanceExtensionNames); !r) {
			return r.err;
		}
		#if defined JC_OS_WINDOWS
			constexpr s8 platformSurfaceExtension = VK_KHR_WIN32_SURFACE_EXTENSION_NAME;
		#else	// JC_OS
			#error("unsupported OS")
		#endif // JC_OS
		if (Res<> r = AddInstanceExtension(Str::Make(platformSurfaceExtension), layerNames, &instanceExtensionNames); !r) {
			return r.err;
		}

		haveVkExtDebugUtils = (bool)AddInstanceExtension(Str::Make(VK_EXT_DEBUG_UTILS_EXTENSION_NAME), layerNames, &instanceExtensionNames);

		VkInstanceCreateInfo vkInstanceCreateInfo = {
			.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
			.pNext                   = nullptr,
			.flags                   = 0,
			.pApplicationInfo        = &vkApplicationInfo,
			.enabledLayerCount       = (u32)layerNames.len,
			.ppEnabledLayerNames     = (const char* const*)layerNames.data,
			.enabledExtensionCount   = (u32)instanceExtensionNames.len,
			.ppEnabledExtensionNames = (const char* const*)instanceExtensionNames.data,
		};

		VkDebugUtilsMessengerCreateInfoEXT vkDebugUtilsMessengerCreateInfoEXT;
		if (haveVkExtDebugUtils) {
			vkDebugUtilsMessengerCreateInfoEXT = {
				.sType           = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
				.pNext           = nullptr,
				.flags           = 0,
				.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
				.messageType     = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_DEVICE_ADDRESS_BINDING_BIT_EXT,
				.pfnUserCallback = JC::VkDebugUtilsCallback,
				.pUserData       = this,
			};
			vkInstanceCreateInfo.pNext = &vkDebugUtilsMessengerCreateInfoEXT;
		}

		/*
		VkAllocationCallbacks vkAllocationCallbacks = {
			.pUserData             = this,
			.pfnAllocation         = ,
			.pfnReallocation       = ,
			.pfnFree               = ,
			.pfnInternalAllocation = ,
			.pfnInternalFree       = ,
		};
		*/

		if (VkResult r = vkCreateInstance(&vkInstanceCreateInfo, vkAllocationCallbacks, &vkInstance)) {
			return JC_MAKE_VK_ERR(vkCreateInstance, r);
		}

		RenderVk::LoadInstanceFns(vkInstance);

		if (haveVkExtDebugUtils) {
			if (VkResult r = vkCreateDebugUtilsMessengerEXT(vkInstance, &vkDebugUtilsMessengerCreateInfoEXT, vkAllocationCallbacks, &vkDebugUtilsMessengerEXT)) {
				JC_LOG_VK_ERR(vkCreateDebugUtilsMessengerEXT, r);
			}
		}

		if (Err* err = EnumeratePhysicalDevices().To(physicalDevices)) {
			return err;
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

		return Ok();
	}

	void Shutdown() override {
		if (vkDebugUtilsMessengerEXT != VK_NULL_HANDLE) {
			vkDestroyDebugUtilsMessengerEXT(vkInstance, vkDebugUtilsMessengerEXT, vkAllocationCallbacks);
		}
		if (vkInstance != VK_NULL_HANDLE) {
			vkDestroyInstance(vkInstance, vkAllocationCallbacks);
		}
		RenderVk::FreeFns();
	}
};

RenderApiImpl renderApiImpl;

RenderApi* RenderApi::Get() {
	return &renderApiImpl;
}

VkBool32 VKAPI_PTR VkDebugUtilsCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT      severity,
    VkDebugUtilsMessageTypeFlagsEXT             types,
    const VkDebugUtilsMessengerCallbackDataEXT* data,
    void*                                       userData
) {
	return ((RenderApiImpl*)userData)->VkDebugUtilsCallback(severity, types, data);
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC