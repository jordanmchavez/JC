#include "JC/Window.h"

#include "JC/Array.h"
#include "JC/Event.h"
#include "JC/Fmt.h"
#include "JC/Log.h"
#include "JC/Mem.h"
#include "JC/Sys_Win.h"
#include "JC/Unicode.h"

#include <ShellScalingApi.h>

static constexpr U32 HID_USAGE_PAGE_GENERIC     = 0x01;
static constexpr U32 HID_USAGE_GENERIC_MOUSE    = 0x02;
static constexpr U32 HID_USAGE_GENERIC_KEYBOARD = 0x06;

#pragma comment(lib, "shcore.lib")

//--------------------------------------------------------------------------------------------------

static constexpr U32 Wnd_MaxDisplays = 32;

struct Wnd_Window {
	HWND            hwnd;
	DWORD           winStyle;
	Wnd_Style       style;
	Wnd_CursorMode  cursorMode;
	IRect           windowRect;
	IRect           clientRect;
	U32             dpi;
	F32             dpiScale;
	Bool            minimized;
	Bool            focused;
};

static Mem*           wnd_tempMem;
static Wnd_Display    wnd_displays[Wnd_MaxDisplays];
static U32            wnd_displaysLen;
static Wnd_Window     wnd_window;

//----------------------------------------------------------------------------------------------

