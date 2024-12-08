#include "JC/Window.h"

#include "JC/Array.h"
#include "JC/Event.h"
#include "JC/Log.h"
#include "JC/Mem.h"
#include "JC/Unicode.h"

#include "JC/MinimalWindows.h"
#include <ShellScalingApi.h>

#pragma comment(lib, "shcore.lib")

namespace JC {

//--------------------------------------------------------------------------------------------------

struct DisplayApiObj : DisplayApi {
	static constexpr u32 MaxDisplayInfos = 64;

	Log*        log                           = 0;
	DisplayInfo displayInfos[MaxDisplayInfos] = {};
	u32         displayInfosLen               = 0;

	//----------------------------------------------------------------------------------------------

	static BOOL MonitorEnumFn(HMONITOR hmonitor, HDC, LPRECT rect, LPARAM userData) {
		DisplayApiObj* displayApi = (DisplayApiObj*)userData;

		MONITORINFOEX monitorInfoEx = {};
		monitorInfoEx.cbSize = sizeof(monitorInfoEx);
		GetMonitorInfoW(hmonitor, &monitorInfoEx);

		UINT dpi    = 0;
		UINT unused = 0;
		GetDpiForMonitor(hmonitor, MDT_EFFECTIVE_DPI, &dpi, &unused);

		Assert(displayApi->displayInfosLen < MaxDisplayInfos);
		displayApi->displayInfos[displayApi->displayInfosLen] = {
			.primary    = (monitorInfoEx.dwFlags & MONITORINFOF_PRIMARY) != 0,
			.rect       = {
				.x      = (i32)rect->left,
				.y      = (i32)rect->top,
				.width  = (i32)(rect->right - rect->left),
				.height = (i32)(rect->bottom - rect->top),
			},
			.dpi       = dpi,
			.dpiScale  = (float)dpi / (float)USER_DEFAULT_SCREEN_DPI,
		};

		const DisplayInfo* di = &displayApi->displayInfos[displayApi->displayInfosLen];
		displayApi->log->Print(
			"  Display {}: pos={}x{}, size={}x{}, dpi={}, dpiScale={}{}", 
			displayApi->displayInfosLen,
			di->rect.x,
			di->rect.y,
			di->rect.width,
			di->rect.height,
			di->dpi,
			di->dpiScale,
			di->primary ? " ***PRIMARY***" : ""
		);

		displayApi->displayInfosLen++;

		return TRUE;
	}

	//----------------------------------------------------------------------------------------------

	Res<> Init(Log* log_) override {
		log = log_;
		return Refresh();
	}

	//----------------------------------------------------------------------------------------------

	Res<> Refresh() override {
		Logf("Refreshing displays:");
		if (EnumDisplayMonitors(0, 0, MonitorEnumFn, (LPARAM)this) == FALSE) {
			return MakeLastErr(EnumDisplayMonitors)->Push(Err_EnumDisplays);
		}
		return Ok();
	}

	//----------------------------------------------------------------------------------------------

	Span<DisplayInfo> GetDisplayInfos() override {
		return displayInfos;
	}
};

//--------------------------------------------------------------------------------------------------

static DisplayApiObj displayApi = {};

DisplayApi* GetDisplayApi() {
	return &displayApi;
}

//--------------------------------------------------------------------------------------------------

struct WindowObj {
};

//--------------------------------------------------------------------------------------------------

struct WindowApiObj : WindowApi {
	static constexpr u32 HID_USAGE_PAGE_GENERIC     = 0x01;
	static constexpr u32 HID_USAGE_GENERIC_MOUSE    = 0x02;
	static constexpr u32 HID_USAGE_GENERIC_KEYBOARD = 0x06;

	DisplayApi* displayApi    = 0;
	EventApi*   eventApi      = 0;
	Log*        log           = 0;
	TempMem*    tempMem       = 0;
	bool        exitRequested = false;
	HWND        hwnd          = 0;
	DWORD       windowStyle   = 0;
	WindowMode  windowMode    = {};
	Rect        windowRect    = {};
	Rect        clientRect    = {};
	u32         dpi           = 0;
	float       dpiScale      = 0.0f;
	u64         flags         = 0;
	CursorMode  cursorMode    = {};

	//----------------------------------------------------------------------------------------------

	static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
		WindowApiObj* windowApi = (WindowApiObj*)GetWindowApi();

