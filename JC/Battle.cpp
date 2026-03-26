#include "JC/Common.h"

#include "JC/App.h"
#include "JC/Battle_Map.h"
#include "JC/Draw.h"
#include "JC/Input.h"
#include "JC/Key.h"
#include "JC/Unit.h"
#include "JC/Window.h"

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

using Side = U32;
static constexpr Side Side_Friendly = 0;
static constexpr Side Side_Enemy    = 1;

static constexpr U32 MaxArmyUnits = 32;

static constexpr F32 Z_Map  = 1.f;
static constexpr F32 Z_Unit = 2.f;

static Vec2                        windowSize;
static Draw::Camera                camera;
static Input::BindingSet           bindingSet;
static Array<Unit::Unit>           units[2];
static Array<Draw::DrawSpriteDesc> unitDrawDescs[2];

//--------------------------------------------------------------------------------------------------

void Init(Mem permMem, Mem tempMem, Window::State const* windowState) {
	units[Side_Friendly].Init(permMem, MaxArmyUnits);
	units[Side_Enemy].Init(permMem, MaxArmyUnits);
	unitDrawDescs[Side_Friendly].Init(permMem, MaxArmyUnits);
	unitDrawDescs[Side_Enemy].Init(permMem, MaxArmyUnits);

	windowSize  = Vec2((F32)windowState->width, (F32)windowState->height);

	camera.pos   = { 0.f, 0.f };
	camera.scale = 3.f;

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

	Battle::Map::Init(permMem, tempMem, Z_Map);
}

//--------------------------------------------------------------------------------------------------

static void RebuildUnitDrawDescs() {
	for (U64 i = 0; i < units[Side_Friendly].len; i++) {
		Unit::Unit const* unit = &units[Side_Friendly][i];
		unitDrawDescs[Side_Friendly][i] = {
			.sprite = unit->sprite,
			.pos    = unit->pos,
			.z      = Z_Unit,
			.flip   = false,

		};
	}
	for (U64 i = 0; i < units[Side_Friendly].len; i++) {
		Unit::Unit const* unit = &units[Side_Friendly][i];
		unitDrawDescs[Side_Friendly][i] = {
			.sprite = unit->sprite,
			.pos    = unit->pos,
			.z      = Z_Unit,
			.flip   = true,

		};
	}
}

//--------------------------------------------------------------------------------------------------

void GenerateRandomArmies() {
	
	RebuildUnitDrawDescs();
}

//--------------------------------------------------------------------------------------------------

static void ZoomCamera(F32 d) {
	F32 const oldScale = camera.scale;
	if (camera.scale + d >= 1.f) {
		camera.scale += d;
	}
	F32 const windowCenterX = windowSize.x * 0.5f;
	F32 const windowCenterY = windowSize.y * 0.5f;
	camera.pos.x -= windowCenterX / camera.scale - windowCenterX / oldScale;
	camera.pos.y -= windowCenterY / camera.scale - windowCenterY / oldScale;
}

//--------------------------------------------------------------------------------------------------

static constexpr F32 CameraSpeedPixelsPerSec = 1000.f;

Res<> Update(App::UpdateData const* appUpdateData) {
	F32 cameraDx = 0.f;
	F32 cameraDy = 0.f;

	bool showEnemyArmyThreatMap = 0;
	for (U64 i = 0; i < appUpdateData->actions.len; i++) {
		Input::Action const action = appUpdateData->actions[i];
		switch (action.id) {
			case ActionId_Exit: return App::Err_Exit();

			case ActionId_LClick: {
				//ScreenPosToHex(action.mouseX, action.mouseY);
				break;
			}
			case ActionId_RClick: {
				//ScreenPosToHex(action.mouseX, action.mouseY);
				break;
			}

			case ActionId_ZoomIn:  ZoomCamera( 1.f); break;
			case ActionId_ZoomOut: ZoomCamera(-1.f); break;

			case ActionId_ScrollMapLeft:  cameraDx--; break;
			case ActionId_ScrollMapRight: cameraDx++; break;
			case ActionId_ScrollMapUp:    cameraDy--; break;
			case ActionId_ScrollMapDown:  cameraDy++; break;

			case ActionId_ShowEnemyArmyThreatMap: showEnemyArmyThreatMap = true; break;

			//case ActionId_NextUnit: SelectNextUnit(); break;

			//case ActionId_EndUnitTurn: EndSelectedUnitTurn(); break;
		}
	}

	camera.pos.x += cameraDx * CameraSpeedPixelsPerSec * appUpdateData->sec / camera.scale;
	camera.pos.y += cameraDy * CameraSpeedPixelsPerSec * appUpdateData->sec / camera.scale;

	return Ok();
}

//--------------------------------------------------------------------------------------------------

void Draw() {
	Draw::SetCamera(camera);
	Map::Draw();
	Draw::DrawSprites(unitDrawDescs[Side_Friendly]);
	Draw::DrawSprites(unitDrawDescs[Side_Enemy]);
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Battle