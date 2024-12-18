#include "JC/Window.h"

#include "JC/Array.h"
#include "JC/Event.h"
#include "JC/Log.h"
#include "JC/Unicode.h"

#include "JC/MinimalWindows.h"
#include <ShellScalingApi.h>

#pragma comment(lib, "shcore.lib")

namespace JC::Window {

//--------------------------------------------------------------------------------------------------

static constexpr u32 MaxDisplays = 32;

static constexpr u32 HID_USAGE_PAGE_GENERIC     = 0x01;
static constexpr u32 HID_USAGE_GENERIC_MOUSE    = 0x02;
static constexpr u32 HID_USAGE_GENERIC_KEYBOARD = 0x06;

struct Window {
	HWND        hwnd       = 0;
	DWORD       winStyle   = 0;
	Style       style      = {};
	Rect        windowRect = {};
	Rect        clientRect = {};
	u32         dpi        = 0;
	f32         dpiScale   = 0.0f;
	u64         stateFlags = 0;
	CursorMode  cursorMode = {};
};
static Arena*      temp;
static Log*        log;
static Display     displays[MaxDisplays];
static u32         displaysLen;
static Window      window;
static bool        exitRequested;

//----------------------------------------------------------------------------------------------

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
	switch (msg) {
		case WM_ACTIVATE:
			if (wparam == WA_INACTIVE) {
				window.stateFlags &= ~StateFlags::Focused;
				if (window.cursorMode != CursorMode::VisibleFree) {
					ClipCursor(0);
				}
			} else {
				window.stateFlags |= StateFlags::Focused;
				if (window.cursorMode != CursorMode::VisibleFree) {
					RECT r;
					GetWindowRect(hwnd, &r);
					ClipCursor(&r);
				}
			}
			break;

		case WM_CLOSE:
			exitRequested = true;
			break;

		case WM_DPICHANGED:
			window.dpi      = LOWORD(wparam);
			window.dpiScale = (f32)LOWORD(wparam) / (f32)USER_DEFAULT_SCREEN_DPI;
			break;

		case WM_EXITSIZEMOVE:
			if (window.cursorMode != CursorMode::VisibleFree && hwnd == GetActiveWindow()) {
				RECT r;
				GetWindowRect(hwnd, &r);
				ClipCursor(&r);
			}
			break;

		case WM_INPUT: {
			const HRAWINPUT hrawInput = (HRAWINPUT)lparam;
			UINT size = 0;
			GetRawInputData(hrawInput, RID_INPUT, 0, &size, sizeof(RAWINPUTHEADER));
			RAWINPUT* rawInput = (RAWINPUT*)temp->Alloc(size);
			GetRawInputData(hrawInput, RID_INPUT, rawInput, &size, sizeof(RAWINPUTHEADER));
			if (rawInput->header.dwType == RIM_TYPEMOUSE) {
				rawInput->data.mouse;
				Event::Add({
					.type = Event::Type::MouseMove,
					.mouseMove = {
						.x = (i32)rawInput->data.mouse.lLastX,
						.y = (i32)rawInput->data.mouse.lLastY,
					},
				});
					
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
				Event::Add({
					.type = Event::Type::Key,
					.key = {
						.down = !(rawInput->data.keyboard.Flags & RI_KEY_BREAK),
						.key  = (Event::Key)virtualKey,
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
				window.stateFlags |= StateFlags::Maximized;
			} else if (wparam == SIZE_MINIMIZED) {
				window.stateFlags |= StateFlags::Minimized;
			} else if (wparam == SIZE_RESTORED) {
				window.stateFlags &= ~(StateFlags::Maximized | StateFlags::Minimized);
			}
			break;

		case WM_SYSCOMMAND:
			if (wparam == SC_CLOSE) {
				exitRequested = true;
				return 0;
			}
			break;

		case WM_WINDOWPOSCHANGED: {
			const WINDOWPOS* wp = (WINDOWPOS*)lparam;
			window.windowRect.x      = wp->x;
			window.windowRect.y      = wp->y;
			window.windowRect.width  = wp->cx;
			window.windowRect.height = wp->cy;
			RECT cr = {};
			GetClientRect(hwnd, &cr);
			window.clientRect.x      = cr.left;
			window.clientRect.y      = cr.top;
			window.clientRect.width  = cr.right - cr.left;
			window.clientRect.height = cr.bottom - cr.top;

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

static BOOL MonitorEnumFn(HMONITOR hmonitor, HDC, LPRECT rect, LPARAM) {
	MONITORINFOEX monitorInfoEx = {};
	monitorInfoEx.cbSize = sizeof(monitorInfoEx);
	GetMonitorInfoW(hmonitor, &monitorInfoEx);

	UINT dpi    = 0;
	UINT unused = 0;
	GetDpiForMonitor(hmonitor, MDT_EFFECTIVE_DPI, &dpi, &unused);

	Assert(displaysLen < MaxDisplays);
	displays[displaysLen] = {
		.primary    = (monitorInfoEx.dwFlags & MONITORINFOF_PRIMARY) != 0,
		.rect       = {
			.x      = (i32)rect->left,
			.y      = (i32)rect->top,
			.width  = (i32)(rect->right - rect->left),
			.height = (i32)(rect->bottom - rect->top),
		},
		.dpi       = dpi,
		.dpiScale  = (f32)dpi / (f32)USER_DEFAULT_SCREEN_DPI,
	};
	displaysLen++;

	return TRUE;
}

//----------------------------------------------------------------------------------------------

Res<> Init(const InitInfo* initInfo) {
	temp = initInfo->temp;
	log  = initInfo->log;

	if (EnumDisplayMonitors(0, 0, MonitorEnumFn, 0) == FALSE) {
		return MakeLastErr(temp, EnumDisplayMonitors);
	}

	if (SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2) == FALSE) {
		return MakeLastErr(temp, SetProcessDpiAwarenessContext);
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
		return MakeLastErr(temp, RegisterClassExW);
	}

	const s16z titlew = Utf8ToWtf16z(temp, initInfo->title);

	window.style = initInfo->style;
	window.winStyle = 0;
	switch (window.style) {
		case Style::Bordered:
			window.winStyle = WS_CAPTION | WS_MAXIMIZEBOX | WS_MINIMIZEBOX | WS_SYSMENU;
			break;
		case Style::BorderedResizable:
			window.winStyle = WS_CAPTION | WS_MAXIMIZEBOX | WS_MINIMIZEBOX | WS_SYSMENU | WS_THICKFRAME;
			break;
		case Style::Borderless:
			window.winStyle = WS_CAPTION | WS_MAXIMIZEBOX | WS_MINIMIZEBOX | WS_POPUP | WS_SYSMENU;
			break;
		case Style::Fullscreen:
			window.winStyle = WS_POPUP;
			break;
	}

	Rect rect = initInfo->rect;
	if (initInfo->style == Style::Fullscreen) {
		Assert(initInfo->fullscreenDisplay <= displaysLen);
		rect = displays[initInfo->fullscreenDisplay].rect;
	}
	RECT r = {};
	::SetRect(&r, rect.x, rect.y, rect.x + rect.width, rect.y + rect.height);

	HMONITOR hmonitor = MonitorFromRect(&r, MONITOR_DEFAULTTONEAREST);
	UINT unused = 0;
	GetDpiForMonitor(hmonitor, MDT_EFFECTIVE_DPI, &window.dpi, &unused);
	AdjustWindowRectExForDpi(&r, window.winStyle, FALSE, 0, window.dpi);

	window.hwnd = CreateWindowExW(
		0,
		L"JC",
		titlew.data,
		window.winStyle,
		r.left,
		r.top,
		r.right - r.left,
		r.bottom - r.top,
		0,
		0,
		GetModuleHandleW(0),
		0
	);
	if (!window.hwnd) {
		return MakeLastErr(temp, CreateWindowExW);
	}

	window.windowRect.x      = r.left;
	window.windowRect.y      = r.bottom;
	window.windowRect.width  = r.right - r.left;
	window.windowRect.height = r.bottom - r.top;
	window.dpiScale          = (f32)window.dpi / (f32)USER_DEFAULT_SCREEN_DPI;

	SetFocus(window.hwnd);
	ShowWindow(window.hwnd, SW_SHOW);
	UpdateWindow(window.hwnd);
	RAWINPUTDEVICE rawInputDevices[2] = {
		{
			.usUsagePage = (USHORT)HID_USAGE_PAGE_GENERIC,
			.usUsage     = (USHORT)HID_USAGE_GENERIC_KEYBOARD,
			.dwFlags     = RIDEV_INPUTSINK,
			.hwndTarget  = window.hwnd,
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

void Shutdown() {
	if (window.hwnd) {
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
		DestroyWindow(window.hwnd);
		window.hwnd = 0;
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

State GetState() {
	return State {
		.rect       = {
			.x      = window.windowRect.y,
			.y      = window.windowRect.x,
			.width  = window.clientRect.width,
			.height = window.clientRect.height,
		},
		.flags      = window.stateFlags,
		.style      = window.style,
		.cursorMode = window.cursorMode,
	};
}

//----------------------------------------------------------------------------------------------

void SetRect(Rect newRect) {
	RECT r;
	::SetRect(&r, newRect.x, newRect.y, newRect.x + newRect.width, newRect.y + newRect.height);
	AdjustWindowRectExForDpi(&r, window.winStyle, FALSE, 0, window.dpi);
	SetWindowPos(window.hwnd, HWND_TOP, r.left, r.top, r.right - r.left, r.bottom - r.top, SWP_NOZORDER | SWP_NOACTIVATE);
}

//----------------------------------------------------------------------------------------------

void SetStyle(Style newStyle) {
	newStyle;
	// TODO
}

//----------------------------------------------------------------------------------------------

void SetCursorMode(CursorMode newCursorMode) {
	if (window.cursorMode != CursorMode::VisibleFree) {
		ClipCursor(0);
	}
	if (newCursorMode != CursorMode::VisibleFree) {
		RECT r;
		GetWindowRect(window.hwnd, &r);
		ClipCursor(&r);
	}
	window.cursorMode = newCursorMode;
}

//----------------------------------------------------------------------------------------------

PlatformInfo GetPlatformInfo() {
	return PlatformInfo {
		.hinstance = GetModuleHandleW(0),
		.hwnd      = window.hwnd,
	};
}

//----------------------------------------------------------------------------------------------

bool IsExitRequested() {
	return exitRequested;
}

//--------------------------------------------------------------------------------------------------
}	// namespace JC::Window