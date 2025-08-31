#pragma once

#include "JC/Core.h"

namespace JC::Log { struct Logger; };

namespace JC::Window {

//--------------------------------------------------------------------------------------------------

struct Display {
	I32 x        = 0;
	I32 y        = 0;
	U32 width    = 0;
	U32 height   = 0;
	U32 dpi      = 0;
	F32 dpiScale = 0.0f;
};

enum struct Style {
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

struct State {
	I32        x          = 0;
	I32        y          = 0;
	U32        width      = 0;
	U32        height     = 0;
	Rect       rect       = {};
	Style      style      = {};
	CursorMode cursorMode = {};
	Bool       minimized  = false;
	Bool       focused    = false;
};

struct InitDesc {
	Mem::TempAllocator* tempAllocator = 0;
	Log::Logger*        logger        = 0;
	Str                 title         = {};
	Style               style         = {};
	U32                 width         = 0;
	U32                 height        = 0;
	U32                 displayIdx    = 0;
};

struct PlatformDesc {
	#if defined JC_PLATFORM_WINDOWS
		void* hinstance = 0;
		void* hwnd      = 0;
	#endif	// JC_PLATFORM
};

Res<>         Init(const InitDesc* initDesc);
void          Shutdown();
Span<Display> GetDisplays();
void          PumpMessages();
State         GetState();
void          SetRect(Rect rect);
void          SetStyle(Style style);
void          SetCursorMode(CursorMode cursorMode);
PlatformDesc  GetPlatformDesc();
Bool          IsExitRequested();

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

}	// namespace JC::Window