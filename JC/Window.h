#pragma once

#include "JC/Common.h"

namespace JC::Key { enum Key : U8; }

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

struct InitDesc {
	Mem   permMem;
	Mem   tempMem;
	Str   title;
	Style style;
	U32   width;
	U32   height;
	U32   displayIdx;
};

struct State {
	I32        x          = 0;
	I32        y          = 0;
	U32        width      = 0;
	U32        height     = 0;
	Style      style      = {};
	CursorMode cursorMode = {};
	bool       minimized  = false;
	bool       focused    = false;
};

struct PlatformDesc {
	#if defined Platform_Windows
		void* hinstance = 0;
		void* hwnd      = 0;
	#endif	// Platform
};

struct KeyEvent {
	Key::Key key;
	bool     down;
	I32      mouseX;
	I32      mouseY;
};

struct Events {
	Span<KeyEvent const> keyEvents;
	I32                  mouseX = 0;
	I32                  mouseY = 0;
	I32                  mouseDeltaX = 0;
	I32                  mouseDeltaY = 0;
	bool                 exitEvent = false;
};

Res<>               Init(InitDesc const* initDesc);
void                Shutdown();
Span<Display const> GetDisplays();
PlatformDesc        GetPlatformDesc();
State               GetState();
void                SetCursorMode(CursorMode cursorMode);
Events              Update();

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Window