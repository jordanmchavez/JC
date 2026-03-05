#include "JC/Input.h"

#include "JC/Key.h"
#include "JC/Log.h"
#include "JC/StrDb.h"
#include "JC/UnitTest.h"
#include "JC/Window.h"

namespace JC::Input {

//--------------------------------------------------------------------------------------------------

static constexpr U32 MaxBindingSets = 32;

struct Binding {
	BindingType bindingType = BindingType::Invalid;
	U64         actionId = 0;
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
	bindingSetNames = Mem::AllocT<Str>(permMem, MaxBindingSets);
	bindingSetsLen  = 1; // reserve 0 for invalid
	bindings        = Mem::AllocT<Binding>(permMem, (U64)Key::Key::Max * MaxBindingSets);
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

void Bind(BindingSet bindingSet, Key::Key key, BindingType bindingType, U64 actionId) {
	Assert(bindingSet.handle > 0 && bindingSet.handle < bindingSetsLen);
	Assert(actionId != 0);
	Assert(bindingType != BindingType::Invalid);
	bindings[((U64)key * MaxBindingSets) + bindingSet.handle] = {
		.bindingType = bindingType,
		.actionId    = actionId,
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
// Tests
//--------------------------------------------------------------------------------------------------

Unit_Test("Input") {
	Input::Init(testMem);

	Unit_SubTest("CreateBindingSet") {
		BindingSet bs1 = CreateBindingSet("a");
		BindingSet bs2 = CreateBindingSet("b");
		Unit_Check(bs1);
		Unit_Check(bs2);
		Unit_CheckNeq(bs1.handle, bs2.handle);
	}

	Unit_SubTest("OnKeyDown") {
		BindingSet bs = CreateBindingSet("game");
		Bind(bs, Key::Key::A, BindingType::OnKeyDown, 1);
		SetBindingSetStack(Span<BindingSet const>({bs}));
		U64 out[16];

		Unit_SubTest("fires on key down") {
			Span<U64 const> r = ProcessKeyEvents(Span<Window::KeyEvent const>({{Key::Key::A, true}}), out, 16);
			Unit_CheckEq(r.len, (U64)1);
			Unit_CheckEq(out[0], (U64)1);
			ProcessKeyEvents(Span<Window::KeyEvent const>({{Key::Key::A, false}}), out, 16);
		}

		Unit_SubTest("no fire on repeated key down") {
			ProcessKeyEvents(Span<Window::KeyEvent const>({{Key::Key::A, true}}), out, 16);
			Span<U64 const> r = ProcessKeyEvents(Span<Window::KeyEvent const>({{Key::Key::A, true}}), out, 16);
			Unit_CheckEq(r.len, (U64)0);
			ProcessKeyEvents(Span<Window::KeyEvent const>({{Key::Key::A, false}}), out, 16);
		}

		Unit_SubTest("no fire on key up") {
			ProcessKeyEvents(Span<Window::KeyEvent const>({{Key::Key::A, true}}), out, 16);
			Span<U64 const> r = ProcessKeyEvents(Span<Window::KeyEvent const>({{Key::Key::A, false}}), out, 16);
			Unit_CheckEq(r.len, (U64)0);
		}
	}

	Unit_SubTest("OnKeyUp") {
		BindingSet bs = CreateBindingSet("game");
		Bind(bs, Key::Key::B, BindingType::OnKeyUp, 2);
		SetBindingSetStack(Span<BindingSet const>({bs}));
		U64 out[16];

		Unit_SubTest("fires on key up") {
			ProcessKeyEvents(Span<Window::KeyEvent const>({{Key::Key::B, true}}), out, 16);
			Span<U64 const> r = ProcessKeyEvents(Span<Window::KeyEvent const>({{Key::Key::B, false}}), out, 16);
			Unit_CheckEq(r.len, (U64)1);
			Unit_CheckEq(out[0], (U64)2);
		}

		Unit_SubTest("no fire on repeated key up") {
			// B is already up — sending another up event is not a state change
			Span<U64 const> r = ProcessKeyEvents(Span<Window::KeyEvent const>({{Key::Key::B, false}}), out, 16);
			Unit_CheckEq(r.len, (U64)0);
		}

		Unit_SubTest("no fire on key down") {
			Span<U64 const> r = ProcessKeyEvents(Span<Window::KeyEvent const>({{Key::Key::B, true}}), out, 16);
			Unit_CheckEq(r.len, (U64)0);
			ProcessKeyEvents(Span<Window::KeyEvent const>({{Key::Key::B, false}}), out, 16);
		}
	}

	Unit_SubTest("Continuous") {
		BindingSet bs = CreateBindingSet("game");
		Bind(bs, Key::Key::C, BindingType::Continuous, 3);
		SetBindingSetStack(Span<BindingSet const>({bs}));
		U64 out[16];
		Span<Window::KeyEvent const> const noEvs;

		Unit_SubTest("fires on the same frame the key goes down") {
			Span<U64 const> r = ProcessKeyEvents(Span<Window::KeyEvent const>({{Key::Key::C, true}}), out, 16);
			Unit_CheckEq(r.len, (U64)1);
			Unit_CheckEq(out[0], (U64)3);
			ProcessKeyEvents(Span<Window::KeyEvent const>({{Key::Key::C, false}}), out, 16);
		}

		Unit_SubTest("fires on subsequent frames while held") {
			ProcessKeyEvents(Span<Window::KeyEvent const>({{Key::Key::C, true}}), out, 16);
			Span<U64 const> r = ProcessKeyEvents(noEvs, out, 16);
			Unit_CheckEq(r.len, (U64)1);
			Unit_CheckEq(out[0], (U64)3);
			ProcessKeyEvents(Span<Window::KeyEvent const>({{Key::Key::C, false}}), out, 16);
		}

		Unit_SubTest("no fire when key is up") {
			Span<U64 const> r = ProcessKeyEvents(noEvs, out, 16);
			Unit_CheckEq(r.len, (U64)0);
		}
	}

	Unit_SubTest("Unbind") {
		BindingSet bs = CreateBindingSet("game");
		Bind(bs, Key::Key::D, BindingType::OnKeyDown, 4);
		SetBindingSetStack(Span<BindingSet const>({bs}));
		U64 out[16];

		Unit_SubTest("no action fires after unbind") {
			Unbind(bs, Key::Key::D);
			Span<U64 const> r = ProcessKeyEvents(Span<Window::KeyEvent const>({{Key::Key::D, true}}), out, 16);
			Unit_CheckEq(r.len, (U64)0);
			ProcessKeyEvents(Span<Window::KeyEvent const>({{Key::Key::D, false}}), out, 16);
		}
	}

	Unit_SubTest("SetBindingSetStack") {
		BindingSet bs = CreateBindingSet("game");
		Bind(bs, Key::Key::E, BindingType::OnKeyDown, 5);
		U64 out[16];

		Unit_SubTest("empty stack fires nothing") {
			SetBindingSetStack({});
			Span<U64 const> r = ProcessKeyEvents(Span<Window::KeyEvent const>({{Key::Key::E, true}}), out, 16);
			Unit_CheckEq(r.len, (U64)0);
			ProcessKeyEvents(Span<Window::KeyEvent const>({{Key::Key::E, false}}), out, 16);
		}

		Unit_SubTest("binding in inactive set fires nothing") {
			BindingSet bs2 = CreateBindingSet("ui");
			SetBindingSetStack(Span<BindingSet const>({bs2}));	// only bs2 active; E bound in bs
			Span<U64 const> r = ProcessKeyEvents(Span<Window::KeyEvent const>({{Key::Key::E, true}}), out, 16);
			Unit_CheckEq(r.len, (U64)0);
			ProcessKeyEvents(Span<Window::KeyEvent const>({{Key::Key::E, false}}), out, 16);
		}
	}

	Unit_SubTest("BindingSetPriority") {
		BindingSet bs1 = CreateBindingSet("low");
		BindingSet bs2 = CreateBindingSet("high");
		Bind(bs1, Key::Key::F, BindingType::OnKeyDown, 10);
		SetBindingSetStack(Span<BindingSet const>({bs1, bs2}));
		U64 out[16];

		Unit_SubTest("higher priority set wins when both have a binding") {
			Bind(bs2, Key::Key::F, BindingType::OnKeyDown, 11);
			Span<U64 const> r = ProcessKeyEvents(Span<Window::KeyEvent const>({{Key::Key::F, true}}), out, 16);
			Unit_CheckEq(r.len, (U64)1);
			Unit_CheckEq(out[0], (U64)11);
			ProcessKeyEvents(Span<Window::KeyEvent const>({{Key::Key::F, false}}), out, 16);
		}

		Unit_SubTest("falls through to lower priority when higher has no binding") {
			// bs2 has no binding for F this run — only bs1 does
			Span<U64 const> r = ProcessKeyEvents(Span<Window::KeyEvent const>({{Key::Key::F, true}}), out, 16);
			Unit_CheckEq(r.len, (U64)1);
			Unit_CheckEq(out[0], (U64)10);
			ProcessKeyEvents(Span<Window::KeyEvent const>({{Key::Key::F, false}}), out, 16);
		}
	}

	Unit_SubTest("MultipleEvents") {
		BindingSet bs = CreateBindingSet("game");
		Bind(bs, Key::Key::G, BindingType::OnKeyDown, 20);
		Bind(bs, Key::Key::H, BindingType::OnKeyDown, 21);
		SetBindingSetStack(Span<BindingSet const>({bs}));
		U64 out[16];

		Unit_SubTest("both actions fire for two key presses in a single call") {
			Window::KeyEvent const presses[] = {{Key::Key::G, true}, {Key::Key::H, true}};
			Span<U64 const> r = ProcessKeyEvents(Span<Window::KeyEvent const>(presses, 2), out, 16);
			Unit_CheckEq(r.len, (U64)2);
			Unit_CheckEq(out[0], (U64)20);
			Unit_CheckEq(out[1], (U64)21);
			Window::KeyEvent const releases[] = {{Key::Key::G, false}, {Key::Key::H, false}};
			ProcessKeyEvents(Span<Window::KeyEvent const>(releases, 2), out, 16);
		}
	}

	Unit_SubTest("OutputOverflow") {
		BindingSet bs = CreateBindingSet("game");
		Bind(bs, Key::Key::I, BindingType::OnKeyDown, 30);
		Bind(bs, Key::Key::J, BindingType::OnKeyDown, 31);
		SetBindingSetStack(Span<BindingSet const>({bs}));

		Unit_SubTest("first action written, second dropped, returns early") {
			U64 out[1];
			Window::KeyEvent const presses[] = {{Key::Key::I, true}, {Key::Key::J, true}};
			Span<U64 const> r = ProcessKeyEvents(Span<Window::KeyEvent const>(presses, 2), out, 1);
			Unit_CheckEq(r.len, (U64)1);
			Unit_CheckEq(out[0], (U64)30);
			U64 cleanupOut[16];
			Window::KeyEvent const releases[] = {{Key::Key::I, false}, {Key::Key::J, false}};
			ProcessKeyEvents(Span<Window::KeyEvent const>(releases, 2), cleanupOut, 16);
		}
	}
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Input