#pragma once

#include "JC/Common.h"

namespace JC {

struct Mem;

//--------------------------------------------------------------------------------------------------

struct DisplayInfo {
	bool primary   = false;
	i32  x         = 0;
	i32  y         = 0;
	u32  width     = 0;
	u32  height    = 0;
	f32  dpiScaleX = 0.0f;
	f32  dpiScaleY = 0.0f;
};

struct DisplayApi {
	static constexpr ErrCode Err_EnumDisplays = { .ns = "disp", .code = 1 };

	virtual Res<>             Init(Mem* mem)    = 0;
	virtual Span<DisplayInfo> GetDisplayInfos() = 0;
	virtual Res<>             Refresh()         = 0;
};

DisplayApi* GetDisplayApi();

//--------------------------------------------------------------------------------------------------

struct WindowApiInit {
};

struct Window {
};

enum WindowCursorMode {
	VisibleFree,
	VisibleConstrained,
	HiddenLocked,	// ->visible: reappear at position hidden, same with alt+tabbing
};
struct WindowApi {
	virtual Res<> Init(WindowApiInit* init) = 0;
	virtual Window CreateWindow() = 0;
	virtual void DestroyWindow(Window window) = 0;
	virtual void PumpMessages(Window window) = 0;
	virtual void WaitMessage() = 0;
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