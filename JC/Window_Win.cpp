#include "JC/Window.h"

#include "JC/Array.h"
#include "JC/Err.h"
#include "JC/Mem.h"

#include "JC/MinimalWindows.h"
#include <ShellScalingApi.h>

namespace JC {

//--------------------------------------------------------------------------------------------------

struct DisplayApiObj : DisplayApi {
	Array<DisplayInfo> displayInfos;

	static BOOL MonitorEnumFn(HMONITOR hmonitor, HDC hdc, LPRECT rect, LPARAM userData) {
		DisplayApiObj* displayApi = (DisplayApiObj*)userData;

		MONITORINFOEX monitorInfoEx = {};
		monitorInfoEx.cbSize = sizeof(monitorInfoEx);
		GetMonitorInfoW(hmonitor, &monitorInfoEx);

		UINT dpiX = 0;
		UINT dpiY = 0;
		GetDpiForMonitor(hmonitor, MDT_EFFECTIVE_DPI, &dpiX, &dpiY);

		displayApi->displayInfos.Add(DisplayInfo {
			.primary   = (monitorInfoEx.dwFlags & MONITORINFOF_PRIMARY) != 0,
			.x         = (i32)rect->left,
			.y         = (i32)rect->top,
			.width     = (u32)(rect->right - rect->left),
			.height    = (u32)(rect->bottom - rect->top),
			.dpiScaleX = (float)dpiX / (float)USER_DEFAULT_SCREEN_DPI,
			.dpiScaleY = (float)dpiY / (float)USER_DEFAULT_SCREEN_DPI,
		});
		return TRUE;
	}

	Res<> Init(Mem* mem) override {
		displayInfos.Init(mem);
		return Refresh();
	}

	Span<DisplayInfo> GetDisplayInfos() override {
		return displayInfos;
	}

	Res<> Refresh() override {
		if (EnumDisplayMonitors(0, 0, MonitorEnumFn, (LPARAM)this) == FALSE) {
			return MakeErr(Err_EnumDisplays);
		}
	}
};

static DisplayApiObj displayApi = {};

DisplayApi* GetDisplayApi() {
	return &displayApi;
}

//--------------------------------------------------------------------------------------------------

struct WindowApiObj : WindowApi {
	Res<> Init(WindowApiInit* init) override {

	}
};

static WindowApiObj windowApi;

WindowApi* GetWindowApi() {
	return &windowApi;
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC