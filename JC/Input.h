#pragma once

#include "JC/Common.h"

namespace JC::Key { enum Key : U8; }
namespace JC::Window { struct KeyEvent; }

namespace JC::Input {

//--------------------------------------------------------------------------------------------------

constexpr U16 MaxActionsPerFrame = 1024;

enum struct BindingType {
	Invalid = 0,
	OnKeyDown,
	OnKeyUp,
	Continuous,
};

DefHandle(BindingSet);

struct Action {
	U64 id;
	I32 mouseX;
	I32 mouseY;
};

void               Init(Mem permMem);
BindingSet         CreateBindingSet(Str name);
void               Bind(BindingSet bindingSet, Key::Key key, BindingType bindType, U64 actionId);	// 0 actionId reserved for invalid
void               Unbind(BindingSet bindingSet, Key::Key key);
void               SetBindingSetStack(Span<BindingSet const> bindingSets);
Span<Action const> ProcessKeyEvents(Span<Window::KeyEvent const> keyEvents);

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Input