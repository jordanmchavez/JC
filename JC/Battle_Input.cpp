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
/*
static Hex const* GetCloseNeighbor(U32 mouseX, U32 mouseY) {
	mouseX;mouseY;
	return nullptr;
}
*/
//--------------------------------------------------------------------------------------------------
/*
static void UpdateMouse(Data* data, U32 mouseX, U32 mouseY) {
	/*
	if (!data->selectedHex) {
		if (!data->hoverHex || !data->hoverHex->unit) {
			return;
		}
		

	} else {
		Assert(data->selectedPathMap);
		data->selectedPathMap->pathLens.len = 0;
		if (data->hoverHex == data->selectedHex) {
			return;

		} else if (!data->hoverHex->unit && data->selectedMoveMap.moveCosts[data->hoverHex->idx]) {
			FindPath(&data->selectedMoveMap, data->selectedHex, data->hoverHex, &data->selectedPath);
		}
//		hover empty+reachable: show path
//		hover attackable: show shortest attack path
//		hover attackble border: show shortest attack going through border hex
//      hover else: clear path
	}

//	
//	target selected
//		always: show movable+attackable overlay
//		always: show move/attack path
//		rclick: seleced = nullptr

	/*
	if (
		!data->selected ||
		!data->hoverHex ||
		(data->hoverHex->unit && data->hoverHex->unit->side == data->selected->unit->side)
	) {
		data->path.len = 0;
		data->target = nullptr;
		return;
	}

	if (!data->hoverHex->unit) {
		if (!data->selectedPathMap.moveCosts[data->hoverHex->idx]) {
			data->path.len = 0;
			data->target = nullptr;
			return;
		}

		// Check if mouse is near border of attackable enemy: player wants attack to go through `hoverHex`
		// Find shortest path ending on `hoverHex`
		Hex const* const closeNeighbor = GetCloseNeighbor(mouseX, mouseY);
		if (
			closeNeighbor &&
			closeNeighbor->unit &&
			closeNeighbor->unit->side != data->selected->unit->side
		) {
			data->target = closeNeighbor;
		} else {
			data->target = data->hoverHex;
		}

	// Hover has enemy -> find closest empty+reachable neighbor
	} else {
		data->target   = nullptr;
		U32 minCost    = U32Max;
		U32 minPathLen = U32Max;
		for (U32 i = 0; i < 6; i++) {
			Hex const* const neighbor = data->hoverHex->neighbors[i];
			if (neighbor == data->selected) {
				data->target = data->selected;
				break;
			}
			if (!neighbor || neighbor->unit) { continue; }
			U32 const idx   = neighbor->idx;
			U32 const cost  = data->selectedPathMap.moveCosts[idx];
			if (cost == 0) { continue; }
			U32 const pathLen = data->selectedPathMap.pathLens[idx];
			if (cost < minCost) {
				data->target = neighbor;
				minCost     = cost;
				minPathLen  = pathLen;
			} else if (cost == minCost && pathLen < minPathLen) {
				data->target = neighbor;
				minCost     = cost;
				minPathLen  = pathLen;
			}
		}
		if (!data->target) {
			data->path.len = 0;
			data->target = nullptr;
		}
	}

	bool const foundPath = FindPathFromMoveCostMap(&data->selectedPathMap, data->selected, data->target, &data->path);
	Assert(foundPath);
	StrBuf sb(tempMem);
	sb.Add("path=[");
	for (U32 i = 0; i < data->path.len; i++) {
		sb.Printf("(%i, %i), ", data->path.hexes[i]->c, data->path.hexes[i]->r);
	}
	if (data->path.len > 0) {
		sb.Remove(2);
	}
	sb.Add(']');
	Logf("path=%s", sb.ToStr());
}
	*/

//--------------------------------------------------------------------------------------------------

// returns rebuild flags
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
		return true;
	} else if (data->hoverHex->unit && data->hoverHex->unit->side == data->activeSide) {
		data->selectedHex = data->hoverHex;
		data->selectedPath.len = 0;
		return true;
	}

	return false;


