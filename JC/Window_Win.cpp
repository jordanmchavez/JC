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
			.dpiScale  = (float)dpi / (float)USER_DEFAULT_SCREEN_DPI,
		};

		const DisplayInfo* di = &displayApi->displayInfos[displayApi->displayInfosLen];
		displayApi->log->Printf(
			"  Display {}: pos={}x{}, size={}x{}, dpiScale={}{}", 
			displayApi->displayInfosLen,
			di->rect.x,
			di->rect.y,
			di->rect.width,
			di->rect.height,
			di->dpiScale,
			di->primary ? " ***PRIMARY***" : ""
		);

		displayApi->displayInfosLen++;

		return TRUE;
	}

	Res<> Init(Log* log_) override {
		log = log_;
		return Refresh();
	}

	Res<> Refresh() override {
		Logf("Refreshing displays:");
		if (EnumDisplayMonitors(0, 0, MonitorEnumFn, (LPARAM)this) == FALSE) {
			return MakeLastErr(EnumDisplayMonitors)->Push(Err_EnumDisplays);
		}
		return Ok();
	}

	Span<DisplayInfo> GetDisplayInfos() override {
		return displayInfos;
	}
};

static DisplayApiObj displayApi = {};

DisplayApi* GetDisplayApi() {
	return &displayApi;
}

//--------------------------------------------------------------------------------------------------

struct WindowObj {
	HWND       hwnd       = 0;
	WindowMode mode       = {};
	Rect       windowRect = {};
	Rect       clientRect = {};
	float      dpiScale   = 0.0f;
	bool       focused    = false;
};

struct WindowApiObj : WindowApi {
	static constexpr u32 MaxWindows = 64;

	DisplayApi* displayApi             = 0;
	EventApi*   eventApi               = 0;
	Log*        log                    = 0;
	TempMem*    tempMem                = 0;
	WindowObj   windowObjs[MaxWindows] = {};

	static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

	Res<>       Init(DisplayApi* displayApi_, EventApi* eventApi_, Log* log_, TempMem* tempMem_) override;
	Res<Window> CreateWindow(s8 title, WindowMode mode, Rect rect, u32 display) override;
	void        PumpMessages() override;
};

static WindowApiObj windowApi;

WindowApi* GetWindowApi() {
	return &windowApi;
}

//--------------------------------------------------------------------------------------------------

void Add(Array<char>* a, s8 s) { a->Add(s.data, s.len); }

