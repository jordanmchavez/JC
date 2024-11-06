#include "J/Input.h"

#include "J/Error.h"
#include "J/Log.h"

namespace J {

//-------------------------------------------------------------------------------------------------

namespace Input {
	J_DefineLogScope(Input);

	u32 const MaxSources = 32;
	Source sources[MaxSources];
	u32 sourceCount = 0;

	u32 const MaxEvents = 128;
	Event events[MaxEvents];
	u32 eventCount = 0;
}

//-------------------------------------------------------------------------------------------------

void Input::Frame() {
	eventCount = 0;

	for (u32 i = 0; i < sourceCount; i++) {
		u32 sourceEventCount = sources[i].EventCount();
		Event const* sourceEvents = sources[i].Events();

		for (u32 j = 0; j < sourceEventCount; j++) {
			events[eventCount] = sourceEvents[j];
			eventCount++;
			if (eventCount > MaxEvents) {
				InputErrorf("Exceeded event buffer size: overwriting oldest events");
				eventCount = 0;
			}
		}
	}
}

//-------------------------------------------------------------------------------------------------

u32 Input::EventCount() {
	return eventCount;
}

//-------------------------------------------------------------------------------------------------

Input::Event const* Input::Events() {
	return events;
}

//-------------------------------------------------------------------------------------------------

void Input::AddSource(Input::Source source) {
	sources[sourceCount++] = source;
	J_CheckFatal(sourceCount < MaxSources);
}

//-------------------------------------------------------------------------------------------------

char const* Input::EventTypeString(EventType eventType) {
	switch (eventType) {
		case EventType::None:            return "None";
		case EventType::KeyDown:         return "KeyDown";
		case EventType::KeyUp:           return "KeyUp";
		case EventType::MouseButtonDown: return "MouseButtonDown";
		case EventType::MouseButtonUp:   return "MouseButtonUp";
		case EventType::MouseWheel:      return "MouseWheel";
		case EventType::MouseMove:       return "MouseMove";
		case EventType::WindowClose:     return "WindowClose";
		default:                         return "<Unrecognized EventType>";
	}
}

//-------------------------------------------------------------------------------------------------

char const* Input::KeyString(Key key) {
	switch (key) {
		case Key::None:         return "None";
		case Key::Enter:        return "Enter";
		case Key::Escape:       return "Escape";
		case Key::Space:        return "Space";
		case Key::Left:         return "Left";
		case Key::Up:           return "Up";
		case Key::Right:        return "Right";
		case Key::Down:         return "Down";
		case Key::Zero:         return "Zero";
		case Key::One:          return "One";
		case Key::Two:          return "Two";
		case Key::Three:        return "Three";
		case Key::Four:         return "Four";
		case Key::Five:         return "Five";
		case Key::Six:          return "Six";
		case Key::Seven:        return "Seven";
		case Key::Eight:        return "Eight";
		case Key::Nine:         return "Nine";
		case Key::A:            return "A";
		case Key::B:            return "B";
		case Key::C:            return "C";
		case Key::D:            return "D";
		case Key::E:            return "E";
		case Key::F:            return "F";
		case Key::G:            return "G";
		case Key::H:            return "H";
		case Key::I:            return "I";
		case Key::J:            return "J";
		case Key::K:            return "K";
		case Key::L:            return "L";
		case Key::M:            return "M";
		case Key::N:            return "N";
		case Key::O:            return "O";
		case Key::P:            return "P";
		case Key::Q:            return "Q";
		case Key::R:            return "R";
		case Key::S:            return "S";
		case Key::T:            return "T";
		case Key::U:            return "U";
		case Key::V:            return "V";
		case Key::W:            return "W";
		case Key::X:            return "X";
		case Key::Y:            return "Y";
		case Key::Z:            return "Z";
		case Key::F1:           return "F1";
		case Key::F2:           return "F2";
		case Key::F3:           return "F3";
		case Key::F4:           return "F4";
		case Key::F5:           return "F5";
		case Key::F6:           return "F6";
		case Key::F7:           return "F7";
		case Key::F8:           return "F8";
		case Key::F9:           return "F9";
		case Key::F10:          return "F10";
		case Key::F11:          return "F11";
		case Key::F12:          return "F12";
		case Key::LeftShift:    return "LeftShift";
		case Key::RightShift:   return "RightShift";
		case Key::LeftControl:  return "LeftControl";
		case Key::RightControl: return "RightControl";
		case Key::LeftAlt:      return "LeftAlt";
		case Key::RightAlt:     return "RightAlt";
		case Key::Numpad0:      return "Numpad0";
		case Key::Numpad1:      return "Numpad1";
		case Key::Numpad2:      return "Numpad2";
		case Key::Numpad3:      return "Numpad3";
		case Key::Numpad4:      return "Numpad4";
		case Key::Numpad5:      return "Numpad5";
		case Key::Numpad6:      return "Numpad6";
		case Key::Numpad7:      return "Numpad7";
		case Key::Numpad8:      return "Numpad8";
		case Key::Numpad9:      return "Numpad9";
		case Key::NumpadEnter:  return "NumpadEnter";
		case Key::NumpadDot:    return "NumpadDot";
		default:                return "<Unrecognized Key>";
	}
}

//-------------------------------------------------------------------------------------------------

char const* Input::MouseButtonString(MouseButton mouseButton) {
	switch (mouseButton) {
		case MouseButton::None:   return "None";
		case MouseButton::Left:   return "Left";
		case MouseButton::Right:  return "Right";
		case MouseButton::Middle: return "Middle";
		case MouseButton::Side1:  return "Side1";
		case MouseButton::Side2:  return "Side2";
		default:                  return "<Unrecognized MouseButton>";
	}
}

//-------------------------------------------------------------------------------------------------

}	// namespace J