#pragma once

#include "J/Common.h"

namespace J {

//-------------------------------------------------------------------------------------------------

namespace Input {
	enum struct EventType {
		None = 0,
		KeyDown,
		KeyUp,
		MouseButtonDown,
		MouseButtonUp,
		MouseWheel,
		MouseMove,
		WindowClose,
	};

	enum struct Key : u32 {
		None = 0x00,
		Enter = 0x0d,
		Escape = 0x1b,
		Space = 0x20,
		Left = 0x25,
		Up = 0x26,
		Right = 0x27,
		Down = 0x28,
		Zero = 0x30,
		One = 0x31,
		Two = 0x32,
		Three = 0x33,
		Four = 0x34,
		Five = 0x35,
		Six = 0x36,
		Seven = 0x37,
		Eight = 0x38,
		Nine = 0x39,
		A =  0x41,
		B =  0x42,
		C =  0x43,
		D =  0x44,
		E =  0x45,
		F =  0x46,
		G =  0x47,
		H =  0x48,
		I =  0x49,
		J =  0x4a,
		K =  0x4b,
		L =  0x4c,
		M =  0x4d,
		N =  0x4e,
		O =  0x4f,
		P =  0x50,
		Q =  0x51,
		R =  0x52,
		S =  0x53,
		T =  0x54,
		U =  0x55,
		V =  0x56,
		W =  0x57,
		X =  0x58,
		Y =  0x59,
		Z =  0x55a,
		F1 = 0x70,
		F2 = 0x71,
		F3 = 0x72,
		F4 = 0x73,
		F5 = 0x74,
		F6 = 0x75,
		F7 = 0x76,
		F8 = 0x77,
		F9 = 0x78,
		F10 = 0x79,
		F11 = 0x7a,
		F12 = 0x7b,
		LeftShift = 0xa0,
		RightShift = 0xa1,
		LeftControl = 0xa2,
		RightControl = 0xa3,
		LeftAlt = 0xa4,
		RightAlt = 0xa5,
		Numpad0 = 0x60,
		Numpad1 = 0x61,
		Numpad2 = 0x62,
		Numpad3 = 0x63,
		Numpad4 = 0x64,
		Numpad5 = 0x65,
		Numpad6 = 0x66,
		Numpad7 = 0x67,
		Numpad8 = 0x68,
		Numpad9 = 0x69,
		NumpadEnter = 0x6c,
		NumpadDot = 0x6e,

		Max,
	};

	enum struct MouseButton {
		None = 0,
		Left,
		Right,
		Middle,
		Side1,
		Side2,
		Max,
	};

	struct Event {
		EventType type;
		union {
			struct {
				float x;
				float y;
				float dx;
				float dy;
			} mouseMove;
			float mouseWheel;
			MouseButton mouseButton;
			Key key;
		};
	};

	struct Source {
		u32          (*EventCount)();
		Event const* (*Events)();
	};

	void         Frame();

	u32          EventCount();
	Event const* Events();

	void         AddSource(Source source);
	
	char const*  EventTypeString(EventType eventType);
	char const*  KeyString(Key key);
	char const*  MouseButtonString(MouseButton mouseButton);
}

//-------------------------------------------------------------------------------------------------

}	// namespace J