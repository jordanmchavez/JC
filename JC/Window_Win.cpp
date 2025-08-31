#include "JC/Window.h"

#include "JC/Array.h"
#include "JC/Event.h"
#include "JC/Log.h"
#include "JC/Sys_Win.h"
#include "JC/Unicode.h"

#include <ShellScalingApi.h>

#pragma comment(lib, "shcore.lib")

namespace JC::Window {

//--------------------------------------------------------------------------------------------------

static constexpr U32 MaxDisplays = 32;

static constexpr U32 HID_USAGE_PAGE_GENERIC     = 0x01;
static constexpr U32 HID_USAGE_GENERIC_MOUSE    = 0x02;
static constexpr U32 HID_USAGE_GENERIC_KEYBOARD = 0x06;

struct Window {
	HWND        hwnd       = 0;
	DWORD       winStyle   = 0;
	Style       style      = {};
	CursorMode  cursorMode = {};
	Rect        windowRect = {};
	Rect        clientRect = {};
	U32         dpi        = 0;
	F32         dpiScale   = 0.0f;
	Bool        minimized  = false;
	Bool        focused    = false;
};
static Mem::TempAllocator* tempAllocator;
static Log::Logger*        logger;
static Display             displays[MaxDisplays];
static U32                 displaysLen;
static Window              window;
static Bool                exitRequested;

//----------------------------------------------------------------------------------------------

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
	switch (msg) {
		case WM_ACTIVATE:
			if (wparam == WA_INACTIVE) {
				window.focused = false;
				if (window.cursorMode != CursorMode::VisibleFree) {
					ClipCursor(0);
				}
			} else {
				window.focused = true;
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
			window.dpiScale = (F32)LOWORD(wparam) / (F32)USER_DEFAULT_SCREEN_DPI;
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
			RAWINPUT* rawInput = (RAWINPUT*)tempAllocator->Alloc(size);
			GetRawInputData(hrawInput, RID_INPUT, rawInput, &size, sizeof(RAWINPUTHEADER));
			if (rawInput->header.dwType == RIM_TYPEMOUSE) {
				const RAWMOUSE* const m = &rawInput->data.mouse;
				if (m->usButtonFlags & RI_MOUSE_WHEEL) {
					Event::Add({
						.type = Event::Type::MouseWheel,
						.mouseWheel = { .delta = (float)((signed short)m->usButtonData) / 120.0f },
					});
				}

				Event::Add({
					.type = Event::Type::MouseMove,
					.mouseMove = {
						.x = (I32)m->lLastX,
						.y = (I32)m->lLastY,
					},
				});
					
			} else if (rawInput->header.dwType == RIM_TYPEKEYBOARD) {
				const RAWKEYBOARD* const k = &rawInput->data.keyboard;
				U32 scanCode   = k->MakeCode;
				U32 virtualKey = k->VKey;
				const Bool e0  = k->Flags & RI_KEY_E0;
				const Bool e1  = k->Flags & RI_KEY_E1;
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
						.down = !(k->Flags & RI_KEY_BREAK),
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
				window.minimized = false;
			} else if (wparam == SIZE_MINIMIZED) {
				window.minimized = true;
			} else if (wparam == SIZE_RESTORED) {
				window.minimized = false;
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
			window.windowRect.x = wp->x;
			window.windowRect.y = wp->y;
			window.windowRect.w = wp->cx;
			window.windowRect.h = wp->cy;
			RECT cr = {};
			GetClientRect(hwnd, &cr);
			window.clientRect.x = cr.left;
			window.clientRect.y = cr.top;
			window.clientRect.w = cr.right - cr.left;
			window.clientRect.h = cr.bottom - cr.top;

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

	JC_ASSERT(displaysLen < MaxDisplays);
	displays[displaysLen] = {
		.x      = (I32)rect->left,
		.y      = (I32)rect->top,
		.width  = (U32)(rect->right - rect->left),
		.height = (U32)(rect->bottom - rect->top),
		.dpi       = dpi,
		.dpiScale  = (F32)dpi / (F32)USER_DEFAULT_SCREEN_DPI,
	};
	displaysLen++;

	if (monitorInfoEx.dwFlags & MONITORINFOF_PRIMARY) {
		Display t = displays[0];
		displays[0] = displays[displaysLen];
		displays[displaysLen] = t;
	}

	return TRUE;
}

//----------------------------------------------------------------------------------------------

Res<> Init(const InitDesc* initDesc) {
	tempAllocator = initDesc->tempAllocator;
	logger       = initDesc->logger;

	if (EnumDisplayMonitors(0, 0, MonitorEnumFn, 0) == FALSE) {
		return Err_WinLast("EnumDisplayMonitors");
	}
	JC_ASSERT(displaysLen > 0);

	if (SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2) == FALSE) {
		return Err_WinLast("SetProcessDpiAwarenessContext");
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
		return Err_WinLast("RegisterClassExW");
	}

	const WStrZ titlew = Unicode::Utf8ToWtf16z(tempAllocator, initDesc->title);

	window.style = initDesc->style;
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

	const Display* const display = &displays[(initDesc->displayIdx >= displaysLen) ? 0 : initDesc->displayIdx];
	RECT r = {};
	if (initDesc->style == Style::Fullscreen) {
		r.left =   display->x;
		r.top    = display->y;
		r.right  = display->x + display->width;
		r.bottom = display->y + display->height;
	} else {
		const U32 w = Min(initDesc->width,  display->width);
		const U32 h = Min(initDesc->height, display->height);
		const I32 x = display->x + (I32)(display->width  - w) / 2;
		const I32 y = display->y + (I32)(display->height - h) / 2;
		r.left =   x;
		r.top    = y;
		r.right  = x + w;
		r.bottom = y + h;
	}

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
		return Err_WinLast("CreateWindowExW");
	}

	window.windowRect.x = r.left;
	window.windowRect.y = r.bottom;
	window.windowRect.w = r.right - r.left;
	window.windowRect.h = r.bottom - r.top;
	window.dpiScale     = (F32)window.dpi / (F32)USER_DEFAULT_SCREEN_DPI;

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

Span<Display> GetDisplays() {
	return Span<Display>(displays, displaysLen);
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
		.x          = window.windowRect.y,
		.y          = window.windowRect.x,
		.width      = window.clientRect.w,
		.height     = window.clientRect.h,
		.style      = window.style,
		.cursorMode = window.cursorMode,
		.minimized  = window.minimized,
		.focused   = window.focused,
	};
}

//----------------------------------------------------------------------------------------------

void SetRect(Rect newRect) {
	RECT r;
	::SetRect(&r, newRect.x, newRect.y, newRect.x + newRect.w, newRect.y + newRect.h);
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

PlatformDesc GetPlatformDesc() {
	return PlatformDesc {
		.hinstance = GetModuleHandleW(0),
		.hwnd      = window.hwnd,
	};
}

//----------------------------------------------------------------------------------------------

Bool IsExitRequested() {
	return exitRequested;
}

//--------------------------------------------------------------------------------------------------
}	// namespace JC::Window