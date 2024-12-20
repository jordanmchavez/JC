#pragma once

#include "JC/Common.h"

namespace JC { struct Log; }

namespace JC::Event {

//--------------------------------------------------------------------------------------------------

enum struct Type {
	Exit,
	WindowFocused,
	WindowUnfocused,
	WindowMinimized,
	WindowRestored,
	Key,
	MouseMove,
};

enum struct Key {
	MouseLeft   = 0x01,
	MouseRight  = 0x02,
	Cancel      = 0x03,
	MouseMiddle = 0x04,
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
	Zero        = 0x30,
	One         = 0x31,
	Two         = 0x32,
	Three       = 0x33,
	Four        = 0x34,
	Five        = 0x35,
	Six         = 0x36,
	Seven       = 0x37,
	Eight       = 0x38,
	Nine        = 0x39,
	A           = 0x41,
	B           = 0x42,
	C           = 0x43,
	D           = 0x44,
	E           = 0x45,
	F           = 0x46,
	G           = 0x47,
	H           = 0x48,
	I           = 0x49,
	J           = 0x4a,
	K           = 0x4b,
	L           = 0x4c,
	M           = 0x4d,
	N           = 0x4e,
	O           = 0x4f,
	P           = 0x50,
	Q           = 0x51,
	R           = 0x52,
	S           = 0x53,
	T           = 0x54,
	U           = 0x55,
	V           = 0x56,
	W           = 0x57,
	X           = 0x58,
	Y           = 0x59,
	Z           = 0x5a,
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
};

s8 KeyStr(Key k);

struct KeyEvent {
	bool down;
	Key  key;
};

struct MouseMoveEvent {
	i32 x;
	i32 y;
};

struct Event {
	Type type;
	union {
		KeyEvent       key;
		MouseMoveEvent mouseMove;
	};
};

void        Init(Log* log, Arena* temp);
void        Add(Event e);
Span<Event> Get();
void        Clear();

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Event