		switch (msg) {
			case WM_ACTIVATE:
				if (wparam == WA_INACTIVE) {
					windowApi->flags &= ~WindowStateFlags::Focused;
					if (windowApi->cursorMode != CursorMode::VisibleFree) {
						ClipCursor(0);
					}
				} else {
					windowApi->flags |= WindowStateFlags::Focused;
					if (windowApi->cursorMode != CursorMode::VisibleFree) {
						RECT r;
						GetWindowRect(windowApi->hwnd, &r);
						ClipCursor(&r);
					}
				}
				break;

			case WM_CLOSE:
				windowApi->exitRequested = true;
				break;

			case WM_DPICHANGED:
				windowApi->dpi      = LOWORD(wparam);
				windowApi->dpiScale = (float)LOWORD(wparam) / (float)USER_DEFAULT_SCREEN_DPI;
				break;

			case WM_EXITSIZEMOVE:
				if (windowApi->cursorMode != CursorMode::VisibleFree && windowApi->hwnd == GetActiveWindow()) {
					RECT r;
					GetWindowRect(windowApi->hwnd, &r);
					ClipCursor(&r);
				}
				break;

			case WM_INPUT: {
				const HRAWINPUT hrawInput = (HRAWINPUT)lparam;
				UINT size = 0;
				GetRawInputData(hrawInput, RID_INPUT, 0, &size, sizeof(RAWINPUTHEADER));
				RAWINPUT* rawInput = (RAWINPUT*)windowApi->tempMem->Alloc(size);
				GetRawInputData(hrawInput, RID_INPUT, rawInput, &size, sizeof(RAWINPUTHEADER));
				if (rawInput->header.dwType == RIM_TYPEMOUSE) {
					rawInput->data.mouse;
					Log* log = windowApi->log;
					Logf(
						"mouse: flags={05b}, usButtonFlags={012b}, usButtonData={}, lLastX={}, lLastY={}",
						rawInput->data.mouse.usFlags,
						rawInput->data.mouse.usButtonFlags,
						rawInput->data.mouse.usButtonData,
						rawInput->data.mouse.lLastX,
						rawInput->data.mouse.lLastY
					);
					
				} else if (rawInput->header.dwType == RIM_TYPEKEYBOARD) {
					u32 scanCode   = rawInput->data.keyboard.MakeCode;
					u32 virtualKey = rawInput->data.keyboard.VKey;
					const bool e0  = rawInput->data.keyboard.Flags & RI_KEY_E0;
					const bool e1  = rawInput->data.keyboard.Flags & RI_KEY_E1;
					if (virtualKey == 255) {
						break;
					} else if (virtualKey == VK_SHIFT) {
						virtualKey = MapVirtualKey(scanCode, MAPVK_VSC_TO_VK_EX);
					} else if (virtualKey == VK_NUMLOCK) {
						scanCode = MapVirtualKey(virtualKey, MAPVK_VK_TO_VSC) | 0x100;
					}
					if (e0 || e1) {
						scanCode = (virtualKey == VK_PAUSE) ? 0x45 : MapVirtualKey(virtualKey, MAPVK_VK_TO_VSC);
					}
					switch (virtualKey) {
						case VK_CONTROL: virtualKey = e0 ? VK_RCONTROL  : VK_LCONTROL; break;
						case VK_MENU:    virtualKey = e0 ? VK_RMENU     : VK_LMENU;    break;
						case VK_RETURN:  virtualKey = e0 ? VK_SEPARATOR : VK_RETURN;   break;
						case VK_INSERT:  virtualKey = e0 ? VK_INSERT    : VK_NUMPAD0;  break;
						case VK_DELETE:  virtualKey = e0 ? VK_DELETE    : VK_DECIMAL;  break;
						case VK_HOME:    virtualKey = e0 ? VK_HOME      : VK_NUMPAD7;  break;
						case VK_END:     virtualKey = e0 ? VK_END       : VK_NUMPAD1;  break;
						case VK_PRIOR:   virtualKey = e0 ? VK_PRIOR     : VK_NUMPAD9;  break;
						case VK_NEXT:    virtualKey = e0 ? VK_NEXT      : VK_NUMPAD3;  break;
						case VK_LEFT:    virtualKey = e0 ? VK_LEFT      : VK_NUMPAD4;  break;
						case VK_RIGHT:   virtualKey = e0 ? VK_RIGHT     : VK_NUMPAD6;  break;
						case VK_UP:      virtualKey = e0 ? VK_UP        : VK_NUMPAD8;  break;
						case VK_DOWN:    virtualKey = e0 ? VK_DOWN      : VK_NUMPAD2;  break;
						case VK_CLEAR:   virtualKey = e0 ? VK_CLEAR     : VK_NUMPAD5;  break;
					}
					windowApi->eventApi->AddEvent({
						.type = EventType::Key,
						.key = {
							.down = !(rawInput->data.keyboard.Flags & RI_KEY_BREAK),
							.key  = (EventKey)virtualKey,
						},
					});
				}
				break;
			}

			case WM_SETCURSOR:
				if (LOWORD(lparam) == HTCLIENT) {
					SetCursor(LoadCursor(0, IDC_NO));
				}
				break;

			case WM_SIZE:
				if (wparam == SIZE_MAXIMIZED) {
					windowApi->flags |= WindowStateFlags::Maximized;
				} else if (wparam == SIZE_MINIMIZED) {
					windowApi->flags |= WindowStateFlags::Minimized;
				} else if (wparam == SIZE_RESTORED) {
					windowApi->flags &= ~(WindowStateFlags::Maximized | WindowStateFlags::Minimized);
				}
				break;

			case WM_SYSCOMMAND:
				if (wparam == SC_CLOSE) {
					windowApi->exitRequested = true;
					return 0;
				}
				break;

			case WM_WINDOWPOSCHANGED: {
				const WINDOWPOS* wp = (WINDOWPOS*)lparam;
				windowApi->windowRect.x      = wp->x;
				windowApi->windowRect.y      = wp->y;
				windowApi->windowRect.width  = wp->cx;
				windowApi->windowRect.height = wp->cy;
				RECT cr = {};
				GetClientRect(windowApi->hwnd, &cr);
				windowApi->clientRect.x      = cr.left;
				windowApi->clientRect.y      = cr.top;
				windowApi->clientRect.width  = cr.right - cr.left;
				windowApi->clientRect.height = cr.bottom - cr.top;

				//if (windowApi->cursorMode != WindowCursorMode::VisibleFree && GetActiveWindow() == windowApi->hwnd) {
				//	RECT r;
				//	GetWindowRect(windowApi->hwnd, &r);
				//	ClipCursor(&r);
				//}
				break;
			}

			case WM_CHAR:
			case WM_UNICHAR:
			case WM_KEYDOWN:
			case WM_KEYUP:
			case WM_SYSKEYDOWN:
			case WM_SYSKEYUP:
			case WM_SYSCHAR:
				break;
		}
		return DefWindowProc(hwnd, msg, wparam, lparam);
	}

