#include "JC/Battle.h"
#include "JC/Battle_Internal.h"

#include "JC/App.h"
#include "JC/Draw.h"
#include "JC/File.h"
#include "JC/Hash.h"
#include "JC/Gpu.h"
#include "JC/Json.h"
#include "JC/Key.h"
#include "JC/Log.h"
#include "JC/Map.h"
#include "JC/Math.h"
#include "JC/Rng.h"
#include "JC/Unit.h"
#include "JC/Window.h"

namespace JC::Battle {

//--------------------------------------------------------------------------------------------------

DefErr(Battle, TerrainNotFound);

//--------------------------------------------------------------------------------------------------

static constexpr U32 MaxTerrain = 64;

struct TerrainJson {
	Str name;
	Str sprite;
	U32 moveCost;
};
Json_Begin(TerrainJson)
	Json_Member("name",     name)
	Json_Member("sprite",   sprite)
	Json_Member("moveCost", moveCost)
Json_End(TerrainJson)

struct BattleJson {
	Span<Str>         atlasJsonPaths;
	Str               borderSprite;
	Str               highlightSprite;
	Span<TerrainJson> terrain;
};
Json_Begin(BattleJson)
	Json_Member("atlasJsonPaths",  atlasJsonPaths)
	Json_Member("borderSprite",    borderSprite)
	Json_Member("highlightSprite", highlightSprite)
	Json_Member("terrain",         terrain)
Json_End(BattleJson)

using ActionFn = Res<>(F32 sec);

enum struct State {
	None,
	UnitSelected,
	ExecutingOrder,
};

static constexpr U32 MaxArmyUnits = 16;

struct Army {
	Unit::Side  side;
	Unit::Unit* units[MaxArmyUnits];
	U32         unitsLen;
	Unit::Unit* readyUnits;
	Unit::Unit* doneUnits;
};
static Army       armies[2];
static Unit::Side activeSide;

//--------------------------------------------------------------------------------------------------

static Mem                tempMem;
static Data               data;
static Terrain*           terrain;
static U32                terrainLen;
static Map<Str, Terrain*> terrainMap;


//--------------------------------------------------------------------------------------------------

Res<> Init(Mem permMem, Mem tempMemIn, Window::State const* windowState) {
	tempMem          = tempMemIn;
	terrain          = Mem::AllocT<Terrain>(permMem, MaxTerrain);
	terrainLen       = 0;
	terrainMap.Init(permMem, MaxTerrain);
	data.hexes     = Mem::AllocT<Hex>(permMem, MaxRows * MaxCols);
	data.hexesLen  = 0;

	InitDraw(permMem, windowState);
	InitPath(permMem);
	InitUtil();


	state = State::None;

	return Ok();
}

//--------------------------------------------------------------------------------------------------

static Res<Terrain const*> GetTerrain(Str name) {
	Terrain const* const* terrainPP = terrainMap.FindOrNull(name);
	if (!terrainPP) {
		return Err_TerrainNotFound("name", name);
	}
	return *terrainPP;
}

//--------------------------------------------------------------------------------------------------

Res<> LoadBattleJson(Str battleJsonPath) {
	Span<char> json; TryTo(File::ReadAllZ(battleJsonPath), json);
	BattleJson battleJson; Try(Json::ToObject(tempMem, tempMem, json.data, (U32)json.len, &battleJson));

	for (U64 i = 0; i < battleJson.atlasJsonPaths.len; i++) {
		Try(Draw::LoadAtlasJson(battleJson.atlasJsonPaths[i]));
	}

	TryTo(Draw::GetSprite(battleJson.borderSprite),    borderSprite);
	TryTo(Draw::GetSprite(battleJson.highlightSprite), highlightSprite);

	Assert(battleJson.terrain.len < MaxTerrain);
	terrainLen = 1;	// reserve 0 for invalid
	for (U64 i = 0; i < battleJson.terrain.len; i++) {
		TerrainJson const* const terrainJson = &battleJson.terrain[i];
		Draw::Sprite sprite; TryTo(Draw::GetSprite(terrainJson->sprite), sprite);
		terrain[terrainLen] = {
			.name     = terrainJson->name,	// interned by Json
			.sprite   = sprite,
			.moveCost = terrainJson->moveCost,
		};
		terrainMap.Put(terrainJson->name, &terrain[terrainLen]);
		terrainLen++;
	}

	Try(Gpu::ImmediateWait());

	return Ok();
}

//--------------------------------------------------------------------------------------------------

static void AddNeighbor(Hex* hex, I32 cOff, I32 rOff) {
	I32 const c = hex->c + cOff;
	I32 const r = hex->r + rOff;

	if (c >= 0 && c <= MaxCols - 1 && r >= 0 && r <= MaxRows - 1) {
		hex->neighbors[hex->neighborsLen++] = &hexes[c + (r * MaxCols)];
	}
}

static Unit::Unit* CreateUnitOn(Unit::UnitDef const* unitDef, Hex* hex, Unit::Side::Enum side) {
	Unit::Unit* const unit = Unit::AllocUnit();
	unit->unitDef = unitDef;
	unit->side    = side;
	unit->pos     = HexToCenterWorldPos(hex);
	unit->hp      = unitDef->hp;
	unit->move    = unitDef->move;
	unit->next    = nullptr;

	hex->unit     = unit;

	return unit;
}

struct TerrainGen {
	Terrain const* terrain;
	U32            chance;
};

Res<> GenerateMap() {
	TerrainGen terrainGen[6];
	U32 maxChance = 0;
	TryTo(GetTerrain("Grassland"), terrainGen[0].terrain); maxChance += 6; terrainGen[0].chance = maxChance;
	TryTo(GetTerrain("Forest"),    terrainGen[1].terrain); maxChance += 3; terrainGen[1].chance = maxChance;
	TryTo(GetTerrain("Swamp"),     terrainGen[2].terrain); maxChance += 1; terrainGen[2].chance = maxChance;
	TryTo(GetTerrain("Hill"),      terrainGen[3].terrain); maxChance += 2; terrainGen[3].chance = maxChance;
	TryTo(GetTerrain("Mountain"),  terrainGen[4].terrain); maxChance += 1; terrainGen[4].chance = maxChance;
	TryTo(GetTerrain("Water"),     terrainGen[5].terrain); maxChance += 1; terrainGen[5].chance = maxChance;

	memset(hexes, 0, MaxCols * MaxRows * sizeof(Hex));
	hexesLen = 0;
	for (U32 c = 0; c < MaxCols; c++) {
		for (U32 r = 0; r < MaxRows; r++) {
			Hex* const hex = &hexes[c + (r * MaxCols)];
			hex->idx = c + (r * MaxCols);
			hex->c = (I32)c;
			hex->r = (I32)r;
			hex->neighborsLen = 0;
			if (r & 1) {
				AddNeighbor(hex,  0, -1);
				AddNeighbor(hex, +1, -1);
				AddNeighbor(hex, -1,  0);
				AddNeighbor(hex, +1,  0);
				AddNeighbor(hex,  0, +1);
				AddNeighbor(hex, +1, +1);
			} else {
				AddNeighbor(hex, -1, -1);
				AddNeighbor(hex,  0, -1);
				AddNeighbor(hex, -1,  0);
				AddNeighbor(hex, +1,  0);
				AddNeighbor(hex, -1, +1);
				AddNeighbor(hex,  0, +1);
			}
			U32 rng = Rng::NextU32(0, maxChance);
			for (U32 i = 0; i < LenOf(terrainGen); i++) {
				if (rng < terrainGen[i].chance) {
					hex->terrain = terrainGen[i].terrain;
					break;
				}
			}
			Assert(hex->terrain);
			hex->unit = nullptr;
			hexesLen++;
		}
	}

	Unit::UnitDef const* unitDef; TryTo(Unit::GetUnitDef("Spearmen"), unitDef);

	U32 startCol = 0;
	Unit::Side::Enum side = Unit::Side::Left;
	for (bool b = true; b; b = false) {
		Army* army = &armies[Unit::Side::Left];
		memset(army, 0, sizeof(Army));
		army->side = Unit::Side::Left;
		for (U32 c = startCol; c < startCol + 2; c++) {
			for (U32 r = 0; r < MaxRows; r++) {
				if (Rng::NextU32(0, 100) < 50) {
					army->units[army->unitsLen++] = CreateUnitOn(unitDef, &hexes[c + (r * MaxCols)], Unit::Side::Left);
					if (army->unitsLen >= MaxArmyUnits) {
						goto DoneUnitGen;
					}
				}
			}
		}
		DoneUnitGen:
		side = Unit::Side::Right;
		startCol = MaxCols - 2;
	}

	return Ok();
}

//--------------------------------------------------------------------------------------------------

Res<> Frame(App::FrameData const* appFrameData) {
	Vec2 const mouseWorldPos = Vec2(
		((F32)appFrameData->mouseX / camera.scale) + camera.pos.x,
		((F32)appFrameData->mouseY / camera.scale) + camera.pos.y
	);

	for (U64 i = 0; i < appFrameData->actions.len; i++) {
		U64 const actionId = appFrameData->actions[i];
		Assert(actionId > 0 && actionId < ActionId_Max);
		Try(actionFns[actionId](appFrameData->sec));
	}

	Hex const* const oldMouseHex = mouseHex;	
	mouseHex = WorldPosToHex(mouseWorldPos);
	if (mouseHex && selectedHex && oldMouseHex != mouseHex) {
		if (selectedHexMoveCostMap.moveCosts[mouseHex->c + (mouseHex->r * MaxCols)] != 0) {
			FindPathFromMoveCostMap(&selectedHexMoveCostMap, selectedHex, mouseHex, &selectedToHoverPath);
			StrBuf sb(tempMem);
			sb.Add("path=[");
			for (U32 i = 0; i < selectedToHoverPath.len; i++) {
				sb.Printf("(%i, %i), ", selectedToHoverPath.hexes[i]->c, selectedToHoverPath.hexes[i]->r);
			}
			sb.Add(']');
			Logf("path=%s", sb.ToStr());
		}
	}

	return Ok();
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Battle

















/*


static constexpr Vec4 MapBackgroundColor = Vec4(13.f/255.f, 30.f/255.f, 22.f/255.f, 1.f);
static constexpr F32  UiPanelWidth = 500.f;
static constexpr Vec4 UiBackgroundColor = Vec4(0.2f, 0.3f, 0.4f, 1.f);

//--------------------------------------------------------------------------------------------------

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
	F32         elapsedSecs;
	F32         durSecs;
	Unit::Unit* unit;
	MapTile*    startMapTile;
	MapTile*    endMapTile;
};

struct AttackOrder {
	AttackOrderState attackOrderState;
	F32              elapsedSecs;
	F32              durSecs;
	MapTile*         unitMapTile;
	Unit::Unit*      unit;
	Unit::Unit*      targetUnit;
	Vec2             targetPos;
};




//--------------------------------------------------------------------------------------------------

/*
static void MoveRequest(MapTile* start, MapTile* end) {
		selectedMapTile->unit = nullptr;

		order.orderType              = OrderType::Move;
		order.moveOrder.elapsedSecs  = 0.f;
		order.moveOrder.durSecs      = 1.f;
		order.moveOrder.unit         = selectedUnit;
		order.moveOrder.startMapTile = selectedMapTile;
		order.moveOrder.endMapTile   = clickedHex;

		selectedMapTile = clickedHex;

		state = State::ExecutingOrder;

		Logf("Executing move order from (%i, %i) -> (%i, %i)", order.moveOrder.startMapTile->mapCoord.col, order.moveOrder.startMapTile->mapCoord.row, order.moveOrder.endMapTile->mapCoord.col, order.moveOrder.endMapTile->mapCoord.row);
}

//--------------------------------------------------------------------------------------------------
/*
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



	/*
	if (state == State::ExecutingOrder) {
		switch (order.orderType) {
			case OrderType::Move:   ExecuteMoveOrder(frameData->secs);   break;
			case OrderType::Attack: ExecuteAttackOrder(frameData->secs); break;
			default: Panic("Unhandled OrderType %u", (U32)order.orderType);
		}
	}
	*cameraOut = camera;
	
	
	
	
	
	
		Draw::DrawRect({
		.pos   = { 0.f, 0.f },
		.z     = 0.f,
		.origin = Draw::Origin::TopLeft,
		.size  = { 1920.f, 1080.f },
		.color = MapBackgroundColor,
	});
	/*
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

	*/