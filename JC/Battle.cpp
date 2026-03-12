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
#include "JC/UnitDef.h"
#include "JC/Window.h"

namespace JC::Battle {

//--------------------------------------------------------------------------------------------------

DefErr(Battle, TerrainNotFound);

static constexpr U32 MaxTerrain = 64;

static Mem                tempMem;
static Data*              data;
static Terrain*           terrain;
static U32                terrainLen;
static Map<Str, Terrain*> terrainMap;

//--------------------------------------------------------------------------------------------------

Res<> Init(Mem permMem, Mem tempMemIn, Window::State const* windowState) {
	tempMem        = tempMemIn;
	terrain        = Mem::AllocT<Terrain>(permMem, MaxTerrain);
	terrainLen     = 0;
	terrainMap.Init(permMem, MaxTerrain);
	data           = Mem::AllocT<Data>(permMem, 1);

	Try(InitDraw(data, tempMem, windowState));
	InitInput(tempMem);
	InitPath(permMem);
	InitUtil();

	data->state = State::WaitingOrder;

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

Json_Begin(TerrainJson)
	Json_Member("name",     name)
	Json_Member("sprite",   sprite)
	Json_Member("moveCost", moveCost)
Json_End(TerrainJson)

Json_Begin(BattleJson)
	Json_Member("atlasPaths",             atlasPaths)
	Json_Member("borderSprite",           borderSprite)
	Json_Member("highlightSprite",        highlightSprite)
	Json_Member("pathTopLeftSprite",      pathTopLeftSprite)
	Json_Member("pathTopRightSprite",     pathTopRightSprite)
	Json_Member("pathRightSprite",        pathRightSprite)
	Json_Member("pathBottomRightSprite",  pathBottomRightSprite)
	Json_Member("pathBottomLeftSprite",   pathBottomLeftSprite)
	Json_Member("pathLeftSprite",         pathLeftSprite)
	Json_Member("arrowTopLeftSprite",     arrowTopLeftSprite)
	Json_Member("arrowTopRightSprite",    arrowTopRightSprite)
	Json_Member("arrowRightSprite",       arrowRightSprite)
	Json_Member("arrowBottomRightSprite", arrowBottomRightSprite)
	Json_Member("arrowBottomLeftSprite",  arrowBottomLeftSprite)
	Json_Member("arrowLeftSprite",        arrowLeftSprite)
	Json_Member("terrain",                terrain)
Json_End(BattleJson)

Res<> Load(Str path) {
	Span<char> json; TryTo(File::ReadAllZ(path), json);
	BattleJson battleJson; Try(Json::ToObject(tempMem, tempMem, json.data, (U32)json.len, &battleJson));

	for (U64 i = 0; i < battleJson.atlasPaths.len; i++) {
		Try(Draw::LoadAtlas(battleJson.atlasPaths[i]));
	}

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

	Try(LoadDraw(&battleJson));

	Try(Gpu::ImmediateWait());

	return Ok();
}

//--------------------------------------------------------------------------------------------------

static Vec2 CalcWorldPos(U32 c, U32 r) {
	return {
		(F32)((HexSize / 2) + (c * HexSize + (r & 1) * (HexSize / 2))),
		(F32)((HexSize / 2) + (r * (HexSize * 3 / 4))),
	};
}

//--------------------------------------------------------------------------------------------------

static void AddNeighbor(Hex* hex, I32 cOff, I32 rOff, U32 neighborIdx) {
	I32 const c = hex->c + cOff;
	I32 const r = hex->r + rOff;
	if (c >= 0 && c <= MaxCols - 1 && r >= 0 && r <= MaxRows - 1) {
		hex->neighbors[neighborIdx] = &data->hexes[c + (r * MaxCols)];
	} else {
		hex->neighbors[neighborIdx] = nullptr;
	}
}

//--------------------------------------------------------------------------------------------------

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

	memset(data->hexes, 0, MaxCols * MaxRows * sizeof(Hex));
	data->hexesLen = 0;
	for (U32 r = 0; r < MaxRows; r++) {
		for (U32 c = 0; c < MaxCols; c++) {
			Hex* const hex = &data->hexes[c + (r * MaxCols)];
			hex->idx = c + (r * MaxCols);
			hex->c = (I32)c;
			hex->r = (I32)r;
			hex->pos = CalcWorldPos(c, r);
			if (r & 1) {	// odd row
				AddNeighbor(hex,  0, -1, NeighborIdx::TopLeft    );
				AddNeighbor(hex, +1, -1, NeighborIdx::TopRight   );
				AddNeighbor(hex, +1,  0, NeighborIdx::Right      );
				AddNeighbor(hex, +1, +1, NeighborIdx::BottomRight);
				AddNeighbor(hex,  0, +1, NeighborIdx::BottomLeft );
				AddNeighbor(hex, -1,  0, NeighborIdx::Left       );
			} else {	// even row
				AddNeighbor(hex, -1, -1, NeighborIdx::TopLeft    );
				AddNeighbor(hex,  0, -1, NeighborIdx::TopRight   );
				AddNeighbor(hex, +1,  0, NeighborIdx::Right      );
				AddNeighbor(hex,  0, +1, NeighborIdx::BottomRight);
				AddNeighbor(hex, -1, +1, NeighborIdx::BottomLeft );
				AddNeighbor(hex, -1,  0, NeighborIdx::Left       );
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
			data->hexesLen++;
		}
	}

	UnitDef::Def const* def; TryTo(UnitDef::GetDef("Spearmen"), def);

	U32 startCol = 0;
	for (Side side = Side::Left; side <= Side::Right; side++) {
		Logf("Creating side %u", (U32)side);
		Army* army = &data->armies[(U32)side];
		memset(army, 0, sizeof(Army));
		army->side = side;
		for (U32 c = startCol; c < startCol + 2; c++) {
			for (U32 r = 0; r < MaxRows; r++) {
				Hex* const hex = &data->hexes[c + (r * MaxCols)];
				if (!hex->unit&& Rng::NextU32(0, 100) < 50) {
					Unit* const unit = &army->units[army->unitsLen++];
					*unit = {
						.def   = def,
						.hex   = hex,
						.pos   = hex->pos,
						.side  = side,
						.hp    = def->hp,
						.move  = def->move,
						.range = 1,
					};
					hex->unit = unit;
					Logf("Created unit for side %u at %u,%u", (U32)side, c, r);
					if (army->unitsLen >= 12) {
						goto DoneArmyGen;
					}
				}
			}
		}
		DoneArmyGen:
		startCol += 2;
	}

	for (Side side = Side::Left; side <= Side::Right; side++) {
		Army* army = &data->armies[(U32)side];
		for (U32 i = 0; i < army->unitsLen; i++) {
			BuildPathMap(data->hexes, &army->units[i]);
		}
	}

	for (Side side = Side::Left; side <= Side::Right; side++) {
		Army* army = &data->armies[(U32)side];
		memset(army->attackMap, 0, sizeof(army->attackMap));
		for (U32 i = 0; i < army->unitsLen; i++) {
			Unit const* unit = &army->units[i];
			for (U32 j = 0; j < MaxCols * MaxRows; j++) {
				// directly reachable
				if (unit->pathMap.moveCosts[j] != U32Max) {
					army->attackMap[j] |= ((U64)1 << i);
					continue;
				}
				// j has a reachable neighbor
				Hex const* hex = &data->hexes[j];
				for (U32 k = 0; k < 6; k++) {
					Hex const* const neighbor = hex->neighbors[k];
					if (
						neighbor &&
						!neighbor->unit &&
						unit->pathMap.moveCosts[neighbor->idx] != U32Max &&
						HexDistance(neighbor, hex) <= unit->range
					) {
						army->attackMap[j] |= ((U64)1 << i);
						goto DoneNeighbors;
					}
				}
				DoneNeighbors:
				;
			}
		}
	}

	data->activeSide = Side::Left;

	return Ok();
}

//--------------------------------------------------------------------------------------------------

Res<> Update(App::UpdateData const* updateData) {
	return HandleInput(data, updateData->sec, updateData->mouseX, updateData->mouseY, updateData-> actions);
}

//--------------------------------------------------------------------------------------------------

void Draw() {
	Draw(data);
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Battle


/*
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
	}*/