static LRESULT CALLBACK Wnd_Proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
	switch (msg) {
		case WM_ACTIVATE:
			if (wparam == WA_INACTIVE) {
				wnd_window.focused = false;
				if (wnd_window.cursorMode != Wnd_CursorMode::VisibleFree) {
					ClipCursor(0);
				}
			} else {
				wnd_window.focused = true;
				if (wnd_window.cursorMode != Wnd_CursorMode::VisibleFree) {
					RECT r;
					GetWindowRect(hwnd, &r);
					ClipCursor(&r);
				}
			}
			break;

		case WM_CLOSE:
			Ev_Add({ .type = Ev_Type::ExitRequest });
			break;

		case WM_DPICHANGED:
			wnd_window.dpi      = LOWORD(wparam);
			wnd_window.dpiScale = (F32)LOWORD(wparam) / (F32)USER_DEFAULT_SCREEN_DPI;
			break;

		case WM_EXITSIZEMOVE:
			if (wnd_window.cursorMode != Wnd_CursorMode::VisibleFree && hwnd == GetActiveWindow()) {
				RECT r;
				GetWindowRect(hwnd, &r);
				ClipCursor(&r);
			}
			break;

		case WM_INPUT: {
			const HRAWINPUT hrawInput = (HRAWINPUT)lparam;
			UINT size = 0;
			GetRawInputData(hrawInput, RID_INPUT, 0, &size, sizeof(RAWINPUTHEADER));
			RAWINPUT* rawInput = (RAWINPUT*)Mem_Alloc(wnd_tempMem, size);
			GetRawInputData(hrawInput, RID_INPUT, rawInput, &size, sizeof(RAWINPUTHEADER));
			if (rawInput->header.dwType == RIM_TYPEMOUSE) {
				const RAWMOUSE* const m = &rawInput->data.mouse;
				if (m->usButtonFlags & RI_MOUSE_WHEEL) {
					Ev_Add({
						.type = Ev_Type::MouseWheel,
						.mouseWheel = { .delta = (float)((signed short)m->usButtonData) / 120.0f },
					});
				}
				if (m->usButtonFlags & RI_MOUSE_BUTTON_1_DOWN) { Ev_Add({ .type = Ev_Type::Key, .key = { .down = true,  .key  = Ev_Key::Mouse1 } }); }
				if (m->usButtonFlags & RI_MOUSE_BUTTON_1_UP  ) { Ev_Add({ .type = Ev_Type::Key, .key = { .down = false, .key  = Ev_Key::Mouse1 } }); }
				if (m->usButtonFlags & RI_MOUSE_BUTTON_2_DOWN) { Ev_Add({ .type = Ev_Type::Key, .key = { .down = true,  .key  = Ev_Key::Mouse2 } }); }
				if (m->usButtonFlags & RI_MOUSE_BUTTON_2_UP  ) { Ev_Add({ .type = Ev_Type::Key, .key = { .down = false, .key  = Ev_Key::Mouse2 } }); }
				if (m->usButtonFlags & RI_MOUSE_BUTTON_3_DOWN) { Ev_Add({ .type = Ev_Type::Key, .key = { .down = true,  .key  = Ev_Key::Mouse3 } }); }
				if (m->usButtonFlags & RI_MOUSE_BUTTON_3_UP  ) { Ev_Add({ .type = Ev_Type::Key, .key = { .down = false, .key  = Ev_Key::Mouse3 } }); }
				if (m->usButtonFlags & RI_MOUSE_BUTTON_4_DOWN) { Ev_Add({ .type = Ev_Type::Key, .key = { .down = true,  .key  = Ev_Key::Mouse4 } }); }
				if (m->usButtonFlags & RI_MOUSE_BUTTON_4_UP  ) { Ev_Add({ .type = Ev_Type::Key, .key = { .down = false, .key  = Ev_Key::Mouse4 } }); }
				if (m->usButtonFlags & RI_MOUSE_BUTTON_5_DOWN) { Ev_Add({ .type = Ev_Type::Key, .key = { .down = true,  .key  = Ev_Key::Mouse5 } }); }
				if (m->usButtonFlags & RI_MOUSE_BUTTON_5_UP  ) { Ev_Add({ .type = Ev_Type::Key, .key = { .down = false, .key  = Ev_Key::Mouse5 } }); }

				::POINT mousePos;
				::GetCursorPos(&mousePos);
				::ScreenToClient(hwnd, &mousePos);
				Ev_Add({
					.type = Ev_Type::MouseMove,
					.mouseMove = {
						.x  = (I32)mousePos.x,
						.y  = (I32)mousePos.y,
						.dx = (I32)m->lLastX,
						.dy = (I32)m->lLastY,
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
				const bool down = !(k->Flags & RI_KEY_BREAK);
				Ev_Add({ .type = Ev_Type::Key, .key = { .down = down, .key  = (Ev_Key)virtualKey } });
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
				wnd_window.minimized = false;
			} else if (wparam == SIZE_MINIMIZED) {
				wnd_window.minimized = true;
			} else if (wparam == SIZE_RESTORED) {
				wnd_window.minimized = false;
			}
			break;

		case WM_SYSCOMMAND:
			if (wparam == SC_CLOSE) {
				Ev_Add({ .type = Ev_Type::ExitRequest });
				return 0;
			}
			break;

		case WM_WINDOWPOSCHANGED: {
			const WINDOWPOS* wp = (WINDOWPOS*)lparam;
			wnd_window.windowRect.x      = wp->x;
			wnd_window.windowRect.y      = wp->y;
			wnd_window.windowRect.width  = wp->cx;
			wnd_window.windowRect.height = wp->cy;
			RECT cr = {};
			GetClientRect(hwnd, &cr);
			wnd_window.clientRect.x      = cr.left;
			wnd_window.clientRect.y      = cr.top;
			wnd_window.clientRect.width  = cr.right - cr.left;
			wnd_window.clientRect.height = cr.bottom - cr.top;

			//if (windowApi->cursorMode != WindowWnd_CursorMode::VisibleFree && GetActiveWindow() == windowApi->hwnd) {
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

static BOOL Wnd_MonitorEnumFn(HMONITOR hmonitor, HDC, LPRECT rect, LPARAM) {
	MONITORINFOEX monitorInfoEx = {};
	monitorInfoEx.cbSize = sizeof(monitorInfoEx);
	GetMonitorInfoW(hmonitor, &monitorInfoEx);

	UINT dpi    = 0;
	UINT unused = 0;
	GetDpiForMonitor(hmonitor, MDT_EFFECTIVE_DPI, &dpi, &unused);

	Assert(wnd_displaysLen < Wnd_MaxDisplays);
	wnd_displays[wnd_displaysLen] = {
		.x      = (I32)rect->left,
		.y      = (I32)rect->top,
		.width  = (U32)(rect->right - rect->left),
		.height = (U32)(rect->bottom - rect->top),
		.dpi       = dpi,
		.dpiScale  = (F32)dpi / (F32)USER_DEFAULT_SCREEN_DPI,
	};
	wnd_displaysLen++;

	if (monitorInfoEx.dwFlags & MONITORINFOF_PRIMARY) {
		Wnd_Display t = wnd_displays[0];
		wnd_displays[0] = wnd_displays[wnd_displaysLen];
		wnd_displays[wnd_displaysLen] = t;
	}

	return TRUE;
}

//----------------------------------------------------------------------------------------------

Res<> Wnd_Init(const Wnd_InitDesc* initDesc) {
	wnd_tempMem = initDesc->tempMem;;

	if (EnumDisplayMonitors(0, 0, Wnd_MonitorEnumFn, 0) == FALSE) {
		return Win_LastErr("EnumDisplayMonitors");
	}
	Assert(wnd_displaysLen > 0);

	if (SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2) == FALSE) {
		return Win_LastErr("SetProcessDpiAwarenessContext");
	}

	const WNDCLASSEXW wndClassExW = {
		.cbSize        = sizeof(wndClassExW),
		.style         = CS_OWNDC | CS_DBLCLKS,
		.lpfnWndProc   = Wnd_Proc,
		.cbClsExtra    = 0,
		.cbWndExtra    = 0,
		.hInstance     = GetModuleHandle(0),
		.hIcon         = 0,
		.hCursor       = LoadCursor(0, IDC_ARROW),
		.hbrBackground = 0,
		.lpszMenuName  = 0,
		.lpszClassName = L"JC",
		.hIconSm       = 0,
	};
	if (!RegisterClassExW(&wndClassExW)) {
		return Win_LastErr("RegisterClassExW");
	}

	Span<wchar_t> titlew = Unicode_Utf8ToWtf16z(wnd_tempMem, initDesc->title);

	wnd_window.style = initDesc->style;
	wnd_window.winStyle = 0;
	switch (wnd_window.style) {
		case Wnd_Style::Bordered:
			wnd_window.winStyle = WS_CAPTION | WS_MAXIMIZEBOX | WS_MINIMIZEBOX | WS_SYSMENU;
			break;
		case Wnd_Style::BorderedResizable:
			wnd_window.winStyle = WS_CAPTION | WS_MAXIMIZEBOX | WS_MINIMIZEBOX | WS_SYSMENU | WS_THICKFRAME;
			break;
		case Wnd_Style::Borderless:
			wnd_window.winStyle = WS_CAPTION | WS_MAXIMIZEBOX | WS_MINIMIZEBOX | WS_POPUP | WS_SYSMENU;
			break;
		case Wnd_Style::Fullscreen:
			wnd_window.winStyle = WS_POPUP;
			break;
	}

	Wnd_Display const* const display = &wnd_displays[(initDesc->displayIdx >= wnd_displaysLen) ? 0 : initDesc->displayIdx];
	RECT r = {};
	if (initDesc->style == Wnd_Style::Fullscreen) {
		r.left =   display->x;
		r.top    = display->y;
		r.right  = display->x + display->width;
		r.bottom = display->y + display->height;
	} else {
		U32 const w = Min(initDesc->width,  display->width);
		U32 const h = Min(initDesc->height, display->height);
		I32 const x = display->x + (I32)(display->width  - w) / 2;
		I32 const y = display->y + (I32)(display->height - h) / 2;
		r.left =   x;
		r.top    = y;
		r.right  = x + w;
		r.bottom = y + h;
	}

	HMONITOR hmonitor = MonitorFromRect(&r, MONITOR_DEFAULTTONEAREST);
	UINT unused = 0;
	GetDpiForMonitor(hmonitor, MDT_EFFECTIVE_DPI, &wnd_window.dpi, &unused);
	AdjustWindowRectExForDpi(&r, wnd_window.winStyle, FALSE, 0, wnd_window.dpi);

	wnd_window.hwnd = CreateWindowExW(
		0,
		L"JC",
		titlew.data,
		wnd_window.winStyle,
		r.left,
		r.top,
		r.right - r.left,
		r.bottom - r.top,
		0,
		0,
		GetModuleHandleW(0),
		0
	);
	if (!wnd_window.hwnd) {
		return Win_LastErr("CreateWindowExW");
	}

	wnd_window.windowRect.x      = r.left;
	wnd_window.windowRect.y      = r.bottom;
	wnd_window.windowRect.width  = r.right - r.left;
	wnd_window.windowRect.height = r.bottom - r.top;
	wnd_window.dpiScale     = (F32)wnd_window.dpi / (F32)USER_DEFAULT_SCREEN_DPI;

	SetFocus(wnd_window.hwnd);
	ShowWindow(wnd_window.hwnd, SW_SHOW);
	UpdateWindow(wnd_window.hwnd);
	RAWINPUTDEVICE rawInputDevices[2] = {
		{
			.usUsagePage = (USHORT)HID_USAGE_PAGE_GENERIC,
			.usUsage     = (USHORT)HID_USAGE_GENERIC_KEYBOARD,
			.dwFlags     = RIDEV_INPUTSINK,
			.hwndTarget  = wnd_window.hwnd,
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
	if (wnd_window.hwnd) {
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
		DestroyWindow(wnd_window.hwnd);
		wnd_window.hwnd = 0;
	}
}

//----------------------------------------------------------------------------------------------

Span<Wnd_Display const> GetDisplays() {
	return Span<Wnd_Display const>(wnd_displays, wnd_displaysLen);
}

//----------------------------------------------------------------------------------------------

void Wnd_Frame() {
	const Bool prevMinimized = wnd_window.minimized;
	const Bool prevFocused   = wnd_window.focused;
	const U32  prevWidth     = wnd_window.windowRect.width;
	const U32  prevHeight    = wnd_window.windowRect.height;


	MSG msg;
	while (PeekMessageW(&msg, 0, 0, 0, PM_REMOVE)) {
		TranslateMessage(&msg);
		DispatchMessageW(&msg);
	}

	if (!prevMinimized && wnd_window.minimized) {
		Ev_Add({ .type = Ev_Type::WindowMinimized });
	} else if (prevMinimized && !wnd_window.minimized) {
		Ev_Add({ .type = Ev_Type::WindowRestored });
	}
	if (!prevFocused && wnd_window.focused) {
		Ev_Add({ .type = Ev_Type::WindowFocused });
	} else if (prevFocused && !wnd_window.focused) {
		Ev_Add({ .type = Ev_Type::WindowUnfocused });
	}
	if (prevWidth != wnd_window.windowRect.width || prevHeight != wnd_window.windowRect.height) {
		Ev_Add({
			.type = Ev_Type::WindowResized,
			.windowResized = {
				.width  = wnd_window.windowRect.width,
				.height = wnd_window.windowRect.height
			},
		});
	}
}

//----------------------------------------------------------------------------------------------

void SetRect(IRect newRect) {
	RECT r;
	::SetRect(&r, newRect.x, newRect.y, newRect.x + newRect.width, newRect.y + newRect.height);
	AdjustWindowRectExForDpi(&r, wnd_window.winStyle, FALSE, 0, wnd_window.dpi);
	SetWindowPos(wnd_window.hwnd, HWND_TOP, r.left, r.top, r.right - r.left, r.bottom - r.top, SWP_NOZORDER | SWP_NOACTIVATE);
}

//----------------------------------------------------------------------------------------------

void Wnd_SetCursorMode(Wnd_CursorMode newCursorMode) {
	if (wnd_window.cursorMode != Wnd_CursorMode::VisibleFree) {
		ClipCursor(0);
	}
	if (newCursorMode != Wnd_CursorMode::VisibleFree) {
		RECT r;
		GetWindowRect(wnd_window.hwnd, &r);
		ClipCursor(&r);
	}
	wnd_window.cursorMode = newCursorMode;
}

//----------------------------------------------------------------------------------------------

Wnd_PlatformDesc Wnd_GetPlatformDesc() {
	return Wnd_PlatformDesc {
		.hinstance = GetModuleHandleW(0),
		.hwnd      = wnd_window.hwnd,
	};
}