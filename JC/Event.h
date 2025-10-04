#pragma once

#include "JC/Common.h"

//----------------------------------------------------------------------------------------------

enum struct Ev_Type {
	Key,
	MouseMove,
	MouseWheel,
	WindowResized,
	WindowMinimized,
	WindowRestored,
	WindowFocused,
	WindowUnfocused,
	ExitRequest,
};

enum struct Ev_Key {
	Mouse1,
	Mouse2,
	Mouse3,
	Mouse4,
	Mouse5,
};

struct Ev_KeyEvent           { Bool down; Ev_Key key; };
struct Ev_MouseMoveEvent     { I32 x, y, dx, dy; };
struct Ev_MouseWheelEvent    { F32 delta; };
struct Ev_WindowResizedEvent { U32 width, height; };

struct Ev_Event {
	Ev_Type type;
	union {
		Ev_KeyEvent           key;
		Ev_MouseMoveEvent     mouseMove;
		Ev_MouseWheelEvent    mouseWheel;
		Ev_WindowResizedEvent windowResized;
	};
};

void            Ev_Add(Ev_Event e);
Ev_Event const* Ev_Get();