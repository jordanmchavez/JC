#include "JC/Battle_Internal.h"

#include "JC/App.h"
#include "JC/Input.h"
#include "JC/Key.h"
#include "JC/Log.h"
#include "JC/UnitDef.h"

namespace JC::Battle {

//--------------------------------------------------------------------------------------------------

enum : U64 {
	ActionId_Exit = 1,
	ActionId_LClick,
	ActionId_RClick,
	ActionId_ZoomIn,
	ActionId_ZoomOut,
	ActionId_ScrollMapLeft,
	ActionId_ScrollMapRight,
	ActionId_ScrollMapUp,
	ActionId_ScrollMapDown,
	ActionId_ShowEnemyArmyThreatMap,
	ActionId_NextUnit,
	ActionId_EndUnitTurn,
};

//--------------------------------------------------------------------------------------------------

static constexpr U16 MaxClickHexes = Input::MaxActionsPerFrame;

static Mem               tempMem;
static Input::BindingSet bindingSet;
static Hex**             clickHexes;

//--------------------------------------------------------------------------------------------------

void InitInput(Mem permMem, Mem tempMemIn) {
	tempMem = tempMemIn;

	bindingSet = Input::CreateBindingSet("Main");
	Input::Bind(bindingSet, Key::Key::Escape,         Input::BindingType::OnKeyDown,  ActionId_Exit);
	Input::Bind(bindingSet, Key::Key::Mouse1,         Input::BindingType::OnKeyUp,    ActionId_LClick);
	Input::Bind(bindingSet, Key::Key::Mouse2,         Input::BindingType::OnKeyUp,    ActionId_RClick);
	Input::Bind(bindingSet, Key::Key::MouseWheelUp,   Input::BindingType::OnKeyDown,  ActionId_ZoomIn);
	Input::Bind(bindingSet, Key::Key::MouseWheelDown, Input::BindingType::OnKeyDown,  ActionId_ZoomOut);
	Input::Bind(bindingSet, Key::Key::A,              Input::BindingType::Continuous, ActionId_ScrollMapLeft);
	Input::Bind(bindingSet, Key::Key::D,              Input::BindingType::Continuous, ActionId_ScrollMapRight);
	Input::Bind(bindingSet, Key::Key::W,              Input::BindingType::Continuous, ActionId_ScrollMapUp);
	Input::Bind(bindingSet, Key::Key::S,              Input::BindingType::Continuous, ActionId_ScrollMapDown);
	Input::Bind(bindingSet, Key::Key::AltLeft,        Input::BindingType::Continuous, ActionId_ShowEnemyArmyThreatMap);
	Input::Bind(bindingSet, Key::Key::AltRight,       Input::BindingType::Continuous, ActionId_ShowEnemyArmyThreatMap);
	Input::Bind(bindingSet, Key::Key::N,              Input::BindingType::OnKeyUp,    ActionId_NextUnit);
	Input::Bind(bindingSet, Key::Key::Space,          Input::BindingType::OnKeyUp,    ActionId_EndUnitTurn);
	Input::SetBindingSetStack({ bindingSet });

	clickHexes = Mem::AllocT<Hex*>(permMem, MaxClickHexes);
}

//--------------------------------------------------------------------------------------------------

Res<InputResult> HandleInput(F32 sec, U32 mouseX, U32 mouseY, Span<Input::Action const> actions) {
	F32 cameraDX = 0.f;
	F32 cameraDY = 0.f;

	U16 clickHexesLen = 0;
	bool showEnemyArmyThreatMap = 0;
	for (U64 i = 0; i < actions.len; i++) {
		Input::Action const action = actions[i];
		switch (action.id) {
			case ActionId_Exit: return App::Err_Exit();

			case ActionId_LClick: {
				if (Hex* const clickHex = ScreenPosToHex(action.mouseX, action.mouseY)) {
					Assert(clickHexesLen < MaxClickHexes);
					clickHexes[clickHexesLen++] = clickHex;
				}
				break;
			}
			case ActionId_RClick: {
				Assert(clickHexesLen < MaxClickHexes);
				clickHexes[clickHexesLen++] = nullptr;
				break;
			}

			case ActionId_ZoomIn:  ZoomCamera( 1.f); break;
			case ActionId_ZoomOut: ZoomCamera(-1.f); break;

			case ActionId_ScrollMapLeft:  cameraDX--; break;
			case ActionId_ScrollMapRight: cameraDX++; break;
			case ActionId_ScrollMapUp:    cameraDY--; break;
			case ActionId_ScrollMapDown:  cameraDY++; break;

			case ActionId_ShowEnemyArmyThreatMap: showEnemyArmyThreatMap = true; break;

			case ActionId_NextUnit: SelectNextUnit(); break;

			case ActionId_EndUnitTurn: EndSelectedUnitTurn(); break;
		}
	}

	MoveCamera(sec, cameraDX, cameraDY);

	return InputResult {
		.clickHexes             = Span<Hex*>(clickHexes, clickHexesLen),
		.hoverHex               = ScreenPosToHex(mouseX, mouseY),
		.showEnemyArmyThreatMap = showEnemyArmyThreatMap,
	};
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Battle