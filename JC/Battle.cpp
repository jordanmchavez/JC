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
static DrawDef            drawDef;

static Unit const* selectedUnit;
static Path  hoverPath;
static Path  targetPath;
static Hex const*  targetAttackHex;
static Hex const* hoverHex;
static bool rebuildOverlay;

struct DrawPathData {
	Vec2              pos[6][MaxHexes];
	U16               len[6];
	Span<Vec2 const>* drawPath;
};

static DrawPathData hoverDrawPathData;
static DrawPathData targetDrawPathData;
static bool showEnemyArmyThreatMap;


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
				AddNeighbor(hex,  0, -1, NeighborIdx_TopLeft    );
				AddNeighbor(hex, +1, -1, NeighborIdx_TopRight   );
				AddNeighbor(hex, +1,  0, NeighborIdx_Right      );
				AddNeighbor(hex, +1, +1, NeighborIdx_BottomRight);
				AddNeighbor(hex,  0, +1, NeighborIdx_BottomLeft );
				AddNeighbor(hex, -1,  0, NeighborIdx_Left       );
			} else {	// even row
				AddNeighbor(hex, -1, -1, NeighborIdx_TopLeft    );
				AddNeighbor(hex,  0, -1, NeighborIdx_TopRight   );
				AddNeighbor(hex, +1,  0, NeighborIdx_Right      );
				AddNeighbor(hex,  0, +1, NeighborIdx_BottomRight);
				AddNeighbor(hex, -1, +1, NeighborIdx_BottomLeft );
				AddNeighbor(hex, -1,  0, NeighborIdx_Left       );
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
			for (U32 j = 0; j < MaxHexes; j++) {
				// directly reachable
				if (unit->pathMap.parents[j]) {
					army->attackMap[j] |= ((U64)1 << i);
					Logf("Unit (%u, %u) directly reaches (%u, %u)", unit->hex->c, unit->hex->r, data->hexes[j].c, data->hexes[j].r);
					continue;
				}
				// j has a reachable neighbor
				Hex const* hex = &data->hexes[j];
				for (U32 k = 0; k < 6; k++) {
					Hex const* const neighbor = hex->neighbors[k];
					if (
						neighbor &&
						!neighbor->unit &&
						unit->pathMap.parents[neighbor->idx] &&
						HexDistance(neighbor, hex) <= unit->range
					) {
						army->attackMap[j] |= ((U64)1 << i);
						Logf("Unit (%u, %u) neighbor reaches (%u, %u)", unit->hex->c, unit->hex->r, neighbor->c, neighbor->r);
						goto DoneNeighbors;
					}
				}
				DoneNeighbors:
				;
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

static void ExecuteOrder(Unit const* unit, Path const* movePath, Hex const* attackHex) {
	unit;movePath;attackHex;
	Logf("Executing order");
}

//--------------------------------------------------------------------------------------------------

static bool FindPathToNeighbor(Unit const* unit, Hex const* hex, Path* pathOut) {
	Hex const* closestReachableNeighbor = nullptr;
	U16 minMoveCost = U16Max;
	for (U8 i = 0; i < 6; i++) {
		Hex const* const neighbor = hex->neighbors[i];
		if (!neighbor) { continue; }
		U16 const neighborMoveCost = unit->pathMap.moveCosts[neighbor->idx];
		if (neighborMoveCost < minMoveCost) {
			minMoveCost = neighborMoveCost;
			closestReachableNeighbor = neighbor;
		}
	}
	if (!closestReachableNeighbor) { return false; }
	return FindPath(unit, closestReachableNeighbor, pathOut);
}

//--------------------------------------------------------------------------------------------------
static NeighborIdx GetNeighborIdx(Hex const* from, Hex const* to) {
	if (from->neighbors[NeighborIdx_TopLeft    ] == to) { return NeighborIdx_TopLeft; }
	if (from->neighbors[NeighborIdx_TopRight   ] == to) { return NeighborIdx_TopRight; }
	if (from->neighbors[NeighborIdx_Right      ] == to) { return NeighborIdx_Right; }
	if (from->neighbors[NeighborIdx_BottomRight] == to) { return NeighborIdx_BottomRight; }
	if (from->neighbors[NeighborIdx_BottomLeft ] == to) { return NeighborIdx_BottomLeft; }
	if (from->neighbors[NeighborIdx_Left       ] == to) { return NeighborIdx_Left; }
	Panic("Neighbors not adjacent");
}

//--------------------------------------------------------------------------------------------------

static void BuildDrawPath(Path const* path, DrawPathData* drawPathData) {
	memset(drawPathData->len, 0, sizeof(drawPathData->len));

	Hex const* from = selectedUnit->hex;
	for (U16 i = 0; i < path->len; i++) {
		Hex const* to = path->hexes[i];
		NeighborIdx const n = GetNeighborIdx(from, to);
		drawPathData->pos[n][i] = from->pos;
		drawPathData->pos[(n + 3) % 6][i] = to->pos;
		from = to;
	}

	for (U8 n = 0; n < 6; n++) {
		drawPathData->drawPath[n] = Span<Vec2 const>(drawPathData->pos[n], drawPathData->len[n]);
	}
}

//--------------------------------------------------------------------------------------------------

static void ClearDrawPath(DrawPathData* drawPathData) {
	for (U8 n = 0; n < 6; n++) {
		drawPathData->len[n] = 0;
		drawPathData->drawPath[n] = Span<Vec2 const>();
	}
}

//--------------------------------------------------------------------------------------------------

static void SetTargetMoveHex(Hex const* hex) {
	targetAttackHex = nullptr;
	FindPath(selectedUnit, hex, &targetPath);
	BuildDrawPath(&targetPath, &targetDrawPathData);
	Logf("Targetted (%u, %u) for move", hex->c, hex->r);
}

//--------------------------------------------------------------------------------------------------

static void SetTargetAttackHex(Hex const* hex) {
	if (targetPath.len) {
		if (HexDistance(targetPath.hexes[targetPath.len - 1], hex) <= selectedUnit->range) {
			targetAttackHex = hex;
			Logf("Targetted (%u, %u) for attack from targetPath", targetAttackHex->c, targetAttackHex->r);
		} else {
			Logf("Target out of range of targetPath");
		}

	} else if (hoverPath.len && HexDistance(hoverPath.hexes[hoverPath.len - 1], hex) <= selectedUnit->range) {
		targetAttackHex = hex;
		targetPath = hoverPath;
		BuildDrawPath(&targetPath, &targetDrawPathData);
		hoverPath.len = 0;
		ClearDrawPath(&hoverDrawPathData);
		Logf("Targetted (%u, %u) for attack from hoverPath", targetAttackHex->c, targetAttackHex->r);

	} else if (FindPathToNeighbor(selectedUnit, hex, &targetPath)) {
		targetAttackHex = hex;
		BuildDrawPath(&targetPath, &targetDrawPathData);
		Logf("Targetted (%u, %u) for attack from hoverPath", targetAttackHex->c, targetAttackHex->r);

	} else {
		Logf("Target out of range");
	}
}

//--------------------------------------------------------------------------------------------------

static void ClearTarget() {
	targetAttackHex = nullptr;
	targetPath.len  = 0;
	ClearDrawPath(&targetDrawPathData);
	Logf("Cleared target");
}

//--------------------------------------------------------------------------------------------------

static void SetSelected(Unit const* unit) {
	selectedUnit    = unit;
	hoverPath.len   = 0;
	rebuildOverlay  = true;
	ClearDrawPath(&hoverDrawPathData);
	ClearTarget();
	Logf("Selected (%u, %u)", unit->hex->c, unit->hex->r);
}

//--------------------------------------------------------------------------------------------------

static void ClearSelected() {
	selectedUnit    = nullptr;
	hoverPath.len   = 0;
	rebuildOverlay  = true;
	ClearDrawPath(&hoverDrawPathData);
	ClearTarget();
	Logf("Cleared selected");
}

//--------------------------------------------------------------------------------------------------

void LeftClick(Hex const* clickHex) {
	Unit const* const clickUnit = clickHex->unit;

	if (clickHex == targetAttackHex) {
		ExecuteOrder(selectedUnit, &targetPath, targetAttackHex);

	} else if (targetPath.len > 0 && clickHex == targetPath.hexes[targetPath.len - 1]) {
		ExecuteOrder(selectedUnit, &targetPath, nullptr);

	} else if (selectedUnit && !clickUnit && selectedUnit->pathMap.parents[clickHex->idx]) {
		SetTargetMoveHex(clickHex);

	} else if (selectedUnit &&  clickUnit && clickUnit->side != selectedUnit->side) {
		SetTargetAttackHex(clickHex);

	} else if (clickUnit &&  clickUnit->side == data->activeSide) {
		SetSelected(clickUnit);

	} else if (clickHex == selectedUnit->hex) {
		ClearSelected();
	}
}

//--------------------------------------------------------------------------------------------------

void RightClick() {
	if (targetPath.len > 0 || targetAttackHex) {
		ClearTarget();

	} else if (selectedUnit) {
		ClearSelected();
	}
}

//--------------------------------------------------------------------------------------------------

static void UpdateHoverHex(Hex const* newHoverHex) {
	if (hoverHex == newHoverHex) { return; }
	hoverHex = newHoverHex;
	Unit const* const hoverUnit = hoverHex->unit;
	rebuildOverlay  = true;
	drawDef.hover = DrawHover::None;

	if (!selectedUnit) {
		if (hoverUnit) {
			if (hoverUnit->side == data->activeSide) {
				drawDef.hover = DrawHover::FriendlySelectable;
			} else {	
				drawDef.hover = DrawHover::Enemy;
			}
		}
	} else {
		if (!hoverUnit) {
			if (FindPath(selectedUnit, hoverHex, &hoverPath)) {
				drawDef.hover = DrawHover::FriendlyMoveable;
			}
		} else if (hoverUnit->side == data->activeSide) {
			drawDef.hover = DrawHover::FriendlySelectable;
		} else {
			if (targetPath.len && HexDistance(targetPath.hexes[targetPath.len - 1], hoverHex) <= selectedUnit->range) {
				drawDef.hover = DrawHover::FriendlyAttackable;
			} else if (hoverPath.len && HexDistance(hoverPath.hexes[hoverPath.len - 1], hoverHex) <= selectedUnit->range) {
				drawDef.hover = DrawHover::FriendlyAttackable;
			} else if (FindPathToNeighbor(selectedUnit, hoverHex, &hoverPath)) {
				drawDef.hover = DrawHover::FriendlyAttackable;
			} else {
				drawDef.hover = DrawHover::Enemy;
			}
		}
	}
}

//--------------------------------------------------------------------------------------------------

void RebuildOverlay() {
	memset(drawDef.overlayFlags, 0, sizeof(drawDef.overlayFlags));

	if (showEnemyArmyThreatMap) {
		Army const* const army = &data->armies[1 - data->activeSide];
		U64 const* const attackMap = army->attackMap;
		for (U16 i = 0; i < MaxHexes; i++) {
			if (attackMap[i]) {
				drawDef.overlayFlags[i] |= OverlayFlags::EnemyThreat;
			}
		}
		if (hoverHex) {
			U64 const attackBits = army->attackMap[hoverHex->idx];
			for (U8 i = 0; i < army->unitsLen; i++) {
				if (attackBits & ((U64)1 << i)) {
					drawDef.overlayFlags[army->units[i].hex->idx] |= OverlayFlags::Attacker;
				}
			}
		}
	}

	Unit const* overlayUnit = selectedUnit ? selectedUnit : ((hoverHex && hoverHex->unit) ? hoverHex->unit : nullptr);
	if (overlayUnit) {
		U64 overlayUnitFlags = (overlayUnit->side == data->activeSide) ? OverlayFlags::FriendlyThreat : OverlayFlags::EnemyThreat;
		for (U16 i = 0; i < MaxHexes; i++) {
			drawDef.overlayFlags[i] |= overlayUnitFlags;
		}
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

	UpdateHoverHex(inputResult.hoverHex);

	if (rebuildOverlay) {
		RebuildOverlay();
	}

	return Ok();
}

//--------------------------------------------------------------------------------------------------

void Draw() {
	Draw(data, &drawDef);
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Battle