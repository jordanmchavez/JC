#include "JC/Window.h"

#include "JC/Bit.h"
#include "JC/Key.h"
#include "JC/Log.h"
#include "JC/Sys_Win.h"
#include "JC/Unicode.h"

#include <ShellScalingApi.h>

static constexpr JC::U32 HID_USAGE_PAGE_GENERIC     = 0x01;
static constexpr JC::U32 HID_USAGE_GENERIC_MOUSE    = 0x02;
static constexpr JC::U32 HID_USAGE_GENERIC_KEYBOARD = 0x06;

#pragma comment(lib, "shcore.lib")

namespace JC::Window {

//--------------------------------------------------------------------------------------------------

static constexpr U32 MaxDisplays = 32;
static constexpr U32 MaxKeyEvents = 1024;

struct Window {
	HWND        hwnd;
	DWORD       winStyle;
	Style       style;
	CursorMode  cursorMode;
	IRect       windowRect;
	IRect       clientRect;
	U32         dpi;
	F32         dpiScale;
	bool        minimized;
	bool        focused;
};

static Mem       tempMem;
static Display*  displays;
static U16       displaysLen;
static Window    window;
static bool      inSizeMove;
static KeyEvent* keyEvents;
static U16       keyEventsLen;
static I64       mouseDeltaX;
static I64       mouseDeltaY;
static bool      exitEvent;

//----------------------------------------------------------------------------------------------

constexpr Key::Key ScanCodeToKey(U32 scanCode, bool e0) {
	switch (scanCode) {
		case 0x01: return Key::Key::Escape;
		case 0x02: return Key::Key::One;
		case 0x03: return Key::Key::Two;
		case 0x04: return Key::Key::Three;
		case 0x05: return Key::Key::Four;
		case 0x06: return Key::Key::Five;
		case 0x07: return Key::Key::Six;
		case 0x08: return Key::Key::Seven;
		case 0x09: return Key::Key::Eight;
		case 0x0A: return Key::Key::Nine;
		case 0x0B: return Key::Key::Zero;
		case 0x0C: return Key::Key::Minus;
		case 0x0D: return Key::Key::Equals;
		case 0x0E: return Key::Key::Backspace;
		case 0x0F: return Key::Key::Tab;
		case 0x10: return Key::Key::Q;
		case 0x11: return Key::Key::W;
		case 0x12: return Key::Key::E;
		case 0x13: return Key::Key::R;
		case 0x14: return Key::Key::T;
		case 0x15: return Key::Key::Y;
		case 0x16: return Key::Key::U;
		case 0x17: return Key::Key::I;
		case 0x18: return Key::Key::O;
		case 0x19: return Key::Key::P;
		case 0x1A: return Key::Key::LeftBracket;
		case 0x1B: return Key::Key::RightBracket;
		case 0x1C: return e0 ? Key::Key::NumpadEnter : Key::Key::Enter;
		case 0x1D: return e0 ? Key::Key::CtrlRight   : Key::Key::CtrlLeft;
		case 0x1E: return Key::Key::A;
		case 0x1F: return Key::Key::S;
		case 0x20: return Key::Key::D;
		case 0x21: return Key::Key::F;
		case 0x22: return Key::Key::G;
		case 0x23: return Key::Key::H;
		case 0x24: return Key::Key::J;
		case 0x25: return Key::Key::K;
		case 0x26: return Key::Key::L;
		case 0x27: return Key::Key::Semicolon;
		case 0x28: return Key::Key::Quote;
		case 0x29: return Key::Key::BackQuote;
		case 0x2A: return Key::Key::ShiftLeft;
		case 0x2B: return Key::Key::Backslash;
		case 0x2C: return Key::Key::Z;
		case 0x2D: return Key::Key::X;
		case 0x2E: return Key::Key::C;
		case 0x2F: return Key::Key::V;
		case 0x30: return Key::Key::B;
		case 0x31: return Key::Key::N;
		case 0x32: return Key::Key::M;
		case 0x33: return Key::Key::Comma;
		case 0x34: return Key::Key::Dot;
		case 0x35: return e0 ? Key::Key::Slash : Key::Key::Slash; // numpad / vs main /; both map to Slash: add NumpadSlash to enum if needed
		case 0x36: return Key::Key::ShiftRight;
		case 0x37: return e0 ? Key::Key::PrintScreen : Key::Key::NumpadStar;
		case 0x38: return e0 ? Key::Key::AltRight : Key::Key::AltLeft;
		case 0x39: return Key::Key::Space;
		case 0x3A: return Key::Key::CapsLock;
		case 0x3B: return Key::Key::F1;
		case 0x3C: return Key::Key::F2;
		case 0x3D: return Key::Key::F3;
		case 0x3E: return Key::Key::F4;
		case 0x3F: return Key::Key::F5;
		case 0x40: return Key::Key::F6;
		case 0x41: return Key::Key::F7;
		case 0x42: return Key::Key::F8;
		case 0x43: return Key::Key::F9;
		case 0x44: return Key::Key::F10;
		case 0x45: return Key::Key::ScrollLock; // 0x45 without E0/E1 = ScrollLock 
		case 0x46: return Key::Key::ScrollLock; // some boards send 0x46 (Pause's second byte also 0x45 but handled above)
		case 0x47: return e0 ? Key::Key::Home : Key::Key::NumPad7;
		case 0x48: return e0 ? Key::Key::Up : Key::Key::NumPad8;
		case 0x49: return e0 ? Key::Key::PageUp : Key::Key::NumPad9;
		case 0x4A: return Key::Key::NumpadMinus;
		case 0x4B: return e0 ? Key::Key::Left : Key::Key::NumPad4;
		case 0x4C: return Key::Key::NumPad5;
		case 0x4D: return e0 ? Key::Key::Right : Key::Key::NumPad6;
		case 0x4E: return Key::Key::NumpadPlus;
		case 0x4F: return e0 ? Key::Key::End : Key::Key::NumPad1;
		case 0x50: return e0 ? Key::Key::Down : Key::Key::NumPad2;
		case 0x51: return e0 ? Key::Key::PageDown : Key::Key::NumPad3;
		case 0x52: return e0 ? Key::Key::Insert : Key::Key::NumPad0;
		case 0x53: return e0 ? Key::Key::Delete : Key::Key::NumpadDot;
		case 0x57: return Key::Key::F11;
		case 0x58: return Key::Key::F12;
		case 0x5B: return Key::Key::Winleft;   // always E0
		case 0x5C: return Key::Key::WinRight;  // always E0
		case 0x5D: return Key::Key::Menu;      // always E0
	}
	return Key::Key::Invalid;
}

static void AddKeyEvent(Key::Key key, bool down) {
	if (keyEventsLen >= MaxKeyEvents) {
		Errorf("Dropping key '%s' %s event", Key::GetKeyStr(key), down ? "down" : "up");
		return;
	}
	keyEvents[keyEventsLen++] = { .key = key, .down = down };
}

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
			exitEvent = true;
			break;

