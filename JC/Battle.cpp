#include "JC/Battle.h"

#include "JC/App.h"
#include "JC/Draw.h"
#include "JC/File.h"
#include "JC/Gpu.h"
#include "JC/Hash.h"
#include "JC/Input.h"
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

static constexpr U32 HexSize = 32;
static constexpr U32 MaxTerrain = 64;
static constexpr F32 CamSpeedPixelsPerSec = 1000.f;

//--------------------------------------------------------------------------------------------------

static constexpr F32 Z_Hex          = 1.f;
static constexpr F32 Z_HexHighlight = 1.1f;

//--------------------------------------------------------------------------------------------------

enum : U64 {
	ActionId_Invalid = 0,

	ActionId_Exit,
	ActionId_Click,
	ActionId_ZoomIn,
	ActionId_ZoomOut,
	ActionId_ScrollMapLeft,
	ActionId_ScrollMapRight,
	ActionId_ScrollMapUp,
	ActionId_ScrollMapDown,

	ActionId_Max
};

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

static constexpr U32 MaxHeapEntries = MaxCols * MaxRows * 6;

struct HeapEntry {
	U32 dist;
	U32 idx;
};

struct Heap {
	HeapEntry* entries;
	U32        len;
	U32        cap;
};

//--------------------------------------------------------------------------------------------------

static Mem                tempMem;
static U8                 hexLut[(HexSize / 2) * (HexSize * 3 / 8)];
static Terrain*           terrain;
static U32                terrainLen;
static Map<Str, Terrain*> terrainMap;
static Hex*               hexes;
static U32                hexesLen = 0;
static Draw::Sprite       borderSprite;
static Draw::Sprite       highlightSprite;
static Vec2               windowSize;
static Draw::Camera       camera;
static Vec2               mouseWorldPos;
static Hex*               mouseHex;
static Hex*               selectedHex;
static Input::BindingSet  bindingSet;
static ActionFn*          actionFns[ActionId_Max];
static U32*               pathScore;
static Hex**              pathParent;
static bool*              pathVisited;
static Heap               pathHeap;
static Draw::Font         numberFont;
static Draw::Font         uiFont;
static F32                uiFontScale = 2.f;
static F32                uiFontLineHeight;
static Draw::Font         fancyFont;
static F32                fancyFontScale = 2.f;

static State              state;


//--------------------------------------------------------------------------------------------------

static void InitLut() {
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
}

//--------------------------------------------------------------------------------------------------

static void HeapPush(Heap* heap, HeapEntry e) {
	Assert(heap->len < heap->cap);	// should never trigger since our heap is 6*#tiles
	heap->entries[(heap->len)++] = e;
	U32 i = heap->len - 1;
	while (i > 0) {
		U32 const p = (i - 1) / 2;
		if (heap->entries[p].dist <= heap->entries[i].dist) { break; }
		HeapEntry tmp = heap->entries[p]; heap->entries[p] = heap->entries[i]; heap->entries[i] = tmp;
		i = p;
	}
}

static HeapEntry HeapPop(Heap* heap) {
	HeapEntry const result = heap->entries[0];
	heap->entries[0] = heap->entries[--heap->len];
	U32 i = 0;
	while (true) {
		U32 const l = 2 * i + 1;
		U32 const r = 2 * i + 2;
		U32 smallest = i;
		if (l < heap->len && heap->entries[l].dist < heap->entries[smallest].dist) { smallest = l; }
		if (r < heap->len && heap->entries[r].dist < heap->entries[smallest].dist) { smallest = r; }
		if (smallest == i) { break; }
		HeapEntry tmp = heap->entries[smallest]; heap->entries[smallest] = heap->entries[i]; heap->entries[i] = tmp;
		i = smallest;
	}
	return result;
}

//--------------------------------------------------------------------------------------------------

static U32 HexDistance(Hex const* a, Hex const* b) {
	I32 const aq = a->hexPos.c - (a->hexPos.r - (a->hexPos.r & 1)) / 2;
	I32 const as = a->hexPos.r;
	I32 const bq = b->hexPos.c - (b->hexPos.r - (b->hexPos.r & 1)) / 2;
	I32 const bs = b->hexPos.r;
	I32 const dq = bq - aq;
	I32 const ds = bs - as;
	I32 const dqSign = dq < 0 ? -1 : 1;
	I32 const dsSign = ds < 0 ? -1 : 1;
	if (dqSign == dsSign) {
		return Math::Abs(dq) + Math::Abs(ds);
	} else {
		return Max(Math::Abs(dq), Math::Abs(ds));
	}
}

