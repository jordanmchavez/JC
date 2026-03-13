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
	ActionId_Select,
	ActionId_Deselect,
	ActionId_ZoomIn,
	ActionId_ZoomOut,
	ActionId_ScrollMapLeft,
	ActionId_ScrollMapRight,
	ActionId_ScrollMapUp,
	ActionId_ScrollMapDown,
	ActionId_ShowEnemyOverlay,
};

//--------------------------------------------------------------------------------------------------

static Mem                tempMem;
static Input::BindingSet  bindingSet;

//--------------------------------------------------------------------------------------------------

void InitInput(Mem tempMemIn) {
	tempMem = tempMemIn;

	bindingSet = Input::CreateBindingSet("Main");
	Input::Bind(bindingSet, Key::Key::Escape,         Input::BindingType::OnKeyDown,  ActionId_Exit);
	Input::Bind(bindingSet, Key::Key::Mouse1,         Input::BindingType::OnKeyUp,    ActionId_Select);
	Input::Bind(bindingSet, Key::Key::Mouse2,         Input::BindingType::OnKeyUp,    ActionId_Deselect);
	Input::Bind(bindingSet, Key::Key::MouseWheelUp,   Input::BindingType::OnKeyDown,  ActionId_ZoomIn);
	Input::Bind(bindingSet, Key::Key::MouseWheelDown, Input::BindingType::OnKeyDown,  ActionId_ZoomOut);
	Input::Bind(bindingSet, Key::Key::A,              Input::BindingType::Continuous, ActionId_ScrollMapLeft);
	Input::Bind(bindingSet, Key::Key::D,              Input::BindingType::Continuous, ActionId_ScrollMapRight);
	Input::Bind(bindingSet, Key::Key::W,              Input::BindingType::Continuous, ActionId_ScrollMapUp);
	Input::Bind(bindingSet, Key::Key::S,              Input::BindingType::Continuous, ActionId_ScrollMapDown);
	Input::Bind(bindingSet, Key::Key::AltLeft,        Input::BindingType::Continuous, ActionId_ShowEnemyOverlay);
	Input::Bind(bindingSet, Key::Key::AltRight,       Input::BindingType::Continuous, ActionId_ShowEnemyOverlay);
	Input::SetBindingSetStack({ bindingSet });
}

//--------------------------------------------------------------------------------------------------

static bool UnitCanAttackHex(Data const* data, Unit const* unit, Hex const* hex) {
	Army const* const army = &data->armies[unit->side];
	U64 const unitIdx = (U64)(unit - army->units);
	return army->attackMap[hex->idx] & ((U64)1 << unitIdx);
}

//--------------------------------------------------------------------------------------------------

static void Select(Data* data) { 
	if (!data->hoverHex) {
		return;
	}

	if (
		data->targetHex &&
		data->hoverHex == data->targetHex
	) {
		// TODO: execute order
		Logf("Executing order");
		return;
	}

	// Selected -> click on reachable hex -> taret
	if (
		data->selectedHex &&
		!data->hoverHex->unit &&
		data->selectedHex->unit->pathMap.parents[data->hoverHex->idx]
	) {
		data->targetHex = data->hoverHex;
		Logf("Targetted (%u, %u) for move", data->targetHex->c, data->targetHex->r);
		return;
	}

	// Selected -> click on targettable hex -> target
	if (
		data->selectedHex && 
		data->hoverHex->unit &&
		data->hoverHex->unit->side != data->selectedHex->unit->side &&
		UnitCanAttackHex(data, data->selectedHex->unit, data->hoverHex)		
	) {
		data->targetHex = data->hoverHex;
		Logf("Targetted (%u, %u) for attack", data->targetHex->c, data->targetHex->r);
		return;
	}

	// Selected -> click on selected -> deselect
	if (data->hoverHex == data->selectedHex) {
		data->selectedHex = nullptr;
		data->targetHex = nullptr;
		Logf("Cleared selected");
		return;
	}

	// Selected -> click on different friendly unit -> select that unit
	if (
		data->hoverHex->unit && 
		data->hoverHex->unit->side == data->activeSide
	) {
		data->selectedHex = data->hoverHex;
		data->targetHex = nullptr;
		Logf("Selected (%u, %u)", data->selectedHex->c, data->selectedHex->r);

		return;
	}
}

//--------------------------------------------------------------------------------------------------

static void Deselect(Data* data) {
	if (data->targetHex) {
		data->targetHex = nullptr;
		Logf("Deselected target");
		return;
	}

	if (data->selectedHex) {
		data->selectedHex = nullptr;
		data->targetHex = nullptr;
		Logf("Deselected");
		return;
	}
}

//--------------------------------------------------------------------------------------------------

Res<> HandleInput(Data* data, F32 sec, U32 mouseX, U32 mouseY, Span<U64 const> actionIds) {
	data->hoverHex = ScreenPosToHex(data, mouseX, mouseY);

	F32 cameraDX = 0.f;
	F32 cameraDY = 0.f;

	data->showEnemyThreatMap = false;
	for (U64 i = 0; i < actionIds.len; i++) {
		switch (actionIds[i]) {
			case ActionId_Exit: return App::Err_Exit();

			case ActionId_Select:   Select(data);   break;
			case ActionId_Deselect: Deselect(data); break;

			case ActionId_ZoomIn:  ZoomCamera(data, 1.f); break;
			case ActionId_ZoomOut: ZoomCamera(data, -1.f); break;

			case ActionId_ScrollMapLeft:  cameraDX--; break;
			case ActionId_ScrollMapRight: cameraDX++; break;
			case ActionId_ScrollMapUp:    cameraDY--; break;
			case ActionId_ScrollMapDown:  cameraDY++; break;

			case ActionId_ShowEnemyOverlay: data->showEnemyThreatMap = true; break;
		}
	}

	MoveCamera(data, sec, cameraDX, cameraDY);


	return Ok();
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Battle