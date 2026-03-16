#include "JC/Input.h"

#include "JC/Key.h"
#include "JC/Log.h"
#include "JC/StrDb.h"
#include "JC/UnitTest.h"
#include "JC/Window.h"

namespace JC::Input {

//--------------------------------------------------------------------------------------------------

static constexpr U8  MaxBindingSets = 32;

struct Binding {
	BindingType bindingType = BindingType::Invalid;
	U64         actionId = 0;
};

struct KeyState {
	bool down;
	I32  mouseX;	// Mouse pos at time of last change
	I32  mouseY;
};

static Str*     bindingSetNames;
static U8       bindingSetsLen;
static Binding* bindings;	// [key * MaxBindingSets + bindSetIdx] -> bind for this key+bindset
static KeyState keyStates[Key::Max];
static Key::Key downKeys[Key::Max];
static U8       downKeysLen;
static U8       activeBindSets[MaxBindingSets];
static U8       activeBindingSetsLen;
static Action*  actions;
static U16      actionsLen;

//--------------------------------------------------------------------------------------------------

void Init(Mem permMem) {
	bindingSetNames = Mem::AllocT<Str>(permMem, MaxBindingSets);
	bindingSetsLen  = 1; // reserve 0 for invalid
	bindings        = Mem::AllocT<Binding>(permMem, (U64)Key::Key::Max * MaxBindingSets);
	actions         = Mem::AllocT<Action>(permMem, MaxActionsPerFrame);
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
		activeBindSets[activeBindingSetsLen++] = (U8)bindingSets[i].handle;
	}
}

//--------------------------------------------------------------------------------------------------

