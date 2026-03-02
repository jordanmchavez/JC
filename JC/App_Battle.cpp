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
static constexpr F32  UiPanelWidth = 300.f;
static constexpr Vec4 UiBackgroundColor = Vec4(0.2f, 0.3f, 0.4f, 1.f);

static constexpr F32 Z_Background   = 0.f;
static constexpr F32 Z_Map          = 1.f;
static constexpr F32 Z_MapHighlight = 2.f;
static constexpr F32 Z_Unit         = 3.f;
static constexpr F32 Z_UnitSelected = 4.f;
static constexpr F32 Z_UiBackground = 5.f;
static constexpr F32 Z_Ui           = 6.f;

static constexpr U64 Action_Exit           = 1;
static constexpr U64 Action_Click          = 2;
static constexpr U64 Action_ZoomInCanvas   = 3;
static constexpr U64 Action_ZoomOutCanvas  = 4;
static constexpr U64 Action_ScrollMapLeft  = 5;
static constexpr U64 Action_ScrollMapRight = 6;
static constexpr U64 Action_ScrollMapUp    = 7;
static constexpr U64 Action_ScrollMapDown  = 8;
static constexpr U64 Action_NextFont       = 9;
static constexpr U64 Action_PrevFont       = 10;

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
static Draw::Canvas      canvas;
static F32               canvasAreaWidth;
static F32               canvasAreaHeight;
static Vec2              canvasSize(MapCols * HexSize + (HexSize / 2), MapRows * (HexSize * 3 / 4) + (HexSize / 4));
static F32               canvasScale = 3.f;
static Vec2              canvasPos;
static Vec2              canvasMinPos;
static Vec2              canvasMaxPos;
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
static Draw::Font        fonts[MaxFonts];
static U32               fontsLen;
static U32               activeFontIdx;
static F32               activeFontLineHeight;
static Vec2              windowSize;
static Input::BindingSet mainBindingSet;

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

static Vec2 MapCoordToCenterPixel(MapCoord mc) {
	return {
		(F32)((HexSize / 2) + mc.col * HexSize + (mc.row & 1) * (HexSize / 2)),
		(F32)((HexSize / 2) + mc.row * (HexSize * 3 / 4)),
	};
}

//---------------------------------------------------------------------------------------------

static MapCoord PixelToMapCoord(Vec2 p) {
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

	TryTo(Draw::CreateCanvas((U32)canvasSize.x, (U32)canvasSize.y), canvas);

	canvasAreaWidth  = windowSize.x - UiPanelWidth;
	canvasAreaHeight = windowSize.y - 100.f;

	F32 const canvasScaledWidth  = canvasSize.x * canvasScale;
	F32 const canvasScaledHeight = canvasSize.y * canvasScale;
	canvasMaxPos.x = (canvasAreaWidth  / 2.f) - (canvasAreaWidth  - canvasScaledWidth) / 2.f;
	canvasMaxPos.y = (canvasAreaHeight / 2.f) - (canvasAreaHeight - canvasScaledHeight) / 2.f;
	if (canvasScaledWidth > canvasAreaWidth) {
		canvasMinPos.x = canvasAreaWidth - canvasMaxPos.x;
	} else {
		canvasMinPos.x = canvasMaxPos.x;
	}
	if (canvasScaledHeight > canvasAreaHeight) {
		canvasMinPos.y = canvasAreaWidth - canvasMaxPos.y;
	} else {
		canvasMinPos.y = canvasMaxPos.y;
	}
	canvasPos = canvasMaxPos;
	Logf("canvasPos=(%.2f, %.2f)", canvasPos.x, canvasPos.y);
	Logf("canvasMinPos=(%.2f, %.2f)", canvasMinPos.x, canvasMinPos.y);
	Logf("canvasMaxPos=(%.2f, %.2f)", canvasMaxPos.x, canvasMaxPos.y);

	Try(Draw::LoadSprites("Assets/Terrain.png", "Assets/Terrain.spritejson"));
	Try(Draw::LoadSprites("Assets/Units.png", "Assets/Units.spritejson"));

	spearmenUnitDef = &unitDefs[unitDefsLen++];
	TryTo(Draw::GetSprite("Unit_Spearmen"), spearmenUnitDef->sprite);
	Draw::Sprite attackSprite; TryTo(Draw::GetSprite("Unit_Spearmen_Attack"), attackSprite);
	spearmenUnitDef->maxHp = 10;
	spearmenUnitDef->size = Draw::GetSpriteSize(spearmenUnitDef->sprite);
	spearmenUnitDef->attackAnimationDef.frameLen = 6;
	spearmenUnitDef->attackAnimationDef.frameDurSecs[0] = 0.2f;
	spearmenUnitDef->attackAnimationDef.frameDurSecs[1] = 0.2f;
	spearmenUnitDef->attackAnimationDef.frameDurSecs[2] = 0.2f;
	spearmenUnitDef->attackAnimationDef.frameDurSecs[3] = 0.2f;
	spearmenUnitDef->attackAnimationDef.frameDurSecs[4] = 0.2f;
	spearmenUnitDef->attackAnimationDef.frameDurSecs[5] = 0.2f;
	spearmenUnitDef->attackAnimationDef.frameSprites[0] = attackSprite;
	spearmenUnitDef->attackAnimationDef.frameSprites[1] = spearmenUnitDef->sprite;
	spearmenUnitDef->attackAnimationDef.frameSprites[2] = attackSprite;
	spearmenUnitDef->attackAnimationDef.frameSprites[3] = spearmenUnitDef->sprite;
	spearmenUnitDef->attackAnimationDef.frameSprites[4] = attackSprite;
	spearmenUnitDef->attackAnimationDef.frameSprites[5] = spearmenUnitDef->sprite;

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
			unit->pos     = MapCoordToCenterPixel(mapTile->mapCoord);
			unit->z       = Z_Unit;
			unit->hp      = spearmenUnitDef->maxHp;
			mapTile->unit = unit;
		}
	}

	Span<Str> fontjsonFileNames; TryTo(File::EnumFiles("Assets/Fonts", ".fontjson"), fontjsonFileNames);
	for (U64 i = 0; i < fontjsonFileNames.len; i++) {
		Str const fontjsonFileName = fontjsonFileNames[i];
		Str const pngFileName = SPrintf(tempMem, "%s.png", File::RemoveExt(fontjsonFileName));
		Assert(fontsLen < MaxFonts);
		TryTo(Draw::LoadFont(fontjsonFileName, pngFileName), fonts[fontsLen]);
		Logf("Loaded font %s", fontjsonFileName);
		fontsLen++;
	}
	Assert(fontsLen > 0);
	activeFontIdx = 25;
	activeFontLineHeight = Draw::GetFontLineHeight(fonts[activeFontIdx]);

	// TODO: this needs to be in all gpu resource load functions...I've forgotten to add this and it's repeatedly cost me lotsa debugging time
	Try(Gpu::ImmediateWait());

	mouseMapCoord = MapCoord(0, 0);

	mainBindingSet = Input::CreateBindingSet("Main");
	Input::Bind(mainBindingSet, Key::Key::Escape,         Input::BindingType::OnKeyDown,  Action_Exit,           "Exit");
	Input::Bind(mainBindingSet, Key::Key::Mouse1,         Input::BindingType::OnKeyUp,    Action_Click,          "Click");
	Input::Bind(mainBindingSet, Key::Key::MouseWheelUp,   Input::BindingType::OnKeyDown,  Action_ZoomInCanvas,   "ZoomInCanvas");
	Input::Bind(mainBindingSet, Key::Key::MouseWheelDown, Input::BindingType::OnKeyDown,  Action_ZoomOutCanvas,  "ZoomOutCanvas");
	Input::Bind(mainBindingSet, Key::Key::W,              Input::BindingType::Continuous, Action_ScrollMapUp,    "ScrollMapUp");
	Input::Bind(mainBindingSet, Key::Key::S,              Input::BindingType::Continuous, Action_ScrollMapDown,  "ScrollMapDown");
	Input::Bind(mainBindingSet, Key::Key::A,              Input::BindingType::Continuous, Action_ScrollMapLeft,  "ScrollMapLeft");
	Input::Bind(mainBindingSet, Key::Key::D,              Input::BindingType::Continuous, Action_ScrollMapRight, "ScrollMapRight");
	Input::Bind(mainBindingSet, Key::Key::Dot,            Input::BindingType::OnKeyDown,  Action_NextFont,       "NextFont");
	Input::Bind(mainBindingSet, Key::Key::Comma,          Input::BindingType::OnKeyDown,  Action_PrevFont,       "PrevFont");
	Input::SetBindingSetStack({ mainBindingSet });
												          
	state = State::WaitingOrder;

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
			order.moveOrder.durSecs      = 0.5f;
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
		Vec2 const startPos = MapCoordToCenterPixel(moveOrder->startMapTile->mapCoord);
		Vec2 const endPos   = MapCoordToCenterPixel(moveOrder->endMapTile->mapCoord);
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
				Vec2 const startPos = MapCoordToCenterPixel(attackOrder->unitMapTile->mapCoord);
				Vec2 const endPos   = attackOrder->targetPos;
				F32 const t         = attackOrder->elapsedSecs / attackOrder->durSecs;
				unit->pos = Math::Lerp(startPos, endPos, t);
			} else {
				attackOrder->attackOrderState = AttackOrderState::Attacking;
				unit->activeAnimationDef        = &unit->unitDef->attackAnimationDef;
				unit->animationFrame            = 0;
				unit->animationFrameElapsedSecs = 0.f;
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
				Vec2 const endPos   = MapCoordToCenterPixel(attackOrder->unitMapTile->mapCoord);
				F32 const t         = attackOrder->elapsedSecs / attackOrder->durSecs;
				unit->pos = Math::Lerp(startPos, endPos, t);
			} else {
				unit->pos = MapCoordToCenterPixel(attackOrder->unitMapTile->mapCoord);
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

static void RecalcCanvasBounds() {
	F32 const canvasScaledWidth  = canvasSize.x * canvasScale;
	F32 const canvasScaledHeight = canvasSize.y * canvasScale;
	canvasMaxPos.x = (canvasAreaWidth  / 2.f) - (canvasAreaWidth  - canvasScaledWidth) / 2.f;
	canvasMaxPos.y = (canvasAreaHeight / 2.f) - (canvasAreaHeight - canvasScaledHeight) / 2.f;
	if (canvasScaledWidth > canvasAreaWidth) {
		canvasMinPos.x = canvasAreaWidth - canvasMaxPos.x;
	} else {
		canvasMinPos.x = canvasMaxPos.x;
	}
	if (canvasScaledHeight > canvasAreaHeight) {
		canvasMinPos.y = canvasAreaHeight - canvasMaxPos.y;
	} else {
		canvasMinPos.y = canvasMaxPos.y;
	}
	canvasPos.x = Clamp(canvasPos.x, canvasMinPos.x, canvasMaxPos.x);
	canvasPos.y = Clamp(canvasPos.y, canvasMinPos.y, canvasMaxPos.y);
	Logf("canvasMinPos=(%.2f, %.2f)", canvasMinPos.x, canvasMinPos.y);
	Logf("canvasMaxPos=(%.2f, %.2f)", canvasMaxPos.x, canvasMaxPos.y);
}

Res<> Frame(App::FrameData const* frameData) {
	if (frameData->exit) {
		return App::Err_Exit();
	}

	const F32 secs = (F32)Time::Secs(frameData->ticks);

	constexpr float CanvasScrollPixelsPerSec = 1000.f;
	for (U64 i = 0; i < frameData->actions.len; i++) {
		U64 const actionId = frameData->actions[i];

		switch (actionId) {
			case Action_Exit: return App::Err_Exit();
			case Action_Click: HandleLeftClick(); break;

			case Action_ZoomInCanvas:  canvasScale += 0.25f; Logf("canvasScale=%.2f", canvasScale); RecalcCanvasBounds(); break;
			case Action_ZoomOutCanvas: canvasScale -= 0.25f; Logf("canvasScale=%.2f", canvasScale); RecalcCanvasBounds(); break;

			case Action_ScrollMapLeft:  { canvasPos.x = Min(canvasMaxPos.x, canvasPos.x + (CanvasScrollPixelsPerSec * secs)); Logf("canvasPos=(%.2f, %.2f)", canvasPos.x, canvasPos.y); }; break;
			case Action_ScrollMapRight: { canvasPos.x = Max(canvasMinPos.x, canvasPos.x - (CanvasScrollPixelsPerSec * secs)); Logf("canvasPos=(%.2f, %.2f)", canvasPos.x, canvasPos.y); }; break;
			case Action_ScrollMapUp:    { canvasPos.y = Min(canvasMaxPos.y, canvasPos.y + (CanvasScrollPixelsPerSec * secs)); Logf("canvasPos=(%.2f, %.2f)", canvasPos.x, canvasPos.y); }; break;
			case Action_ScrollMapDown:  { canvasPos.y = Max(canvasMinPos.y, canvasPos.y - (CanvasScrollPixelsPerSec * secs)); Logf("canvasPos=(%.2f, %.2f)", canvasPos.x, canvasPos.y); }; break;

			case Action_NextFont: {
				activeFontIdx++;
				if (activeFontIdx >= fontsLen) {
					activeFontIdx = 0;
				}
				activeFontLineHeight = Draw::GetFontLineHeight(fonts[activeFontIdx]);
				Logf("Selected font %u/%u %s with lineHeight = %f", activeFontIdx, fontsLen, Draw::GetFontPath(fonts[activeFontIdx]), activeFontLineHeight);
				break;
			}

			case Action_PrevFont: {
				if (activeFontIdx == 0) {
					activeFontIdx = fontsLen - 1;
				} else {
					activeFontIdx--;
				}
				activeFontLineHeight = Draw::GetFontLineHeight(fonts[activeFontIdx]);
				Logf("Selected font %u/%u %s with lineHeight = %f", activeFontIdx, fontsLen, Draw::GetFontPath(fonts[activeFontIdx]), activeFontLineHeight);
				break;
			}

			default: Panic("Unhandled actionId %u", actionId);
		}
	}
	
	Vec2 const canvasTopLeft = Vec2(
		canvasPos.x - (canvasSize.x * canvasScale) / 2.f,
		canvasPos.y - (canvasSize.y * canvasScale) / 2.f
	);
	
	Vec2 const mousePos((F32)frameData->mouseX, (F32)frameData->mouseY);
	Vec2 const canvasMousePos = Math::Scale(Math::Sub(mousePos, canvasTopLeft), 1.f / canvasScale);
	mouseMapCoord = PixelToMapCoord(canvasMousePos);
	if (mouseMapCoord.col >= 0 && mouseMapCoord.col < MapCols && mouseMapCoord.row >= 0 && mouseMapCoord.row < MapRows) {
		hoverMapTile = &mapTiles[mouseMapCoord.col + mouseMapCoord.row * MapCols];
	} else {
		hoverMapTile = nullptr;
	}

	if (state == State::ExecutingOrder) {
		switch (order.orderType) {
			case OrderType::Move:   ExecuteMoveOrder(secs);   break;
			case OrderType::Attack: ExecuteAttackOrder(secs); break;
			default: Panic("Unhandled OrderType %u", (U32)order.orderType);
		}
	}

	return Ok();
}

//--------------------------------------------------------------------------------------------------

Res<> Draw(Gpu::FrameData const* gpuFrameData) {
	Draw::BeginFrame(gpuFrameData);	// TODO: move to App.cpp

	Draw::SetCanvas(canvas);
	Draw::DrawRect({
		.pos   = Vec2(canvasSize.x / 2.f, canvasSize.y / 2.f),
		.z     = Z_Background,
		.size  = canvasSize,
		.color = MapBackgroundColor,
	});

	for (I32 col = 0; col < MapCols; col++) {
		for (I32 row = 0; row < MapRows; row++) {
			MapTile const* const mapTile = &mapTiles[col + row * MapCols];
			Vec2 const centerPos = MapCoordToCenterPixel(mapTile->mapCoord);
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
			.pos    = MapCoordToCenterPixel(mouseMapCoord),
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
			.outlineColor = SelectedColor,
			.outlineWidth = outlineWidth,
		});
		constexpr F32 HpBarHeight = 2.f;
		F32 y = unit->pos.y - (unit->unitDef->size.y / 2.f) - (HpBarHeight / 2.f);
		Draw::DrawRect({
			.pos   = Vec2(unit->pos.x, y),
			.z     = unit->z,
			.size  = Vec2(unit->unitDef->size.x, HpBarHeight),
			.color = Vec4(1.f, 0.f, 0.f, 1.f),
		});

		y -= (HpBarHeight / 2.f);
		Draw::DrawFont({
			.font   = fonts[activeFontIdx],
			.str    = SPrintf(tempMem, "%u", unit->id),
			.pos    = Vec2(unit->pos.x, y),
			.z      = unit->z,
			.origin = Draw::Origin::BottomCenter,
			.scale  = Vec2(1.f, 1.f),
			.color  = Vec4(0.8f, 0.8f, 1.f, 1.f),
		});
	}

	Draw::SetDefaultCanvas();
	Draw::DrawCanvas({
		.canvas = canvas,
		.pos    = canvasPos,
		.scale  = Vec2(canvasScale, canvasScale),
	});
	
	Draw::DrawRect({
		.pos   = Vec2(windowSize.x - UiPanelWidth / 2.f, windowSize.y / 2.f),
		.z     = Z_UiBackground,
		.size  = Vec2(UiPanelWidth, windowSize.y),
		.color = UiBackgroundColor,
	});
	constexpr F32 UiScale = 2.f;
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
		"66",
		"66666666",
		"`!@#$%^&*()",
		"_+[]{};':\"",
		"<>/?-=,.",
	};
	Draw::FontDrawDef fontDrawDef = {
		.font   = fonts[activeFontIdx],
		.pos    = Vec2(canvasAreaWidth + 10.f, 10.f + (activeFontLineHeight * UiScale)),
		.z      = Z_Ui,
		.origin = Draw::Origin::TopLeft,
		.scale  = Vec2(UiScale, UiScale),
		.color  = Vec4(1.f, 1.f, 1.f, 1.f),
	};
	for (U32 i = 0; i < LenOf(lines); i++) {
		fontDrawDef.str = lines[i];
		Draw::DrawFont(fontDrawDef);
		fontDrawDef.pos.y += activeFontLineHeight * UiScale;
	}

	Draw::EndFrame();

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
	Draw::DestroyCanvas(canvas);
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

}	// namespace JC::Battle