#include "JC/Event.h"

namespace JC::Event {

//----------------------------------------------------------------------------------------------

static constexpr U32 MaxEvents = 1024;

static Event events[MaxEvents];
static U32   head;
static U32   tail;

//----------------------------------------------------------------------------------------------

void AddEvent(Event event) {
	U32 const nextHead = (head + 1) & (MaxEvents - 1);
	if (nextHead == tail) {
		Panic("Ran out of events");
	}
	events[head] = event;
	head = nextHead;
}

//----------------------------------------------------------------------------------------------

// TODO: consider just returning Span<Event const>
bool GetEvent(Event* eventOut) {
	if (head == tail) {
		return false;
	}
	*eventOut = events[tail];
	tail = (tail + 1) & (MaxEvents - 1);
	return true;
}

//----------------------------------------------------------------------------------------------

Str KeyStr(Key key) {
	switch (key) {
		case Key::MouseLeft: return "MouseLeft";
		case Key::MouseRight: return "MouseRight";
		case Key::Cancel: return "Cancel";
		case Key::MouseMiddle: return "MouseMiddle";
		case Key::Mouse4: return "Mouse4";
		case Key::Mouse5: return "Mouse5";
		case Key::Backspace: return "Backspace";
		case Key::Tab: return "Tab";
		case Key::Enter: return "Enter";
		case Key::Pause: return "Pause";
		case Key::CapsLock: return "CapsLock";
		case Key::Escape: return "Escape";
		case Key::Space: return "Space";
		case Key::PageUp: return "PageUp";
		case Key::PageDown: return "PageDown";
		case Key::End: return "End";
		case Key::Home: return "Home";
		case Key::Left: return "Left";
		case Key::Up: return "Up";
		case Key::Right: return "Right";
		case Key::Down: return "Down";
		case Key::PrintScreen: return "PrintScreen";
		case Key::Insert: return "Insert";
		case Key::Delete: return "Delete";
		case Key::Zero: return "Zero";
		case Key::One: return "One";
		case Key::Two: return "Two";
		case Key::Three: return "Three";
		case Key::Four: return "Four";
		case Key::Five: return "Five";
		case Key::Six: return "Six";
		case Key::Seven: return "Seven";
		case Key::Eight: return "Eight";
		case Key::Nine: return "Nine";
		case Key::A: return "A";
		case Key::B: return "B";
		case Key::C: return "C";
		case Key::D: return "D";
		case Key::E: return "E";
		case Key::F: return "F";
		case Key::G: return "G";
		case Key::H: return "H";
		case Key::I: return "I";
		case Key::J: return "J";
		case Key::K: return "K";
		case Key::L: return "L";
		case Key::M: return "M";
		case Key::N: return "N";
		case Key::O: return "O";
		case Key::P: return "P";
		case Key::Q: return "Q";
		case Key::R: return "R";
		case Key::S: return "S";
		case Key::T: return "T";
		case Key::U: return "U";
		case Key::V: return "V";
		case Key::W: return "W";
		case Key::X: return "X";
		case Key::Y: return "Y";
		case Key::Z: return "Z";
		case Key::Winleft: return "Winleft";
		case Key::WinRight: return "WinRight";
		case Key::Menu: return "Menu";
		case Key::NumPad0: return "NumPad0";
		case Key::NumPad1: return "NumPad1";
		case Key::NumPad2: return "NumPad2";
		case Key::NumPad3: return "NumPad3";
		case Key::NumPad4: return "NumPad4";
		case Key::NumPad5: return "NumPad5";
		case Key::NumPad6: return "NumPad6";
		case Key::NumPad7: return "NumPad7";
		case Key::NumPad8: return "NumPad8";
		case Key::NumPad9: return "NumPad9";
		case Key::NumpadStar: return "NumpadStar";
		case Key::NumpadPlus: return "NumpadPlus";
		case Key::NumpadEnter: return "NumpadEnter";
		case Key::NumpadMinus: return "NumpadMinus";
		case Key::NumpadDot: return "NumpadDot";
		case Key::Slash: return "Slash";
		case Key::F1: return "F1";
		case Key::F2: return "F2";
		case Key::F3: return "F3";
		case Key::F4: return "F4";
		case Key::F5: return "F5";
		case Key::F6: return "F6";
		case Key::F7: return "F7";
		case Key::F8: return "F8";
		case Key::F9: return "F9";
		case Key::F10: return "F10";
		case Key::F11: return "F11";
		case Key::F12: return "F12";
		case Key::F13: return "F13";
		case Key::F14: return "F14";
		case Key::F15: return "F15";
		case Key::F16: return "F16";
		case Key::F17: return "F17";
		case Key::F18: return "F18";
		case Key::F19: return "F19";
		case Key::F20: return "F20";
		case Key::F21: return "F21";
		case Key::F22: return "F22";
		case Key::F23: return "F23";
		case Key::F24: return "F24";
		case Key::NumLock: return "NumLock";
		case Key::ScrollLock: return "ScrollLock";
		case Key::ShiftLeft: return "ShiftLeft";
		case Key::ShiftRight: return "ShiftRight";
		case Key::CtrlLeft: return "CtrlLeft";
		case Key::CtrlRight: return "CtrlRight";
		case Key::AltLeft: return "AltLeft";
		case Key::AltRight: return "AltRight";
		default: Panic("Invalid key %u", (U32)key);
	}
}
//----------------------------------------------------------------------------------------------

}	// namespace JC::Event