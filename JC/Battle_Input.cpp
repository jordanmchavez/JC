#include "JC/Battle_Internal.h"

#include "JC/Input.h"
#include "JC/Key.h"

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
	Input::Bind(bindingSet, Key::Key::W,              Input::BindingType::Continuous, ActionId_ScrollMapUp);
	Input::Bind(bindingSet, Key::Key::S,              Input::BindingType::Continuous, ActionId_ScrollMapDown);
	Input::Bind(bindingSet, Key::Key::A,              Input::BindingType::Continuous, ActionId_ScrollMapLeft);
	Input::Bind(bindingSet, Key::Key::D,              Input::BindingType::Continuous, ActionId_ScrollMapRight);
	Input::SetBindingSetStack({ bindingSet });

}

static Res<> Action_Exit(F32) { return App::Err_Exit(); };

//--------------------------------------------------------------------------------------------------

static Res<> Action_Click(F32) { 
	if (!mouseHex) {
		return Ok();
	}

	if (state == State::None) {
		if (selectedHex == mouseHex) {
			Logf("Deselected hex (%i,%i)", selectedHex->c, selectedHex->r);
			selectedHex = nullptr;
		} else if (mouseHex->unit) {
			Logf("Selected hex (%i, %i)", mouseHex->c, mouseHex->r);
			selectedHex = mouseHex;
			BuildMoveCostMap(selectedHex, selectedHex->unit->unitDef->move, selectedHex->unit->side, &selectedHexMoveCostMap);
		} else {
			Logf("Ignoring click on empty hex (%i, %i)", mouseHex->c, mouseHex->r);
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

static Res<> Action_Zoom(F32 scaleDelta) {
	F32 const oldScale = camera.scale;
	if (camera.scale + scaleDelta >= 1.f) {
		camera.scale += scaleDelta;
	}
	F32 const windowCenterX = windowSize.x * 0.5f;
	F32 const windowCenterY = windowSize.y * 0.5f;
	camera.pos.x -= windowCenterX / camera.scale - windowCenterX / oldScale;
	camera.pos.y -= windowCenterY / camera.scale - windowCenterY / oldScale;
	Logf("camera.scale = %f", camera.scale);
	return Ok();
}
static Res<> Action_ZoomIn(F32)  { return Action_Zoom(1.f); }
static Res<> Action_ZoomOut(F32) { return Action_Zoom(-1.f); }

//--------------------------------------------------------------------------------------------------

static Res<> Action_ScrollMapLeft (F32 sec) { camera.pos.x -= (CamSpeedPixelsPerSec * sec) / camera.scale; return Ok(); }
static Res<> Action_ScrollMapRight(F32 sec) { camera.pos.x += (CamSpeedPixelsPerSec * sec) / camera.scale; return Ok(); }
static Res<> Action_ScrollMapUp   (F32 sec) { camera.pos.y -= (CamSpeedPixelsPerSec * sec) / camera.scale; return Ok(); }
static Res<> Action_ScrollMapDown (F32 sec) { camera.pos.y += (CamSpeedPixelsPerSec * sec) / camera.scale; return Ok(); }


//--------------------------------------------------------------------------------------------------

}	// namespace JC::Battle