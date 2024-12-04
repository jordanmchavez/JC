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

struct Window { u64 handle = 0; };

enum WindowCursorMode {
	VisibleFree,
	VisibleConstrained,
	HiddenLocked,	// ->visible: reappear at position hidden, same with alt+tabbing
};

enum struct WindowMode {
	Bordered,
	BorderedResizable,
	Borderless,
	Fullscreen,
};

struct WindowApi {
	virtual Res<>       Init(DisplayApi* displayApi, EventApi* eventApi, Log* log, TempMem* tempMem) = 0;
	virtual Res<Window> CreateWindow(s8 title, WindowMode mode, Rect rect, u32 fullscreenDisplay) = 0;
	//virtual void        DestroyWindow(Window window) = 0;
	virtual void        PumpMessages() = 0;
	//virtual void WaitMessage() = 0;
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