	//----------------------------------------------------------------------------------------------

	Res<> Init(const WindowApiInit* init) override {
		displayApi = init->displayApi;
		eventApi   = init->eventApi;
		log        = init->log;
		tempMem    = init->tempMem;

		if (SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2) == FALSE) {
			return MakeLastErr(SetProcessDpiAwarenessContext);
		}

		const WNDCLASSEXW wndClassExW = {
			.cbSize        = sizeof(wndClassExW),
			.style         = CS_OWNDC | CS_DBLCLKS,
			.lpfnWndProc   = WndProc,
			.cbClsExtra    = 0,
			.cbWndExtra    = 0,
			.hInstance     = GetModuleHandle(0),
			.hIcon         = 0,
			.hCursor       = 0,
			.hbrBackground = 0,
			.lpszMenuName  = 0,
			.lpszClassName = L"JC",
			.hIconSm       = 0,
		};
		if (!RegisterClassExW(&wndClassExW)) {
			return MakeLastErr(RegisterClassExW);
		}

		Res<s16z> titlew = Unicode::Utf8ToWtf16z(tempMem, init->title);
		if (!titlew) {
			return titlew.err;
		}

		windowStyle = 0;
		switch (init->windowMode) {
			case WindowMode::Bordered:
				windowStyle = WS_CAPTION | WS_MAXIMIZEBOX | WS_MINIMIZEBOX | WS_SYSMENU;
				break;
			case WindowMode::BorderedResizable:
				windowStyle = WS_CAPTION | WS_MAXIMIZEBOX | WS_MINIMIZEBOX | WS_SYSMENU | WS_THICKFRAME;
				break;
			case WindowMode::Borderless:
				windowStyle = WS_CAPTION | WS_MAXIMIZEBOX | WS_MINIMIZEBOX | WS_POPUP | WS_SYSMENU;
				break;
			case WindowMode::Fullscreen:
				windowStyle = WS_POPUP;
				break;
		}

		Rect rect = init->rect;
		if (windowMode == WindowMode::Fullscreen) {
			Span<DisplayInfo> displayInfos = displayApi->GetDisplayInfos();
			Assert(init->fullscreenDisplay <= displayInfos.len);
			rect = displayInfos[init->fullscreenDisplay].rect;
		}
		RECT r = {};
		::SetRect(&r, rect.x, rect.y, rect.x + rect.width, rect.y + rect.height);

		HMONITOR hmonitor = MonitorFromRect(&r, MONITOR_DEFAULTTONEAREST);
		UINT unused = 0;
		GetDpiForMonitor(hmonitor, MDT_EFFECTIVE_DPI, &dpi, &unused);
		AdjustWindowRectExForDpi(&r, windowStyle, FALSE, 0, dpi);

