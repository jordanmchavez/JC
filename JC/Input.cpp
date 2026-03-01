#include "JC/Input.h"

#include "JC/Key.h"
#include "JC/Log.h"
#include "JC/StrDb.h"
#include "JC/Window.h"

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

//--------------------------------------------------------------------------------------------------

void Init(Mem permMem) {
	bindingSetNames   = Mem::AllocT<Str>(permMem, MaxBindingSets);
	bindingSetsLen    = 1; // reserve 0 for invalid
	bindings          = Mem::AllocT<Binding>(permMem, (U64)Key::Key::Max * MaxBindingSets);
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

void SetBindingSetStack(Span<BindingSet const> bindingSets) {
	Assert(bindingSets.len <= MaxBindingSets);
	activeBindingSetsLen = 0;
	for (U64 i = 0; i < bindingSets.len; i++) {
		Assert(bindingSets[i].handle > 0 && bindingSets[i].handle < bindingSetsLen);
		activeBindSets[activeBindingSetsLen++] = (U16)bindingSets[i].handle;
	}
}

//--------------------------------------------------------------------------------------------------

Span<U64 const> ProcessKeyEvents(Span<Window::KeyEvent const> keyEvents, U64* outActionIds, U64 outActionIdsMaxLen) {
	U64 outActionIdsLen = 0;
	for (U64 i = 0; i < keyEvents.len; i++) {
		Key::Key const key = keyEvents[i].key;
		Assert(key > Key::Key::Invalid && key < Key::Key::Max);

		bool const down    = keyEvents[i].down;
		bool const changed = keyDown[(U16)key] != down;
		if (down) {
			if (changed) {
				Assert(downKeysLen < (U16)Key::Key::Max);
				downKeys[downKeysLen++] = key;
				Logf("Key up -> down: %s", Key::GetKeyStr(key));
			}
			keyDown[(U16)key] = true;
		} else {	// key up
			if (changed) {
				// O(n) okay, few keys down at a time
				for (U16 j = 0; j < downKeysLen; j++) {
					if (downKeys[j] == key) {
						downKeys[j] = downKeys[downKeysLen - 1];
						downKeysLen--;
						break;
					}
				}
				Logf("Key down -> up: %s", Key::GetKeyStr(key));
			}
			keyDown[(U16)key] = false;
		}

		if (!changed) { continue; }

		Binding* const keyBindings = bindings + (U64)key * MaxBindingSets;
		BindingType const targetBindingType = down ? BindingType::OnKeyDown : BindingType::OnKeyUp;
		for (I16 j = (I16)activeBindingSetsLen - 1; j >= 0; j--) {
			Binding const* const binding = &keyBindings[activeBindSets[j]];
			if (binding->actionId && (binding->bindingType == targetBindingType)) {
				if (outActionIdsLen >= outActionIdsMaxLen) {
					Errorf("Dropped action ID %u", binding->actionId);
					return Span<U64 const>(outActionIds, outActionIdsLen);
				}
				outActionIds[outActionIdsLen++] = binding->actionId;
				break;
			}
		}
	}

	for (U16 i = 0; i < downKeysLen; i++) {
		Binding* const keyBindings = bindings + (U64)downKeys[i] * MaxBindingSets;
		for (I16 j = (I16)activeBindingSetsLen - 1; j >= 0; j--) {
			Binding const* const binding = &keyBindings[activeBindSets[j]];
			if (binding->actionId && binding->bindingType == BindingType::Continuous) {
				if (outActionIdsLen >= outActionIdsMaxLen) {
					Errorf("Dropped action ID %u", binding->actionId);
					return Span<U64 const>(outActionIds, outActionIdsLen);
				}
				// This binding set wins for this key
				// break and go to next down key
				outActionIds[outActionIdsLen++] = binding->actionId;
				break;
			}
		}
	}

	return Span<U64 const>(outActionIds, outActionIdsLen);
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Input