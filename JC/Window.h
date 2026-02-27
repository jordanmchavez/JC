#pragma once

#include "JC/Common.h"

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

struct InitDef {
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

struct PlatformDef {
	#if defined Platform_Windows
		void* hinstance = 0;
		void* hwnd      = 0;
	#endif	// Platform
};

Res<>               Init(const InitDef* initDef);
void                Shutdown();
Span<const Display> GetDisplays();
void                Frame();
PlatformDef         GetPlatformDef();
State               GetState();

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Window