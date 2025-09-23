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
	TempAllocator* tempAllocator = 0;
	Log::Logger*   logger        = 0;
	Str            title         = {};
	Style          style         = {};
	U32            width         = 0;
	U32            height        = 0;
	U32            displayIdx    = 0;
};

struct PlatformDesc {
	#if defined JC_PLATFORM_WINDOWS
		void* hinstance = 0;
		void* hwnd      = 0;
	#endif	// JC_PLATFORM
};

enum struct EventType {
	Exit,
	WindowResized,
	WindowFocused,
	WindowUnfocused,
	WindowMinimized,
	WindowRestored,
	Key,
	MouseMove,
	MouseWheel,
};

enum struct Key {
	Mouse1      = 0x01,
	Mouse2      = 0x02,
	Cancel      = 0x03,
	Mouse3      = 0x04,
	Mouse4      = 0x05,
	Mouse5      = 0x06,
	Backspace   = 0x08,
	Tab         = 0x09,
	Enter       = 0x0d,
	Pause       = 0x13,
	CapsLock    = 0x14,
	Escape      = 0x1b,
	Space       = 0x20,
	PageUp      = 0x21,
	PageDown    = 0x22,
	End         = 0x23,
	Home        = 0x24,
	Left        = 0x25,
	Up          = 0x26,
	Right       = 0x27,
	Down        = 0x28,
	PrintScreen = 0x2c,
	Insert      = 0x2d,
	Delete      = 0x2e,
	Winleft     = 0x5b,
	WinRight    = 0x5c,
	Menu        = 0x5d,
	NumPad0     = 0x60,
	NumPad1     = 0x61,
	NumPad2     = 0x62,
	NumPad3     = 0x63,
	NumPad4     = 0x64,
	NumPad5     = 0x65,
	NumPad6     = 0x66,
	NumPad7     = 0x67,
	NumPad8     = 0x68,
	NumPad9     = 0x69,
	NumpadStar  = 0x6a,
	NumpadPlus  = 0x6b,
	NumpadEnter = 0x6c,
	NumpadMinus = 0x6d,
	NumpadDot   = 0x6e,
	Slash       = 0x6f,
	F1          = 0x70,
	F2          = 0x71,
	F3          = 0x72,
	F4          = 0x73,
	F5          = 0x74,
	F6          = 0x75,
	F7          = 0x76,
	F8          = 0x77,
	F9          = 0x78,
	F10         = 0x79,
	F11         = 0x7a,
	F12         = 0x7b,
	F13         = 0x7c,
	F14         = 0x7d,
	F15         = 0x7e,
	F16         = 0x7f,
	F17         = 0x80,
	F18         = 0x81,
	F19         = 0x82,
	F20         = 0x83,
	F21         = 0x84,
	F22         = 0x85,
	F23         = 0x86,
	F24         = 0x87,
	NumLock     = 0x90,
	ScrollLock  = 0x91,
	ShiftLeft   = 0xa0,
	ShiftRight  = 0xa1,
	CtrlLeft    = 0xa2,
	CtrlRight   = 0xa3,
	AltLeft     = 0xa4,
	AltRight    = 0xa5,
	Max,
};

Str KeyStr(Key k);

struct ResizedEvent {
	U32 width;
	U32 height;
};

struct KeyEvent {
	Bool down;
	Key  key;
};

struct MouseMoveEvent {
	I32 x;
	I32 y;
	I32 dx;
	I32 dy;
};

struct MouseWheelEvent {
	float delta;
};

struct Event {
	EventType type;
	union {
		ResizedEvent    windowResized;
		KeyEvent        key;
		MouseMoveEvent  mouseMove;
		MouseWheelEvent mouseWheel;
	};
};

//--------------------------------------------------------------------------------------------------

Res<>               Init(const InitDesc* initDesc);
void                Shutdown();
Span<const Display> GetDisplays();
void                PumpMessages();
State               GetState();
void                SetRect(Rect rect);
void                SetStyle(Style style);
void                SetCursorMode(CursorMode cursorMode);
PlatformDesc        GetPlatformDesc();
Span<const Event>   GetEvents();
void                ClearEvents();
bool                IsKeyDown(Key key);
Str                 KeyStr(Key key);
Str                 EventStr(Event event);

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