		case WM_DPICHANGED:
			window.dpi      = LOWORD(wparam);
			window.dpiScale = (F32)LOWORD(wparam) / (F32)USER_DEFAULT_SCREEN_DPI;
			break;

		case WM_ENTERSIZEMOVE:
			inSizeMove = true;
			break;

		case WM_EXITSIZEMOVE:
			if (window.cursorMode != CursorMode::VisibleFree && hwnd == GetActiveWindow()) {
				RECT r;
				GetWindowRect(hwnd, &r);
				ClipCursor(&r);
			}
			inSizeMove = false;
			break;

		case WM_INPUT: {
			if (inSizeMove) {
				break;
			}

			const HRAWINPUT hrawInput = (HRAWINPUT)lparam;
			UINT size = 0;
			GetRawInputData(hrawInput, RID_INPUT, 0, &size, sizeof(RAWINPUTHEADER));
			RAWINPUT* rawInput = (RAWINPUT*)Mem::Alloc(tempMem, size);
			GetRawInputData(hrawInput, RID_INPUT, rawInput, &size, sizeof(RAWINPUTHEADER));
			if (rawInput->header.dwType == RIM_TYPEMOUSE) {
				const RAWMOUSE* const rawMouse = &rawInput->data.mouse;
				if (rawMouse->usButtonFlags & RI_MOUSE_WHEEL) {
					I16 const wheelDelta = (I16)rawMouse->usButtonData;
					Key::Key const key = (wheelDelta > 0) ? Key::Key::MouseWheelUp : Key::Key::MouseWheelDown;
					const U32 numEvents =  (U32)(wheelDelta > 0 ? wheelDelta : -wheelDelta) / 120;
					for (U32 i = 0; i < numEvents; i++) {
						// Both an up and down event are necessary to make Key::MouseWheelUp/Down behavior like a normal key.
						// See the key event processing code in Input.cpp for where this matters.
						AddKeyEvent(key, true);
						AddKeyEvent(key, false);
					}
				}
				if (rawMouse->usButtonFlags & RI_MOUSE_BUTTON_1_DOWN) { AddKeyEvent(Key::Key::Mouse1, true); }
				if (rawMouse->usButtonFlags & RI_MOUSE_BUTTON_2_DOWN) { AddKeyEvent(Key::Key::Mouse2, true); }
				if (rawMouse->usButtonFlags & RI_MOUSE_BUTTON_3_DOWN) { AddKeyEvent(Key::Key::Mouse3, true); }
				if (rawMouse->usButtonFlags & RI_MOUSE_BUTTON_4_DOWN) { AddKeyEvent(Key::Key::Mouse4, true); }
				if (rawMouse->usButtonFlags & RI_MOUSE_BUTTON_5_DOWN) { AddKeyEvent(Key::Key::Mouse5, true); }
				if (rawMouse->usButtonFlags & RI_MOUSE_BUTTON_1_UP  ) { AddKeyEvent(Key::Key::Mouse1, false); }
				if (rawMouse->usButtonFlags & RI_MOUSE_BUTTON_2_UP  ) { AddKeyEvent(Key::Key::Mouse2, false); }
				if (rawMouse->usButtonFlags & RI_MOUSE_BUTTON_3_UP  ) { AddKeyEvent(Key::Key::Mouse3, false); }
				if (rawMouse->usButtonFlags & RI_MOUSE_BUTTON_4_UP  ) { AddKeyEvent(Key::Key::Mouse4, false); }
				if (rawMouse->usButtonFlags & RI_MOUSE_BUTTON_5_UP  ) { AddKeyEvent(Key::Key::Mouse5, false); }

				mouseDeltaX += (I64)rawMouse->lLastX;
				mouseDeltaY += (I64)rawMouse->lLastY;

			} else if (rawInput->header.dwType == RIM_TYPEKEYBOARD) {
				const RAWKEYBOARD* const rawKeyboard = &rawInput->data.keyboard;
				bool const e0      = rawKeyboard->Flags & RI_KEY_E0;
				bool const e1      = rawKeyboard->Flags & RI_KEY_E1;
				U32 const scanCode = rawKeyboard->MakeCode;

				Key::Key key = Key::Key::Invalid;
				if (e1 && scanCode == 0x1D)
				{
					key = Key::Key::Pause;
				} else {
					key = ScanCodeToKey(scanCode, e0);
				}
				Logf("scan code %x -> key %s", scanCode, Key::GetKeyStr(key));

				if (key == Key::Key::Invalid) {
					Errorf("Unrecognized scan code: %u", scanCode);
				} else {
					bool const down = !(rawKeyboard->Flags & RI_KEY_BREAK);
					AddKeyEvent(key, down);
				}
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
			if (inSizeMove) {
				break;
			}
			if (wparam == SC_CLOSE) {
				exitEvent = true;
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

Res<> Init(const InitDef* initDef) {

	tempMem = initDef->tempMem;

	displays = Mem::AllocT<Display>(initDef->permMem, MaxDisplays);
	keyEvents = Mem::AllocT<KeyEvent>(initDef->permMem, MaxKeyEvents);

	if (EnumDisplayMonitors(0, 0, MonitorEnumFn, 0) == FALSE) {
		return Win_LastErr("EnumDisplayMonitors");
	}
	Assert(displaysLen > 0);

	if (SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2) == FALSE) {
		return Win_LastErr("SetProcessDpiAwarenessContext");
	}

	const WNDCLASSEXW wndClassExW = {
		.cbSize        = sizeof(wndClassExW),
		.style         = CS_OWNDC | CS_DBLCLKS,
		.lpfnWndProc   = WndProc,
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

	Span<wchar_t> titlew = Unicode::Utf8ToWtf16z(tempMem, initDef->title);

	window.style = initDef->style;
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

	Display const* const display = &displays[(initDef->displayIdx >= displaysLen) ? 0 : initDef->displayIdx];
	RECT r = {};
	if (initDef->style == Style::Fullscreen) {
		r.left =   display->x;
		r.top    = display->y;
		r.right  = display->x + display->width;
		r.bottom = display->y + display->height;
	} else {
		U32 const w = Min(initDef->width,  display->width);
		U32 const h = Min(initDef->height, display->height);
		I32 const x = display->x + (I32)(display->width  - w) / 2;
		I32 const y = display->y + (I32)(display->height - h) / 2;
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
		return Win_LastErr("CreateWindowExW");
	}

	window.windowRect.x      = r.left;
	window.windowRect.y      = r.bottom;
	window.windowRect.width  = r.right - r.left;
	window.windowRect.height = r.bottom - r.top;
	window.dpiScale     = (F32)window.dpi / (F32)USER_DEFAULT_SCREEN_DPI;

	ShowWindow(window.hwnd, SW_SHOW);
	UpdateWindow(window.hwnd);
	SetFocus(window.hwnd);
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

Span<Display const> GetDisplays() {
	return Span<Display const>(displays, displaysLen);
}

//----------------------------------------------------------------------------------------------

PlatformDef GetPlatformDef() {
	return PlatformDef {
		.hinstance = GetModuleHandleW(0),
		.hwnd      = window.hwnd,
	};
}

//----------------------------------------------------------------------------------------------

State GetState() {
	return {
		.x          = window.windowRect.y,
		.y          = window.windowRect.x,
		.width      = window.clientRect.width,
		.height     = window.clientRect.height,
		.style      = window.style,
		.cursorMode = window.cursorMode,
		.minimized  = window.minimized,
		.focused    = window.focused,
	};
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

Events Frame() {
	keyEventsLen = 0;
	mouseDeltaX  = 0;
	mouseDeltaY  = 0;
	exitEvent    = false;

	MSG msg;
	while (PeekMessageW(&msg, 0, 0, 0, PM_REMOVE)) {
		TranslateMessage(&msg);
		DispatchMessageW(&msg);
	}

	::POINT mousePos;
	::GetCursorPos(&mousePos);
	::ScreenToClient(window.hwnd, &mousePos);
	return Events {
		.keyEvents   = Span<KeyEvent const>(keyEvents, keyEventsLen),
		.mouseX      = (I32)mousePos.x,
		.mouseY      = (I32)mousePos.y,
		.mouseDeltaX = mouseDeltaX,
		.mouseDeltaY = mouseDeltaY,
		.exitEvent   = exitEvent,
	};
}

//----------------------------------------------------------------------------------------------

}	// namespace JC::Window