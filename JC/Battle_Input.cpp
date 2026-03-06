#include "JC/Battle_Internal.h"

#include "JC/App.h"
#include "JC/Input.h"
#include "JC/Key.h"
#include "JC/Log.h"
#include "JC/Unit.h"

namespace JC::Battle {

//--------------------------------------------------------------------------------------------------

enum : U64 {
	ActionId_Exit = 1,
	ActionId_Click,
	ActionId_ZoomIn,
	ActionId_ZoomOut,
	ActionId_ScrollMapLeft,
	ActionId_ScrollMapRight,
	ActionId_ScrollMapUp,
	ActionId_ScrollMapDown,
};

//--------------------------------------------------------------------------------------------------

static Input::BindingSet  bindingSet;

//--------------------------------------------------------------------------------------------------

void InitInput() {
	bindingSet = Input::CreateBindingSet("Main");
	Input::Bind(bindingSet, Key::Key::Escape,         Input::BindingType::OnKeyDown,  ActionId_Exit);
	Input::Bind(bindingSet, Key::Key::Mouse1,         Input::BindingType::OnKeyUp,    ActionId_Click);
	Input::Bind(bindingSet, Key::Key::MouseWheelUp,   Input::BindingType::OnKeyDown,  ActionId_ZoomIn);
	Input::Bind(bindingSet, Key::Key::MouseWheelDown, Input::BindingType::OnKeyDown,  ActionId_ZoomOut);
	Input::Bind(bindingSet, Key::Key::A,              Input::BindingType::Continuous, ActionId_ScrollMapLeft);
	Input::Bind(bindingSet, Key::Key::D,              Input::BindingType::Continuous, ActionId_ScrollMapRight);
	Input::Bind(bindingSet, Key::Key::W,              Input::BindingType::Continuous, ActionId_ScrollMapUp);
	Input::Bind(bindingSet, Key::Key::S,              Input::BindingType::Continuous, ActionId_ScrollMapDown);
	Input::SetBindingSetStack({ bindingSet });
}

//--------------------------------------------------------------------------------------------------

static Res<> Click(Data* data) { 
	if (!data->hoverHex) {
		return Ok();
	}

	if (data->state == State::WaitingOrder) {
		if (data->selectedHex == data->hoverHex) {
			Logf("Deselected hex (%i,%i)", data->selectedHex->c, data->selectedHex->r);
			data->selectedHex = nullptr;
			data->selectedHexToHoverHexPath.len = 0;
		} else if (data->hoverHex->unitData) {
			Logf("Selected hex (%i, %i)", data->hoverHex->c, data->hoverHex->r);
			data->selectedHex = data->hoverHex;
			BuildPathMap(
				data->hexes,
				data->selectedHex,
				data->selectedHex->unitData->move,
				data->selectedHex->unitData->side,
				&data->selectedHexPathMap
			);
			BuildAttackableMap(data->hexes, &data->selectedHexPathMap, data->selectedHex->unitData->side, 1, data->selectedHexAttackable);
			data->selectedHexToHoverHexPath.len = 0;
		} else {
			Logf("Ignoring click on empty hex (%i, %i)", data->hoverHex->c, data->hoverHex->r);
		}
		return Ok();
	}
	/*
		if (clickedMapTile->unit) {
			selectedMapTile = clickedMapTile;
			Logf("Selected map tile (%i, %i) with %s unit %p", clickedHexPos.col, clickedHexPos.row, clickedMapTile->unit->def->name, clickedMapTile->unit);
		} else {
			Logf("Ignoring click on empty map tile");
		}
		return;
	}

	if (state == State::UnitSelected) {
		if (clickedMapTile == selectedMapTile) {
			Logf("Deselected map tile");
			selectedMapTile = nullptr;
			return;
		}
		/*
		if (!clickedMapTile->unit) {
			MoveRequest(selectedMapTile, clickedMapTile);
			Vec2 const targetUnitPos  = clickedMapTile->unit->pos;
			Vec2 const targetUnitSize = Math::Scale(clickedMapTile->unit->unitDef->size, 0.5f);

			order.orderType = OrderType::Attack;
			order.attackOrder.attackOrderState = AttackOrderState::MovingTo;
			order.attackOrder.elapsedSecs      = 0.f;
			order.attackOrder.durSecs          = 0.5f;
			order.attackOrder.unitMapTile      = selectedMapTile;
			order.attackOrder.unit             = selectedUnit;
			order.attackOrder.targetUnit       = clickedHex->unit;
			bool intersected = Math::IntersectLineSegmentAabb(selectedUnit->pos, targetUnitPos, Math::Sub(targetUnitPos, targetUnitSize), Math::Add(targetUnitPos, targetUnitSize), &order.attackOrder.targetPos);
			Assert(intersected);
			if (!intersected) {
				order.attackOrder.targetPos = targetUnitPos;
			}

			state = State::ExecutingOrder;

			Logf("Executing attack order");
		}
	}
	*/
	return Ok();
}

//--------------------------------------------------------------------------------------------------


Res<> HandleActions(Data* data, F32 sec, Span<U64 const> actionIds) {
	F32 cameraDX = 0.f;
	F32 cameraDY = 0.f;

	for (U64 i = 0; i < actionIds.len; i++) {
		switch (actionIds[i]) {
			case ActionId_Exit: return App::Err_Exit();

			case ActionId_Click: return Click(data);

			case ActionId_ZoomIn:  ZoomCamera(data, 1.f); break;
			case ActionId_ZoomOut: ZoomCamera(data, -1.f); break;

			case ActionId_ScrollMapLeft:  cameraDX--; break;
			case ActionId_ScrollMapRight: cameraDX++; break;
			case ActionId_ScrollMapUp:    cameraDY--; break;
			case ActionId_ScrollMapDown:  cameraDY++; break;
		}
	}

	MoveCamera(data, sec, cameraDX, cameraDY);

	return Ok();
}
//--------------------------------------------------------------------------------------------------

}	// namespace JC::Battle