/*
	if (data->state == State::WaitingOrder) {
		if (!data->selected) {
			if (!data->hoverHex->unit || data->hoverHex->unit->side != data->activeSide) {
				Logf("Ignoring click on %s hex (%i, %i)", data->hoverHex->unit ? "friendly" : "empty", data->hoverHex->c, data->hoverHex->r);
				return Ok();
			}
			Logf("Selected hex (%i, %i)", data->hoverHex->c, data->hoverHex->r);
			data->selected = data->hoverHex;
			BuildMoveMap(
				data->hexes,
				data->selected,
				data->selected->unit->move,
				data->selected->unit->side,
				&data->selectedMoveMap
			);
			BuildAttackMap(
				data->hexes,
				&data->selectedMoveMap,
				&data->armies[1 - data->selected->unit->side],
				1,
				data->selectedCanAttack
			);
			data->path.len = 0;
		} else {
		}

		if (data->selected == data->hoverHex) {
			Logf("Deselected hex (%i,%i)", data->selected->c, data->selected->r);
			data->selected = nullptr;
			data->plan.path.len = 0;
			memset(data->selectedHexAttackable, 0, sizeof(data->selectedHexAttackable));
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
}

//--------------------------------------------------------------------------------------------------


Res<> HandleInput(Data* data, F32 sec, U32 mouseX, U32 mouseY, Span<U64 const> actionIds) {

	F32 cameraDX = 0.f;
	F32 cameraDY = 0.f;

	bool rebuildHexFlags = false;
	bool oldShowEnemyAttackers = data->showEnemyAttackers;
	data->showEnemyAttackers = false;
	for (U64 i = 0; i < actionIds.len; i++) {
		switch (actionIds[i]) {
			case ActionId_Exit: return App::Err_Exit();

			case ActionId_Click: rebuildHexFlags = Click(data); break;

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
		rebuildHexFlags = true;
	}

	Hex const* const oldEndHex = data->hoverHex;
	data->hoverHex = ScreenPosToHex(data, mouseX, mouseY);
	if (oldEndHex != data->hoverHex) {
		rebuildHexFlags = true;
	}

	if (!rebuildHexFlags) {
		return Ok();
	}

	Army const* const friendlyArmy = &data->armies[data->activeSide];
	Army const* const enemyArmy    = &data->armies[1 - data->activeSide];

	Unit const* const hoverUnit    = data->hoverHex    ? data->hoverHex->unit    : nullptr;
	Unit const* const selectedUnit = data->selectedHex ? data->selectedHex->unit : nullptr;

	U64 hoverUnitIdx    = hoverUnit    ? (U64)(hoverUnit    - data->armies[hoverUnit->side   ].units) : 0;
	U64 selectedUnitIdx = selectedUnit ? (U64)(selectedUnit - data->armies[selectedUnit->side].units) : 0;

	memset(data->hexFlags, 0, sizeof(data->hexFlags));
	for (U32 i = 0; i < MaxCols * MaxRows; i++) {
		Hex const* const hex = &data->hexes[i];
		U64 flags = 0;
		if (
			!selectedUnit &&
			hoverUnit &&
			hoverUnit->side == data->activeSide &&
			(!hex->unit || hex->unit->side == enemyArmy->side) &&
			hoverUnit->pathMap.moveCosts[i] != U32Max
		) {
			flags |= HexFlags::FriendlyMovable;
		};

		if (
			selectedUnit &&
			(!hex->unit || hex->unit->side == enemyArmy->side) &&
			selectedUnit->pathMap.moveCosts[i] != U32Max
		) {
			flags |= HexFlags::FriendlyMovable;
		}

		if (
			!selectedUnit &&
			hoverUnit &&
			hoverUnit->side == friendlyArmy->side &&
			data->hexes[i].unit &&
			data->hexes[i].unit->side == enemyArmy->side &&
			(friendlyArmy->attackMap[i] & ((U64)1 << hoverUnitIdx))
		) {
			flags |= HexFlags::FriendlyMovable;
			//flags |= HexFlags::FriendlyTargetable;
		}

		if (
			selectedUnit &&
			data->hexes[i].unit &&
			data->hexes[i].unit->side == enemyArmy->side &&
			(friendlyArmy->attackMap[i] & ((U64)1 << selectedUnitIdx))
		) {
			flags |= HexFlags::FriendlyMovable;
			//flags |= HexFlags::FriendlyTargetable;
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

		data->hexFlags[i] = flags;
	}

	if (selectedUnit) {
		data->selectedPath.len = 0;
		if (data->hoverHex && (!hoverUnit || hoverUnit->side == enemyArmy->side)) {
			if (FindPath(selectedUnit, data->hoverHex, &data->selectedPath)) {
				Logf("path from (%u, %u) -> (%u, %u)", selectedUnit->hex->c, selectedUnit->hex->r, data->hoverHex->c, data->hoverHex->r);
			} else {
				Logf("no path");
			}
		}
	}

	for (U32 i = 0; i < data->selectedPath.len; i++) {
	}

	return Ok();
}
//--------------------------------------------------------------------------------------------------

}	// namespace JC::Battle