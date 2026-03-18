#include "JC/Battle_Common.h"
#include "JC/UnitTest.h"

namespace JC::Battle {

//--------------------------------------------------------------------------------------------------

static constexpr U32 MaxHeapEntries = MaxHexes * 6;

struct HeapEntry {
	U16 dist;
	U16 pathLen;
	U16 idx;
};

struct Heap {
	HeapEntry* entries;
	U32        len;
	U32        cap;
};

//--------------------------------------------------------------------------------------------------

static bool* pathVisited;	// TODO: bitmap
static U16*  pathLens;
static Heap  pathHeap;

//--------------------------------------------------------------------------------------------------

void InitPath(Mem permMem) {
	pathVisited      = Mem::AllocT<bool>(permMem, MaxHexes);
	pathLens         = Mem::AllocT<U16>(permMem, MaxHexes);
	pathHeap.entries = Mem::AllocT<HeapEntry>(permMem, MaxHexes * 6);
	pathHeap.len     = 0;
	pathHeap.cap     = MaxHexes * 6;
}

//--------------------------------------------------------------------------------------------------

static void HeapPush(Heap* heap, HeapEntry e) {
	Assert(heap->len < heap->cap);	// should never trigger since our heap is 6*#hexes
	heap->entries[(heap->len)++] = e;
	U32 i = heap->len - 1;
	while (i > 0) {
		U32 const p = (i - 1) / 2;
		HeapEntry const& ep = heap->entries[p]; HeapEntry const& ei = heap->entries[i];
		if (ep.dist < ei.dist || (ep.dist == ei.dist && ep.pathLen <= ei.pathLen)) { break; }
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
		auto heapLess = [&](U32 a, U32 b) {
			return heap->entries[a].dist < heap->entries[b].dist ||
			       (heap->entries[a].dist == heap->entries[b].dist && heap->entries[a].pathLen < heap->entries[b].pathLen);
		};
		if (l < heap->len && heapLess(l, smallest)) { smallest = l; }
		if (r < heap->len && heapLess(r, smallest)) { smallest = r; }
		if (smallest == i) { break; }
		HeapEntry tmp = heap->entries[smallest]; heap->entries[smallest] = heap->entries[i]; heap->entries[i] = tmp;
		i = smallest;
	}
	return result;
}

//--------------------------------------------------------------------------------------------------

static bool IsHexPassable(Hex const* hex, U32 side) {
	return hex->terrain->moveCost && (!hex->unit || hex->unit->side == side);
}

//--------------------------------------------------------------------------------------------------

// Djikstra flood fill
void BuildPathMap(Hex* hexes, Unit* unit) {
	U16* const  moveCosts = unit->pathMap.moveCosts;
	Hex** const parents   = unit->pathMap.parents;

	memset(moveCosts,   0xff, MaxHexes * sizeof(moveCosts[0]));
	memset(pathLens,    0xff, MaxHexes * sizeof(pathLens[0]));
	memset(parents,     0,    MaxHexes * sizeof(parents[0]));
	memset(pathVisited, 0,    MaxHexes * sizeof(pathVisited[0]));
	pathHeap.len = 0;

	Hex const* const startHex = unit->hex;

	moveCosts[startHex->idx] = 0;
	pathLens[startHex->idx] = 0;
	HeapPush(&pathHeap, { .dist = 0, .pathLen = 0, .idx = startHex->idx });

	Side const side = unit->side;

	while (pathHeap.len > 0) {
		HeapEntry const entry = HeapPop(&pathHeap);
		if (entry.dist >= unit->move) {
			break;
		}
		if (pathVisited[entry.idx]) {
			continue;
		}
		pathVisited[entry.idx] = true;

		Hex* const entryHex = &hexes[entry.idx];
		for (U32 i = 0; i < 6; i++) {
			Hex* const neighbor = entryHex->neighbors[i];
			if (!neighbor || pathVisited[neighbor->idx] || !IsHexPassable(neighbor, side)) {
				continue;
			}
			U16 const tentativeScore     = moveCosts[entry.idx] + neighbor->terrain->moveCost;
			U16 const tentativePathLen   = entry.pathLen + 1;
			bool const betterCost        = tentativeScore < moveCosts[neighbor->idx];
			bool const sameCostFewerHops = tentativeScore == moveCosts[neighbor->idx] && tentativePathLen < pathLens[neighbor->idx];
			if (betterCost || sameCostFewerHops) {
				moveCosts[neighbor->idx] = tentativeScore;
				pathLens[neighbor->idx]  = tentativePathLen;
				parents[neighbor->idx]   = entryHex;
				HeapPush(&pathHeap, { .dist = tentativeScore, .pathLen = tentativePathLen, .idx = neighbor->idx });
			}
		}
	}
}

//--------------------------------------------------------------------------------------------------

// Simple back-traversal using `parents`
void BuildPathOrPanic(Unit const* unit, Hex* end, Path* pathOut) {
	pathOut->len = 0;
	if (end == unit->hex) {
		return;
	}

	Hex* const* parents = unit->pathMap.parents;
	Assert(parents[end->idx]);

	for (Hex* hex = end; hex != unit->hex; hex = parents[hex->idx]) {
		pathOut->hexes[pathOut->len++] = hex;
	}
	U16 const halfLen = pathOut->len / 2;
	for (U64 i = 0; i < halfLen; i++) {
		Swap(pathOut->hexes[i], pathOut->hexes[pathOut->len - i - 1]);
	}
}

//--------------------------------------------------------------------------------------------------

Unit_Test("Battle_Path") {
	InitPath(testMem);

	Terrain const passable = { .moveCost = 1 };
	Terrain const costly   = { .moveCost = 3 };
	Terrain const blocked  = { .moveCost = 0 };

	//----------------------------------------------------------------------------------------------
	// BuildPathOrPanic: trivial case — destination is the unit's own hex

	Unit_SubTest("BuildPathOrPanic: same hex") {
		Hex h; memset(&h, 0, sizeof(h)); h.idx = 0; h.terrain = &passable;
		Unit unit; memset(&unit, 0, sizeof(unit));
		unit.hex = &h; unit.move = 5; unit.side = Side_Left;
		BuildPathMap(&h, &unit);
		Path path;
		BuildPathOrPanic(&unit, &h, &path);
		Unit_CheckEq(path.len, (U16)0);
	}

	//----------------------------------------------------------------------------------------------
	// BuildPathOrPanic: verify path is returned in forward (start → end) order

	Unit_SubTest("BuildPathOrPanic: forward path order") {
		// 4-hex chain; path from hex0 to hex3 should be [hex1, hex2, hex3]
		Hex hexes[4];
		memset(hexes, 0, sizeof(hexes));
		for (U32 i = 0; i < 4; i++) { hexes[i].idx = (U16)i; hexes[i].terrain = &passable; }
		for (U32 i = 0; i + 1 < 4; i++) {
			hexes[i    ].neighbors[Dir::Right] = &hexes[i + 1];
			hexes[i + 1].neighbors[Dir::Left ] = &hexes[i    ];
		}
		Unit unit; memset(&unit, 0, sizeof(unit));
		unit.hex = &hexes[0]; unit.move = 10; unit.side = Side_Left;
		BuildPathMap(hexes, &unit);
		Path path;
		BuildPathOrPanic(&unit, &hexes[3], &path);
		Unit_CheckEq(path.len, (U16)3);
		Unit_Check(path.hexes[0] == &hexes[1]);
		Unit_Check(path.hexes[1] == &hexes[2]);
		Unit_Check(path.hexes[2] == &hexes[3]);
	}

	//----------------------------------------------------------------------------------------------
	// BuildPathMap: moveCosts should equal cumulative hop count when all terrain is uniform

	Unit_SubTest("BuildPathMap: uniform cost accumulation") {
		Hex hexes[5];
		memset(hexes, 0, sizeof(hexes));
		for (U32 i = 0; i < 5; i++) { hexes[i].idx = (U16)i; hexes[i].terrain = &passable; }
		for (U32 i = 0; i + 1 < 5; i++) {
			hexes[i    ].neighbors[Dir::Right] = &hexes[i + 1];
			hexes[i + 1].neighbors[Dir::Left ] = &hexes[i    ];
		}
		Unit unit; memset(&unit, 0, sizeof(unit));
		unit.hex = &hexes[0]; unit.move = 10; unit.side = Side_Left;
		BuildPathMap(hexes, &unit);
		Unit_CheckEq(unit.pathMap.moveCosts[0], (U16)0);
		Unit_CheckEq(unit.pathMap.moveCosts[1], (U16)1);
		Unit_CheckEq(unit.pathMap.moveCosts[2], (U16)2);
		Unit_CheckEq(unit.pathMap.moveCosts[3], (U16)3);
		Unit_CheckEq(unit.pathMap.moveCosts[4], (U16)4);
	}

	//----------------------------------------------------------------------------------------------
	// BuildPathMap: hexes at cost == move are pushed and reachable; cost > move are not

	Unit_SubTest("BuildPathMap: move range cutoff") {
		Hex hexes[5];
		memset(hexes, 0, sizeof(hexes));
		for (U32 i = 0; i < 5; i++) { hexes[i].idx = (U16)i; hexes[i].terrain = &passable; }
		for (U32 i = 0; i + 1 < 5; i++) {
			hexes[i    ].neighbors[Dir::Right] = &hexes[i + 1];
			hexes[i + 1].neighbors[Dir::Left ] = &hexes[i    ];
		}
		Unit unit; memset(&unit, 0, sizeof(unit));
		unit.hex = &hexes[0]; unit.move = 3; unit.side = Side_Left;
		BuildPathMap(hexes, &unit);
		// Cost 1, 2, 3 — all have parents set
		Unit_Check(unit.pathMap.parents[1] != nullptr);
		Unit_Check(unit.pathMap.parents[2] != nullptr);
		Unit_Check(unit.pathMap.parents[3] != nullptr);
		// Cost 4 — never pushed
		Unit_Check(unit.pathMap.parents[4] == nullptr);
		Unit_CheckEq(unit.pathMap.moveCosts[4], (U16)0xffff);
		// cost == move: reachable
		Path path;
		BuildPathOrPanic(&unit, &hexes[3], &path);
	}

	//----------------------------------------------------------------------------------------------
	// BuildPathMap: moveCosts must reflect the actual terrain entry costs, not hop count

	Unit_SubTest("BuildPathMap: varied terrain cost") {
		// Chain: hex0(cost1) → hex1(cost3) → hex2(cost1) → hex3(cost1)
		// Accumulated costs from hex0: 0, 3, 4, 5
		Hex hexes[4];
		memset(hexes, 0, sizeof(hexes));
		hexes[0].idx = 0; hexes[0].terrain = &passable;
		hexes[1].idx = 1; hexes[1].terrain = &costly;
		hexes[2].idx = 2; hexes[2].terrain = &passable;
		hexes[3].idx = 3; hexes[3].terrain = &passable;
		for (U32 i = 0; i + 1 < 4; i++) {
			hexes[i    ].neighbors[Dir::Right] = &hexes[i + 1];
			hexes[i + 1].neighbors[Dir::Left ] = &hexes[i    ];
		}
		Unit unit; memset(&unit, 0, sizeof(unit));
		unit.hex = &hexes[0]; unit.move = 10; unit.side = Side_Left;
		BuildPathMap(hexes, &unit);
		Unit_CheckEq(unit.pathMap.moveCosts[0], (U16)0);
		Unit_CheckEq(unit.pathMap.moveCosts[1], (U16)3);
		Unit_CheckEq(unit.pathMap.moveCosts[2], (U16)4);
		Unit_CheckEq(unit.pathMap.moveCosts[3], (U16)5);
	}

	//----------------------------------------------------------------------------------------------
	// BuildPathMap: hexes with moveCost==0 are impassable — cannot enter or traverse them

	Unit_SubTest("BuildPathMap: impassable terrain blocks") {
		// hex1 is impassable → hex1 and hex2 both unreachable
		Hex hexes[3];
		memset(hexes, 0, sizeof(hexes));
		hexes[0].idx = 0; hexes[0].terrain = &passable;
		hexes[1].idx = 1; hexes[1].terrain = &blocked;
		hexes[2].idx = 2; hexes[2].terrain = &passable;
		for (U32 i = 0; i + 1 < 3; i++) {
			hexes[i    ].neighbors[Dir::Right] = &hexes[i + 1];
			hexes[i + 1].neighbors[Dir::Left ] = &hexes[i    ];
		}
		Unit unit; memset(&unit, 0, sizeof(unit));
		unit.hex = &hexes[0]; unit.move = 10; unit.side = Side_Left;
		BuildPathMap(hexes, &unit);
		Unit_Check(unit.pathMap.parents[1] == nullptr);
		Unit_Check(unit.pathMap.parents[2] == nullptr);
	}

	//----------------------------------------------------------------------------------------------
	// BuildPathMap: a hex occupied by an enemy unit is treated as impassable

	Unit_SubTest("BuildPathMap: enemy unit blocks") {
		Hex hexes[3];
		memset(hexes, 0, sizeof(hexes));
		for (U32 i = 0; i < 3; i++) { hexes[i].idx = (U16)i; hexes[i].terrain = &passable; }
		for (U32 i = 0; i + 1 < 3; i++) {
			hexes[i    ].neighbors[Dir::Right] = &hexes[i + 1];
			hexes[i + 1].neighbors[Dir::Left ] = &hexes[i    ];
		}
		Unit enemy; memset(&enemy, 0, sizeof(enemy)); enemy.side = Side_Right;
		hexes[1].unit = &enemy;
		Unit unit; memset(&unit, 0, sizeof(unit));
		unit.hex = &hexes[0]; unit.move = 10; unit.side = Side_Left;
		BuildPathMap(hexes, &unit);
		Unit_Check(unit.pathMap.parents[1] == nullptr);
		Unit_Check(unit.pathMap.parents[2] == nullptr);
	}

	//----------------------------------------------------------------------------------------------
	// BuildPathMap: a hex occupied by a friendly unit is passable (can traverse through it)

	Unit_SubTest("BuildPathMap: friendly unit is passable") {
		Hex hexes[3];
		memset(hexes, 0, sizeof(hexes));
		for (U32 i = 0; i < 3; i++) { hexes[i].idx = (U16)i; hexes[i].terrain = &passable; }
		for (U32 i = 0; i + 1 < 3; i++) {
			hexes[i    ].neighbors[Dir::Right] = &hexes[i + 1];
			hexes[i + 1].neighbors[Dir::Left ] = &hexes[i    ];
		}
		Unit friendly; memset(&friendly, 0, sizeof(friendly)); friendly.side = Side_Left;
		hexes[1].unit = &friendly;
		Unit unit; memset(&unit, 0, sizeof(unit));
		unit.hex = &hexes[0]; unit.move = 10; unit.side = Side_Left;
		BuildPathMap(hexes, &unit);
		Unit_Check(unit.pathMap.parents[1] != nullptr);
		Unit_Check(unit.pathMap.parents[2] != nullptr);
		Path path;
		BuildPathOrPanic(&unit, &hexes[2], &path);
		Unit_CheckEq(path.len, (U16)2);
	}

	//----------------------------------------------------------------------------------------------
	// BuildPathMap: when two paths share equal total cost, the one with fewer hops is preferred

	Unit_SubTest("BuildPathMap: tie-break fewer hops") {
		// Path A: hex0 → hex1(cost=2) → hex3(cost=1)  total=3, 2 hops
		// Path B: hex0 → hex2(cost=1) → hex4(cost=1) → hex3(cost=1)  total=3, 3 hops
		// Expected: parents[hex3] == hex1 (Path A)
		Terrain t2 = { .moveCost = 2 };
		Hex hexes[5];
		memset(hexes, 0, sizeof(hexes));
		hexes[0].idx = 0; hexes[0].terrain = &passable;
		hexes[1].idx = 1; hexes[1].terrain = &t2;
		hexes[2].idx = 2; hexes[2].terrain = &passable;
		hexes[3].idx = 3; hexes[3].terrain = &passable;
		hexes[4].idx = 4; hexes[4].terrain = &passable;
		// Path A edges
		hexes[0].neighbors[0] = &hexes[1]; hexes[1].neighbors[0] = &hexes[3];
		// Path B edges
		hexes[0].neighbors[1] = &hexes[2]; hexes[2].neighbors[0] = &hexes[4]; hexes[4].neighbors[0] = &hexes[3];
		Unit unit; memset(&unit, 0, sizeof(unit));
		unit.hex = &hexes[0]; unit.move = 10; unit.side = Side_Left;
		BuildPathMap(hexes, &unit);
		Unit_CheckEq(unit.pathMap.moveCosts[3], (U16)3);
		Unit_Check(unit.pathMap.parents[3] == &hexes[1]); // Path A wins
		Path path;
		BuildPathOrPanic(&unit, &hexes[3], &path);
		Unit_CheckEq(path.len, (U16)2);
		Unit_Check(path.hexes[0] == &hexes[1]);
		Unit_Check(path.hexes[1] == &hexes[3]);
	}

	//----------------------------------------------------------------------------------------------
	// End-to-end: build a map with varied costs, verify moveCosts and multiple BuildPathOrPanic calls

	Unit_SubTest("End-to-end") {
		// Chain: hex0 → hex1(1) → hex2(3) → hex3(1) → hex4(1)
		// Costs from hex0:   0     1         4         5         6
		// unit.move=5: hexes 0–3 reachable, hex4 is not (cost 6 > 5)
		Hex hexes[5];
		memset(hexes, 0, sizeof(hexes));
		hexes[0].idx = 0; hexes[0].terrain = &passable;
		hexes[1].idx = 1; hexes[1].terrain = &passable;
		hexes[2].idx = 2; hexes[2].terrain = &costly;
		hexes[3].idx = 3; hexes[3].terrain = &passable;
		hexes[4].idx = 4; hexes[4].terrain = &passable;
		for (U32 i = 0; i + 1 < 5; i++) {
			hexes[i    ].neighbors[Dir::Right] = &hexes[i + 1];
			hexes[i + 1].neighbors[Dir::Left ] = &hexes[i    ];
		}
		Unit unit; memset(&unit, 0, sizeof(unit));
		unit.hex = &hexes[0]; unit.move = 5; unit.side = Side_Left;
		BuildPathMap(hexes, &unit);

		// Verify accumulated costs
		Unit_CheckEq(unit.pathMap.moveCosts[0], (U16)0);
		Unit_CheckEq(unit.pathMap.moveCosts[1], (U16)1);
		Unit_CheckEq(unit.pathMap.moveCosts[2], (U16)4);
		Unit_CheckEq(unit.pathMap.moveCosts[3], (U16)5);
		Unit_CheckEq(unit.pathMap.moveCosts[4], (U16)0xffff);

		// Path to a single adjacent hex
		Path path;
		BuildPathOrPanic(&unit, &hexes[1], &path);
		Unit_CheckEq(path.len, (U16)1);
		Unit_Check(path.hexes[0] == &hexes[1]);

		// Path across the costly hex to hex3 (cost == move, still reachable)
		BuildPathOrPanic(&unit, &hexes[3], &path);
		Unit_CheckEq(path.len, (U16)3);
		Unit_Check(path.hexes[0] == &hexes[1]);
		Unit_Check(path.hexes[1] == &hexes[2]);
		Unit_Check(path.hexes[2] == &hexes[3]);

		// hex4 is beyond move range — verified via parents/moveCosts above, not callable
	}
}
//--------------------------------------------------------------------------------------------------

}	// namespace JC::Battle