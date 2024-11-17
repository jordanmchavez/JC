#pragma once

#include "JC/Render.h"
#include "JC/Render_Vk.h"

#include "JC/Array.h"
#include "JC/Log.h"

namespace JC {

//--------------------------------------------------------------------------------------------------

namespace Render {
	Array<VkLayerProperties>     vkLayerProperties;
	Array<VkExtensionProperties> globalInstanceExtensions;
}

//--------------------------------------------------------------------------------------------------

static Err* _MakeVkErr(Mem* perm, s8 file, i32 line, s8 fn, VkResult vkResult) {
	return Err::Make(
		perm,
		0,
		file,
		line,
		ErrCode { .ns = "vk", .code = (u64)vkResult },
		"fn", fn,
		"desc", Render::ResultStr(vkResult)
	);
}
#define MakeVkErr(perm, Fn, vkResult) \
	_MakeVkErr(perm, __FILE__, __LINE__, #Fn, vkResult)

//static void _LogVkErr(Mem scratch, s8 file, i32 line, s8 fn, VkResult vkResult) {
//	Log::PrintErr(scratch, file, line, _MakeVkErr(&scratch, file, line, fn, vkResult));
//}
//
//#define LogVkErr(scratch, Fn, vkResult) \
//	_LogVkErr(scratch, __FILE__, __LINE__, #Fn, vkResult)
//
//--------------------------------------------------------------------------------------------------

Res<Array<VkLayerProperties>> EnumerateLayers(Mem* perm, Mem* scratch) {
	VkResult vkResult = VK_SUCCESS;
	u32 n = 0;
	Array<VkLayerProperties> vkLayerProperties = { .mem = perm };
	do {
		if (vkResult = vkEnumerateInstanceLayerProperties(&n, nullptr); vkResult == VK_SUCCESS) {
			vkResult = vkEnumerateInstanceLayerProperties(&n, vkLayerProperties.Resize(n));
		}
	} while (vkResult == VK_INCOMPLETE);
	if (vkResult) {
		vkLayerProperties.Free();
		return MakeVkErr(scratch, vkEnumerateInstanceLayerProperties, vkResult);
	}
	return vkLayerProperties;
}

//--------------------------------------------------------------------------------------------------

Res<Array<VkExtensionProperties>> EnumerateInstanceExtensions(Mem* perm, const char* layerName) {
	Mem tentative = *perm;
	VkResult vkResult = VK_SUCCESS;
	u32 n = 0;
	Array<VkExtensionProperties> vkExtensionProperties = { .mem = &tentative };
	do {
		if (vkResult = vkEnumerateInstanceExtensionProperties(layerName, &n, nullptr); vkResult == VK_SUCCESS) {
			vkResult = vkEnumerateInstanceExtensionProperties(layerName, &n, vkExtensionProperties.Resize(n));
		}
	} while (vkResult == VK_INCOMPLETE);
	if (vkResult != VK_SUCCESS) {
		return MakeVkErr(perm, vkEnumerateInstanceExtensionProperties, vkResult);
	}
	*perm = tentative;
	return vkExtensionProperties;
}
//--------------------------------------------------------------------------------------------------

Res<Array<VkExtensionProperties>> EnumerateInstanceExtensions(Mem* perm, const char* layerName) {
	Mem reset = *perm;
	VkResult vkResult = VK_SUCCESS;
	u32 n = 0;
	Array<VkExtensionProperties> vkExtensionProperties = { .mem = perm };
	do {
		if (vkResult = vkEnumerateInstanceExtensionProperties(layerName, &n, nullptr); vkResult == VK_SUCCESS) {
			vkResult = vkEnumerateInstanceExtensionProperties(layerName, &n, vkExtensionProperties.Resize(n));
		}
	} while (vkResult == VK_INCOMPLETE);
	if (vkResult != VK_SUCCESS) {
		*perm = reset;
		return MakeVkErr(perm, vkEnumerateInstanceExtensionProperties, vkResult);
	}
	return vkExtensionProperties;
}
//--------------------------------------------------------------------------------------------------

Res<> Render::Init(Mem* perm, Mem* scratch) {
	if (Res<> r = LoadRootFns(scratch); !r) {
		return r;
	}

	if (Res<> r = EnumerateLayers(perm, scratch).To(vkLayerProperties); !r) {
		LogErr(scratch, r.err);
	}

	Log(Mem(*perm), "{} layers", vkLayerProperties.len);
	for (u32 i = 0; i < vkLayerProperties.len; i++) {
		VkLayerProperties* l = &vkLayerProperties[i];
		Log(Mem(*perm), "  {}: specVersion={}, implVersion={}, description={}", l->layerName, l->specVersion, l->implementationVersion, l->description);
	}

	if (Res<> r = EnumerateInstanceExtensions(perm, 0).To(globalInstanceExtensions); !r) {
		LogErr(Mem(*perm), r.err);
	}
	Log(Mem(*perm), "{} global instance extensions", globalInstanceExtensions.len);
	for (u64 i = 0; i < globalInstanceExtensions.len; i++) {
		Log(Mem(*perm), "  {}: specVersion={}", globalInstanceExtensions[i].extensionName, globalInstanceExtensions[i].specVersion);
	}

	return Ok();
}

//--------------------------------------------------------------------------------------------------

void Render::Shutdown() {
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC