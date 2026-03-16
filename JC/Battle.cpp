#include "JC/Battle.h"
#include "JC/Battle_Internal.h"

#include "JC/App.h"
#include "JC/Bit.h"
#include "JC/Draw.h"
#include "JC/Effect.h"
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

enum struct State {
	WaitingForOrder = 0,
	ExecutingOrder,
};

static constexpr U16 MaxOrderSteps = 512;

enum struct OrderStepType {
	None = 0,
	Move,
	AttackIn,
	AttackOut,
	Die,
};

struct OrderStep {
	OrderStepType type;
	Hex*          hex;
	Unit*         unit;
	F32           durSec;
};

struct Order {
	Unit*     unit;
	OrderStep steps[MaxOrderSteps];
	U16       stepsLen;
	U16       stepsIdx;
	F32       elapsedSec;
	bool      unitMoved;
};

static constexpr U32 MaxTerrain = 64;

static Mem                tempMem;
static Data*              data;
static Terrain*           terrain;
static U32                terrainLen;
static Map<Str, Terrain*> terrainMap;

static Unit* selectedUnit;
static Path  hoverPath;
static Path  targetPath;
static Hex const* targetAttackHex;
static Hex* hoverHex;
static bool rebuildOverlay;

static bool showEnemyArmyThreatMap;
static DrawDef drawDef;

static State state;

static Order order;
static Draw::Font numberFont;

//--------------------------------------------------------------------------------------------------

