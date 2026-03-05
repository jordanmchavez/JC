#pragma warning(disable: 4505)
#pragma warning(disable: 4189)

#include "JC/App.h"
#include "JC/Array.h"
#include "JC/BattleMap.h"
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
#include "JC/Unit.h"
#include "JC/Window.h"

namespace JC::Battle {

//--------------------------------------------------------------------------------------------------

static constexpr I32  MapCols = 16;
static constexpr I32  MapRows = 14;
static constexpr Vec4 MapBackgroundColor = Vec4(13.f/255.f, 30.f/255.f, 22.f/255.f, 1.f);
static constexpr F32  UiPanelWidth = 500.f;
static constexpr Vec4 UiBackgroundColor = Vec4(0.2f, 0.3f, 0.4f, 1.f);
static constexpr F32  CamSpeedPixelsPerSec = 1000.f;

//--------------------------------------------------------------------------------------------------

static constexpr U64 Action_Exit            = 1;
static constexpr U64 Action_Click           = 2;
static constexpr U64 Action_ZoomIn          = 3;
static constexpr U64 Action_ZoomOut         = 4;
static constexpr U64 Action_ScrollMapLeft   = 5;
static constexpr U64 Action_ScrollMapRight  = 6;
static constexpr U64 Action_ScrollMapUp     = 7;
static constexpr U64 Action_ScrollMapDown   = 8;

//--------------------------------------------------------------------------------------------------

struct MapTile {
	BMap::Hex   hex;
	Unit::Unit* unit;
	U32         moveCost;
};

enum struct State {
	None,
	UnitSelected,
	ExecutingOrder,
};

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

static Mem               permMem;
static Mem               tempMem;
static State             state;
static Vec2              windowSize;
static Input::BindingSet mainBindingSet;
static Draw::Font        numberFont;
static Draw::Font        uiFont;
static F32               uiFontScale = 2.f;
static F32               uiFontLineHeight;
static Draw::Font        fancyFont;
static F32               fancyFontScale = 2.f;
static Draw::Camera      camera;
static MapTile           mapTiles[MapCols * MapRows];
static Vec2              mouseWorldPos;
static HexPos            mouseHexPos;
static BMap::Hex         mouseHex;
static MapTile*          mouseMapTile;
static MapTile*          selectedMapTile;

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
	windowSize = Vec2((F32)windowState->width, (F32)windowState->height);

	camera.pos = { 0.f, 0.f };
	camera.scale = 3.f;

	Try(BMap::LoadBattleMapJson("Assets/BattleMap.battlemapjson"));
	BMap::Terrain terrain[6];
	TryTo(BMap::GetTerrain("Grassland"), terrain[0]);
	TryTo(BMap::GetTerrain("Forest"), terrain[1]);
	TryTo(BMap::GetTerrain("Swamp"), terrain[2]);
	TryTo(BMap::GetTerrain("Hill"), terrain[3]);
	TryTo(BMap::GetTerrain("Mountain"), terrain[4]);
	TryTo(BMap::GetTerrain("Water"), terrain[5]);

	for (I32 c = 0; c < MapCols; c++) {
		for (I32 r = 0; r < MapRows; r++) {
			U32 idx = Rng::NextU32(0, LenOf(terrain));
			BMap::Hex const hex = BMap::CreateHex(
				terrain[idx],
				{ c, r }
			);
			mapTiles[c + (r * MapCols)] = {
				.hex      = hex,
				.unit     = nullptr,
				.moveCost = BMap::GetHexMoveCost(hex),
			};
		}
	}

	TryTo(Draw::LoadFontJson("Assets/8_EverydayStandard.fontjson"), numberFont);
	TryTo(Draw::LoadFontJson("Assets/10_CelticTime.fontjson"),      uiFont);
	TryTo(Draw::LoadFontJson("Assets/21_OldeTome.fontjson"),        fancyFont);
	uiFontLineHeight = Draw::GetFontLineHeight(uiFont);

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
												          
	state = State::None;

	Try(Gpu::ImmediateWait());

	return Ok();
}

//--------------------------------------------------------------------------------------------------

static constexpr U32 MaxHexPath = MapRows * MapCols;

struct HexPath {
	MapTile const* mapTiles[MaxHexPath];
	U32            mapTilesLen;
};

static U32 HexDistance(HexPos a, HexPos b) {
	I32 const aq = a.c - (a.r - (a.r & 1)) / 2;
	I32 const as = a.r;
	I32 const bq = b.c - (b.r - (b.r & 1)) / 2;
	I32 const bs = b.r;
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

static U32 GetTileIdx(MapTile const* mapTile) { return (U32)(mapTile - mapTiles); }

static HexPos GetTileHexPos(MapTile const* mapTile) {
	U32 const d = (U32)(mapTile - mapTiles);
	return HexPos(d % MapCols, d / MapCols);
}

struct NeighborTiles {
	MapTile* tiles[6];
	U32      tilesLen;
};

static NeighborTiles GetNeighborTiles(HexPos hexPos) {
	I32 const c = hexPos.c;
	I32 const r = hexPos.r;
	NeighborTiles nt;
	nt.tilesLen = 0;
	if (c > 0                             ) { nt.tiles[nt.tilesLen++] = &mapTiles[(c - 1) + ( r      * MapCols)]; }	// (-1,  0)
	if (c < MapCols - 1                   ) { nt.tiles[nt.tilesLen++] = &mapTiles[(c + 1) + ( r      * MapCols)]; }	// (+1,  0)
	if (                   r > 0          ) { nt.tiles[nt.tilesLen++] = &mapTiles[ c      + ((r - 1) * MapCols)]; }	// ( 0, -1)
	if (                   r < MapRows - 1) { nt.tiles[nt.tilesLen++] = &mapTiles[ c      + ((r + 1) * MapCols)]; }	// ( 0, +1)
	if (r & 1) {
		// odd row: (0,-1),(+1,-1),(-1,0),(+1,0),(0,+1),(+1,+1)
		if (c < MapCols - 1 && r > 0          ) { nt.tiles[nt.tilesLen++] = &mapTiles[(c + 1) + ((r - 1) * MapCols)]; }	// (+1, -1)
		if (c < MapCols - 1 && r < MapRows - 1) { nt.tiles[nt.tilesLen++] = &mapTiles[(c + 1) + ((r + 1) * MapCols)]; }	// (+1, +1)
	} else {
		// even row: (-1,-1),(0,-1),(-1,0),(+1,0),(-1,+1),(0,+1)
		if (                   r > 0          ) { nt.tiles[nt.tilesLen++] = &mapTiles[(c - 1) + ((r - 1) * MapCols)]; }	// (-1, -1)
		if (                   r < MapRows - 1) { nt.tiles[nt.tilesLen++] = &mapTiles[(c - 1) + ((r + 1) * MapCols)]; }	// (-1, +1)
	}
	return nt;
}

static bool IsTilePassable(MapTile const* mapTile, Unit::Side::Enum mySide) {
	return mapTile->moveCost && (!mapTile->unit || mapTile->unit->side == mySide);
}

static bool IsTileLandable(MapTile const* mapTile) {
	return mapTile->moveCost && !mapTile->unit;
}

static constexpr U32 MaxHeapEntries = MapCols * MapRows * 6;
struct HeapEntry { U32 dist; U32 tileIdx; };
struct Heap {
	HeapEntry entries[MaxHeapEntries];
	U32       entriesLen = 0;
};

static void HeapPush(Heap* heap, HeapEntry e) {
	heap->entries[(heap->entriesLen)++] = e;
	U32 i = heap->entriesLen - 1;
	while (i > 0) {
		U32 const p = (i - 1) / 2;
		if (heap->entries[p].dist <= heap->entries[i].dist) { break; }
		HeapEntry tmp = heap->entries[p]; heap->entries[p] = heap->entries[i]; heap->entries[i] = tmp;
		i = p;
	}
}

static HeapEntry HeapPop(Heap* heap) {
	HeapEntry const result = heap->entries[0];
	heap->entries[0] = heap->entries[--heap->entriesLen];
	U32 i = 0;
	while (true) {
		U32 const l = 2 * i + 1;
		U32 const r = 2 * i + 2;
		U32 smallest = i;
		if (l < heap->entriesLen && heap->entries[l].dist < heap->entries[smallest].dist) { smallest = l; }
		if (r < heap->entriesLen && heap->entries[r].dist < heap->entries[smallest].dist) { smallest = r; }
		if (smallest == i) { break; }
		HeapEntry tmp = heap->entries[smallest]; heap->entries[smallest] = heap->entries[i]; heap->entries[i] = tmp;
		i = smallest;
	}
	return result;
}

// Finds the shortest path between two hex positions on a 64x64 odd-r offset hex grid.
//
// GRID & COORDINATES:
//   - Grid is MapCols x MapRows (64x64), origin (0,0) at top-left.
//   - use `mapTiles` as the grid data structure
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
//     - If goal set is empty: no path exists, hexPathOut->hexPosLen = 0, return false.
//
// OUTPUT & TRUNCATION:
//   - Start hex IS NOT included in the output path or the cost. A unit that starts in a cell has already "paid" the cost to get there.
//   - On success: write hexes start->goal into hexPathOut->mapTiles, set hexPosLen
//   - If shortest path length > MaxHexPath (128): write first MaxHexPath hexes
//   - If no path exists: set hexPosLen = 0
//
// HEURISTIC:
//   - Convert odd-r offset to axial: q = c - (r - (r & 1)) / 2, s = r.
//   - Hex distance in axial coords: if sign(dq)==sign(dr): |dq|+|dr|, else max(|dq|,|dr|).
//   - Heuristic is admissible given moveCost >= 1 for passable cells.
//
// ALGORITHM: A* with binary min-heap open set, flat arrays indexed by (c + (r * MapCols)).
bool BuildHexPath(MapTile const* startTile, MapTile const* endTile, HexPath* hexPathOut) {
	MapTile const* goalTiles[6];
	U32 goalTilesLen = 0;
	if (IsTileLandable(endTile)) {
		if (startTile == endTile) {
			hexPathOut->mapTilesLen = 0;
			return true;
		}
		goalTiles[0] = endTile;
		goalTilesLen = 1;
	} else {
		NeighborTiles const nt = GetNeighborTiles(GetTileHexPos(endTile));
		for (U32 i = 0; i < nt.tilesLen; i++) {
			if (IsTileLandable(nt.tiles[i])) {
				if (startTile == nt.tiles[i]) {
					hexPathOut->mapTilesLen = 0;
					return true;
				}
				goalTiles[goalTilesLen++] = nt.tiles[i];
			}
		}
	}
	if (goalTilesLen == 0) {
		hexPathOut->mapTilesLen = 0;
		return false;
	}

	U32 score[MapCols * MapRows];
	MapTile const* parent[MapCols * MapRows];
	bool visited[MapCols * MapRows];
	memset(score,   0xff, sizeof(score));
	memset(parent,  0,    sizeof(parent));
	memset(visited, 0,    sizeof(visited));

	Assert(startTile->unit);
	Unit::Side::Enum const mySide = startTile->unit->side;
	Heap heap;
	U32 minDist = U32Max;
	HexPos const startHexPos = GetTileHexPos(startTile);
	for (U32 i = 0; i < goalTilesLen; i++) {
		U32 const dist = HexDistance(startHexPos, GetTileHexPos(goalTiles[i]));
		if (dist < minDist) {
			minDist = dist;
		}
	}
	score[GetTileIdx(startTile)] = 0;
	HeapPush(&heap, { .dist = minDist, .tileIdx = GetTileIdx(startTile) });

	MapTile const* foundGoalTile = nullptr;
	for (;;) {
		if (heap.entriesLen == 0) {
			hexPathOut->mapTilesLen = 0;
			return false;
		}
		HeapEntry const entry = HeapPop(&heap);
		if (visited[entry.tileIdx]) {
			continue;
		}
		visited[entry.tileIdx] = true;
		for (U32 i = 0; i < goalTilesLen; i++) {
			if (goalTiles[i] == &mapTiles[entry.tileIdx]) {
				foundGoalTile = goalTiles[i];
				goto FoundGoalTile;
			}
		}

		NeighborTiles const nt = GetNeighborTiles(GetTileHexPos(&mapTiles[entry.tileIdx]));
		for (U32 i = 0; i < nt.tilesLen; i++) {
			if (visited[GetTileIdx(nt.tiles[i])]) {
				continue;
			}
			if (!IsTilePassable(nt.tiles[i], mySide)) {
				continue;
			}
			U32 const tentativeScore = score[entry.tileIdx] + nt.tiles[i]->moveCost;
			U32 const neighborIdx = GetTileIdx(nt.tiles[i]);
			if (tentativeScore < score[neighborIdx]) {
				score[neighborIdx] = tentativeScore;
				parent[neighborIdx] = &mapTiles[entry.tileIdx];

				U32 minDist = U32Max;
				HexPos const neighborHexPos = GetTileHexPos(nt.tiles[i]);
				for (U32 g = 0; g < goalTilesLen; g++) {
					U32 const dist = HexDistance(neighborHexPos, GetTileHexPos(goalTiles[g]));
					if (dist < minDist) {
						minDist = dist;
					}
				}
				HeapPush(&heap, { .dist = tentativeScore + minDist, .tileIdx = neighborIdx });
			}
		}
	}

	FoundGoalTile:
	Assert(foundGoalTile != startTile);
	MapTile const** iter = hexPathOut->mapTiles;
	for (MapTile const* tile = foundGoalTile; tile != startTile; tile = parent[GetTileIdx(tile)]) {
		*iter++ = tile;
	}
	U64 const len = (U64)(iter - hexPathOut->mapTiles);
	U64 const halfLen = hexPathOut->mapTilesLen / 2;
	for (U64 i = 0; i < halfLen ; i++) {
		Swap(hexPathOut->mapTiles[i], hexPathOut->mapTiles[len - i - 1]);
	}
	hexPathOut->mapTilesLen = len;

	return true;
}

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
*/

static void HandleLeftClick() {
	if (!mouseHex) {
		return;
	}
	if (state == State::None) {
		if (selectedMapTile) {
			BMap::HideHexHighlight(selectedMapTile->hex);
		}
		if (selectedMapTile == mouseMapTile) {
			selectedMapTile = nullptr;
		} else {
			BMap::ShowHexHighlight(mouseMapTile->hex, Vec4(1.f, 0.f, 0.f, 0.5f));
			selectedMapTile = mouseMapTile;
		}
		
	/*
		if (clickedMapTile->unit) {
			selectedMapTile = clickedMapTile;
			Logf("Selected map tile (%i, %i) with %s unit %p", clickedHexPos.col, clickedHexPos.row, clickedMapTile->unit->def->name, clickedMapTile->unit);
		} else {
			Logf("Ignoring click on empty map tile");
		}
		return;
		*/
	}
/*
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
*/
//--------------------------------------------------------------------------------------------------

Res<> Frame(App::FrameData const* frameData, Draw::Camera* cameraOut) {
	if (frameData->exit) {
		return App::Err_Exit();
	}

	mouseWorldPos = Vec2(
		((F32)frameData->mouseX / camera.scale) + camera.pos.x,
		((F32)frameData->mouseY / camera.scale) + camera.pos.y
	);

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
	
	if (mouseHex) {
		BMap::HideHexBorder(mouseHex);
	}
	BMap::Hex oldMouseHex = mouseHex;
	mouseHex = BMap::GetHexByWorldPos(mouseWorldPos);
	mouseMapTile = nullptr;
	if (mouseHex) {
		mouseHexPos = BMap::GetHexPos(mouseHex);
		BMap::ShowHexBorder(mouseHex, Vec4(1.f, 1.f, 1.f, 1.f));
		//Logf("Mouse: (%i, %i) -> (%f, %f) -> (%i, %i)", frameData->mouseX, frameData->mouseY, mouseWorldPos.x, mouseWorldPos.y, mouseHexPos.c, mouseHexPos.r);
		mouseMapTile = &mapTiles[mouseHexPos.c + mouseHexPos.r * MapCols];

		if (oldMouseHex != mouseHex) {
			Logf("Mouse hex (%i, %i)", mouseHexPos.c, mouseHexPos.r);
		}
	}

	// construct shortest path from selectedMapTile -> mouseMapTile


	/*
	if (state == State::ExecutingOrder) {
		switch (order.orderType) {
			case OrderType::Move:   ExecuteMoveOrder(frameData->secs);   break;
			case OrderType::Attack: ExecuteAttackOrder(frameData->secs); break;
			default: Panic("Unhandled OrderType %u", (U32)order.orderType);
		}
	}
	*/
	*cameraOut = camera;

	return Ok();
}

//--------------------------------------------------------------------------------------------------

Res<> Draw() {

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
	*/
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