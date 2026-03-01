#include "JC/Input.h"

#include "JC/Key.h"
#include "JC/StrDb.h"

namespace JC::Input {

//--------------------------------------------------------------------------------------------------

static constexpr U32 MaxBindingSets = 32;

struct Binding {
	BindingType bindingType = BindingType::Invalid;
	U64         actionId = 0;
	Str         actionStr;
};

static Str*     bindingSetNames;
static U16      bindingSetsLen;
static Binding* bindings;	// [key * MaxBindingSets + bindSetIdx] -> bind for this key+bindset
static bool     keyDown[(U16)Key::Key::Max];
static Key::Key downKeys[(U16)Key::Key::Max];
static U16      downKeysLen;
static U16      activeBindSets[MaxBindingSets];
static U16      activeBindingSetsLen;
static U64*     continuousActions;
static U16      continuousActionsLen;

//--------------------------------------------------------------------------------------------------

void Init(Mem permMem) {
	bindingSetNames   = Mem::AllocT<Str>(permMem, MaxBindingSets);
	bindingSetsLen    = 1; // reserve 0 for invalid
	bindings          = Mem::AllocT<Binding>(permMem, (U64)Key::Key::Max * MaxBindingSets);
	continuousActions = Mem::AllocT<U64>(permMem, (U64)Key::Key::Max);
}

//--------------------------------------------------------------------------------------------------

BindingSet CreateBindingSet(Str name) {
	Assert(bindingSetsLen < MaxBindingSets);
	bindingSetNames[bindingSetsLen] = StrDb::Intern(name);
	U64 const handle = (U64)bindingSetsLen;
	bindingSetsLen++;
	return { .handle = handle };
}

//--------------------------------------------------------------------------------------------------

void Bind(BindingSet bindingSet, Key::Key key, BindingType bindingType, U64 actionId, Str actionStr) {
	Assert(bindingSet.handle > 0 && bindingSet.handle < bindingSetsLen);
	Assert(actionId != 0);
	Assert(bindingType != BindingType::Invalid);
	bindings[((U64)key * MaxBindingSets) + bindingSet.handle] = {
		.bindingType = bindingType,
		.actionId    = actionId,
		.actionStr   = StrDb::Intern(actionStr),
	};
}

//--------------------------------------------------------------------------------------------------

void Unbind(BindingSet bindingSet, Key::Key key) {
	Assert(bindingSet.handle > 0 && bindingSet.handle < bindingSetsLen);
	Binding* const binding = &bindings[((U64)key * MaxBindingSets) + bindingSet.handle];
	memset(binding, 0, sizeof(*binding));
}

//--------------------------------------------------------------------------------------------------

void ActivateBindingSets(Span<BindingSet> bindingSets) {
	Assert(bindingSets.len <= MaxBindingSets);
	activeBindingSetsLen = 0;
	for (U64 i = 0; i < bindingSets.len; i++) {
		Assert(bindingSets[i].handle > 0 && bindingSets[i].handle < bindingSetsLen);
		activeBindSets[activeBindingSetsLen++] = (U16)bindingSets[i].handle;
	}
}

//--------------------------------------------------------------------------------------------------

U64 KeyEvent(Key::Key key, bool down) {
	Assert(key > Key::Key::Invalid && key < Key::Key::Max);
	if (down) {
		if (!keyDown[(U16)key]) {
			Assert(downKeysLen < (U16)Key::Key::Max);
			downKeys[downKeysLen++] = key;
		}
		keyDown[(U16)key] = true;
	} else {	// key up
		if (keyDown[(U16)key]) {
			for (U32 i = 0; i < downKeysLen; i++) {
				if (downKeys[i] == key) {
					downKeys[i] = downKeys[downKeysLen - 1];
					downKeysLen--;
					break;
				}
			}
		}
		keyDown[(U16)key] = false;
	}

	Binding* const keyBindings = bindings + (U64)key * MaxBindingSets;
	BindingType const targetBindingType = down ? BindingType::OnKeyDown : BindingType::OnKeyUp;
	for (I16 i = (I16)activeBindingSetsLen - 1; i >= 0; i--) {
		Binding const* const binding = &keyBindings[activeBindSets[i]];
		if (binding->actionId && (binding->bindingType == targetBindingType)) {
			return binding->actionId;
		}
	}

	return 0;
}

//--------------------------------------------------------------------------------------------------

Span<U64> GetContinuousActions() {
	continuousActionsLen = 0;
	for (U16 i = 0; i < downKeysLen; i++) {
		Binding* const keyBindings = bindings + (U64)downKeys[i] * MaxBindingSets;
		for (I16 j = (I16)activeBindingSetsLen - 1; j >= 0; j--) {
			Binding const* const binding = &keyBindings[activeBindSets[j]];
			if (binding->actionId && binding->bindingType == BindingType::Continuous) {
				continuousActions[continuousActionsLen++] = binding->actionId;
				break;
			}
		}
	}
	return Span<U64>(continuousActions, continuousActionsLen);
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Input