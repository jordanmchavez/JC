#pragma once

#include "JC/Common_Std.h"

namespace JC::Event {

//----------------------------------------------------------------------------------------------

enum struct Type {
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

enum struct Key {
	Mouse1,
	Mouse2,
	Mouse3,
	Mouse4,
	Mouse5,
};

struct KeyEvent           { bool down; Key key; };
struct MouseMoveEvent     { I32 x, y, dx, dy; };
struct MouseWheelEvent    { F32 delta; };
struct WindowResizedEvent { U32 width, height; };

struct Event {
	Type type;
	union {
		KeyEvent           key;
		MouseMoveEvent     mouseMove;
		MouseWheelEvent    mouseWheel;
		WindowResizedEvent windowResized;
	};
};

void         Add(Event e);
Event const* Get();

//----------------------------------------------------------------------------------------------

}	// namespace JC::Event