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
static constexpr U32 MaxEvents = 1024;

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

static Mem     tempMem;
static Display displays[MaxDisplays];
static U32     displaysLen;
static Window  window;
static bool    inSizeMove;
static Event   events[MaxEvents];
static U32     eventsHead;
static U32     eventsTail;

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
			AddEvent({ .eventType = EventType::ExitRequest });
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
				const RAWMOUSE* const m = &rawInput->data.mouse;
				if (m->usButtonFlags & RI_MOUSE_WHEEL) {
					I16 const wheelDelta = (I16)m->usButtonData;
					Key::Key const key = (wheelDelta > 0) ? Key::Key::MouseWheelUp : Key::Key::MouseWheelDown;
					const U32 numEvents =  (U32)(wheelDelta > 0 ? wheelDelta : -wheelDelta) / 120;
					for (U32 i = 0; i < numEvents; i++) {
						AddEvent({ .eventType = EventType::Key, .key = { .down = true, .key = key } });
					}
				}
				if (m->usButtonFlags & RI_MOUSE_BUTTON_1_DOWN) { AddEvent({ .eventType = EventType::Key, .key = { .down = true,  .key  = Key::Key::Mouse1} }); }
				if (m->usButtonFlags & RI_MOUSE_BUTTON_1_UP  ) { AddEvent({ .eventType = EventType::Key, .key = { .down = false, .key  = Key::Key::Mouse1} }); }
				if (m->usButtonFlags & RI_MOUSE_BUTTON_2_DOWN) { AddEvent({ .eventType = EventType::Key, .key = { .down = true,  .key  = Key::Key::Mouse2} }); }
				if (m->usButtonFlags & RI_MOUSE_BUTTON_2_UP  ) { AddEvent({ .eventType = EventType::Key, .key = { .down = false, .key  = Key::Key::Mouse2} }); }
				if (m->usButtonFlags & RI_MOUSE_BUTTON_3_DOWN) { AddEvent({ .eventType = EventType::Key, .key = { .down = true,  .key  = Key::Key::Mouse3} }); }
				if (m->usButtonFlags & RI_MOUSE_BUTTON_3_UP  ) { AddEvent({ .eventType = EventType::Key, .key = { .down = false, .key  = Key::Key::Mouse3} }); }
				if (m->usButtonFlags & RI_MOUSE_BUTTON_4_DOWN) { AddEvent({ .eventType = EventType::Key, .key = { .down = true,  .key  = Key::Key::Mouse4 } }); }
				if (m->usButtonFlags & RI_MOUSE_BUTTON_4_UP  ) { AddEvent({ .eventType = EventType::Key, .key = { .down = false, .key  = Key::Key::Mouse4 } }); }
				if (m->usButtonFlags & RI_MOUSE_BUTTON_5_DOWN) { AddEvent({ .eventType = EventType::Key, .key = { .down = true,  .key  = Key::Key::Mouse5 } }); }
				if (m->usButtonFlags & RI_MOUSE_BUTTON_5_UP  ) { AddEvent({ .eventType = EventType::Key, .key = { .down = false, .key  = Key::Key::Mouse5 } }); }

				::POINT mousePos;
				::GetCursorPos(&mousePos);
				::ScreenToClient(hwnd, &mousePos);
				AddEvent({
					.eventType = EventType::MouseMove,
					.mouseMove = {
						.x  = (I32)mousePos.x,
						.y  = (I32)mousePos.y,
						.dx = (I32)m->lLastX,
						.dy = (I32)m->lLastY,
					},
				});
					
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
					switch (scanCode) {
						case 0x01: key = Key::Key::Escape;
						case 0x02: key = Key::Key::One;
						case 0x03: key = Key::Key::Two;
						case 0x04: key = Key::Key::Three;
						case 0x05: key = Key::Key::Four;
						case 0x06: key = Key::Key::Five;
						case 0x07: key = Key::Key::Six;
						case 0x08: key = Key::Key::Seven;
						case 0x09: key = Key::Key::Eight;
						case 0x0A: key = Key::Key::Nine;
						case 0x0B: key = Key::Key::Zero;
						case 0x0C: key = Key::Key::Minus;
						case 0x0D: key = Key::Key::Equals;
						case 0x0E: key = Key::Key::Backspace;
						case 0x0F: key = Key::Key::Tab;
						case 0x10: key = Key::Key::Q;
						case 0x11: key = Key::Key::W;
						case 0x12: key = Key::Key::E;
						case 0x13: key = Key::Key::R;
						case 0x14: key = Key::Key::T;
						case 0x15: key = Key::Key::Y;
						case 0x16: key = Key::Key::U;
						case 0x17: key = Key::Key::I;
						case 0x18: key = Key::Key::O;
						case 0x19: key = Key::Key::P;
						case 0x1A: key = Key::Key::LeftBracket;
						case 0x1B: key = Key::Key::RightBracket;
						case 0x1C: key = e0 ? Key::Key::NumpadEnter : Key::Key::Enter;
						case 0x1D: key = e0 ? Key::Key::CtrlRight   : Key::Key::CtrlLeft;
						case 0x1E: key = Key::Key::A;
						case 0x1F: key = Key::Key::S;
						case 0x20: key = Key::Key::D;
						case 0x21: key = Key::Key::F;
						case 0x22: key = Key::Key::G;
						case 0x23: key = Key::Key::H;
						case 0x24: key = Key::Key::J;
						case 0x25: key = Key::Key::K;
						case 0x26: key = Key::Key::L;
						case 0x27: key = Key::Key::Semicolon;
						case 0x28: key = Key::Key::Quote;
						case 0x29: key = Key::Key::BackQuote;
						case 0x2A: key = Key::Key::ShiftLeft;
						case 0x2B: key = Key::Key::Backslash;
						case 0x2C: key = Key::Key::Z;
						case 0x2D: key = Key::Key::X;
						case 0x2E: key = Key::Key::C;
						case 0x2F: key = Key::Key::V;
						case 0x30: key = Key::Key::B;
						case 0x31: key = Key::Key::N;
						case 0x32: key = Key::Key::M;
						case 0x33: key = Key::Key::Comma;
						case 0x34: key = Key::Key::Dot;
						case 0x35: key = e0 ? Key::Key::Slash : Key::Key::Slash; // numpad / vs main /; both map to Slash: add NumpadSlash to enum if needed
						case 0x36: key = Key::Key::ShiftRight;
						case 0x37: key = e0 ? Key::Key::PrintScreen : Key::Key::NumpadStar;
						case 0x38: key = e0 ? Key::Key::AltRight : Key::Key::AltLeft;
						case 0x39: key = Key::Key::Space;
						case 0x3A: key = Key::Key::CapsLock;
						case 0x3B: key = Key::Key::F1;
						case 0x3C: key = Key::Key::F2;
						case 0x3D: key = Key::Key::F3;
						case 0x3E: key = Key::Key::F4;
						case 0x3F: key = Key::Key::F5;
						case 0x40: key = Key::Key::F6;
						case 0x41: key = Key::Key::F7;
						case 0x42: key = Key::Key::F8;
						case 0x43: key = Key::Key::F9;
						case 0x44: key = Key::Key::F10;
						case 0x45: key = Key::Key::ScrollLock; // 0x45 without E0/E1 = ScrollLock
						// (Pause's secon byte also 0x45 but handled above)
						case 0x46: key = Key::Key::ScrollLock; // some boards send 0x46
						case 0x47: key = e0 ? Key::Key::Home : Key::Key::NumPad7;
						case 0x48: key = e0 ? Key::Key::Up : Key::Key::NumPad8;
						case 0x49: key = e0 ? Key::Key::PageUp : Key::Key::NumPad9;
						case 0x4A: key = Key::Key::NumpadMinus;
						case 0x4B: key = e0 ? Key::Key::Left : Key::Key::NumPad4;
						case 0x4C: key = Key::Key::NumPad5;
						case 0x4D: key = e0 ? Key::Key::Right : Key::Key::NumPad6;
						case 0x4E: key = Key::Key::NumpadPlus;
						case 0x4F: key = e0 ? Key::Key::End : Key::Key::NumPad1;
						case 0x50: key = e0 ? Key::Key::Down : Key::Key::NumPad2;
						case 0x51: key = e0 ? Key::Key::PageDown : Key::Key::NumPad3;
						case 0x52: key = e0 ? Key::Key::Insert : Key::Key::NumPad0;
						case 0x53: key = e0 ? Key::Key::Delete : Key::Key::NumpadDot;
						case 0x57: key = Key::Key::F11;
						case 0x58: key = Key::Key::F12;
						case 0x5B: key = Key::Key::Winleft;   // always E0
						case 0x5C: key = Key::Key::WinRight;  // always E0
						case 0x5D: key = Key::Key::Menu;      // always E0
					}
				}

				if (key == Key::Key::Invalid) {
					Errorf("Unrecognized scan code: %u", scanCode);
				} else {
					AddEvent({
						.eventType = EventType::Key,
						.key = {
							.down = !(rawKeyboard->Flags & RI_KEY_BREAK),
							.key  = key,
						}
					});
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
				AddEvent({ .eventType = EventType::ExitRequest });
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

void Frame() {
	const bool prevMinimized = window.minimized;
	const bool prevFocused   = window.focused;
	U32 const  prevWidth     = window.windowRect.width;
	U32 const  prevHeight    = window.windowRect.height;


	MSG msg;
	while (PeekMessageW(&msg, 0, 0, 0, PM_REMOVE)) {
		TranslateMessage(&msg);
		DispatchMessageW(&msg);
	}

	if (!prevMinimized && window.minimized) {
		AddEvent({ .eventType = EventType::WindowMinimized });
	} else if (prevMinimized && !window.minimized) {
		AddEvent({ .eventType = EventType::WindowRestored });
	}
	if (!prevFocused && window.focused) {
		AddEvent({ .eventType = EventType::WindowFocused });
	} else if (prevFocused && !window.focused) {
		AddEvent({ .eventType = EventType::WindowUnfocused });
	}
	if (prevWidth != window.windowRect.width || prevHeight != window.windowRect.height) {
		AddEvent({
			.eventType = EventType::WindowResized,
			.windowResized = {
				.width  = window.windowRect.width,
				.height = window.windowRect.height
			},
		});
	}
}

//----------------------------------------------------------------------------------------------

void SetRect(IRect newRect) {
	RECT r;
	::SetRect(&r, newRect.x, newRect.y, newRect.x + newRect.width, newRect.y + newRect.height);
	AdjustWindowRectExForDpi(&r, window.winStyle, FALSE, 0, window.dpi);
	SetWindowPos(window.hwnd, HWND_TOP, r.left, r.top, r.right - r.left, r.bottom - r.top, SWP_NOZORDER | SWP_NOACTIVATE);
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

static_assert(Bit::IsPow2(MaxEvents));

void AddEvent(Event event) {

	U32 const nextHead = (eventsHead + 1) & (MaxEvents - 1);
	if (nextHead == eventsTail) {
		Errorf("Overwrote event");
		eventsTail = (eventsTail + 1) & (MaxEvents - 1);
	}
	events[eventsHead] = event;
	eventsHead = nextHead;
}

//----------------------------------------------------------------------------------------------

bool GetEvent(Event* eventOut) {
	if (eventsHead == eventsTail) {
		return false;
	}
	*eventOut = events[eventsTail];
	eventsTail = (eventsTail + 1) & (MaxEvents - 1);
	return true;
}

//----------------------------------------------------------------------------------------------

}	// namespace JC::Window