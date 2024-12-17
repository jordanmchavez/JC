#pragma once

#include "JC/Common.h"

namespace JC {
	struct Log;
	namespace Event { struct Api; }

	struct Rect {
		i32 x      = 0;
		i32 y      = 0;
		i32 width  = 0;
		i32 height = 0;
	};
}

namespace JC::Window {

//--------------------------------------------------------------------------------------------------

struct Display {
	bool primary  = false;
	Rect rect     = {};
	u32  dpi      = 0;
	f32  dpiScale = 0.0f;
};

Res<Span<Display>> EnumDisplays();

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

namespace StateFlags {
	constexpr u64 Minimized = 1 << 0;
	constexpr u64 Maximized = 1 << 1;
	constexpr u64 Focused   = 1 << 2;
	constexpr u64 Hidden    = 1 << 3;
};

struct State {
	Rect       rect       = {};
	u64        flags      = 0;
	WindowMode windowMode = {};
	CursorMode cursorMode = {};
};

struct ApiInit {
	Event::Api*   eventApi          = 0;
	Log*          log               = 0;
	Arena*        temp              = 0;
	s8            title             = {};
	WindowMode    windowMode        = {};
	Rect          rect              = {};
	u32           fullscreenDisplay = 0;
};

struct PlatformData {
	#if defined Platform_Windows
		void* hinstance = 0;
		void* hwnd      = 0;
	#else	// Platform_
		#error("unsupported platform");
	#endif	// Platform_
};

struct Api {
	virtual Res<>        Init(const ApiInit* init) = 0;
	virtual void         Shutdown() = 0;
	virtual void         PumpMessages() = 0;
	virtual State        GetState() = 0;
	virtual void         SetRect(Rect rect) = 0;
	virtual void         SetWindowMode(WindowMode windowMode) = 0;
	virtual void         SetCursorMode(CursorMode cursorMode) = 0;
	virtual PlatformData GetPlatformData() = 0;
	virtual bool         IsExitRequested() = 0;

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

Api* GetApi();

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Window