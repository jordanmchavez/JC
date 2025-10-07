#pragma once

#include "JC/Common_Mem.h"
#include "JC/Common_Res.h"
#include "JC/Common_Std.h"

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
	Mem::Mem* tempMem;
	Str       title;
	Style     style;
	U32       width;
	U32       height;
	U32       displayIdx;
};

struct PlatformDesc {
	#if defined Platform_Windows
		void* hinstance = 0;
		void* hwnd      = 0;
	#endif	// Platform
};

Res<>               Init(const InitDesc* initDesc);
void                Shutdown();
Span<const Display> GetDisplays();
void                Frame();
PlatformDesc        GetPlatformDesc();

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Window