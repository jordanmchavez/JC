#include "JC/Window.h"
#include "JC/Render_Vk.h"

VkResult Render_CreateSurface(VkInstance vkInstance, const VkAllocationCallbacks* vkAllocationCallbacks, VkSurfaceKHR* vkSurface) {

	Window_SysData sysData = Window_GetSysData();
	VkWin32SurfaceCreateInfoKHR vkWin32SurfaceCreateInfo = {
		.sType     = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
		.pNext     = nullptr,
		.flags     = 0,
		.hinstance = (HINSTANCE)sysData.hinstance,
		.hwnd      = (HWND)sysData.hwnd,
	};
	return vkCreateWin32SurfaceKHR(vkInstance, &vkWin32SurfaceCreateInfo, vkAllocationCallbacks, vkSurface);
}