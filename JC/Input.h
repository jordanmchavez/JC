#pragma once

#include "JC/Common.h"

namespace JC::Key { enum struct Key : U16; }

namespace JC::Input {

//--------------------------------------------------------------------------------------------------

enum struct BindingType {
	Invalid = 0,
	OnKeyDown,
	OnKeyUp,
	Continuous,
};

DefHandle(BindingSet);

void            Init(Mem permMem);
BindingSet      CreateBindingSet(Str name);
void            Bind(BindingSet bindingSet, Key::Key key, BindingType bindType, U64 actionId, Str actionStr);	// 0 actionId reserved for invalid
void            Unbind(BindingSet bindingSet, Key::Key key);
void            SetBindingSetStack(Span<BindingSet const> bindingSets);
Span<U64 const> ProcessKeyEvents(Span<Key::Key const> keyDownEvents, Span<Key::Key const> keyUpEvents);

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Input