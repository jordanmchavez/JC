#include "JC/Key.h"

namespace JC::Key {

//--------------------------------------------------------------------------------------------------

Str KeyStr(Key key) {
	switch (key) {
		case Key::Mouse1: return "Mouse1";
		case Key::Mouse2: return "Mouse2";
		case Key::Mouse3: return "Mouse3";
		case Key::Mouse4: return "Mouse4";
		case Key::Mouse5: return "Mouse5";
		case Key::MouseWheelUp: return "MouseWheelUp";
		case Key::MouseWheelDown: return "MouseWheelDown";
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
		case Key::Zero: return "0";
		case Key::One: return "1";
		case Key::Two: return "2";
		case Key::Three: return "3";
		case Key::Four: return "4";
		case Key::Five: return "5";
		case Key::Six: return "6";
		case Key::Seven: return "7";
		case Key::Eight: return "8";
		case Key::Nine: return "8";
		case Key::Escape: return "9";
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
		case Key::PrintScreen: return "PrintScreen";
		case Key::ScrollLock: return "ScrollLock";
		case Key::Pause: return "Pause";
		case Key::BackQuote: return "BackQuote";
		case Key::Minus: return "Minus";
		case Key::Equals: return "Equals";
		case Key::Backspace: return "Backspace";
		case Key::LeftBracket: return "LeftBracket";
		case Key::RightBracket: return "RightBracket";
		case Key::Backslash: return "Backslash";
		case Key::Semicolon: return "Semicolon";
		case Key::Quote: return "Quote";
		case Key::Comma: return "Comma";
		case Key::Dot: return "Dot";
		case Key::Slash: return "Slash";
		case Key::Tab: return "Tab";
		case Key::CapsLock: return "CapsLock";
		case Key::Space: return "Space";
		case Key::Enter: return "Enter";
		case Key::Insert: return "Insert";
		case Key::Delete: return "Delete";
		case Key::Home: return "Home";
		case Key::End: return "End";
		case Key::PageUp: return "PageUp";
		case Key::PageDown: return "PageDown";
		case Key::Left: return "Left";
		case Key::Right: return "Right";
		case Key::Up: return "Up";
		case Key::Down: return "Down";
		case Key::ShiftLeft: return "ShiftLeft";
		case Key::ShiftRight: return "ShiftRight";
		case Key::CtrlLeft: return "CtrlLeft";
		case Key::CtrlRight: return "CtrlRight";
		case Key::AltLeft: return "AltLeft";
		case Key::AltRight: return "AltRight";
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
		default: return "<Invalid Key>";
	}
}

//----------------------------------------------------------------------------------------------

}	// namespace JC::Key