static bool IsTilePassable(Hex const* hex, Unit::Side::Enum mySide) {
	return hex->terrain->moveCost && (!hex->unit || hex->unit->side == mySide);
}

static bool IsHexLandable(Hex const* hex) {
	return hex->terrain->moveCost && !hex->unit;
}

//--------------------------------------------------------------------------------------------------

// Finds the shortest path between two hex positions on a 64x64 odd-r offset hex grid.
//
// GRID & COORDINATES:
//   - Grid is MaxCols x MaxRows (64x64), origin (0,0) at top-left.
//   - use `hexes` as the grid data structure
//   - Odd-r offset: rows with odd zero-based index are shifted right.
//   - Neighbor deltas differ by row parity:
//       Even row: (-1,-1),(0,-1),(-1,0),(+1,0),(-1,+1),(0,+1)
//       Odd row:  (0,-1),(+1,-1),(-1,0),(+1,0),(0,+1),(+1,+1)
//
// MOVEMENT COSTS:
//   - moveCost is the cost to ENTER that cell from any neighbor.
//   - moveCost == 0 means impassable; skip as neighbor.
//   - moveCost range is small (0-3 typical).
//
// UNIT BLOCKING:
//   - Cells occupied by an enemy unit (unit->side != mySide) are impassable.
//   - Cells occupied by a friendly unit (unit->side == mySide) are passable.
//   - The moving unit's side must be passed into this function.
//
// GOAL RESOLUTION (applied before pathfinding):
//   - If end cell is passable AND unoccupied: goal set = { end }.
//   - Otherwise: goal set = all 6 neighbors of end that are passable and unoccupied.
//     - If goal set is empty: no path exists, pathOut->hexPosLen = 0, return false.
//
// OUTPUT & TRUNCATION:
//   - Start hex IS NOT included in the output path or the cost. A unit that starts in a cell has already "paid" the cost to get there.
//   - On success: write hexes start->goal into pathOut->hexes, set hexPosLen
//   - If shortest path length > MaxHexPath (128): write first MaxHexPath hexes
//   - If no path exists: set hexPosLen = 0
//
// HEURISTIC:
//   - Convert odd-r offset to axial: q = c - (r - (r & 1)) / 2, s = r.
//   - Hex distance in axial coords: if sign(dq)==sign(dr): |dq|+|dr|, else max(|dq|,|dr|).
//   - Heuristic is admissible given moveCost >= 1 for passable cells.
//
// ALGORITHM: A* with binary min-pathHeap open set, flat arrays indexed by (c + (r * MaxCols)).
bool FindPath(Hex* startHex, Hex* endHex, Path* pathOut, Unit::Side::Enum mySide) {
	Hex* goalHexes[6];
	U32 goalHexesLen = 0;
	if (IsHexLandable(endHex)) {
		if (startHex == endHex) {
			pathOut->len = 0;
			return true;
		}
		goalHexes[0] = endHex;
		goalHexesLen = 1;
	} else {
		for (U32 i = 0; i < endHex->neighborsLen; i++) {
			if (IsHexLandable(endHex->neighbors[i])) {
				if (startHex == endHex->neighbors[i]) {
					pathOut->len = 0;
					return true;
				}
				goalHexes[goalHexesLen++] = endHex->neighbors[i];
			}
		}
	}
	if (goalHexesLen == 0) {
		pathOut->len = 0;
		return false;
	}

	// TODO: should I just roll these into Hex? We're already going with the meganode approach
	// Probably not, makes it harder to reset?
	memset(pathScore,   0xff, MaxCols * MaxRows * sizeof(pathScore[0]));
	memset(pathParent,  0,    MaxCols * MaxRows * sizeof(pathParent[0]));
	memset(pathVisited, 0,    MaxCols * MaxRows * sizeof(pathVisited[0]));
	pathHeap.len = 0;

	U32 minGoalDist = U32Max;
	for (U32 i = 0; i < goalHexesLen; i++) {
		U32 const dist = HexDistance(startHex, goalHexes[i]);
		if (dist < minGoalDist) {
			minGoalDist = dist;
		}
	}
	pathScore[startHex->idx] = 0;
	HeapPush(&pathHeap, { .dist = minGoalDist, .idx = startHex->idx });

	Hex* foundGoalHex = nullptr;
	for (;;) {
		if (pathHeap.len == 0) {
			pathOut->len = 0;
			return false;
		}
		HeapEntry const entry = HeapPop(&pathHeap);
		if (pathVisited[entry.idx]) {
			continue;
		}
		pathVisited[entry.idx] = true;
		for (U32 i = 0; i < goalHexesLen; i++) {
			if (goalHexes[i] == &hexes[entry.idx]) {
				foundGoalHex = goalHexes[i];
				goto FoundGoalTile;
			}
		}

		Hex* const entryHex = &hexes[entry.idx];
		for (U32 i = 0; i < entryHex->neighborsLen; i++) {
			Hex* const neighbor = entryHex->neighbors[i];
			if (pathVisited[neighbor->idx]) {
				continue;
			}
			if (!IsTilePassable(entryHex->neighbors[i], mySide)) {
				continue;
			}
			U32 const tentativeScore = pathScore[entry.idx] + entryHex->neighbors[i]->terrain->moveCost;
			if (tentativeScore < pathScore[neighbor->idx]) {
				pathScore[neighbor->idx] = tentativeScore;
				pathParent[neighbor->idx] = &hexes[entry.idx];

				U32 minDist = U32Max;
				for (U32 g = 0; g < goalHexesLen; g++) {
					U32 const dist = HexDistance(neighbor, goalHexes[g]);
					if (dist < minDist) {
						minDist = dist;
					}
				}
				HeapPush(&pathHeap, { .dist = tentativeScore + minDist, .idx = neighbor->idx });
			}
		}
	}

	FoundGoalTile:
	Assert(foundGoalHex != startHex);
	Hex** iter = pathOut->hexes;
	for (Hex* hex = foundGoalHex; hex != startHex; hex = pathParent[hex->idx]) {
		*iter++ = hex;
	}
	U64 const len = (U64)(iter - pathOut->hexes);
	U64 const halfLen = pathOut->len / 2;
	for (U64 i = 0; i < halfLen ; i++) {
		Swap(pathOut->hexes[i], pathOut->hexes[len - i - 1]);
	}
	pathOut->len = (U32)len;

	return true;
}

//--------------------------------------------------------------------------------------------------

static Res<> Action_Exit(F32) { return App::Err_Exit(); };

//--------------------------------------------------------------------------------------------------

static Res<> Action_Click(F32) { 
	if (!mouseHex) {
		return Ok();;
	}

	if (state == State::None) {
		if (selectedHex) {
			selectedHex->highlight = false;
		}
		if (selectedHex == mouseHex) {
			selectedHex = nullptr;
		} else {
			mouseHex->highlight = true;
			mouseHex->highlightColor = Vec4(1.f, 0.f, 0.f, 0.5f);
			selectedHex = mouseHex;
		}
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

Res<> Init(Mem permMem, Mem tempMemIn, Window::State const* windowState) {
	tempMem          = tempMemIn;
	terrain          = Mem::AllocT<Terrain>(permMem, MaxTerrain);
	terrainLen       = 0;
	terrainMap.Init(permMem, MaxTerrain);
	hexes            = Mem::AllocT<Hex>(permMem, MaxRows * MaxCols);
	hexesLen         = 0;
	pathScore        = Mem::AllocT<U32> (permMem, MaxCols * MaxRows);
	pathParent       = Mem::AllocT<Hex*>(permMem, MaxCols * MaxRows);
	pathVisited      = Mem::AllocT<bool>(permMem, MaxCols * MaxRows);
	pathHeap.entries = Mem::AllocT<HeapEntry>(permMem, MaxCols * MaxRows * 6);
	pathHeap.len     = 0;
	pathHeap.cap     = MaxCols * MaxRows * 6;

	InitLut();

	windowSize = Vec2((F32)windowState->width, (F32)windowState->height);

	camera.pos = { 0.f, 0.f };
	camera.scale = 3.f;

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

	actionFns[ActionId_Exit]           = Action_Exit;
	actionFns[ActionId_Click]          = Action_Click;
	actionFns[ActionId_ZoomIn]         = Action_ZoomIn;
	actionFns[ActionId_ZoomOut]        = Action_ZoomOut;
	actionFns[ActionId_ScrollMapUp]    = Action_ScrollMapUp;
	actionFns[ActionId_ScrollMapDown]  = Action_ScrollMapDown;
	actionFns[ActionId_ScrollMapLeft]  = Action_ScrollMapLeft;
	actionFns[ActionId_ScrollMapRight] = Action_ScrollMapRight;

	TryTo(Draw::LoadFontJson("Assets/6_EverydayStandard.fontjson"), numberFont);
	TryTo(Draw::LoadFontJson("Assets/15_CelticTime.fontjson"),      uiFont);
	TryTo(Draw::LoadFontJson("Assets/21_OldeTome.fontjson"),        fancyFont);
	uiFontLineHeight = Draw::GetFontLineHeight(uiFont);

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
	I32 const c = hex->hexPos.c + cOff;
	I32 const r = hex->hexPos.r + rOff;

	if (c >= 0 && c <= MaxCols - 1 && r >= 0 && r <= MaxRows - 1) {
		hex->neighbors[hex->neighborsLen++] = &hexes[c + (r * MaxCols)];
	}
}

Res<> GenerateMap() {
	Terrain const* terrainTable[6];
	TryTo(GetTerrain("Grassland"), terrainTable[0]);
	TryTo(GetTerrain("Forest"), terrainTable[1]);
	TryTo(GetTerrain("Swamp"), terrainTable[2]);
	TryTo(GetTerrain("Hill"), terrainTable[3]);
	TryTo(GetTerrain("Mountain"), terrainTable[4]);
	TryTo(GetTerrain("Water"), terrainTable[5]);

	for (U32 c = 0; c < MaxCols; c++) {
		for (U32 r = 0; r < MaxRows; r++) {
			Hex* const hex = &hexes[c + (r * MaxCols)];
			hex->idx     = c + (r * MaxCols);
			hex->hexPos  = HexPos((I32)c, (I32)r);
			hex->terrain = terrainTable[Rng::NextU32(0, LenOf(terrainTable))];
			hex->border = false;
			hex->highlight = false;
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
		}
	}
	hexesLen = MaxCols * MaxRows;

	return Ok();
}

//--------------------------------------------------------------------------------------------------

static Hex* WorldPosToHex(Vec2 p) {
	I32 const hsize   = (I32)HexSize;
	I32 const rowStep = hsize * 3 / 4;
	I32 const iy      = (I32)p.y;
	I32 r = iy / rowStep - (iy % rowStep != 0 && iy < 0 ? 1 : 0);	// floor division
	U8  const parity  = (U8)(r & 1);

	I32 const ix  = (I32)p.x - (I32)parity * (hsize / 2);
	I32 c = ix / hsize - (ix % hsize != 0 && ix < 0 ? 1 : 0);	// floor division

	I32 const lx = ix - c * hsize;
	I32 const ly = iy - r * rowStep;
	Assert(lx >= 0 && lx < hsize && ly >= 0 && ly < rowStep);

	U8 const l = (hexLut[(ly / 2) * (hsize / 2) + (lx / 2)] >> ((lx & 1) * 4)) & 0xf;
	switch (l) {
		case 1: c -= 1 - parity; r -= 1; break;
		case 2: c += parity;     r -= 1; break;
	}

	if (c >= 0 && c < MaxCols && r >= 0 && r < MaxRows) {
		return &hexes[c + (r * MaxCols)];
	}
	return nullptr;
}

//--------------------------------------------------------------------------------------------------

static Path mousePath;

Res<> Frame(App::FrameData const* appFrameData) {
	mouseWorldPos = Vec2(
		((F32)appFrameData->mouseX / camera.scale) + camera.pos.x,
		((F32)appFrameData->mouseY / camera.scale) + camera.pos.y
	);

	for (U64 i = 0; i < appFrameData->actions.len; i++) {
		U64 const actionId = appFrameData->actions[i];
		Assert(actionId > 0 && actionId < ActionId_Max);
		Try(actionFns[actionId](appFrameData->sec));
	}
	
	if (mouseHex) {
		mouseHex->border = false;
	}
	Hex* oldMouseHex = mouseHex;
	mouseHex = WorldPosToHex(mouseWorldPos);
	if (mouseHex) {
		mouseHex->border = true;
		mouseHex->borderColor = Vec4(1.f, 1.f, 1.f, 1.f);
		if (oldMouseHex != mouseHex) {
			Logf("Mouse hex (%i, %i)", mouseHex->hexPos.c, mouseHex->hexPos.r);

			if (selectedHex) {
				for (U32 i = 0; i < mousePath.len; i++) {
					mousePath.hexes[i]->highlight = false;
				}
				bool foundPath = FindPath(selectedHex, mouseHex, &mousePath, Unit::Side::Left);
				Logf("foundPath=%t", foundPath);
				StrBuf sb(tempMem);
				sb.Add('[');
				for (U32 i = 0; i < mousePath.len; i++) {
					mousePath.hexes[i]->highlight = true;
					mousePath.hexes[i]->highlightColor = Vec4(1.f, 1.f, 1.f, 0.5f);
					sb.Printf("(%i, %i), ", mousePath.hexes[i]->hexPos.c, mousePath.hexes[i]->hexPos.r);
				}
				sb.Add(']');
				Logf("path=%s", sb.ToStr());
			}
		}
	}

	// construct shortest path from selectedMapTile -> mouseMapTile

	return Ok();
}

//--------------------------------------------------------------------------------------------------

static Vec2 ColRowToTopLeftScreenPos(U32 col, U32 row) {
	return {
		(F32)(col * HexSize + (row & 1) * (HexSize / 2)),
		(F32)(row * (HexSize * 3 / 4)),
	};
}

void Draw() {
	Draw::SetCamera(camera);

	for (U32 i = 0; i < hexesLen; i++) {
		Hex const* const hex = &hexes[i];
		Vec2 const topLeftPos = ColRowToTopLeftScreenPos(hex->hexPos.c, hex->hexPos.r);
		Draw::SpriteDrawDef drawDef = {
			.sprite = hex->terrain->sprite,
			.pos    = topLeftPos,
			.z      = Z_Hex,
			.origin = Draw::Origin::TopLeft,
		};
		Draw::DrawSprite(drawDef);

		Draw::DrawStr({
			.font   = numberFont,
			.str    = SPrintf(tempMem, "%u", hex->terrain->moveCost),
			.pos    = Vec2(topLeftPos.x + (F32)HexSize / 2.f, topLeftPos.y + (F32)HexSize / 2.f),
			.z      = Z_Hex + 0.2f,
			.origin = Draw::Origin::Center,
			.color  = Vec4(1.f, 1.f, 1.f, 1.f),
		});

		if (hex->border) {
			drawDef.sprite = borderSprite;
			drawDef.color  = hex->borderColor;
			drawDef.z = Z_HexHighlight + 0.1f;
			Draw::DrawSprite(drawDef);
		}

		if (hex->highlight) {
			drawDef.sprite = highlightSprite;
			drawDef.color  = hex->highlightColor;
			drawDef.z = Z_HexHighlight + 0.1f;
			Draw::DrawSprite(drawDef);
		}
	}
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Battle

















/*


static constexpr Vec4 MapBackgroundColor = Vec4(13.f/255.f, 30.f/255.f, 22.f/255.f, 1.f);
static constexpr F32  UiPanelWidth = 500.f;
static constexpr Vec4 UiBackgroundColor = Vec4(0.2f, 0.3f, 0.4f, 1.f);

//--------------------------------------------------------------------------------------------------


Hex GetHexByWorldPos(Vec2 p) {
	HexPos const hexPos = GetHexPosByWorldPos(p);
	HexObj** hexObjPtr = hexMap.FindOrNull(HexMapKey(hexPos));
	if (!hexObjPtr) { return Hex(); }
	return { .handle = (U64)(*hexObjPtr - hexObjs) };
}

static Unit::Side::Enum activeSide;
static Unit::Unit*      readyUnits[Unit::Side::Max];
static Unit::Unit*      doneUnits[Unit::Side::Max];

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
	Draw::SetCamera(camera);

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
		F32 const hpFrac = (F32)unit->hp / (F32)unit->unitDef->maxHp;
		F32 const x = unit->pos.x - (unit->unitDef->size.x / 2.f);
		Draw::DrawRect({
			.pos    = { x, y },
			.z      = unit->z + 1,
			.origin = Draw::Origin::BottomLeft,
			.size   = { hpFrac * unit->unitDef->size.x, HpBarHeight },
			.color  = { 1.f, 0.f, 0.f, 1.f },
		});
		Draw::DrawRect({
			.pos    = { x + hpFrac * unit->unitDef->size.x, y },
			.z      = unit->z,
			.origin = Draw::Origin::BottomLeft,
			.size   = { (1.f - hpFrac) * unit->unitDef->size.x, HpBarHeight },
			.color  = { 1.f, 0.f, 0.f, 0.5f },
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

	*/