LRESULT CALLBACK WindowApiObj::WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
	WindowObj* windowObj = (WindowObj*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
	if (!windowObj) {
		return DefWindowProcW(hwnd, msg, wparam, lparam);
	}

	Log* log = windowApi.log;

	switch (msg) {
		case WM_ACTIVATE:
			Logf("WM_ACTIVATE: wparam={}", wparam);
			windowObj->focused = (wparam != WA_INACTIVE);
			if (wparam == WA_INACTIVE) {
				Logf("Disabling clip cursor");
				ClipCursor(0);
			} else {
				RECT r;
				GetWindowRect(windowObj->hwnd, &r);
				Logf("Enabling clip cursor: {}x{} - {}x{}", r.left, r.top, r.right, r.bottom);
				ClipCursor(&r);
			}
			break;
		case WM_CLOSE:
			Logf("WM_CLOSE");
			windowApi.eventApi->AddEvent(Event {
				.type = EventType::Exit
			});
			break;
		case WM_SYSCOMMAND:
			Logf("WM_SYSCOMMAND: wparam={}", wparam);
			if (wparam == SC_CLOSE) {
				windowApi.eventApi->AddEvent(Event {
					.type = EventType::Exit
				});
				return 0;
			}
			break;
		case WM_EXITSIZEMOVE: {
			Logf("WM_EXITSIZEMOVE");
			if (windowObj->hwnd == GetActiveWindow()) {
				RECT r;
				GetWindowRect(windowObj->hwnd, &r);
				Logf("Enabling clip cursor: {}x{} - {}x{}", r.left, r.top, r.right, r.bottom);
				ClipCursor(&r);
			}
			break;
		}

		case WM_WINDOWPOSCHANGED: {
			const WINDOWPOS* wp = (WINDOWPOS*)lparam;
			Array<char> flagStr(windowApi.tempMem);
			if (wp->flags & SWP_DRAWFRAME)      { Add(&flagStr, "SWP_DRAWFRAME|"); }
			if (wp->flags & SWP_FRAMECHANGED)   { Add(&flagStr, "SWP_FRAMECHANGED|"); }
			if (wp->flags & SWP_HIDEWINDOW)     { Add(&flagStr, "SWP_HIDEWINDOW|"); }
			if (wp->flags & SWP_NOACTIVATE)     { Add(&flagStr, "SWP_NOACTIVATE|"); }
			if (wp->flags & SWP_NOCOPYBITS)     { Add(&flagStr, "SWP_NOCOPYBITS|"); }
			if (wp->flags & SWP_NOMOVE)         { Add(&flagStr, "SWP_NOMOVE|"); }
			if (wp->flags & SWP_NOOWNERZORDER)  { Add(&flagStr, "SWP_NOOWNERZORDER|"); }
			if (wp->flags & SWP_NOREDRAW)       { Add(&flagStr, "SWP_NOREDRAW|"); }
			if (wp->flags & SWP_NOREPOSITION)   { Add(&flagStr, "SWP_NOREPOSITION|"); }
			if (wp->flags & SWP_NOSENDCHANGING) { Add(&flagStr, "SWP_NOSENDCHANGING|"); }
			if (wp->flags & SWP_NOSIZE)         { Add(&flagStr, "SWP_NOSIZE|"); }
			if (wp->flags & SWP_NOZORDER)       { Add(&flagStr, "SWP_NOZORDER|"); }
			if (wp->flags & SWP_SHOWWINDOW)     { Add(&flagStr, "SWP_SHOWWINDOW|"); }
			if (flagStr.len) { flagStr.len--; }
			Logf("WM_WINDOWPOSCHANGED: windowPos=({}x{}, {}x{}), flags={}", wp->x, wp->y, wp->cx, wp->cy, s8(flagStr.data, flagStr.len));
			windowObj->windowRect.x      = wp->x;
			windowObj->windowRect.y      = wp->y;
			windowObj->windowRect.width  = wp->cx;
			windowObj->windowRect.height = wp->cy;
			RECT cr = {};
			GetClientRect(windowObj->hwnd, &cr);
			windowObj->clientRect.x      = cr.left;
			windowObj->clientRect.y      = cr.top;
			windowObj->clientRect.width  = cr.right - cr.left;
			windowObj->clientRect.height = cr.bottom - cr.top;

			if (GetActiveWindow() == windowObj->hwnd) {
				RECT r;
				GetWindowRect(windowObj->hwnd, &r);
				Logf("Enabling clip cursor: {}x{} - {}x{}", r.left, r.top, r.right, r.bottom);
				ClipCursor(&r);
			}
			break;
		}
		case WM_DPICHANGED:
			Logf("WM_DPICHANGE: wparam={}, dpi={}", wparam, LOWORD(wparam));
			windowObj->dpiScale = (float)LOWORD(wparam) / (float)USER_DEFAULT_SCREEN_DPI;
			break;
		case WM_SETCURSOR: {
			if (LOWORD(lparam) == HTCLIENT) {
				//Logf("WM_SETCUSOR in HTCLIENT");
				SetCursor(LoadCursor(0, IDC_NO));
			}
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

//--------------------------------------------------------------------------------------------------

Res<> WindowApiObj::Init(DisplayApi* displayApi_, EventApi* eventApi_, Log* log_, TempMem* tempMem_) {
	displayApi = displayApi_;
	eventApi   = eventApi_;
	log        = log_;
	tempMem    = tempMem_;
	MemSet(windowObjs, 0, sizeof(windowObjs));

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

	return Ok();
}

//--------------------------------------------------------------------------------------------------

Res<Window> WindowApiObj::CreateWindow(s8 title, WindowMode mode, Rect rect, u32 display) {
	WindowObj* windowObj = 0;
	for (u32 i = 1; i < MaxWindows; i++) {
		if (!windowObjs[i].hwnd) {
			windowObj = &windowObjs[i];
		}
	}
	Assert(windowObj);

	Res<s16z> titlew = Unicode::Utf8ToWtf16z(tempMem, title);
	if (!titlew) {
		return titlew.err;
	}

	DWORD style = 0;
	switch (mode) {
		case WindowMode::Bordered:
			style = WS_CAPTION | WS_MAXIMIZEBOX | WS_MINIMIZEBOX | WS_SYSMENU;
			break;
		case WindowMode::BorderedResizable:
			style = WS_CAPTION | WS_MAXIMIZEBOX | WS_MINIMIZEBOX | WS_SYSMENU | WS_THICKFRAME;
			break;
		case WindowMode::Borderless:
			style = WS_CAPTION | WS_MAXIMIZEBOX | WS_MINIMIZEBOX | WS_POPUP | WS_SYSMENU;
			break;
		case WindowMode::Fullscreen:
			style = WS_POPUP;
			break;
	}

	RECT windowRect = {};
	if (mode == WindowMode::Fullscreen) {
		Span<DisplayInfo> displayInfos = displayApi->GetDisplayInfos();
		Assert(display <= displayInfos.len);
		rect = displayInfos[display].rect;
	}
	SetRect(&windowRect, rect.x, rect.y, rect.x + rect.width, rect.y + rect.height);

	HMONITOR hmonitor = MonitorFromRect(&windowRect, MONITOR_DEFAULTTONEAREST);
	UINT dpi    = 0;
	UINT unused = 0;
	GetDpiForMonitor(hmonitor, MDT_EFFECTIVE_DPI, &dpi, &unused);
	AdjustWindowRectExForDpi(&windowRect, style, FALSE, 0, dpi);

	Logf("Creating window: style={}, windowRect=({}x{} - {}x{})", style, windowRect.left, windowRect.top, windowRect.right, windowRect.bottom);
	const HWND hwnd = CreateWindowExW(
		0,
		L"JC",
		titlew.val.data,
		style,
		windowRect.left,
		windowRect.top,
		windowRect.right - windowRect.left,
		windowRect.bottom - windowRect.top,
		0,
		0,
		GetModuleHandle(0),
		0
	);
	if (!hwnd) {
		return MakeLastErr(CreateWindowEx);
	}
	SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)windowObj);

	windowObj->hwnd              = hwnd;
	windowObj->mode              = mode;
	windowObj->windowRect.x      = windowRect.left;
	windowObj->windowRect.y      = windowRect.bottom;
	windowObj->windowRect.width  = windowRect.right - windowRect.left;
	windowObj->windowRect.height = windowRect.bottom - windowRect.top;
	windowObj->dpiScale          = (float)dpi / (float)USER_DEFAULT_SCREEN_DPI;

	SetFocus(hwnd);
	ShowWindow(hwnd, SW_SHOW);

	return Window { .handle = (u64)(windowObj - windowObjs) };
}

//--------------------------------------------------------------------------------------------------

void WindowApiObj::PumpMessages() {
	MSG msg;
	while (PeekMessageW(&msg, 0, 0, 0, PM_REMOVE)) {
		TranslateMessage(&msg);
		DispatchMessageW(&msg);
	}
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC