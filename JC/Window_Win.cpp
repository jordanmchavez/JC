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

static Arena*      temp;
static Log*        log;
static Display     displays[MaxDisplays];
static u32         displaysLen;
static bool        exitRequested;
static HWND        hwnd;
static DWORD       winStyle;
static Style       style;
static Rect        windowRect;
static Rect        clientRect;
static u32         dpi;
static float       dpiScale;
static u64         stateFlags;
static CursorMode  cursorMode;

//----------------------------------------------------------------------------------------------

static LRESULT CALLBACK WndProc(HWND, UINT msg, WPARAM wparam, LPARAM lparam) {
	switch (msg) {
		case WM_ACTIVATE:
			if (wparam == WA_INACTIVE) {
				stateFlags &= ~StateFlags::Focused;
				if (cursorMode != CursorMode::VisibleFree) {
					ClipCursor(0);
				}
			} else {
				stateFlags |= StateFlags::Focused;
				if (cursorMode != CursorMode::VisibleFree) {
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
			dpi      = LOWORD(wparam);
			dpiScale = (float)LOWORD(wparam) / (float)USER_DEFAULT_SCREEN_DPI;
			break;

		case WM_EXITSIZEMOVE:
			if (cursorMode != CursorMode::VisibleFree && hwnd == GetActiveWindow()) {
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
				stateFlags |= StateFlags::Maximized;
			} else if (wparam == SIZE_MINIMIZED) {
				stateFlags |= StateFlags::Minimized;
			} else if (wparam == SIZE_RESTORED) {
				stateFlags &= ~(StateFlags::Maximized | StateFlags::Minimized);
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
			windowRect.x      = wp->x;
			windowRect.y      = wp->y;
			windowRect.width  = wp->cx;
			windowRect.height = wp->cy;
			RECT cr = {};
			GetClientRect(hwnd, &cr);
			clientRect.x      = cr.left;
			clientRect.y      = cr.top;
			clientRect.width  = cr.right - cr.left;
			clientRect.height = cr.bottom - cr.top;

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

	UINT monitorDpi    = 0;
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
		.dpi       = monitorDpi,
		.dpiScale  = (float)monitorDpi / (float)USER_DEFAULT_SCREEN_DPI,
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

	winStyle = 0;
	switch (initInfo->style) {
		case Style::Bordered:
			winStyle = WS_CAPTION | WS_MAXIMIZEBOX | WS_MINIMIZEBOX | WS_SYSMENU;
			break;
		case Style::BorderedResizable:
			winStyle = WS_CAPTION | WS_MAXIMIZEBOX | WS_MINIMIZEBOX | WS_SYSMENU | WS_THICKFRAME;
			break;
		case Style::Borderless:
			winStyle = WS_CAPTION | WS_MAXIMIZEBOX | WS_MINIMIZEBOX | WS_POPUP | WS_SYSMENU;
			break;
		case Style::Fullscreen:
			winStyle = WS_POPUP;
			break;
	}

	Rect rect = initInfo->rect;
	if (style == Style::Fullscreen) {
		Assert(initInfo->fullscreenDisplay <= displaysLen);
		rect = displays[initInfo->fullscreenDisplay].rect;
	}
	RECT r = {};
	::SetRect(&r, rect.x, rect.y, rect.x + rect.width, rect.y + rect.height);

	HMONITOR hmonitor = MonitorFromRect(&r, MONITOR_DEFAULTTONEAREST);
	UINT unused = 0;
	GetDpiForMonitor(hmonitor, MDT_EFFECTIVE_DPI, &dpi, &unused);
	AdjustWindowRectExForDpi(&r, winStyle, FALSE, 0, dpi);

	hwnd = CreateWindowExW(
		0,
		L"JC",
		titlew.data,
		winStyle,
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
		return MakeLastErr(temp, CreateWindowEx);
	}

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

void Shutdown() {
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

State GetState() {
	return State {
		.rect       = {
			.x      = windowRect.y,
			.y      = windowRect.x,
			.width  = clientRect.width,
			.height = clientRect.height,
		},
		.flags      = stateFlags,
		.style      = style,
		.cursorMode = cursorMode,
	};
}

//----------------------------------------------------------------------------------------------

void SetRect(Rect newRect) {
	RECT r;
	::SetRect(&r, newRect.x, newRect.y, newRect.x + newRect.width, newRect.y + newRect.height);
	AdjustWindowRectExForDpi(&r, winStyle, FALSE, 0, dpi);
	SetWindowPos(hwnd, HWND_TOP, r.left, r.top, r.right - r.left, r.bottom - r.top, SWP_NOZORDER | SWP_NOACTIVATE);
}

//----------------------------------------------------------------------------------------------

void SetStyle(Style newStyle) {
	newStyle;
	// TODO
}

//----------------------------------------------------------------------------------------------

void SetCursorMode(CursorMode newCursorMode) {
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

PlatformInfo GetPlatformInfo() {
	return PlatformInfo {
		.hinstance = GetModuleHandleW(0),
		.hwnd      = hwnd,
	};
}

//----------------------------------------------------------------------------------------------

bool IsExitRequested() {
	return exitRequested;
}

//--------------------------------------------------------------------------------------------------
}	// namespace JC::Window