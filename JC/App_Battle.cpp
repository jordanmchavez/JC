#pragma warning(disable: 4505)
#pragma warning(disable: 4189)

#include "JC/App.h"
#include "JC/Cfg.h"
#include "JC/Draw.h"
#include "JC/Effect.h"
#include "JC/File.h"
#include "JC/Gpu.h"
#include "JC/Input.h"
#include "JC/Key.h"
#include "JC/Log.h"
#include "JC/Math.h"
#include "JC/Rng.h"
#include "JC/Time.h"
#include "JC/Window.h"

namespace JC::Battle {

//--------------------------------------------------------------------------------------------------

static constexpr I16  HexSize = 32;
static constexpr I32  MapCols = 16;
static constexpr I32  MapRows = 14;
static constexpr U32  MaxUnitDefs = 64;
static constexpr U32  MaxUnits = 256;
static constexpr U32  MaxFonts = 1024;
static constexpr Vec4 MapBackgroundColor = Vec4(13.f/255.f, 30.f/255.f, 22.f/255.f, 1.f);
static constexpr Vec4 SelectedColor = Vec4(0.f, 1.f, 0.f, 1.f);
static constexpr Vec4 HoverColor = Vec4(1.f, 1.f, 1.f, 0.75f);
static constexpr F32  UiPanelWidth = 500.f;
static constexpr Vec4 UiBackgroundColor = Vec4(0.2f, 0.3f, 0.4f, 1.f);
static constexpr F32  CamSpeedPixelsPerSec = 1000.f;

static constexpr F32 Z_Background   = 0.f;
static constexpr F32 Z_Map          = 1.f;
static constexpr F32 Z_MapHighlight = 2.f;
static constexpr F32 Z_Unit         = 3.f;
static constexpr F32 Z_UnitSelected = 4.f;
static constexpr F32 Z_UiBackground = 5.f;
static constexpr F32 Z_Ui           = 6.f;

static constexpr U64 Action_Exit            = 1;
static constexpr U64 Action_Click           = 2;
static constexpr U64 Action_ZoomIn          = 3;
static constexpr U64 Action_ZoomOut         = 4;
static constexpr U64 Action_ScrollMapLeft   = 5;
static constexpr U64 Action_ScrollMapRight  = 6;
static constexpr U64 Action_ScrollMapUp     = 7;
static constexpr U64 Action_ScrollMapDown   = 8;

struct HexCoord {
	I16 x = 0;
	I16 y = 0;
};

struct MapCoord {
	I16 col = 0;
	I16 row = 0;
};

static constexpr U32 MaxSpriteAnimationFrames = 16;
struct AnimationDef {
	Draw::Sprite frameSprites[MaxSpriteAnimationFrames];
	F32          frameDurSecs[MaxSpriteAnimationFrames];
	U32          frameLen = 0;
};

struct UnitDef {
	Draw::Sprite sprite;
	Vec2         size;
	U32          maxHp = 0;
	AnimationDef attackAnimationDef;
};

struct Unit {
	U64                 id = 0;
	bool                alive = false;
	UnitDef const*      unitDef = nullptr;
	Vec2                pos;
	F32                 z = Z_Unit;
	U32                 hp = 0;
	AnimationDef const* activeAnimationDef;
	U16                 animationFrame = 0;
	F32                 animationFrameElapsedSecs = 0.f;
};

enum struct TerrainType {
	Invalid  = 0,
	Grass,
	Forest,
	Mountain,
	Hill,
	Swamp,
	Water,
	Max,
};

struct TerrainFeature {
	Draw::Sprite sprite;
	HexCoord     hexCoord;
};

struct MapTile {
	MapCoord       mapCoord;
	TerrainType    terrainType = TerrainType::Invalid;
	Unit*          unit = nullptr;
};

enum struct State {
	WaitingOrder,
	ExecutingOrder,
};

enum struct OrderType {
	Invalid = 0,
	Move,
	Attack,
};

enum struct AttackOrderState {
	Invalid = 0,
	MovingTo,
	Attacking,
	MovingBack,
};

struct MoveOrder {
	F32      elapsedSecs = 0.f;
	F32      durSecs = 0.f;
	Unit*    unit = nullptr;
	MapTile* startMapTile = nullptr;
	MapTile* endMapTile = nullptr;
};

struct AttackOrder {
	AttackOrderState attackOrderState = AttackOrderState::Invalid;
	F32              elapsedSecs = 0.f;
	F32              durSecs = 0.f;
	MapTile*         unitMapTile = nullptr;
	Unit*            unit = nullptr;
	Unit*            targetUnit = nullptr;
	Vec2             targetPos;
};

struct Order {
	OrderType   orderType;
	MoveOrder   moveOrder;
	AttackOrder attackOrder;
};

static Mem               permMem;
static Mem               tempMem;
static U8                hexLut[(HexSize / 2) * (HexSize * 3 / 8)];
static HexCoord          hexCoordLut[(HexSize / 2) * (HexSize / 2)];
static U32               hexCoordLutLen;
static MapTile           mapTiles[MapCols * MapRows];
static MapCoord          mouseMapCoord;
static UnitDef           unitDefs[MaxUnitDefs];
static U32               unitDefsLen;
static U64               nextUnitId = 1;
static UnitDef*          spearmenUnitDef;
static Unit              units[MaxUnits];
static U32               unitsLen;
static State             state;
static Draw::Sprite      hexBorderSprite;
static Draw::Sprite      terrainSprites[(U32)TerrainType::Max];
static Unit*             selectedUnit;
static MapTile*          selectedMapTile;
static MapTile*          hoverMapTile;
static Order             order;
static Vec2              windowSize;
static Input::BindingSet mainBindingSet;
static Draw::Font        numberFont;
static Draw::Font        uiFont;
static F32               uiFontScale = 2.f;
static F32               uiFontLineHeight;
static Draw::Font        fancyFont;
static F32               fancyFontScale = 2.f;
static Draw::Camera      camera;


//--------------------------------------------------------------------------------------------------

static void InitLuts() {
	memset(hexLut, 0, sizeof(hexLut));
	U8 start = (HexSize / 2) - 1;
	U8 end   = HexSize / 2;
	for (U8 y = 0; y < HexSize / 4; y++) {
		for (U8 x = 0; x < start; x++) {
			hexLut[((y / 2) * (HexSize / 2)) + (x / 2)] |= (1 << ((x & 1) * 4));
		}
		for (U8 x = end + 1; x < HexSize; x++) {
			hexLut[((y / 2) * (HexSize / 2)) + (x / 2)] |= (2 << ((x & 1) * 4));
		}
		start -= 2;
		end   += 2;
	}

	hexCoordLutLen = 0;
	I16 xStart = (HexSize / 2) - 1;
	I16 y = 0;
	for (; y <= (HexSize / 4) - 1; y++) {
		for (I16 x = xStart; x < (HexSize / 2); x++) {
			hexCoordLut[hexCoordLutLen++] = {
				.x = x,
				.y = y,
			};
		}
		xStart -= 2;
	}
	for (; y < (HexSize / 2); y++) {
		for (I16 x = 0; x < (HexSize / 2); x++) {
			hexCoordLut[hexCoordLutLen++] = {
				.x = x,
				.y = y,
			};
		}
	}
}

static HexCoord RngHexCoord() {
	HexCoord hc = hexCoordLut[Rng::NextU32(0, hexCoordLutLen)];
	const U32 flip = Rng::NextU32(0, 4);
    if (flip & 1) { hc.x = HexSize - hc.x; }
    if (flip & 2) { hc.y = HexSize - hc.y; }
	return hc;
}

//--------------------------------------------------------------------------------------------------

static Vec2 MapCoordToCenterScreenPos(MapCoord mc) {
	return {
		(F32)((HexSize / 2) + mc.col * HexSize + (mc.row & 1) * (HexSize / 2)),
		(F32)((HexSize / 2) + mc.row * (HexSize * 3 / 4)),
	};
}

//---------------------------------------------------------------------------------------------

static MapCoord ScreenPosToMapCoord(Vec2 p) {
	I32 iy = (I32)p.y;
	I32 row = iy / (HexSize * 3 / 4);
	const U8 parity = (row & 1);

	I32 ix = (I32)p.x - parity * (HexSize / 2);
	I32 col = ix / HexSize;
	if (ix < 0 || iy < 0) {
		return MapCoord { .col = -1, .row = -1 };
	}

	I32 lx = ix - (col * HexSize);
	I32 ly = iy - (row * (HexSize * 3 / 4));
	Assert(lx >= 0 && ly >= 0);

	const U8 l = (hexLut[(ly / 2) * (HexSize / 2) + (lx / 2)] >> ((lx & 1) * 4)) & 0xf;
	switch (l) {
		case 1: col -= 1 * (1 - parity); row -= 1; break;
		case 2: col += 1 * parity;       row -= 1; break;
	}
	return MapCoord { .col = (I16)col, .row = (I16)row };
}


//--------------------------------------------------------------------------------------------------

static Unit* AllocUnit() {
	Assert(unitsLen < MaxUnits);
	Unit* const unit = &units[unitsLen++];
	unit->id = nextUnitId;
	nextUnitId++;
	return unit;
}

//--------------------------------------------------------------------------------------------------

Res<> PreInit(Mem permMemIn, Mem tempMemIn) {
	permMem = permMemIn;
	tempMem = tempMemIn;
	Cfg::SetStr(App::Cfg_Title, "4x Fantasy");
	Cfg::SetU32(App::Cfg_WindowWidth, 1920);
	Cfg::SetU32(App::Cfg_WindowHeight, 1080);
	return Ok();
}

//--------------------------------------------------------------------------------------------------

Res<> Init(Window::State const* windowState) {
	InitLuts();

	windowSize = Vec2((F32)windowState->width, (F32)windowState->height);

	camera.pos = { 0.f, 0.f };
	camera.scale = 3.f;

	Try(Draw::LoadSprites("Assets/Terrain.png", "Assets/Terrain.spritejson"));
	Try(Draw::LoadSprites("Assets/Units.png", "Assets/Units.spritejson"));

	spearmenUnitDef = &unitDefs[unitDefsLen++];
	TryTo(Draw::GetSprite("Unit_Spearmen"), spearmenUnitDef->sprite);

	Draw::Sprite attackSprite; TryTo(Draw::GetSprite("Unit_Spearmen_Attack"), attackSprite);
	spearmenUnitDef->maxHp = 10;
	spearmenUnitDef->size = Draw::GetSpriteSize(spearmenUnitDef->sprite);
	U32 frameLen = 0;
	spearmenUnitDef->attackAnimationDef.frameSprites[frameLen] = attackSprite;
	spearmenUnitDef->attackAnimationDef.frameDurSecs[frameLen] = 0.2f;
	frameLen++;
	spearmenUnitDef->attackAnimationDef.frameSprites[frameLen] = spearmenUnitDef->sprite;
	spearmenUnitDef->attackAnimationDef.frameDurSecs[frameLen] = 0.2f;
	frameLen++;
	spearmenUnitDef->attackAnimationDef.frameSprites[frameLen] = attackSprite;
	spearmenUnitDef->attackAnimationDef.frameDurSecs[frameLen] = 0.2f;
	frameLen++;
	spearmenUnitDef->attackAnimationDef.frameSprites[frameLen] = spearmenUnitDef->sprite;
	spearmenUnitDef->attackAnimationDef.frameDurSecs[frameLen] = 0.2f;
	frameLen++;
	spearmenUnitDef->attackAnimationDef.frameLen = frameLen;

	TryTo(Draw::GetSprite("Hex_Border_32"), hexBorderSprite);
	TryTo(Draw::GetSprite("Hex_Grass_32"), terrainSprites[(U32)TerrainType::Grass]);
	TryTo(Draw::GetSprite("Hex_Forest_32"), terrainSprites[(U32)TerrainType::Forest]);
	TryTo(Draw::GetSprite("Hex_Hill_32"), terrainSprites[(U32)TerrainType::Hill]);
	TryTo(Draw::GetSprite("Hex_Mountain_32"), terrainSprites[(U32)TerrainType::Mountain]);
	TryTo(Draw::GetSprite("Hex_Swamp_32"), terrainSprites[(U32)TerrainType::Swamp]);
	TryTo(Draw::GetSprite("Hex_Water_32"), terrainSprites[(U32)TerrainType::Water]);

	for (I16 c = 0; c < MapCols; c++) {
		for (I16 r = 0; r < MapRows; r++) {
			MapTile* const mapTile = &mapTiles[c + r * MapCols];
			mapTile->mapCoord    = { .col = c, .row = r };
			mapTile->terrainType = (TerrainType)Rng::NextU32(1, (U32)TerrainType::Max);
			if (Rng::NextU32(0, 100) < 50) { continue; }

			Unit* const unit = AllocUnit();
			unit->alive   = true;
			unit->unitDef = spearmenUnitDef;
			unit->pos     = MapCoordToCenterScreenPos(mapTile->mapCoord);
			unit->z       = Z_Unit;
			unit->hp      = spearmenUnitDef->maxHp;
			mapTile->unit = unit;
		}
	}

	TryTo(Draw::LoadFont("Assets/Fonts/8_EverydayStandard.fontjson", "Assets/Fonts/8_EverydayStandard.png"), numberFont);
	TryTo(Draw::LoadFont("Assets/Fonts/10_CelticTime.fontjson",      "Assets/Fonts/10_CelticTime.png"),      uiFont);
	TryTo(Draw::LoadFont("Assets/Fonts/21_OldeTome.fontjson",        "Assets/Fonts/21_OldeTome.png"),        fancyFont);

	uiFontLineHeight = Draw::GetFontLineHeight(uiFont);

	mouseMapCoord = MapCoord(0, 0);

	mainBindingSet = Input::CreateBindingSet("Main");
	Input::Bind(mainBindingSet, Key::Key::Escape,         Input::BindingType::OnKeyDown,  Action_Exit,            "");
	Input::Bind(mainBindingSet, Key::Key::Mouse1,         Input::BindingType::OnKeyUp,    Action_Click,           "");
	Input::Bind(mainBindingSet, Key::Key::MouseWheelUp,   Input::BindingType::OnKeyDown,  Action_ZoomIn,          "");
	Input::Bind(mainBindingSet, Key::Key::MouseWheelDown, Input::BindingType::OnKeyDown,  Action_ZoomOut,         "");
	Input::Bind(mainBindingSet, Key::Key::W,              Input::BindingType::Continuous, Action_ScrollMapUp,     "");
	Input::Bind(mainBindingSet, Key::Key::S,              Input::BindingType::Continuous, Action_ScrollMapDown,   "");
	Input::Bind(mainBindingSet, Key::Key::A,              Input::BindingType::Continuous, Action_ScrollMapLeft,   "");
	Input::Bind(mainBindingSet, Key::Key::D,              Input::BindingType::Continuous, Action_ScrollMapRight,  "");
	Input::SetBindingSetStack({ mainBindingSet });
												          
	state = State::WaitingOrder;

	Try(Gpu::ImmediateWait());

	return Ok();
}

//--------------------------------------------------------------------------------------------------

static void HandleLeftClick() {
	if (mouseMapCoord.col < 0 || mouseMapCoord.col >= MapCols || mouseMapCoord.row < 0 || mouseMapCoord.row >= MapRows) {
		return;
	}

	MapTile* const clickedMapTile = &mapTiles[mouseMapCoord.col + mouseMapCoord.row * MapCols];
	if (state == State::WaitingOrder) {
		if (!selectedUnit) {
			Assert(!selectedMapTile);
			if (clickedMapTile->unit) {
				selectedUnit = clickedMapTile->unit;
				selectedUnit->z = Z_UnitSelected;
				selectedMapTile = clickedMapTile;
				Logf("Selected unit (%i, %i)", clickedMapTile->mapCoord.col, clickedMapTile->mapCoord.row);
			} else {
				Logf("Ignoring click on empty map tile");
			}
			return;
		}

		if (selectedUnit) {
			Assert(selectedMapTile);
			if (clickedMapTile->unit == selectedUnit) {
				Logf("Deselected unit");
				selectedUnit = nullptr;
				selectedMapTile = nullptr;
				return;
			}
			if (clickedMapTile->unit) {
				Vec2 const targetUnitPos  = clickedMapTile->unit->pos;
				Vec2 const targetUnitSize = Math::Scale(clickedMapTile->unit->unitDef->size, 0.5f);

				order.orderType = OrderType::Attack;
				order.attackOrder.attackOrderState = AttackOrderState::MovingTo;
				order.attackOrder.elapsedSecs      = 0.f;
				order.attackOrder.durSecs          = 0.5f;
				order.attackOrder.unitMapTile      = selectedMapTile;
				order.attackOrder.unit             = selectedUnit;
				order.attackOrder.targetUnit       = clickedMapTile->unit;
				bool intersected = Math::IntersectLineSegmentAabb(selectedUnit->pos, targetUnitPos, Math::Sub(targetUnitPos, targetUnitSize), Math::Add(targetUnitPos, targetUnitSize), &order.attackOrder.targetPos);
				Assert(intersected);
				if (!intersected) {
					order.attackOrder.targetPos = targetUnitPos;
				}

				state = State::ExecutingOrder;

				Logf("Executing attack order");

				return;
			}

			selectedMapTile->unit = nullptr;

			order.orderType              = OrderType::Move;
			order.moveOrder.elapsedSecs  = 0.f;
			order.moveOrder.durSecs      = 1.f;
			order.moveOrder.unit         = selectedUnit;
			order.moveOrder.startMapTile = selectedMapTile;
			order.moveOrder.endMapTile   = clickedMapTile;

			selectedMapTile = clickedMapTile;

			state = State::ExecutingOrder;

			Logf("Executing move order from (%i, %i) -> (%i, %i)", order.moveOrder.startMapTile->mapCoord.col, order.moveOrder.startMapTile->mapCoord.row, order.moveOrder.endMapTile->mapCoord.col, order.moveOrder.endMapTile->mapCoord.row);
		}
	}
}

//--------------------------------------------------------------------------------------------------

static void ExecuteMoveOrder(F32 secs) {
	MoveOrder* const moveOrder = &order.moveOrder;
	moveOrder->elapsedSecs += secs;
	if (moveOrder->elapsedSecs < moveOrder->durSecs) {
		Vec2 const startPos = MapCoordToCenterScreenPos(moveOrder->startMapTile->mapCoord);
		Vec2 const endPos   = MapCoordToCenterScreenPos(moveOrder->endMapTile->mapCoord);
		F32 const t         = moveOrder->elapsedSecs / moveOrder->durSecs;
		moveOrder->unit->pos = Math::Lerp(startPos, endPos, t);
	} else {
		moveOrder->endMapTile->unit = moveOrder->unit;
		moveOrder->unit->z = Z_Unit;
		memset(&order, 0, sizeof(order));
		selectedMapTile = nullptr;
		selectedUnit    = nullptr;
		state = State::WaitingOrder;
	}
}

static void ExecuteAttackOrder(F32 secs) {
	AttackOrder* const attackOrder = &order.attackOrder;
	Unit* const unit = attackOrder->unit;
	switch (attackOrder->attackOrderState) {
		case AttackOrderState::MovingTo: {
			attackOrder->elapsedSecs += secs;
			if (attackOrder->elapsedSecs < attackOrder->durSecs) {
				Vec2 const startPos = MapCoordToCenterScreenPos(attackOrder->unitMapTile->mapCoord);
				Vec2 const endPos   = attackOrder->targetPos;
				F32 const t         = attackOrder->elapsedSecs / attackOrder->durSecs;
				unit->pos = Math::Lerp(startPos, endPos, t);
			} else {
				attackOrder->attackOrderState = AttackOrderState::Attacking;
				unit->activeAnimationDef        = &unit->unitDef->attackAnimationDef;
				unit->animationFrame            = 0;
				unit->animationFrameElapsedSecs = 0.f;
				Unit* const targetUnit = attackOrder->targetUnit;
				if (targetUnit->hp > 0) {
					targetUnit->hp--;
				}
				Effect::CreateFloatingStr({
					.font   = numberFont,
					.str    = "-1",
					.durSec = 2.f,
					.x      = targetUnit->pos.x,
					.yStart = targetUnit->pos.y - targetUnit->unitDef->size.y,
					.yEnd   = targetUnit->pos.y - targetUnit->unitDef->size.y * 2,
				});
			}
			break;
		}

		case AttackOrderState::Attacking:
			unit->animationFrameElapsedSecs += secs;
			if (unit->animationFrameElapsedSecs < unit->activeAnimationDef->frameDurSecs[unit->animationFrame]) {
				// nothing
			} else {
				if (unit->animationFrame < unit->activeAnimationDef->frameLen - 1) {
					unit->animationFrame++;
					unit->animationFrameElapsedSecs = 0.f;
				} else {
					unit->activeAnimationDef        = nullptr;
					unit->animationFrame            = 0;
					unit->animationFrameElapsedSecs = 0.f;
					attackOrder->attackOrderState = AttackOrderState::MovingBack;
					attackOrder->elapsedSecs     = 0.f;
					attackOrder->durSecs         = 0.5f;
				}
			}
			break;

		case AttackOrderState::MovingBack: {
			attackOrder->elapsedSecs += secs;
			if (attackOrder->elapsedSecs < attackOrder->durSecs) {
				Vec2 const startPos = attackOrder->targetPos;
				Vec2 const endPos   = MapCoordToCenterScreenPos(attackOrder->unitMapTile->mapCoord);
				F32 const t         = attackOrder->elapsedSecs / attackOrder->durSecs;
				unit->pos = Math::Lerp(startPos, endPos, t);
			} else {
				unit->pos = MapCoordToCenterScreenPos(attackOrder->unitMapTile->mapCoord);
				unit->z = Z_Unit;
				memset(&order, 0, sizeof(order));
				selectedMapTile = nullptr;
				selectedUnit    = nullptr;
				state = State::WaitingOrder;
			}
			break;
		}

		default: Panic("Unhandled AttackOrderState %u", (U32)attackOrder->attackOrderState);
	}
}

//--------------------------------------------------------------------------------------------------

Res<> Frame(App::FrameData const* frameData, Draw::Camera* cameraOut) {
	if (frameData->exit) {
		return App::Err_Exit();
	}

	for (U64 i = 0; i < frameData->actions.len; i++) {
		U64 const actionId = frameData->actions[i];

		switch (actionId) {
			case Action_Exit: return App::Err_Exit();
			case Action_Click: HandleLeftClick(); break;

			case Action_ZoomIn: {
				F32 const oldScale = camera.scale;
				camera.scale += 1.f;
				F32 const windowCenterX = windowSize.x * 0.5f;
				F32 const windowCenterY = windowSize.y * 0.5f;
				camera.pos.x -= windowCenterX / camera.scale - windowCenterX / oldScale;
				camera.pos.y -= windowCenterY / camera.scale - windowCenterY / oldScale;
				Logf("camera.scale = %f", camera.scale);
				break;
			}
			case Action_ZoomOut: {
				if (camera.scale > 1.f) {
					F32 const oldScale = camera.scale;
					camera.scale -= 1.f;
					F32 const windowCenterX = windowSize.x * 0.5f;
					F32 const windowCenterY = windowSize.y * 0.5f;
					camera.pos.x -= windowCenterX / camera.scale - windowCenterX / oldScale;
					camera.pos.y -= windowCenterY / camera.scale - windowCenterY / oldScale;
					Logf("camera.scale = %f", camera.scale);
				}
				break;
			}

			case Action_ScrollMapLeft:  camera.pos.x -= (CamSpeedPixelsPerSec * frameData->secs) / camera.scale; break;
			case Action_ScrollMapRight: camera.pos.x += (CamSpeedPixelsPerSec * frameData->secs) / camera.scale; break;
			case Action_ScrollMapUp:    camera.pos.y -= (CamSpeedPixelsPerSec * frameData->secs) / camera.scale; break;
			case Action_ScrollMapDown:  camera.pos.y += (CamSpeedPixelsPerSec * frameData->secs) / camera.scale; break;

			default: Panic("Unhandled actionId %u", actionId);
		}
	}
	
	Vec2 const mousePos((F32)frameData->mouseX, (F32)frameData->mouseY);

	Vec2 const canvasMousePos = {
		(mousePos.x / camera.scale) + camera.pos.x,
		(mousePos.y / camera.scale) + camera.pos.y,
	};
	mouseMapCoord = ScreenPosToMapCoord(canvasMousePos);
	if (mouseMapCoord.col >= 0 && mouseMapCoord.col < MapCols && mouseMapCoord.row >= 0 && mouseMapCoord.row < MapRows) {
		hoverMapTile = &mapTiles[mouseMapCoord.col + mouseMapCoord.row * MapCols];
	} else {
		hoverMapTile = nullptr;
	}

	if (state == State::ExecutingOrder) {
		switch (order.orderType) {
			case OrderType::Move:   ExecuteMoveOrder(frameData->secs);   break;
			case OrderType::Attack: ExecuteAttackOrder(frameData->secs); break;
			default: Panic("Unhandled OrderType %u", (U32)order.orderType);
		}
	}

	*cameraOut = camera;

	return Ok();
}

//--------------------------------------------------------------------------------------------------

Res<> Draw() {

	Draw::DrawRect({
		.pos   = { 0.f, 0.f },
		.z     = Z_Background,
		.origin = Draw::Origin::TopLeft,
		.size  = { 1920.f, 1080.f },
		.color = MapBackgroundColor,
	});

	Draw::SetCamera(camera);

	for (I32 col = 0; col < MapCols; col++) {
		for (I32 row = 0; row < MapRows; row++) {
			MapTile const* const mapTile = &mapTiles[col + row * MapCols];
			Vec2 const centerPos = MapCoordToCenterScreenPos(mapTile->mapCoord);
			Draw::DrawSprite({
				.sprite = terrainSprites[(U32)mapTile->terrainType],
				.pos    = centerPos,
				.z      = Z_Map,
			});

			if (mapTile == selectedMapTile) {
				Draw::DrawSprite({
					.sprite = hexBorderSprite,
					.pos    = centerPos,
					.z      = Z_MapHighlight,
					.color  = SelectedColor
				});
			}
		}
	}

	if (hoverMapTile && hoverMapTile != selectedMapTile) {
		Draw::DrawSprite({
			.sprite = hexBorderSprite,
			.pos    = MapCoordToCenterScreenPos(mouseMapCoord),
			.z      = Z_MapHighlight,
			.color  = HoverColor
		});
	}

	for (U32 i = 0; i < unitsLen; i++) {
		Unit const* const unit = &units[i];
		if (!unit->alive) { continue; }
		F32 outlineWidth = 0.f;
		if (unit == selectedUnit) {
			outlineWidth = 1.f;
		}
		Draw::Sprite sprite = unit->unitDef->sprite;
		if (unit->activeAnimationDef) {
			sprite = unit->activeAnimationDef->frameSprites[unit->animationFrame];
		}

		Draw::DrawSprite({
			.sprite       = sprite,
			.pos          = unit->pos,
			.z            = unit->z,
			.outlineWidth = outlineWidth,
			.outlineColor = SelectedColor,
		});

		constexpr F32 HpBarHeight = 2.f;
		F32 y = unit->pos.y - (unit->unitDef->size.y / 2.f);
		Draw::DrawRect({
			.pos    = { unit->pos.x - (unit->unitDef->size.x / 2.f), y },
			.z      = unit->z,
			.origin = Draw::Origin::BottomLeft,
			.size   = { unit->unitDef->size.x, HpBarHeight },
			.color  = { 1.f, 0.f, 0.f, 0.5f },
		});
		Draw::DrawRect({
			.pos    = { unit->pos.x - (unit->unitDef->size.x / 2.f), y },
			.z      = unit->z + 1,
			.origin = Draw::Origin::BottomLeft,
			.size   = { ((F32)unit->hp / (F32)unit->unitDef->maxHp) * unit->unitDef->size.x, HpBarHeight },
			.color  = { 1.f, 0.f, 0.f, 1.f },
		});

		y -= HpBarHeight;
		Draw::DrawStr({
			.font         = numberFont,
			.str          = SPrintf(tempMem, "%u", unit->id),
			.pos          = { unit->pos.x, y },
			.z            = unit->z,
			.origin       = Draw::Origin::BottomCenter,
			.color        = { 0.8f, 0.8f, 1.f, 1.f },
			.outlineWidth = 1.f,
			.outlineColor = { 0.f, 0.f, 0.f, 1.f },
		});
	}

	Draw::ClearCamera();

	Draw::DrawRect({
		.pos   = Vec2(windowSize.x - UiPanelWidth / 2.f, windowSize.y / 2.f),
		.z     = Z_UiBackground,
		.size  = Vec2(UiPanelWidth, windowSize.y),
		.color = UiBackgroundColor,
	});
	constexpr Str lines[] = {
		"Spearman",
		"HP: 10",
		"Atk: 2",
		"Def: 3",
		"abcdefghjiklm",
		"nopqrstuvwxyz",
		"ABCDEFGHJIKLM",
		"NOPQRSTUVWXYZ",
		"0123456789",
		"`!@#$%^&*()",
		"_+[]{};':\"",
		"<>/?-=,.",
		"Armoury",
		"129 GP",
		"-43 Mana",
		"1/2 food",
		"+3% production",
		"Reduces normal",
		"unit cost by 5%",
	};

	Draw::StrDrawDef strDrawDef = {
		.font   = uiFont,
		.pos    = { windowSize.x - UiPanelWidth + 10.f, 10.f },
		.z      = Z_Ui,
		.origin = Draw::Origin::TopLeft,
		.scale  = { uiFontScale, uiFontScale },
		.color  = { 1.f, 1.f, 1.f, 1.f },

	};
	for (U32 i = 0; i < LenOf(lines); i++) {
		strDrawDef.str = lines[i];
		Draw::DrawStr(strDrawDef);
		strDrawDef.pos.y += (uiFontLineHeight + 2) * uiFontScale;
	}

	return Ok();
}

//--------------------------------------------------------------------------------------------------

Res<> ResizeWindow(U32 windowWidth, U32 windowHeight) {
	windowWidth;
	windowHeight;
	return Ok();
}

//--------------------------------------------------------------------------------------------------

void Shutdown() {
}

//--------------------------------------------------------------------------------------------------

App::App app = {
	.PreInit      = PreInit,
	.Init         = Init,
	.Shutdown     = Shutdown,
	.Frame        = Frame,
	.Draw         = Draw,
	.ResizeWindow = ResizeWindow,
};

App::App* GetApp() { return &app; }

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Battle