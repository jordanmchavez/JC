#pragma once

#include "JC/Common.h"

//--------------------------------------------------------------------------------------------------

struct Wnd_Display {
	I32 x        = 0;
	I32 y        = 0;
	U32 width    = 0;
	U32 height   = 0;
	U32 dpi      = 0;
	F32 dpiScale = 0.0f;
};

enum struct Wnd_Style {
	Bordered,
	BorderedResizable,
	Borderless,
	Fullscreen,
};

enum Wnd_CursorMode {
	VisibleFree,
	VisibleConstrained,
	HiddenLocked,	// ->visible: reappear at position hidden, same with alt+tabbing
};

struct Wnd_InitDesc {
	Mem*      tempMem;
	Str       title;
	Wnd_Style style;
	U32       width;
	U32       height;
	U32       displayIdx;
};

struct Wnd_PlatformDesc {
	#if defined Platform_Windows
		void* hinstance = 0;
		void* hwnd      = 0;
	#endif	// Platform
};

//--------------------------------------------------------------------------------------------------

Res<>                   Wnd_Init(const Wnd_InitDesc* initDesc);
void                    Wnd_Shutdown();
Span<const Wnd_Display> Wnd_GetDisplays();
void                    Wnd_Frame();
Wnd_PlatformDesc        Wnd_GetPlatformDesc();