#include "JC/Event.h"
#include "JC/Log.h"

static constexpr u32 Event_MaxEvents = 8192;

static Event event_events[Event_MaxEvents] = {};
static u32   event_read = 0;
static u32   event_write = 0;

LOG_DEFINE_FNS(Event)

void Event_Frame() {
	if (event_read < event_write) {
		Event_LogErr("Dropped unread events on frame: n={}", event_write - event_read);
	}
	event_read = 0;
	event_write = 0;
}

void Event_Add(Event e) {
	if (event_write >= Event_MaxEvents) {
		Event_LogErr("Dropped event due to full buffer: type={}", e.type);
		return;
	}
	memcpy(&event_events[event_write++], &e, sizeof(e));
}

const Event* Event_Next() {
	if (event_read == event_write) {
		return nullptr;
	}
	return &event_events[event_read++];
}

s8 Event_KeyStr(Event_Key k) {
	switch (k) {
		case Event_Key_MouseLeft:   return "MouseLeft";
		case Event_Key_MouseRight:  return "MouseRight";
		case Event_Key_Cancel:      return "Cancel";
		case Event_Key_MouseMiddle: return "MouseMiddle";
		case Event_Key_Mouse4:      return "Mouse4";
		case Event_Key_Mouse5:      return "Mouse5";
		case Event_Key_Backspace:   return "Backspace";
		case Event_Key_Tab:         return "Tab";
		case Event_Key_Enter:       return "Enter";
		case Event_Key_Pause:       return "Pause";
		case Event_Key_CapsLock:    return "CapsLock";
		case Event_Key_Escape:      return "Escape";
		case Event_Key_Space:       return "Space";
		case Event_Key_PageUp:      return "PageUp";
		case Event_Key_PageDown:    return "PageDown";
		case Event_Key_End:         return "End";
		case Event_Key_Home:        return "Home";
		case Event_Key_Left:        return "Left";
		case Event_Key_Up:          return "Up";
		case Event_Key_Right:       return "Right";
		case Event_Key_Down:        return "Down";
		case Event_Key_PrintScreen: return "PrintScreen";
		case Event_Key_Insert:      return "Insert";
		case Event_Key_Delete:      return "Delete";
		case Event_Key_0:           return "0";
		case Event_Key_1:           return "1";
		case Event_Key_2:           return "2";
		case Event_Key_3:           return "3";
		case Event_Key_4:           return "4";
		case Event_Key_5:           return "5";
		case Event_Key_6:           return "6";
		case Event_Key_7:           return "7";
		case Event_Key_8:           return "8";
		case Event_Key_9:           return "9";
		case Event_Key_A:           return "A";
		case Event_Key_B:           return "B";
		case Event_Key_C:           return "C";
		case Event_Key_D:           return "D";
		case Event_Key_E:           return "E";
		case Event_Key_F:           return "F";
		case Event_Key_G:           return "G";
		case Event_Key_H:           return "H";
		case Event_Key_I:           return "I";
		case Event_Key_J:           return "J";
		case Event_Key_K:           return "K";
		case Event_Key_L:           return "L";
		case Event_Key_M:           return "M";
		case Event_Key_N:           return "N";
		case Event_Key_O:           return "O";
		case Event_Key_P:           return "P";
		case Event_Key_Q:           return "Q";
		case Event_Key_R:           return "R";
		case Event_Key_S:           return "S";
		case Event_Key_T:           return "T";
		case Event_Key_U:           return "U";
		case Event_Key_V:           return "V";
		case Event_Key_W:           return "W";
		case Event_Key_X:           return "X";
		case Event_Key_Y:           return "Y";
		case Event_Key_Z:           return "Z";
		case Event_Key_Winleft:     return "Winleft";
		case Event_Key_WinRight:    return "WinRight";
		case Event_Key_Menu:        return "Menu";
		case Event_Key_NumPad0:     return "NumPad0";
		case Event_Key_NumPad1:     return "NumPad1";
		case Event_Key_NumPad2:     return "NumPad2";
		case Event_Key_NumPad3:     return "NumPad3";
		case Event_Key_NumPad4:     return "NumPad4";
		case Event_Key_NumPad5:     return "NumPad5";
		case Event_Key_NumPad6:     return "NumPad6";
		case Event_Key_NumPad7:     return "NumPad7";
		case Event_Key_NumPad8:     return "NumPad8";
		case Event_Key_NumPad9:     return "NumPad9";
		case Event_Key_NumpadStar:  return "NumpadStar";
		case Event_Key_NumpadPlus:  return "NumpadPlus";
		case Event_Key_NumpadEnter: return "NumpadEnter";
		case Event_Key_NumpadMinus: return "NumpadMinus";
		case Event_Key_NumpadDot:   return "NumpadDot";
		case Event_Key_Slash:       return "Slash";
		case Event_Key_F1:          return "F1";
		case Event_Key_F2:          return "F2";
		case Event_Key_F3:          return "F3";
		case Event_Key_F4:          return "F4";
		case Event_Key_F5:          return "F5";
		case Event_Key_F6:          return "F6";
		case Event_Key_F7:          return "F7";
		case Event_Key_F8:          return "F8";
		case Event_Key_F9:          return "F9";
		case Event_Key_F10:         return "F10";
		case Event_Key_F11:         return "F11";
		case Event_Key_F12:         return "F12";
		case Event_Key_F13:         return "F13";
		case Event_Key_F14:         return "F14";
		case Event_Key_F15:         return "F15";
		case Event_Key_F16:         return "F16";
		case Event_Key_F17:         return "F17";
		case Event_Key_F18:         return "F18";
		case Event_Key_F19:         return "F19";
		case Event_Key_F20:         return "F20";
		case Event_Key_F21:         return "F21";
		case Event_Key_F22:         return "F22";
		case Event_Key_F23:         return "F23";
		case Event_Key_F24:         return "F24";
		case Event_Key_NumLock:     return "NumLock";
		case Event_Key_ScrollLock:  return "ScrollLock";
		case Event_Key_ShiftLeft:   return "ShiftLeft";
		case Event_Key_ShiftRight:  return "ShiftRight";
		case Event_Key_CtrlLeft:    return "CtrlLeft";
		case Event_Key_CtrlRight:   return "CtrlRight";
		case Event_Key_AltLeft:     return "AltLeft";
		case Event_Key_AltRight:    return "AltRight";
		default:                    return "<Unknown Key>";
	}
}