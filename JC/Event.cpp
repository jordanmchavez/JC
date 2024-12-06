#include "JC/Event.h"

#include "JC/Fmt.h"
#include "JC/Log.h"
#include "JC/Mem.h"

namespace JC {

//--------------------------------------------------------------------------------------------------


s8 EventKeyStr(EventKey k) {
	switch (k) {
		case EventKey::MouseLeft:   return "MouseLeft";
		case EventKey::MouseRight:  return "MouseRight";
		case EventKey::Cancel:      return "Cancel";
		case EventKey::MouseMiddle: return "MouseMiddle";
		case EventKey::Mouse4:      return "Mouse4";
		case EventKey::Mouse5:      return "Mouse5";
		case EventKey::Backspace:   return "Backspace";
		case EventKey::Tab:         return "Tab";
		case EventKey::Enter:       return "Enter";
		case EventKey::Pause:       return "Pause";
		case EventKey::CapsLock:    return "CapsLock";
		case EventKey::Escape:      return "Escape";
		case EventKey::Space:       return "Space";
		case EventKey::PageUp:      return "PageUp";
		case EventKey::PageDown:    return "PageDown";
		case EventKey::End:         return "End";
		case EventKey::Home:        return "Home";
		case EventKey::Left:        return "Left";
		case EventKey::Up:          return "Up";
		case EventKey::Right:       return "Right";
		case EventKey::Down:        return "Down";
		case EventKey::PrintScreen: return "PrintScreen";
		case EventKey::Insert:      return "Insert";
		case EventKey::Delete:      return "Delete";
		case EventKey::Zero:        return "0";
		case EventKey::One:         return "1";
		case EventKey::Two:         return "2";
		case EventKey::Three:       return "3";
		case EventKey::Four:        return "4";
		case EventKey::Five:        return "5";
		case EventKey::Six:         return "6";
		case EventKey::Seven:       return "7";
		case EventKey::Eight:       return "8";
		case EventKey::Nine:        return "9";
		case EventKey::A:           return "A";
		case EventKey::B:           return "B";
		case EventKey::C:           return "C";
		case EventKey::D:           return "D";
		case EventKey::E:           return "E";
		case EventKey::F:           return "F";
		case EventKey::G:           return "G";
		case EventKey::H:           return "H";
		case EventKey::I:           return "I";
		case EventKey::J:           return "J";
		case EventKey::K:           return "K";
		case EventKey::L:           return "L";
		case EventKey::M:           return "M";
		case EventKey::N:           return "N";
		case EventKey::O:           return "O";
		case EventKey::P:           return "P";
		case EventKey::Q:           return "Q";
		case EventKey::R:           return "R";
		case EventKey::S:           return "S";
		case EventKey::T:           return "T";
		case EventKey::U:           return "U";
		case EventKey::V:           return "V";
		case EventKey::W:           return "W";
		case EventKey::X:           return "X";
		case EventKey::Y:           return "Y";
		case EventKey::Z:           return "Z";
		case EventKey::Winleft:     return "Winleft";
		case EventKey::WinRight:    return "WinRight";
		case EventKey::Menu:        return "Menu";
		case EventKey::NumPad0:     return "NumPad0";
		case EventKey::NumPad1:     return "NumPad1";
		case EventKey::NumPad2:     return "NumPad2";
		case EventKey::NumPad3:     return "NumPad3";
		case EventKey::NumPad4:     return "NumPad4";
		case EventKey::NumPad5:     return "NumPad5";
		case EventKey::NumPad6:     return "NumPad6";
		case EventKey::NumPad7:     return "NumPad7";
		case EventKey::NumPad8:     return "NumPad8";
		case EventKey::NumPad9:     return "NumPad9";
		case EventKey::NumpadStar:  return "NumpadStar";
		case EventKey::NumpadPlus:  return "NumpadPlus";
		case EventKey::NumpadEnter: return "NumpadEnter";
		case EventKey::NumpadMinus: return "NumpadMinus";
		case EventKey::NumpadDot:   return "NumpadDot";
		case EventKey::Slash:       return "Slash";
		case EventKey::F1:          return "F1";
		case EventKey::F2:          return "F2";
		case EventKey::F3:          return "F3";
		case EventKey::F4:          return "F4";
		case EventKey::F5:          return "F5";
		case EventKey::F6:          return "F6";
		case EventKey::F7:          return "F7";
		case EventKey::F8:          return "F8";
		case EventKey::F9:          return "F9";
		case EventKey::F10:         return "F10";
		case EventKey::F11:         return "F11";
		case EventKey::F12:         return "F12";
		case EventKey::F13:         return "F13";
		case EventKey::F14:         return "F14";
		case EventKey::F15:         return "F15";
		case EventKey::F16:         return "F16";
		case EventKey::F17:         return "F17";
		case EventKey::F18:         return "F18";
		case EventKey::F19:         return "F19";
		case EventKey::F20:         return "F20";
		case EventKey::F21:         return "F21";
		case EventKey::F22:         return "F22";
		case EventKey::F23:         return "F23";
		case EventKey::F24:         return "F24";
		case EventKey::NumLock:     return "NumLock";
		case EventKey::ScrollLock:  return "ScrollLock";
		case EventKey::ShiftLeft:   return "ShiftLeft";
		case EventKey::ShiftRight:  return "ShiftRight";
		case EventKey::CtrlLeft:    return "CtrlLeft";
		case EventKey::CtrlRight:   return "CtrlRight";
		case EventKey::AltLeft:     return "AltLeft";
		case EventKey::AltRight:    return "AltRight";
		default:                    return "<UnrecognizedEventKey>";
	}
}


struct EventApiObj:  EventApi {
	static constexpr u32 MaxEvents = 64 * 1024;

	Log*     log               = 0;
	TempMem* tempMem           = 0;
	Event    events[MaxEvents] = {};
	u32      eventsLen         = 0;

	void Init(Log* log_, TempMem* tempMem_) override{
		log       = log_;
		tempMem   = tempMem_;
		eventsLen = 0;
	}

	s8 EventStr(Event e) {
		switch (e.type) {
			case EventType::Exit:  return Fmt(tempMem, "ExitEvent");
			case EventType::Focus: return Fmt(tempMem, "FocusEvent(focused={})", e.focus.focused);
			default:               Panic("Unhandled EventType {}", e.type);
		}
	}

	void AddEvent(Event e) override{
		if (eventsLen >= MaxEvents) {
			Errorf("Dropped event: {}", EventStr(e));
			return;
		}
		events[eventsLen] = e;
		eventsLen++;
	}

	Span<Event> GetEvents() override {
		return Span<Event>(events, eventsLen);
	}

	void ClearEvents() override {
		eventsLen = 0;
	}
};

static EventApiObj eventApi;

EventApi* GetEventApi() {
	return &eventApi;
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC