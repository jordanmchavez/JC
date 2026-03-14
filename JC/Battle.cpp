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

static void Rebuild() {
	U8 const          activeSide      = data->activeSide;

	Hex* const  hoverHex        = data->hoverHex;
	U16 const hoverHexIdx     = hoverHex ? hoverHex->idx : 0;
	Unit const* const hoverUnit       = hoverHex ? hoverHex->unit : nullptr;
	U8 const          hoverUnitSide       = hoverUnit ? hoverUnit->side : Side_Max;
	bool const        hoverUnitFriendly = hoverUnitSide == activeSide;
	bool const        hoverUnitEnemy    = hoverUnitSide == (1 - activeSide);
	Army const* const hoverUnitArmy    = hoverUnit ? &data->armies[hoverUnit->side] : nullptr;
	U8  const         hoverUnitIdx    = hoverUnit ? (U8)(hoverUnit - hoverUnitArmy->units) : 0;
	U64 const          hoverUnitBit = (U64)1 << hoverUnitIdx;
	Hex* const  selectedHex     = data->selectedHex;
	Unit const* const selectedUnit    = selectedHex ? selectedHex->unit : nullptr;
	Army const* const selectedUnitArmy    = selectedUnit ? &data->armies[selectedUnit->side] : nullptr;
	U8 const          selectedUnitIdx = selectedUnitArmy ? (U8)(selectedUnit - selectedUnitArmy->units) : 0;
	U64 const          selectedUnitBit = (U64)1 << selectedUnitIdx;
	PathMap const* const  selectedUnitPathMap = selectedUnit ? &selectedUnit->pathMap : nullptr;
	PathMap const* const  hoverUnitPathMap = hoverUnit ? &hoverUnit->pathMap : nullptr;
	Army const* const friendlyArmy = &data->armies[activeSide];
	U64 const* const friendlyAttackMap = friendlyArmy->attackMap;
	Army const* const enemyArmy      = &data->armies[1 - activeSide];
	U64 const* const  enemyAttackMap = enemyArmy->attackMap;
	Hex * const  targetHex       = data->targetHex;
	bool const showEnemyThreatMap = data->showEnemyThreatMap;
	Unit const* targetUnit = targetHex ? targetHex->unit : nullptr;

	for (U16 i = 0; i < MaxHexes; i++) {
		Hex* hex = &data->hexes[i];
		Unit const* const hexUnit = hex->unit;
		U8 const hexUnitSide = hexUnit ? hexUnit->side : Side_Max;
		U8 const hexUnitIdx = hexUnit ? ((U8)(hexUnit - data->armies[hexUnitSide].units)) : 0;
		U64 const hexUnitBit = (U64)1 << hexUnitIdx;
		bool const hexUnitEnemy = hexUnitSide == (1 - activeSide);

		U64 flags = 0;
		if (
			(!selectedUnit && hoverUnitFriendly && ((hoverUnitPathMap->parents[i] && !hexUnit) || (hexUnitEnemy && (friendlyAttackMap[i] & hoverUnitBit)))) ||
			(selectedUnit && ((selectedUnitPathMap->parents[i] && !hexUnit) || (hexUnitEnemy && (friendlyAttackMap[i] & selectedUnitBit))))
		) {
			flags |= HexFlags::FriendlyMoveableOrAttackable;
		}

		if (
			(showEnemyThreatMap && enemyAttackMap[i]) ||
			(!selectedUnit && hoverUnitEnemy && (enemyAttackMap[i] & hoverUnitBit))
		) {
			flags |= HexFlags::EnemyAttackable;
		}

		if (
			showEnemyThreatMap &&
			hoverHex &&
			hexUnitEnemy &&
			(enemyAttackMap[hoverHexIdx] & hexUnitBit)
		) {

			flags |= HexFlags::EnemyAttacker;
		}

		hex->flags = flags;
	}

	if (selectedHex) {
		selectedHex->flags |= HexFlags::Selected;
		if (targetUnit) {
			targetHex->flags |= HexFlags::SelectedAttackTarget;
		}
		if (hoverHex) {
			if (
				(!hoverUnit && selectedUnitPathMap->parents[hoverHexIdx]) ||
				(hoverUnit && hoverUnitFriendly)
			) {
				hoverHex->flags |= HexFlags::HoverValid;
			} else if (hoverUnit && hoverUnitEnemy && (friendlyAttackMap[hoverHexIdx] & selectedUnitBit)) {
				hoverHex->flags |= HexFlags::SelectedAttackTarget;
			} else {
				hoverHex->flags |= HexFlags::HoverInvalid;
			}
		}
	} else {
		if (hoverHex) {
			if (hoverUnitFriendly) {
				hoverHex->flags |= HexFlags::HoverValid;
			} else {
				hoverHex->flags |= HexFlags::HoverInvalid;
			}
		}
	}

	Path* const hoverPath = &data->hoverPath;
	Hex* prevPathEndHex = hoverPath->len ? hoverPath->hexes[hoverPath->len - 1] : nullptr;
	if (selectedUnit) {
		if (prevPathEndHex) {
			Logf("prevPathEndHex=(%u,%u)", prevPathEndHex->c, prevPathEndHex->r);
		}
		hoverPath->len = 0;

		if (hoverHex && !hoverUnit && selectedUnitPathMap->parents[hoverHexIdx]) {
			FindPath(selectedUnit, data->hoverHex, hoverPath);
			Logf("Found path to empty: %a", Addr(PathPrinter(hoverPath)));
		}

		if (hoverUnitEnemy) {
			if (prevPathEndHex && AreHexesAdjacent(prevPathEndHex, hoverHex)) {
				if (FindPath(selectedUnit, prevPathEndHex, hoverPath)) {
					hoverUnit->hex->flags |= HexFlags::SelectedAttackTarget;
					AddHoverAttackFlags(prevPathEndHex, hoverHex);
					Logf("Found path to prev adjacent: %a", Addr(PathPrinter(hoverPath)));
				}
			}
			else if (!prevPathEndHex && AreHexesAdjacent(selectedHex, hoverHex)) {
				if (FindPath(selectedUnit, data->selectedHex, hoverPath)) {
					hoverUnit->hex->flags |= HexFlags::SelectedAttackTarget;
					AddHoverAttackFlags(selectedHex, hoverHex);
					Logf("Found path to hover adjacent: %a", Addr(PathPrinter(hoverPath)));
				}
			}
			else {
				U32 minCost = U32Max;
				Hex* minNeighbor = nullptr;
				for (U32 i = 0; i < 6; i++) {
					Hex* const neighbor = hoverHex->neighbors[i];
					if (!neighbor || neighbor->unit) { continue; }
					U32 const cost = selectedUnitPathMap->moveCosts[neighbor->idx];
					if (cost < minCost) {
						minCost = cost;
						minNeighbor = neighbor;
					}
				}

				if (minNeighbor) {
					if (FindPath(selectedUnit, minNeighbor, hoverPath)) {
						hoverUnit->hex->flags |= HexFlags::SelectedAttackTarget;
						AddHoverAttackFlags(minNeighbor, hoverUnit->hex);
						Logf("Found path to min neighbor: %a", Addr(PathPrinter(hoverPath)));
					}
				}
			}
		}

		Hex* fromHex = selectedHex;
		for (U32 i = 0; i < hoverPath->len; i++) {
			Hex* toHex = hoverPath->hexes[i];
			AddHoverPathFlags(fromHex, toHex);
			fromHex = toHex;
		}
	}

	if (targetHex) {
		Path* const targetPath = &data->targetPath;

		Hex* prevPathEndHex = targetPath->len ? targetPath->hexes[targetPath->len - 1] : nullptr;
		if (prevPathEndHex) {
			Logf("prevPathEndHex=(%u,%u)", prevPathEndHex->c, prevPathEndHex->r);
		}
		targetPath->len = 0;

		if (!targetUnit) {
			FindPath(selectedUnit, targetHex, targetPath);
			Logf("Found path to empty: %a", Addr(PathPrinter(targetPath)));

		} else  {
			if (prevPathEndHex && AreHexesAdjacent(prevPathEndHex, targetHex)) {
				if (FindPath(selectedUnit, prevPathEndHex, targetPath)) {
					hoverUnit->hex->flags |= HexFlags::SelectedAttackTarget;
					AddAttackFlags(prevPathEndHex, hoverHex);
					Logf("Found path to prev adjacent: %a", Addr(PathPrinter(targetPath)));
				}
			}
			else if (!prevPathEndHex && AreHexesAdjacent(selectedHex, hoverHex)) {
				if (FindPath(selectedUnit, data->selectedHex, targetPath)) {
					hoverUnit->hex->flags |= HexFlags::SelectedAttackTarget;
					AddAttackFlags(selectedHex, hoverHex);
					Logf("Found path to hover adjacent: %a", Addr(PathPrinter(targetPath)));
				}
			}
			else {
				U32 minCost = U32Max;
				Hex* minNeighbor = nullptr;
				for (U32 i = 0; i < 6; i++) {
					Hex* const neighbor = hoverHex->neighbors[i];
					if (!neighbor || neighbor->unit) { continue; }
					U32 const cost = selectedUnitPathMap->moveCosts[neighbor->idx];
					if (cost < minCost) {
						minCost = cost;
						minNeighbor = neighbor;
					}
				}

				if (minNeighbor) {
					if (FindPath(selectedUnit, minNeighbor, targetPath)) {
						hoverUnit->hex->flags |= HexFlags::SelectedAttackTarget;
						AddAttackFlags(minNeighbor, hoverUnit->hex);
						Logf("Found path to min neighbor: %a", Addr(PathPrinter(targetPath)));
					}
				}
			}
		}
	}
}