		hwnd = CreateWindowExW(
			0,
			L"JC",
			titlew.val.data,
			windowStyle,
			r.left,
			r.top,
			r.right - r.left,
			r.bottom - r.top,
			0,
			0,
			GetModuleHandle(0),
			0
		);
		if (!hwnd) {
			return MakeLastErr(CreateWindowEx);
		}

		windowMode        = windowMode;
		windowRect.x      = r.left;
		windowRect.y      = r.bottom;
		windowRect.width  = r.right - r.left;
		windowRect.height = r.bottom - r.top;
		dpiScale          = (float)dpi / (float)USER_DEFAULT_SCREEN_DPI;

		SetFocus(hwnd);
		ShowWindow(hwnd, SW_SHOW);
		UpdateWindow(hwnd);
		RAWINPUTDEVICE rawInputDevices[2] = {
			{
				.usUsagePage = (USHORT)HID_USAGE_PAGE_GENERIC,
				.usUsage     = (USHORT)HID_USAGE_GENERIC_KEYBOARD,
				.dwFlags     = RIDEV_INPUTSINK,
				.hwndTarget  = hwnd,
			},
			{
				.usUsagePage = (USHORT)HID_USAGE_PAGE_GENERIC,
				.usUsage     = (USHORT)HID_USAGE_GENERIC_MOUSE,
				.dwFlags     = 0,
				.hwndTarget  = 0,
			},
		};
		RegisterRawInputDevices(rawInputDevices, 2, sizeof(rawInputDevices[0]));

		return Ok();
	}

	//----------------------------------------------------------------------------------------------

	void Shutdown() override {
		if (hwnd) {
			RAWINPUTDEVICE rawInputDevices[2] = {
				{
					.usUsagePage = (USHORT)HID_USAGE_PAGE_GENERIC,
					.usUsage     = (USHORT)HID_USAGE_GENERIC_KEYBOARD,
					.dwFlags     = RIDEV_REMOVE,
					.hwndTarget  = 0,
				},
				{
					.usUsagePage = (USHORT)HID_USAGE_PAGE_GENERIC,
					.usUsage     = (USHORT)HID_USAGE_GENERIC_MOUSE,
					.dwFlags     = RIDEV_REMOVE,
					.hwndTarget  = 0,
				},
			};
			RegisterRawInputDevices(rawInputDevices, 2, sizeof(rawInputDevices[0]));
			DestroyWindow(hwnd);
			hwnd = 0;
		}
	}

	//----------------------------------------------------------------------------------------------

	void PumpMessages() {
		MSG msg;
		while (PeekMessageW(&msg, 0, 0, 0, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessageW(&msg);
		}
	}

	//----------------------------------------------------------------------------------------------

	WindowState GetState() override {
		return WindowState {
			.rect       = {
				.x      = windowRect.y,
				.y      = windowRect.x,
				.width  = clientRect.width,
				.height = clientRect.height,
			},
			.flags      = flags,
			.windowMode = windowMode,
			.cursorMode = cursorMode,
		};
	}

	//----------------------------------------------------------------------------------------------

	void SetRect(Rect newRect) override {
		RECT r;
		::SetRect(&r, newRect.x, newRect.y, newRect.x + newRect.width, newRect.y + newRect.height);
		AdjustWindowRectExForDpi(&r, windowStyle, FALSE, 0, dpi);
		SetWindowPos(hwnd, HWND_TOP, r.left, r.top, r.right - r.left, r.bottom - r.top, SWP_NOZORDER | SWP_NOACTIVATE);
	}

	//----------------------------------------------------------------------------------------------

	void SetWindowMode(WindowMode newWindowMode) override {
		newWindowMode;
	}

	//----------------------------------------------------------------------------------------------

	void SetCursorMode(CursorMode newCursorMode) override {
		if (cursorMode != CursorMode::VisibleFree) {
			ClipCursor(0);
		}
		if (newCursorMode != CursorMode::VisibleFree) {
			RECT r;
			GetWindowRect(hwnd, &r);
			ClipCursor(&r);
		}
		cursorMode = newCursorMode;
	}

	//----------------------------------------------------------------------------------------------

	WindowPlatformData GetPlatformData() override {
		return WindowPlatformData {
			.hinstance = GetModuleHandleW(0),
			.hwnd      = hwnd,
		};
	}

	//----------------------------------------------------------------------------------------------

	bool IsExitRequested() override {
		return exitRequested;
	}
};

//--------------------------------------------------------------------------------------------------

static WindowApiObj windowApi;

WindowApi* GetWindowApi() {
	return &windowApi;
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC