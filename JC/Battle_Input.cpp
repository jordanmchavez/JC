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
	ActionId_Click,
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
	Input::Bind(bindingSet, Key::Key::Mouse1,         Input::BindingType::OnKeyUp,    ActionId_Click);
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

static bool Click(Data* data) { 
//	selected
//		lclick attackable: target = hover, lock path
//		click attackable border: target = enemy, lock path
//		rclick: selected = nullptr
//	
//	target selected
//		lclick selected: selected = nullptr
//		lclick target: execute order
//		lclick empty+reachable: target = hover
//		lclick friendly: selected = hover
//		lclick attackable: target = hover, lock path
//		lclick attackable border: target = enemy, lock path
//		lclick else: target = nullptr
//		click anywhere else
//			click selected: selected = nullptr
//		rclick: seleced = nullptr

	if (!data->hoverHex) {
		return false;
	}

	if (data->hoverHex == data->selectedHex) {
		data->selectedHex = nullptr;
		data->selectedPath.len = 0;
		data->selectedLastEmptyHex = nullptr;
		return true;
	}

	if (data->hoverHex->unit && data->hoverHex->unit->side == data->activeSide) {
		data->selectedHex = data->hoverHex;
		data->selectedPath.len = 0;
		data->selectedLastEmptyHex = nullptr;
		return true;
	}

	return false;
}

//--------------------------------------------------------------------------------------------------

static void AddPathFlags(Hex* fromHex, Hex* toHex) {
	if (fromHex->neighbors[NeighborIdx::TopLeft] == toHex) {
		fromHex->flags |= HexFlags::PathTopLeft;
		toHex->flags   |= HexFlags::PathBottomRight;
	} else if (fromHex->neighbors[NeighborIdx::TopRight] == toHex) {
		fromHex->flags |= HexFlags::PathTopRight;
		toHex->flags   |= HexFlags::PathBottomLeft;
	} else if (fromHex->neighbors[NeighborIdx::Right] == toHex) {
		fromHex->flags |= HexFlags::PathRight;
		toHex->flags   |= HexFlags::PathLeft;
	} else if (fromHex->neighbors[NeighborIdx::BottomRight] == toHex) {
		fromHex->flags |= HexFlags::PathBottomRight;
		toHex->flags   |= HexFlags::PathTopLeft;
	} else if (fromHex->neighbors[NeighborIdx::BottomLeft] == toHex) {
		fromHex->flags |= HexFlags::PathBottomLeft;
		toHex->flags   |= HexFlags::PathTopRight;
	} else if (fromHex->neighbors[NeighborIdx::Left] == toHex) {
		fromHex->flags |= HexFlags::PathLeft;
		toHex->flags   |= HexFlags::PathRight;
	} else {
		Panic("Hexes (%u, %u) and (%u, %u) are not adjacent!", fromHex->c, fromHex->r, toHex->c, toHex->r);
	}
}

//--------------------------------------------------------------------------------------------------

static void AddAttackFlags(Hex* fromHex, Hex* toHex) {
	if (fromHex->neighbors[NeighborIdx::TopLeft] == toHex) {
			fromHex->flags |= HexFlags::AttackTopLeft | HexFlags::PathTopLeft;
	} else if (fromHex->neighbors[NeighborIdx::TopRight] == toHex) {
		fromHex->flags |= HexFlags::AttackTopRight | HexFlags::PathTopRight;
	} else if (fromHex->neighbors[NeighborIdx::Right] == toHex) {
		fromHex->flags |= HexFlags::AttackRight | HexFlags::PathRight;
	} else if (fromHex->neighbors[NeighborIdx::BottomRight] == toHex) {
		fromHex->flags |= HexFlags::AttackBottomRight | HexFlags::PathBottomRight;
	} else if (fromHex->neighbors[NeighborIdx::BottomLeft] == toHex) {
		fromHex->flags |= HexFlags::AttackBottomLeft | HexFlags::PathBottomLeft;
	} else if (fromHex->neighbors[NeighborIdx::Left] == toHex) {
		fromHex->flags |= HexFlags::AttackLeft | HexFlags::PathLeft;
	} else {
		Panic("Hexes (%u, %u) and (%u, %u) are not adjacent!", fromHex->c, fromHex->r, toHex->c, toHex->r);
	}
}