//--------------------------------------------------------------------------------------------------

static void ExecuteOrder(Unit const* unit, Path const* movePath, Hex const* attackHex);

static Unit* selectedUnit;
static Path  hoverPath;
static Path  targetPath;
static Hex*  targetAttackHex;

//--------------------------------------------------------------------------------------------------

static bool UnitCanAttackHex(Data const* data, Unit const* unit, Hex const* hex) {
	Army const* const army = &data->armies[unit->side];
	U64 const unitIdx = (U64)(unit - army->units);
	return army->attackMap[hex->idx] & ((U64)1 << unitIdx);
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

static void ClearTarget() {
	targetPath.len  = 0;
	targetAttackHex = nullptr;
}

//--------------------------------------------------------------------------------------------------

static void SetMoveTarget(Hex* hex) {
	targetAttackHex = nullptr;

	FindPath(selectedUnit, hex, &targetPath);

	Hex* fromHex = selectedUnit->hex;
	for (U32 i = 0; i < targetPath.len; i++) {
		Hex* toHex = targetPath.hexes[i];
		fromHex = toHex;
	}

	Logf("Targetted (%u, %u) for move", hex->c, hex->r);
}

//--------------------------------------------------------------------------------------------------

static void SetAttackTarget(Hex* hex) {
	if (hoverPath.len && AreHexesAdjacent(hoverPath.hexes[hoverPath.len - 1], hex)) {
		targetPath = hoverPath;

	} else {
		bool foundPath = FindPathToNeighbor(selectedUnit, hex, &targetPath);
		Assert(foundPath);
	}
	targetAttackHex = hex;

	Logf("Targetted (%u, %u) for attack", targetAttackHex->c, targetAttackHex->r);
}

//--------------------------------------------------------------------------------------------------

static void ClearSelected() {
	selectedUnit    = nullptr;
	hoverPath.len   = 0;
	targetPath.len  = 0;
	targetAttackHex = nullptr;
	Logf("Cleared selected");
}

//--------------------------------------------------------------------------------------------------

static void SetSelected(Unit* unit) {
	selectedUnit    = unit;
	hoverPath.len   = 0;
	targetPath.len  = 0;
	targetAttackHex = nullptr;
	Logf("Selected (%u, %u)", selectedUnit->hex->c, selectedUnit->hex->r);
}

//--------------------------------------------------------------------------------------------------

void LeftClickHex(Hex* clickHex) {
	// Target -> click target -> execute order
	if (clickHex == targetAttackHex) {
		ExecuteOrder(selectedUnit, &targetPath, targetAttackHex);
		return;
	}
	if (targetPath.len > 0 && clickHex == targetPath.hexes[targetPath.len - 1]) {
		ExecuteOrder(selectedUnit, &targetPath, nullptr);
		return;
	}

	// Selected -> click reachable hex -> target
	Unit* const clickUnit = clickHex->unit;
	if (
		selectedUnit &&
		!clickUnit &&
		selectedUnit->pathMap.parents[clickHex->idx]
	) {
		SetMoveTarget(clickHex);
		return;
	}

	// Selected -> click targettable hex -> target
	if (
		selectedUnit && 
		clickUnit &&
		clickUnit->side != selectedUnit->side &&
		UnitCanAttackHex(data, selectedUnit, clickHex)		
	) {
		SetAttackTarget(clickHex);
		return;
	}

	// Selected -> click selected -> deselect
	if (clickHex == selectedUnit->hex) {

		ClearSelected();
		return;
	}

	// Selected -> click friendly unit -> select that unit
	if (
		clickUnit && 
		clickUnit->side == data->activeSide
	) {
		SetSelected(clickUnit);
		return;
	}
}

//--------------------------------------------------------------------------------------------------

void RightClick() {
	if (targetPath.len > 0 || targetAttackHex) {
		ClearTarget();
		return;
	}
	if (selectedUnit) {
		ClearSelected();
	}
}

//--------------------------------------------------------------------------------------------------

void HoverHex(Hex const* hex) {
	// target
	// selected

	// show friendly overlay
	// show enemy overlay
	// show enemy attackers for hex

	// selected -> path to hover
}

//--------------------------------------------------------------------------------------------------

Res<> Update(App::UpdateData const* updateData) {
	Try(HandleInput(data, updateData->sec, updateData->mouseX, updateData->mouseY, updateData->actions));

	return Ok();
}

//--------------------------------------------------------------------------------------------------

void Draw() {
	Draw(data);
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Battle