Res<> Init(Mem permMem, Mem tempMemIn, Window::State const* windowState) {
	tempMem        = tempMemIn;
	terrain        = Mem::AllocT<Terrain>(permMem, MaxTerrain);
	terrainLen     = 0;
	terrainMap.Init(permMem, MaxTerrain);
	data           = Mem::AllocT<Data>(permMem, 1);

	Try(InitDraw(tempMem, windowState));
	InitInput(permMem, tempMem);
	InitPath(permMem);
	InitUtil();

	state = State::WaitingForOrder;

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
	Json_Member("numberFont",             numberFont)
	Json_Member("uiFont",                 uiFont)
	Json_Member("fancyFont",              fancyFont)
	Json_Member("borderSprite",           borderSprite)
	Json_Member("innerSprite",            innerSprite)
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
			.moveCost = (U16)terrainJson->moveCost,
		};
		terrainMap.Put(terrainJson->name, &terrain[terrainLen]);
		terrainLen++;
	}

	TryTo(Draw::LoadFont(battleJson.numberFont), numberFont);

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

	memset(data->hexes, 0, MaxHexes * sizeof(Hex));
	data->hexesLen = 0;
	for (U16 r = 0; r < MaxRows; r++) {
		for (U16 c = 0; c < MaxCols; c++) {
			Hex* const hex = &data->hexes[c + (r * MaxCols)];
			hex->idx = c + (r * MaxCols);
			hex->c = c;
			hex->r = r;
			hex->pos = CalcWorldPos(c, r);
			if (r & 1) {	// odd row
				AddNeighbor(hex,  0, -1, Dir::TopLeft    );
				AddNeighbor(hex, +1, -1, Dir::TopRight   );
				AddNeighbor(hex, +1,  0, Dir::Right      );
				AddNeighbor(hex, +1, +1, Dir::BottomRight);
				AddNeighbor(hex,  0, +1, Dir::BottomLeft );
				AddNeighbor(hex, -1,  0, Dir::Left       );
			} else {	// even row
				AddNeighbor(hex, -1, -1, Dir::TopLeft    );
				AddNeighbor(hex,  0, -1, Dir::TopRight   );
				AddNeighbor(hex, +1,  0, Dir::Right      );
				AddNeighbor(hex,  0, +1, Dir::BottomRight);
				AddNeighbor(hex, -1, +1, Dir::BottomLeft );
				AddNeighbor(hex, -1,  0, Dir::Left       );
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
	for (Side side = Side_Left; side <= Side_Right; side++) {
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

	for (Side side = Side_Left; side <= Side_Right; side++) {
		Army* army = &data->armies[(U32)side];
		for (U32 i = 0; i < army->unitsLen; i++) {
			BuildPathMap(data->hexes, &army->units[i]);
		}
	}

	for (Side side = Side_Left; side <= Side_Right; side++) {
		Army* army = &data->armies[(U32)side];
		memset(army->attackMap, 0, sizeof(army->attackMap));
		for (U32 i = 0; i < army->unitsLen; i++) {
			Unit const* unit = &army->units[i];
			U64 const unitBit = (U64)1 << (U64)(unit - army->units);
			for (U32 j = 0; j < MaxHexes; j++) {
				// directly reachable
				if (unit->pathMap.parents[j]) {
					army->attackMap[j] |= unitBit;
					Logf("Unit (%u, %u) directly reaches (%u, %u)", unit->hex->c, unit->hex->r, data->hexes[j].c, data->hexes[j].r);
					continue;
				}
				// j has a reachable neighbor
				// TODO: pretty sure we have a utility fn for this
				Hex const* hex = &data->hexes[j];
				for (U32 k = 0; k < 6; k++) {
					Hex const* const neighbor = hex->neighbors[k];
					if (
						neighbor &&
						!neighbor->unit &&
						unit->pathMap.parents[neighbor->idx] &&
						HexDistance(neighbor, hex) <= unit->range
					) {
						army->attackMap[j] |= unitBit;
						Logf("Unit (%u, %u) neighbor reaches (%u, %u)", unit->hex->c, unit->hex->r, neighbor->c, neighbor->r);
						break;
					}
				}
			}
		}
	}

	data->activeSide = Side_Left;

	return Ok();
}

//--------------------------------------------------------------------------------------------------

struct PathPrinter : Printer {
	Path const* path;

	PathPrinter(Path const* pathIn) { path = pathIn; }

	void Print(StrBuf* sb) override {
		sb->Add('[');
		U16 i = 0;
		for (; i < path->len; i++) {
			sb->Printf("(%u,%u), ", path->hexes[i]->c, path->hexes[i]->r);
		}
		if (i > 0) { sb->Remove(2); }
		sb->Add(']');
	}
};

//--------------------------------------------------------------------------------------------------

static void CreateOrder(Unit* unit, Path* movePath, Unit* attackUnit) {
	memset(&order, 0, sizeof(order));

	order.unit = unit;
	for (U16 i = 0; i < movePath->len; i++) {
		order.steps[order.stepsLen++] = {
			.type   = OrderStepType::Move,
			.hex    = movePath->hexes[i],
			.unit   = nullptr,
			.durSec = 0.5f,
		};
	}
	if (attackUnit) {
		order.steps[order.stepsLen++] = {
			.type   = OrderStepType::AttackIn,
			.hex    = attackUnit->hex,
			.unit   = attackUnit,
			.durSec = 0.5f,
		};
		order.steps[order.stepsLen++] = {
			.type   = OrderStepType::AttackOut,
			.hex    = attackUnit->hex,
			.unit   = attackUnit,
			.durSec = 0.5f,
		};
	}

	state = State::ExecutingOrder;

	selectedUnit          = nullptr;
	hoverPath.len         = 0;
	targetPath.len        = 0;
	targetAttackHex       = nullptr;
	hoverHex              = nullptr;
	drawDef.overlayLen    = 0;
	drawDef.selected.type = DrawType_None;
	drawDef.hoverPathLen  = 0;
	drawDef.targetPathLen = 0;

	Logf("Executing order");
}

//--------------------------------------------------------------------------------------------------

// Does not modify pathOut if fails
static bool TryFindPathToNeighbor(Unit const* unit, Hex const* hex, Path* pathOut) {
	Hex* closestReachableNeighbor = nullptr;
	U16 minMoveCost = U16Max;
	for (U8 i = 0; i < 6; i++) {
		Hex* const neighbor = hex->neighbors[i];
		if (!neighbor) { continue; }
		if (neighbor == unit->hex) {
			closestReachableNeighbor = unit->hex;
			break;
		}
		if (neighbor->unit) { continue; }
		U16 const neighborMoveCost = unit->pathMap.moveCosts[neighbor->idx];
		if (neighborMoveCost < minMoveCost) {
			minMoveCost = neighborMoveCost;
			closestReachableNeighbor = neighbor;
		}
	}
	if (!closestReachableNeighbor) { return false; }
	FindPathOrPanic(unit, closestReachableNeighbor, pathOut);
	return true;
}

//--------------------------------------------------------------------------------------------------

static U8 GetDir(Hex const* from, Hex const* to) {
	if (from->neighbors[Dir::TopLeft    ] == to) { return Dir::TopLeft; }
	if (from->neighbors[Dir::TopRight   ] == to) { return Dir::TopRight; }
	if (from->neighbors[Dir::Right      ] == to) { return Dir::Right; }
	if (from->neighbors[Dir::BottomRight] == to) { return Dir::BottomRight; }
	if (from->neighbors[Dir::BottomLeft ] == to) { return Dir::BottomLeft; }
	if (from->neighbors[Dir::Left       ] == to) { return Dir::Left; }
	Panic("Neighbors not adjacent");
}

//--------------------------------------------------------------------------------------------------

static Vec2 GetBorderPosForDir(Hex const* hex, U8 dir) {
	switch (dir) {
		case Dir::TopLeft    : return Vec2(hex->pos.x - (HexSize / 4), hex->pos.y - HexSize * 3 / 8);
		case Dir::TopRight   : return Vec2(hex->pos.x + (HexSize / 4), hex->pos.y - HexSize * 3 / 8);
		case Dir::Right      : return Vec2(hex->pos.x + (HexSize / 2), hex->pos.y                  );
		case Dir::BottomRight: return Vec2(hex->pos.x + (HexSize / 4), hex->pos.y + HexSize * 3 / 8);
		case Dir::BottomLeft : return Vec2(hex->pos.x - (HexSize / 4), hex->pos.y + HexSize * 3 / 8);
		case Dir::Left       : return Vec2(hex->pos.x - (HexSize / 2), hex->pos.y                  );
	}
	Panic("Neighbors not adjacent");
}

static Vec2 GetBorderPosBetween(Hex const* from, Hex const* to) {
	if (from->neighbors[Dir::TopLeft    ] == to) { return Vec2(from->pos.x - (HexSize / 4), from->pos.y - HexSize * 3 / 8); }
	if (from->neighbors[Dir::TopRight   ] == to) { return Vec2(from->pos.x + (HexSize / 4), from->pos.y - HexSize * 3 / 8); }
	if (from->neighbors[Dir::Right      ] == to) { return Vec2(from->pos.x + (HexSize / 2), from->pos.y                  ); }
	if (from->neighbors[Dir::BottomRight] == to) { return Vec2(from->pos.x + (HexSize / 4), from->pos.y + HexSize * 3 / 8); }
	if (from->neighbors[Dir::BottomLeft ] == to) { return Vec2(from->pos.x - (HexSize / 4), from->pos.y + HexSize * 3 / 8); }
	if (from->neighbors[Dir::Left       ] == to) { return Vec2(from->pos.x - (HexSize / 2), from->pos.y                  ); }
	Panic("Neighbors not adjacent");
}

//--------------------------------------------------------------------------------------------------

static U16 BuildDrawPathObjs(Hex const* startHex, Path const* path, Hex const* attackHex, DrawType base, DrawType attackType, DrawObj* drawObjs) {
	U16 len = 0;
	Hex const* from = startHex;
	for (U16 i = 0; i < path->len; i++) {
		Hex const* to = path->hexes[i];
		U8 const dir = GetDir(from, to);
		drawObjs[len++] = {
			.pos  = from->pos,
			.type = (DrawType)(base + dir),
		};
		drawObjs[len++] = {
			.pos  = to->pos,
			.type = (DrawType)(base + ((dir + 3) % 6)),	// opposite side
		};
		from = to;
	}
	if (attackHex) {
		U8 const dir = GetDir(from, attackHex);
		drawObjs[len++] = {
			.pos  = from->pos,
			.type = (DrawType)(base + dir),
		};
		drawObjs[len++] = {
			.pos  = GetBorderPosForDir(from, dir),
			.type = attackType,
		};
	}
	return len;
}

//--------------------------------------------------------------------------------------------------

static void SetTargetMoveHex(Hex* hex) {
	targetAttackHex = nullptr;
	FindPathOrPanic(selectedUnit, hex, &targetPath);
	drawDef.targetPathLen = BuildDrawPathObjs(selectedUnit->hex, &targetPath, nullptr, DrawType_TargetPathBase, DrawType_TargetAttack, drawDef.targetPath);
	Logf("Targetted (%u, %u) for move", hex->c, hex->r);
}

//--------------------------------------------------------------------------------------------------

static void SetTargetAttackHex(Hex const* hex) {
	if (hoverPath.len && HexDistance(hoverPath.hexes[hoverPath.len - 1], hex) <= selectedUnit->range) {
		targetAttackHex = hex;
		targetPath = hoverPath;
		drawDef.targetPathLen = BuildDrawPathObjs(selectedUnit->hex, &targetPath, targetAttackHex, DrawType_TargetPathBase, DrawType_TargetAttack, drawDef.targetPath);
		Logf("Targetted (%u, %u) for attack from hoverPath", targetAttackHex->c, targetAttackHex->r);

	} else if (targetPath.len && HexDistance(targetPath.hexes[targetPath.len - 1], hex) <= selectedUnit->range) {
		targetAttackHex = hex;
		drawDef.targetPathLen = BuildDrawPathObjs(selectedUnit->hex, &targetPath, targetAttackHex, DrawType_TargetPathBase, DrawType_TargetAttack, drawDef.targetPath);
		Logf("Targetted (%u, %u) for attack from targetPath", targetAttackHex->c, targetAttackHex->r);


//Problem: ambiguous what to do with target hex and hver hex pointing to the same hex.;
	} else if (TryFindPathToNeighbor(selectedUnit, hex, &targetPath)) {
		targetAttackHex = hex;
		drawDef.targetPathLen = BuildDrawPathObjs(selectedUnit->hex, &targetPath, targetAttackHex, DrawType_TargetPathBase, DrawType_TargetAttack, drawDef.targetPath);
		Logf("Targetted (%u, %u) for attack from closest neighbor", targetAttackHex->c, targetAttackHex->r);

	} else {
		Logf("Target out of range");
	}
}

//--------------------------------------------------------------------------------------------------

static void ClearTarget() {
	targetAttackHex       = nullptr;
	targetPath.len        = 0;
	drawDef.targetPathLen = 0;
	Logf("Cleared target");
}

//--------------------------------------------------------------------------------------------------

static void SetSelected(Unit* unit) {
	selectedUnit          = unit;
	hoverPath.len         = 0;
	rebuildOverlay        = true;
	drawDef.hoverPathLen  = 0;
	drawDef.selected.pos  = selectedUnit->hex->pos;
	drawDef.selected.type = DrawType_Selected;
	ClearTarget();
	Logf("Selected (%u, %u)", unit->hex->c, unit->hex->r);
}

//--------------------------------------------------------------------------------------------------

static void ClearSelected() {
	selectedUnit          = nullptr;
	hoverPath.len         = 0;
	rebuildOverlay        = true;
	drawDef.hoverPathLen  = 0;
	drawDef.selected.type = DrawType_None;
	ClearTarget();
	Logf("Cleared selected");
}

//--------------------------------------------------------------------------------------------------

void LeftClick(Hex* clickHex) {
	if (state == State::ExecutingOrder) { return; }

	Unit* const clickUnit = clickHex->unit;

	if (clickHex == targetAttackHex) {
		CreateOrder(selectedUnit, &targetPath, targetAttackHex->unit);

	} else if (!targetAttackHex && targetPath.len > 0 && clickHex == targetPath.hexes[targetPath.len - 1]) {
		CreateOrder(selectedUnit, &targetPath, nullptr);

	} else if (selectedUnit && !clickUnit && selectedUnit->pathMap.parents[clickHex->idx]) {
		SetTargetMoveHex(clickHex);

	} else if (selectedUnit &&  clickUnit && clickUnit->side != selectedUnit->side) {
		SetTargetAttackHex(clickHex);

	} else if (selectedUnit && clickUnit == selectedUnit) {
		ClearSelected();

	} else if (clickUnit &&  clickUnit->side == data->activeSide) {
		SetSelected(clickUnit);
	}
}

//--------------------------------------------------------------------------------------------------

void RightClick() {
	if (state == State::ExecutingOrder) { return; }

	if (targetPath.len > 0 || targetAttackHex) {
		ClearTarget();

	} else if (selectedUnit) {
		ClearSelected();
	}
}

//--------------------------------------------------------------------------------------------------

static void UpdateHoverHex() {
	drawDef.hover.type = DrawType_None;
	if (!hoverHex) { return; }
	drawDef.hover.pos  = hoverHex->pos;
	drawDef.hover.type = DrawType_HoverDefault;

	Unit const* const hoverUnit = hoverHex->unit;
	if (!selectedUnit) {
		if (hoverUnit) {
			if (hoverUnit->side == data->activeSide) {
				drawDef.hover.type = DrawType_HoverSelectableFriendly;
				Logf("Hover selectable friendly");
			} else {	
				drawDef.hover.type = DrawType_HoverUnattackableEnemy;
				Logf("Hover unattackable enemy");
			}
		}
	} else {
		if (!hoverUnit) {
			if (selectedUnit->pathMap.parents[hoverHex->idx]) {
				FindPathOrPanic(selectedUnit, hoverHex, &hoverPath);
				drawDef.hover.type = DrawType_HoverMoveable;
				drawDef.hoverPathLen = BuildDrawPathObjs(selectedUnit->hex, &hoverPath, nullptr, DrawType_HoverPathBase, DrawType_HoverAttack, drawDef.hoverPath);
				Logf("Hover moveable");
			}
		} else if (hoverUnit->side == data->activeSide) {
			if (hoverUnit == selectedUnit) {
				hoverPath.len = 0;
				drawDef.hoverPathLen = 0;
			}
			drawDef.hover.type = DrawType_HoverSelectableFriendly;
			Logf("Hover selectable friendly");
		} else {
			if (hoverPath.len && HexDistance(hoverPath.hexes[hoverPath.len - 1], hoverHex) <= selectedUnit->range) {
				drawDef.hover.type = DrawType_HoverAttackableEnemy;
				drawDef.hoverPathLen = BuildDrawPathObjs(selectedUnit->hex, &hoverPath, hoverHex, DrawType_HoverPathBase, DrawType_HoverAttack, drawDef.hoverPath);
				Logf("Hover attackable enemy via hover path");
			} else if (TryFindPathToNeighbor(selectedUnit, hoverHex, &hoverPath)) {
				drawDef.hover.type = DrawType_HoverAttackableEnemy;
				drawDef.hoverPathLen = BuildDrawPathObjs(selectedUnit->hex, &hoverPath, hoverHex, DrawType_HoverPathBase, DrawType_HoverAttack, drawDef.hoverPath);
				Logf("Hover attackable enemy via nearest neighbor");
			} else {
				drawDef.hover.type = DrawType_HoverUnattackableEnemy;
				Logf("Hover unattackable enemy");
			}
		}
	}
}

//--------------------------------------------------------------------------------------------------

void RebuildOverlay() {
	drawDef.overlayLen = 0;

	Army const* const friendlyArmy   = &data->armies[data->activeSide];
	Army const* const enemyArmy      = &data->armies[1 - data->activeSide];
	U64 const* const  enemyAttackMap = enemyArmy->attackMap;

	Unit const* friendlyUnit = nullptr;
	Unit const* enemyUnit    = nullptr;
	if (selectedUnit) {
		friendlyUnit = selectedUnit;
	} else if (hoverHex && hoverHex->unit) {
		if (hoverHex->unit->side == data->activeSide) {
			friendlyUnit = hoverHex->unit;
		} else {
			enemyUnit = hoverHex->unit;
		}
	}

	if (showEnemyArmyThreatMap) {
		U16 dummyMoveCosts[MaxHexes];
		U16 const* moveCosts = nullptr;
		if (friendlyUnit) {
			moveCosts = friendlyUnit->pathMap.moveCosts;
		} else {
			memset(&dummyMoveCosts, 0xff, sizeof(dummyMoveCosts));
			moveCosts = dummyMoveCosts;
		}

		for (U16 i = 0; i < MaxHexes; i++) {
			Unit const* hexUnit = data->hexes[i].unit;
			bool const friendlyMoveable = moveCosts[i] != U16Max && (!hexUnit || hexUnit->side != data->activeSide);
			bool const enemyAttackable  = enemyAttackMap[i] != 0;
			if (friendlyMoveable && enemyAttackable) {
				drawDef.overlay[drawDef.overlayLen++] = {
					.pos  = data->hexes[i].pos,
					.type = DrawType_FriendlyMoveableAndEnemyAttackable,
				};
			} else if (friendlyMoveable) {
				drawDef.overlay[drawDef.overlayLen++] = {
					.pos  = data->hexes[i].pos,
					.type = DrawType_FriendlyMoveable,
				};
			} else if (enemyAttackable) {
				drawDef.overlay[drawDef.overlayLen++] = {
					.pos  = data->hexes[i].pos,
					.type = DrawType_EnemyAttackable,
				};
			}
		}
		if (hoverHex) {
			U64 const enemyAttackBits = enemyArmy->attackMap[hoverHex->idx];
			for (U8 i = 0; i < enemyArmy->unitsLen; i++) {
				if (enemyAttackBits & ((U64)1 << i)) {
					drawDef.overlay[drawDef.overlayLen++] = {
						.pos  = enemyArmy->units[i].hex->pos,
						.type = DrawType_EnemyAttacker,
					};
				}
			}
		}

	// no enemy threat map
	} else if (friendlyUnit) {
		U64 const friendlyUnitBit = (U64)1 << (U64)(friendlyUnit - friendlyArmy->units);
		for (U16 i = 0; i < MaxHexes; i++) {
			Unit const* const hexUnit = data->hexes[i].unit;
			if (hexUnit && hexUnit->side == data->activeSide) { continue; }
			if (friendlyUnit->pathMap.parents[i]) {
				drawDef.overlay[drawDef.overlayLen++] = {
					.pos  = data->hexes[i].pos,
					.type = DrawType_FriendlyMoveable,
				};
			} else if (hexUnit && hexUnit->side != data->activeSide && (friendlyArmy->attackMap[i] & friendlyUnitBit)) {
				drawDef.overlay[drawDef.overlayLen++] = {
					.pos  = data->hexes[i].pos,
					.type = DrawType_FriendlyAttackable,
				};
			}
		}
	} else if (enemyUnit) {
		U64 const enemyUnitBit = (U64)1 << (U64)(enemyUnit - enemyArmy->units);
		for (U16 i = 0; i < MaxHexes; i++) {
			if (enemyArmy->attackMap[i] & enemyUnitBit) {
				drawDef.overlay[drawDef.overlayLen++] = {
					.pos  = data->hexes[i].pos,
					.type = DrawType_EnemyAttackable,
				};
			}
		}
	}
}

//--------------------------------------------------------------------------------------------------

static void ExecuteOrder(F32 sec) {
	OrderStep const* step = &order.steps[order.stepsIdx];
	order.elapsedSec += sec;
	if (order.elapsedSec >= step->durSec) {
		switch (step->type) {
			case OrderStepType::Move: {
				order.unit->hex->unit = nullptr;
				order.unit->hex = step->hex;
				step->hex->unit = order.unit;
				order.unit->move = Max(0u, order.unit->move - step->hex->terrain->moveCost);
				order.unitMoved = true;
				break;
			}

			case OrderStepType::AttackIn: {
				Effect::CreateFloatingStr({
					.font   = numberFont,
					.str    = SPrintf(tempMem, "-1"),
					.durSec = 2.f,
					.x      = step->unit->pos.x,
					.yStart = step->unit->pos.y - step->unit->def->size.y,
					.yEnd   = step->unit->pos.y - step->unit->def->size.y - 10.f,
				});
				step->unit->hp = Max(0u, step->unit->hp - 1);
				if (step->unit->hp == 0) {
					order.steps[order.stepsLen++] = {
						.type  = OrderStepType::Die,
						.hex   = nullptr,
						.unit  = step->unit,
						.durSec = 1.f,
					};
				}
				break;
			}

			case OrderStepType::AttackOut:
				break;

			case OrderStepType::Die: {
				Army* const army = &data->armies[order.unit->side];
				U8 const unitIdx = (U8)(order.unit - army->units);
				*order.unit = army->units[--army->unitsLen];

				for (U16 i = 0; i < MaxHexes; i++) {
					army->attackMap[i] = Bit::MoveBit(army->attackMap[i], army->unitsLen, unitIdx);
				}
				break;
			}

			default:
				Panic("Unhandled OrderStepType %u", (U32)step->type);
		}

		order.elapsedSec -= step->durSec;	// may be > 0, this is fine
		step++;
		order.stepsIdx++;

		if (order.stepsIdx >= order.stepsLen) {
			if (order.unitMoved) {
				BuildPathMap(data->hexes, order.unit);

				// TODO: move this whole thing to a "unit moved" utility
				Army* const army      = &data->armies[order.unit->side];
				U64*  const attackMap = army->attackMap;
				U64   const unitBit   = (U64)1 << (U64)(order.unit - army->units);
				for (U32 j = 0; j < MaxHexes; j++) {
					attackMap[j] &= ~unitBit;
					if (order.unit->pathMap.parents[j]) {
						attackMap[j] |= unitBit;
						continue;
					}
					Hex const* hex = &data->hexes[j];
					for (U32 k = 0; k < 6; k++) {
						Hex const* const neighbor = hex->neighbors[k];
						if (
							neighbor &&
							!neighbor->unit &&
							order.unit->pathMap.parents[neighbor->idx] &&
							HexDistance(neighbor, hex) <= order.unit->range
						) {
							attackMap[j] |= unitBit;
							break;
						}
					}
				}
			}
			state = State::WaitingForOrder;
			if (order.unit->move > 0) {
				SetSelected(order.unit);
			}
			UpdateHoverHex();
			RebuildOverlay();
			Logf("Order completed");
			return;
		}
	}

	F32 const t = order.elapsedSec / step->durSec;
	switch (step->type) {
		case OrderStepType::Move: {
			order.unit->pos = Math::Lerp(order.unit->hex->pos, step->hex->pos, t);
			break;
		}

		case OrderStepType::AttackIn: {
			Vec2 const pos = GetBorderPosBetween(order.unit->hex, step->hex);
			order.unit->pos = Math::Lerp(order.unit->hex->pos, pos, t);
			break;
		}

		case OrderStepType::AttackOut: {
			Vec2 const pos = GetBorderPosBetween(order.unit->hex, step->hex);
			order.unit->pos = Math::Lerp(pos, order.unit->hex->pos, t);
			break;
		}

		case OrderStepType::Die: {
			break;
		}

		default:
			Panic("Unhandled OrderStepType %u", (U32)step->type);
	}
}

//--------------------------------------------------------------------------------------------------

Res<> Update(App::UpdateData const* updateData) {
	rebuildOverlay = false;

	InputResult inputResult; TryTo(HandleInput(data, updateData->sec, updateData->mouseX, updateData->mouseY, updateData->actions), inputResult);

	for (U64 i = 0; i < inputResult.clickHexes.len; i++) {
		inputResult.clickHexes[i] ? LeftClick(inputResult.clickHexes[i]) : RightClick();
	}
	if (showEnemyArmyThreatMap != inputResult.showEnemyArmyThreatMap) {
		rebuildOverlay = true;
	}
	showEnemyArmyThreatMap = inputResult.showEnemyArmyThreatMap;

	if (hoverHex != inputResult.hoverHex) {
		hoverHex = inputResult.hoverHex;
		if (state != State::ExecutingOrder) {
			rebuildOverlay = true;
			UpdateHoverHex();
		}
	}

	if (rebuildOverlay && state != State::ExecutingOrder) {
		RebuildOverlay();
	}

	if (state == State::ExecutingOrder) {
		ExecuteOrder(updateData->sec);
	}

	Effect::Update(updateData->sec);

	return Ok();
}

//--------------------------------------------------------------------------------------------------

void Draw() {
	Draw(data, &drawDef);
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Battle