Span<Action const> ProcessKeyEvents(Span<Window::KeyEvent const> keyEvents) {
	actionsLen = 0;
	for (U64 i = 0; i < keyEvents.len; i++) {
		Key::Key const key = keyEvents[i].key;
		Assert(key > Key::Key::Invalid && key < Key::Key::Max);

		bool const down    = keyEvents[i].down;
		bool const changed = keyStates[key].down != down;
		if (down) {
			if (changed) {
				Assert(downKeysLen < Key::Key::Max);
				downKeys[downKeysLen++] = key;
			}
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
		}
		keyStates[key] = {
			.down   = down,
			.mouseX = keyEvents[i].mouseX,
			.mouseY = keyEvents[i].mouseY,
		};

		if (!changed) { continue; }

		Binding* const keyBindings = bindings + ((U64)key * MaxBindingSets);
		BindingType const targetBindingType = down ? BindingType::OnKeyDown : BindingType::OnKeyUp;
		for (I8 j = (I8)activeBindingSetsLen - 1; j >= 0; j--) {
			Binding const* const binding = &keyBindings[activeBindSets[j]];
			if (binding->actionId && (binding->bindingType == targetBindingType)) {
				if (actionsLen >= MaxActionsPerFrame) {
					Errorf("Dropped action ID %u", binding->actionId);
					return Span<Action const>(actions, actionsLen);
				}
				actions[actionsLen++] = {
					.id     = binding->actionId,
					.mouseX = keyStates[key].mouseX,
					.mouseY = keyStates[key].mouseY,
				};
				break;
			}
		}
	}

	for (U16 i = 0; i < downKeysLen; i++) {
		Key::Key const key = downKeys[i];
		Binding* const keyBindings = bindings + ((U64)key * MaxBindingSets);
		for (I8 j = (I8)activeBindingSetsLen - 1; j >= 0; j--) {
			Binding const* const binding = &keyBindings[activeBindSets[j]];
			if (binding->actionId && binding->bindingType == BindingType::Continuous) {
				if (actionsLen >= MaxActionsPerFrame) {
					Errorf("Dropped action ID %u", binding->actionId);
					return Span<Action const>(actions, actionsLen);
				}
				// This binding set wins for this key
				// break and go to next down key
				actions[actionsLen++] = {
					.id     = binding->actionId,
					.mouseX = keyStates[key].mouseX,
					.mouseY = keyStates[key].mouseY,
				};
				break;
			}
		}
	}

	return Span<Action const>(actions, actionsLen);
}

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

		Unit_SubTest("fires on key down") {
			Span<Action const> r = ProcessKeyEvents(Span<Window::KeyEvent const>({{Key::Key::A, true, 0, 0}}));
			Unit_CheckEq(r.len, (U64)1);
			Unit_CheckEq(r[0].id, (U64)1);
			ProcessKeyEvents(Span<Window::KeyEvent const>({{Key::Key::A, false, 0, 0}}));
		}

		Unit_SubTest("no fire on repeated key down") {
			ProcessKeyEvents(Span<Window::KeyEvent const>({{Key::Key::A, true, 0, 0}}));
			Span<Action const> r = ProcessKeyEvents(Span<Window::KeyEvent const>({{Key::Key::A, true, 0, 0}}));
			Unit_CheckEq(r.len, (U64)0);
			ProcessKeyEvents(Span<Window::KeyEvent const>({{Key::Key::A, false, 0, 0}}));
		}

		Unit_SubTest("no fire on key up") {
			ProcessKeyEvents(Span<Window::KeyEvent const>({{Key::Key::A, true, 0, 0}}));
			Span<Action const> r = ProcessKeyEvents(Span<Window::KeyEvent const>({{Key::Key::A, false, 0, 0}}));
			Unit_CheckEq(r.len, (U64)0);
		}
	}

	Unit_SubTest("OnKeyUp") {
		BindingSet bs = CreateBindingSet("game");
		Bind(bs, Key::Key::B, BindingType::OnKeyUp, 2);
		SetBindingSetStack(Span<BindingSet const>({bs}));

		Unit_SubTest("fires on key up") {
			ProcessKeyEvents(Span<Window::KeyEvent const>({{Key::Key::B, true, 0, 0}}));
			Span<Action const> r = ProcessKeyEvents(Span<Window::KeyEvent const>({{Key::Key::B, false, 0, 0}}));
			Unit_CheckEq(r.len, (U64)1);
			Unit_CheckEq(r[0].id, (U64)2);
		}

		Unit_SubTest("no fire on repeated key up") {
			// B is already up — sending another up event is not a state change
			Span<Action const> r = ProcessKeyEvents(Span<Window::KeyEvent const>({{Key::Key::B, false, 0, 0}}));
			Unit_CheckEq(r.len, (U64)0);
		}

		Unit_SubTest("no fire on key down") {
			Span<Action const> r = ProcessKeyEvents(Span<Window::KeyEvent const>({{Key::Key::B, true, 0, 0}}));
			Unit_CheckEq(r.len, (U64)0);
			ProcessKeyEvents(Span<Window::KeyEvent const>({{Key::Key::B, false, 0, 0}}));
		}
	}

	Unit_SubTest("Continuous") {
		BindingSet bs = CreateBindingSet("game");
		Bind(bs, Key::Key::C, BindingType::Continuous, 3);
		SetBindingSetStack(Span<BindingSet const>({bs}));
		Span<Window::KeyEvent const> const noEvs;

		Unit_SubTest("fires on the same frame the key goes down") {
			Span<Action const> r = ProcessKeyEvents(Span<Window::KeyEvent const>({{Key::Key::C, true, 0, 0}}));
			Unit_CheckEq(r.len, (U64)1);
			Unit_CheckEq(r[0].id, (U64)3);
			ProcessKeyEvents(Span<Window::KeyEvent const>({{Key::Key::C, false, 0, 0}}));
		}

		Unit_SubTest("fires on subsequent frames while held") {
			ProcessKeyEvents(Span<Window::KeyEvent const>({{Key::Key::C, true, 0, 0}}));
			Span<Action const> r = ProcessKeyEvents(noEvs);
			Unit_CheckEq(r.len, (U64)1);
			Unit_CheckEq(r[0].id, (U64)3);
			ProcessKeyEvents(Span<Window::KeyEvent const>({{Key::Key::C, false, 0, 0}}));
		}

		Unit_SubTest("no fire when key is up") {
			Span<Action const> r = ProcessKeyEvents(noEvs);
			Unit_CheckEq(r.len, (U64)0);
		}
	}

	Unit_SubTest("Unbind") {
		BindingSet bs = CreateBindingSet("game");
		Bind(bs, Key::Key::D, BindingType::OnKeyDown, 4);
		SetBindingSetStack(Span<BindingSet const>({bs}));

		Unit_SubTest("no action fires after unbind") {
			Unbind(bs, Key::Key::D);
			Span<Action const> r = ProcessKeyEvents(Span<Window::KeyEvent const>({{Key::Key::D, true, 0, 0}}));
			Unit_CheckEq(r.len, (U64)0);
			ProcessKeyEvents(Span<Window::KeyEvent const>({{Key::Key::D, false, 0, 0}}));
		}
	}

	Unit_SubTest("SetBindingSetStack") {
		BindingSet bs = CreateBindingSet("game");
		Bind(bs, Key::Key::E, BindingType::OnKeyDown, 5);

		Unit_SubTest("empty stack fires nothing") {
			SetBindingSetStack({});
			Span<Action const> r = ProcessKeyEvents(Span<Window::KeyEvent const>({{Key::Key::E, true, 0, 0}}));
			Unit_CheckEq(r.len, (U64)0);
			ProcessKeyEvents(Span<Window::KeyEvent const>({{Key::Key::E, false, 0, 0}}));
		}

		Unit_SubTest("binding in inactive set fires nothing") {
			BindingSet bs2 = CreateBindingSet("ui");
			SetBindingSetStack(Span<BindingSet const>({bs2}));	// only bs2 active; E bound in bs
			Span<Action const> r = ProcessKeyEvents(Span<Window::KeyEvent const>({{Key::Key::E, true, 0, 0}}));
			Unit_CheckEq(r.len, (U64)0);
			ProcessKeyEvents(Span<Window::KeyEvent const>({{Key::Key::E, false, 0, 0}}));
		}
	}

	Unit_SubTest("BindingSetPriority") {
		BindingSet bs1 = CreateBindingSet("low");
		BindingSet bs2 = CreateBindingSet("high");
		Bind(bs1, Key::Key::F, BindingType::OnKeyDown, 10);
		SetBindingSetStack(Span<BindingSet const>({bs1, bs2}));

		Unit_SubTest("higher priority set wins when both have a binding") {
			Bind(bs2, Key::Key::F, BindingType::OnKeyDown, 11);
			Span<Action const> r = ProcessKeyEvents(Span<Window::KeyEvent const>({{Key::Key::F, true, 0, 0}}));
			Unit_CheckEq(r.len, (U64)1);
			Unit_CheckEq(r[0].id, (U64)11);
			ProcessKeyEvents(Span<Window::KeyEvent const>({{Key::Key::F, false, 0, 0}}));
		}

		Unit_SubTest("falls through to lower priority when higher has no binding") {
			Unbind(bs2, Key::Key::F);
			Span<Action const> r = ProcessKeyEvents(Span<Window::KeyEvent const>({{Key::Key::F, true, 0, 0}}));
			Unit_CheckEq(r.len, (U64)1);
			Unit_CheckEq(r[0].id, (U64)10);
			ProcessKeyEvents(Span<Window::KeyEvent const>({{Key::Key::F, false, 0, 0}}));
		}
	}

	Unit_SubTest("MultipleEvents") {
		BindingSet bs = CreateBindingSet("game");
		Bind(bs, Key::Key::G, BindingType::OnKeyDown, 20);
		Bind(bs, Key::Key::H, BindingType::OnKeyDown, 21);
		SetBindingSetStack(Span<BindingSet const>({bs}));

		Unit_SubTest("both actions fire for two key presses in a single call") {
			Window::KeyEvent const presses[] = {{Key::Key::G, true, 0, 0}, {Key::Key::H, true, 0, 0}};
			Span<Action const> r = ProcessKeyEvents(Span<Window::KeyEvent const>(presses, 2));
			Unit_CheckEq(r.len, (U64)2);
			Unit_CheckEq(r[0].id, (U64)20);
			Unit_CheckEq(r[1].id, (U64)21);
			Window::KeyEvent const releases[] = {{Key::Key::G, false, 0, 0}, {Key::Key::H, false, 0, 0}};
			ProcessKeyEvents(Span<Window::KeyEvent const>(releases, 2));
		}
	}

	Unit_SubTest("MouseCoords") {
		BindingSet bs = CreateBindingSet("game");
		Bind(bs, Key::Key::K, BindingType::OnKeyDown, 40);
		Bind(bs, Key::Key::L, BindingType::OnKeyUp, 41);
		Bind(bs, Key::Key::M, BindingType::Continuous, 42);
		SetBindingSetStack(Span<BindingSet const>({bs}));

		Unit_SubTest("OnKeyDown action carries mouse coords from the event") {
			Span<Action const> r = ProcessKeyEvents(Span<Window::KeyEvent const>({{Key::Key::K, true, 100, 200}}));
			Unit_CheckEq(r.len, (U64)1);
			Unit_CheckEq(r[0].mouseX, (I32)100);
			Unit_CheckEq(r[0].mouseY, (I32)200);
			ProcessKeyEvents(Span<Window::KeyEvent const>({{Key::Key::K, false, 0, 0}}));
		}

		Unit_SubTest("OnKeyUp action carries mouse coords from the event") {
			ProcessKeyEvents(Span<Window::KeyEvent const>({{Key::Key::L, true, 0, 0}}));
			Span<Action const> r = ProcessKeyEvents(Span<Window::KeyEvent const>({{Key::Key::L, false, 300, 400}}));
			Unit_CheckEq(r.len, (U64)1);
			Unit_CheckEq(r[0].mouseX, (I32)300);
			Unit_CheckEq(r[0].mouseY, (I32)400);
		}

		Unit_SubTest("Continuous action carries mouse coords from last key event") {
			ProcessKeyEvents(Span<Window::KeyEvent const>({{Key::Key::M, true, 50, 60}}));
			Span<Window::KeyEvent const> const noEvs;
			Span<Action const> r = ProcessKeyEvents(noEvs);
			Unit_CheckEq(r.len, (U64)1);
			Unit_CheckEq(r[0].mouseX, (I32)50);
			Unit_CheckEq(r[0].mouseY, (I32)60);
			ProcessKeyEvents(Span<Window::KeyEvent const>({{Key::Key::M, false, 0, 0}}));
		}
	}

	Unit_SubTest("BindingTypeMismatchFallthrough") {
		BindingSet bs1 = CreateBindingSet("low");
		BindingSet bs2 = CreateBindingSet("high");
		Bind(bs1, Key::Key::N, BindingType::OnKeyDown, 50);
		Bind(bs2, Key::Key::N, BindingType::OnKeyUp, 51);
		SetBindingSetStack(Span<BindingSet const>({bs1, bs2}));

		Unit_SubTest("lower-priority OnKeyDown fires when higher-priority has wrong binding type") {
			Span<Action const> r = ProcessKeyEvents(Span<Window::KeyEvent const>({{Key::Key::N, true, 0, 0}}));
			Unit_CheckEq(r.len, (U64)1);
			Unit_CheckEq(r[0].id, (U64)50);
			ProcessKeyEvents(Span<Window::KeyEvent const>({{Key::Key::N, false, 0, 0}}));
		}
	}

	Unit_SubTest("ContinuousPriority") {
		BindingSet bs1 = CreateBindingSet("low");
		BindingSet bs2 = CreateBindingSet("high");
		Bind(bs1, Key::Key::O, BindingType::Continuous, 60);
		Bind(bs2, Key::Key::O, BindingType::Continuous, 61);
		SetBindingSetStack(Span<BindingSet const>({bs1, bs2}));
		Span<Window::KeyEvent const> const noEvs;

		Unit_SubTest("higher priority continuous binding wins") {
			ProcessKeyEvents(Span<Window::KeyEvent const>({{Key::Key::O, true, 0, 0}}));
			Span<Action const> r = ProcessKeyEvents(noEvs);
			Unit_CheckEq(r.len, (U64)1);
			Unit_CheckEq(r[0].id, (U64)61);
			ProcessKeyEvents(Span<Window::KeyEvent const>({{Key::Key::O, false, 0, 0}}));
		}
	}

	Unit_SubTest("DownUpSameCall") {
		Unit_SubTest("both OnKeyDown and OnKeyUp fire when down and up in same call") {
			BindingSet bs1 = CreateBindingSet("low");
			BindingSet bs2 = CreateBindingSet("high");
			Bind(bs1, Key::Key::P, BindingType::OnKeyUp, 70);
			Bind(bs2, Key::Key::P, BindingType::OnKeyDown, 71);
			SetBindingSetStack(Span<BindingSet const>({bs1, bs2}));
			Window::KeyEvent const evs[] = {{Key::Key::P, true, 0, 0}, {Key::Key::P, false, 0, 0}};
			Span<Action const> r = ProcessKeyEvents(Span<Window::KeyEvent const>(evs, 2));
			Unit_CheckEq(r.len, (U64)2);
			Unit_CheckEq(r[0].id, (U64)71);
			Unit_CheckEq(r[1].id, (U64)70);
		}

		Unit_SubTest("Continuous does not fire when down and up in same call") {
			BindingSet bs = CreateBindingSet("game");
			Bind(bs, Key::Key::Q, BindingType::Continuous, 72);
			SetBindingSetStack(Span<BindingSet const>({bs}));
			Window::KeyEvent const evs[] = {{Key::Key::Q, true, 0, 0}, {Key::Key::Q, false, 0, 0}};
			Span<Action const> r = ProcessKeyEvents(Span<Window::KeyEvent const>(evs, 2));
			Unit_CheckEq(r.len, (U64)0);
		}
	}

	Unit_SubTest("ContinuousStopsWhenStackCleared") {
		BindingSet bs = CreateBindingSet("game");
		Bind(bs, Key::Key::R, BindingType::Continuous, 80);
		SetBindingSetStack(Span<BindingSet const>({bs}));
		Span<Window::KeyEvent const> const noEvs;

		Unit_SubTest("no fire on next frame after stack is cleared while key is held") {
			ProcessKeyEvents(Span<Window::KeyEvent const>({{Key::Key::R, true, 0, 0}}));
			SetBindingSetStack({});
			Span<Action const> r = ProcessKeyEvents(noEvs);
			Unit_CheckEq(r.len, (U64)0);
			ProcessKeyEvents(Span<Window::KeyEvent const>({{Key::Key::R, false, 0, 0}}));
		}
	}
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Input