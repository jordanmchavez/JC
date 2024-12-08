#pragma once

#include "JC/Common.h"

namespace JC {

struct EventApi;
struct Log;
struct Mem;
struct TempMem;

struct Rect {
	i32 x      = 0;
	i32 y      = 0;
	i32 width  = 0;
	i32 height = 0;
};

//--------------------------------------------------------------------------------------------------

struct DisplayInfo {
	bool primary  = false;
	Rect rect     = {};
	u32  dpi      = 0;
	f32  dpiScale = 0.0f;
};

struct DisplayApi {
	static constexpr ErrCode Err_EnumDisplays = { .ns = "disp", .code = 1 };

	virtual Res<>             Init(Log* log)    = 0;
	virtual Res<>             Refresh()         = 0;
	virtual Span<DisplayInfo> GetDisplayInfos() = 0;
};

DisplayApi* GetDisplayApi();

//--------------------------------------------------------------------------------------------------

enum struct WindowMode {
	Bordered,
	BorderedResizable,
	Borderless,
	Fullscreen,
};

enum CursorMode {
	VisibleFree,
	VisibleConstrained,
	HiddenLocked,	// ->visible: reappear at position hidden, same with alt+tabbing
};

namespace WindowStateFlags {
	constexpr u64 Minimized = 1 << 0;
	constexpr u64 Maximized = 1 << 1;
	constexpr u64 Focused   = 1 << 2;
	constexpr u64 Hidden    = 1 << 3;
};

struct WindowState {
	Rect       rect       = {};
	u64        flags      = 0;
	WindowMode windowMode = {};
	CursorMode cursorMode = {};
};

struct WindowApiInit {
	DisplayApi* displayApi        = 0;
	EventApi*   eventApi          = 0;
	Log*        log               = 0;
	TempMem*    tempMem           = 0;
	s8          title             = {};
	WindowMode  windowMode        = {};
	Rect        rect              = {};
	u32         fullscreenDisplay = 0;
};

struct WindowPlatformData {
	#if defined Platform_Windows
		void* hinstance = 0;
		void* hwnd      = 0;
	#else	// Platform_
		#error("unsupported platform");
	#endif	// Platform_
};

struct WindowApi {
	virtual Res<>              Init(const WindowApiInit* init) = 0;
	virtual void               Shutdown() = 0;
	virtual void               PumpMessages() = 0;
	virtual WindowState        GetState() = 0;
	virtual void               SetRect(Rect rect) = 0;
	virtual void               SetWindowMode(WindowMode windowMode) = 0;
	virtual void               SetCursorMode(CursorMode cursorMode) = 0;
	virtual WindowPlatformData GetPlatformData() = 0;
	virtual bool               IsExitRequested() = 0;

/*	get rect
	set rect
	get dpi scale factor
	adjust rect
	user request close as an event
	set cursor mode
	get cursor mode
	get window status: focused, under cursor, min, max, hidden, was maximized
	get os data
	set window title
	set window status
	set focus
	theming
	theme change as event
	*/
};

WindowApi* GetWindowApi();

//--------------------------------------------------------------------------------------------------

}	// namespace JC