//--------------------------------------------------------------------------------------------------

Res<> HandleInput(Data* data, F32 sec, U32 mouseX, U32 mouseY, Span<U64 const> actionIds) {

	F32 cameraDX = 0.f;
	F32 cameraDY = 0.f;

	bool rebuildHexFlagsAndPath = false;
	bool oldShowEnemyAttackers = data->showEnemyAttackers;
	data->showEnemyAttackers = false;
	for (U64 i = 0; i < actionIds.len; i++) {
		switch (actionIds[i]) {
			case ActionId_Exit: return App::Err_Exit();

			case ActionId_Click: rebuildHexFlagsAndPath = Click(data); break;

			case ActionId_ZoomIn:  ZoomCamera(data, 1.f); break;
			case ActionId_ZoomOut: ZoomCamera(data, -1.f); break;

			case ActionId_ScrollMapLeft:  cameraDX--; break;
			case ActionId_ScrollMapRight: cameraDX++; break;
			case ActionId_ScrollMapUp:    cameraDY--; break;
			case ActionId_ScrollMapDown:  cameraDY++; break;

			case ActionId_ShowEnemyOverlay: data->showEnemyAttackers = true; break;
		}
	}

	MoveCamera(data, sec, cameraDX, cameraDY);

	if (oldShowEnemyAttackers != data->showEnemyAttackers) {
		rebuildHexFlagsAndPath = true;
	}

	Hex* const oldHoverHex = data->hoverHex;
	data->hoverHex = ScreenPosToHex(data, mouseX, mouseY);
	if (oldHoverHex != data->hoverHex) {
		rebuildHexFlagsAndPath = true;
	}

	if (!rebuildHexFlagsAndPath) {
		return Ok();
	}

	data->selectedPath.len = 0;

	Army const* const friendlyArmy = &data->armies[data->activeSide];
	Army const* const enemyArmy    = &data->armies[1 - data->activeSide];

	Unit const* const hoverUnit    = data->hoverHex    ? data->hoverHex->unit    : nullptr;
	Unit const* const selectedUnit = data->selectedHex ? data->selectedHex->unit : nullptr;

	U64 hoverUnitIdx    = hoverUnit    ? (U64)(hoverUnit    - data->armies[hoverUnit->side   ].units) : 0;
	U64 selectedUnitIdx = selectedUnit ? (U64)(selectedUnit - data->armies[selectedUnit->side].units) : 0;

	for (U32 i = 0; i < MaxCols * MaxRows; i++) {
		Hex* const hex = &data->hexes[i];
		U64 flags = 0;
		if (
			!selectedUnit &&
			hoverUnit &&
			hoverUnit->side == data->activeSide &&
			(!hex->unit || hex->unit->side == enemyArmy->side) &&
			hoverUnit->pathMap.moveCosts[i] != U32Max
		) {
			flags |= HexFlags::FriendlyMoveable;
		};

		if (
			selectedUnit &&
			(!hex->unit || hex->unit->side == enemyArmy->side) &&
			selectedUnit->pathMap.moveCosts[i] != U32Max
		) {
			flags |= HexFlags::FriendlyMoveable;
		}

		if (
			!selectedUnit &&
			hoverUnit &&
			hoverUnit->side == friendlyArmy->side &&
			data->hexes[i].unit &&
			data->hexes[i].unit->side == enemyArmy->side &&
			(friendlyArmy->attackMap[i] & ((U64)1 << hoverUnitIdx))
		) {
			flags |= HexFlags::FriendlyAttackable;
		}

		if (
			selectedUnit &&
			data->hexes[i].unit &&
			data->hexes[i].unit->side == enemyArmy->side &&
			(friendlyArmy->attackMap[i] & ((U64)1 << selectedUnitIdx))
		) {
			//flags |= HexFlags::FriendlyMoveable;
			flags |= HexFlags::FriendlyAttackable;
		}

		if (
			!selectedUnit &&
			hoverUnit &&
			hoverUnit->side != friendlyArmy->side &&
			(enemyArmy->attackMap[i] & ((U64)1 << hoverUnitIdx))
		) {
			flags |= HexFlags::EnemyAttackable;
		}

		if (
			data->showEnemyAttackers &&
			enemyArmy->attackMap[i]
		) {
			flags |= HexFlags::EnemyAttackable;
		}

		
		if (
			data->showEnemyAttackers &&
			data->hoverHex &&
			data->hexes[i].unit &&
			data->hexes[i].unit->side != data->activeSide &&
			enemyArmy->attackMap[data->hoverHex->idx] & ((U64)1 << (U64)(data->hexes[i].unit - enemyArmy->units))
		) {
			flags |= HexFlags::EnemyAttacker;
		}

		hex->flags = flags;
	}


	if (data->hoverHex) {
		data->hoverHex->flags |= HexFlags::Hover;
	}

	if (data->selectedHex) {
		data->selectedHex->flags |= HexFlags::Selected;
	}

	if (selectedUnit) {
		data->selectedPath.len = 0;
		if (data->hoverHex == data->selectedHex) {
			data->selectedLastEmptyHex = nullptr;
		}

		if (data->hoverHex && !hoverUnit) {
			if (selectedUnit->pathMap.moveCosts[data->hoverHex->idx] != U32Max) {
				data->selectedLastEmptyHex = data->hoverHex;
				data->selectedLastEmptyHex->flags |= HexFlags::SelectedMoveEnd;
				FindPath(selectedUnit, data->hoverHex, &data->selectedPath);
			} else {
				data->selectedLastEmptyHex = nullptr;
				data->hoverHex->flags |= HexFlags::SelectedUnreachable;
			}
		}
		if (data->hoverHex && hoverUnit && hoverUnit->side == enemyArmy->side) {
			if (data->selectedLastEmptyHex && AreHexesAdjacent(data->selectedLastEmptyHex, data->hoverHex)) {
				data->selectedLastEmptyHex->flags |= HexFlags::SelectedMoveEnd;
				if (FindPath(selectedUnit, data->selectedLastEmptyHex, &data->selectedPath)) {
					data->hoverHex->flags |= HexFlags::SelectedTarget;
					AddAttackFlags(data->selectedLastEmptyHex, data->hoverHex);
				}
			}
			else if (!data->selectedLastEmptyHex && AreHexesAdjacent(data->selectedHex, data->hoverHex)) {
				if (FindPath(selectedUnit, data->selectedHex, &data->selectedPath)) {
					data->hoverHex->flags |= HexFlags::SelectedTarget;
					AddAttackFlags(data->selectedHex, data->hoverHex);
				}
			}
			else {
				U32 minCost = U32Max;
				Hex* minNeighbor = nullptr;
				for (U32 i = 0; i < 6; i++) {
					Hex* const neighbor = data->hoverHex->neighbors[i];
					if (!neighbor || neighbor->unit) { continue; }
					U32 const cost = selectedUnit->pathMap.moveCosts[neighbor->idx];
					if (cost < minCost) {
						minCost = cost;
						minNeighbor = neighbor;
					}
				}

				if (minNeighbor) {
					if (FindPath(selectedUnit, minNeighbor, &data->selectedPath)) {
						data->hoverHex->flags |= HexFlags::SelectedTarget;
						AddAttackFlags(minNeighbor, data->hoverHex);
						if (minNeighbor != data->selectedHex) {
							data->selectedLastEmptyHex = minNeighbor;
							data->selectedLastEmptyHex->flags |= HexFlags::SelectedMoveEnd;
						}
					}
				}
			}
		}

		Hex* fromHex = data->selectedHex;
		for (U32 i = 0; i < data->selectedPath.len; i++) {
			Hex* toHex = data->selectedPath.hexes[i];
			AddPathFlags(fromHex, toHex);
			fromHex = toHex;
		}
	}

	return Ok();
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Battle