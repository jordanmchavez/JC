#include "JC/Battle_Internal.h"

namespace JC::Battle {

//--------------------------------------------------------------------------------------------------

static constexpr U32 MaxHeapEntries = MaxCols * MaxRows * 6;

struct HeapEntry {
	U32 dist;
	U32 pathLen;
	U32 idx;
};

struct Heap {
	HeapEntry* entries;
	U32        len;
	U32        cap;
};

//--------------------------------------------------------------------------------------------------

static bool* pathVisited;
static Heap  pathHeap;

//--------------------------------------------------------------------------------------------------

void InitPath(Mem permMem) {
	pathVisited      = Mem::AllocT<bool>(permMem, MaxCols * MaxRows);
	pathHeap.entries = Mem::AllocT<HeapEntry>(permMem, MaxCols * MaxRows * 6);
	pathHeap.len     = 0;
	pathHeap.cap     = MaxCols * MaxRows * 6;
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
void BuildPathMap(Hex const* hexes, Unit* unit) {
	PathMap* const pathMap = &unit->pathMap;
	memset(pathMap->moveCosts,  0xff, sizeof(pathMap->moveCosts));
	memset(pathMap->pathLens,   0xff, sizeof(pathMap->pathLens));
	memset(pathMap->parents,    0,    sizeof(pathMap->parents));
	memset(pathVisited,         0,    MaxCols * MaxRows * sizeof(pathVisited[0]));
	pathHeap.len = 0;

	Hex const* const startHex = unit->hex;

	pathMap->moveCosts[startHex->idx] = 0;
	pathMap->pathLens[startHex->idx] = 0;
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

		Hex const* const entryHex = &hexes[entry.idx];
		for (U32 i = 0; i < 6; i++) {
			Hex* const neighbor = entryHex->neighbors[i];
			if (!neighbor || pathVisited[neighbor->idx] || !IsHexPassable(neighbor, side)) {
				continue;
			}
			U32 const tentativeScore   = pathMap->moveCosts[entry.idx] + neighbor->terrain->moveCost;
			U32 const tentativePathLen = entry.pathLen + 1;
			bool const betterCost      = tentativeScore < pathMap->moveCosts[neighbor->idx];
			bool const fewerHops       = tentativeScore == pathMap->moveCosts[neighbor->idx] && tentativePathLen < pathMap->pathLens[neighbor->idx];
			if (betterCost || fewerHops) {
				pathMap->moveCosts[neighbor->idx] = tentativeScore;
				pathMap->pathLens[neighbor->idx]  = tentativePathLen;
				pathMap->parents[neighbor->idx]   = entryHex;
				HeapPush(&pathHeap, { .dist = tentativeScore, .pathLen = tentativePathLen, .idx = neighbor->idx });
			}
		}
	}
}

//--------------------------------------------------------------------------------------------------

// Simple back-traversal using `parents`
bool FindPath(PathMap const* pathMap, Hex const* startHex, Hex const* end, Path* pathOut) {
	if (end == startHex) {
		pathOut->len = 0;
		return true;
	}
	if (!pathMap->parents[end->idx]) {
		pathOut->len = 0;
		return false;
	}

	Hex const** iter = pathOut->hexes;
	for (Hex const* hex = end; hex != startHex; hex = pathMap->parents[hex->idx]) {
		*iter++ = hex;
	}
	U64 const len     = (U64)(iter - pathOut->hexes);
	U64 const halfLen = len / 2;
	for (U64 i = 0; i < halfLen; i++) {
		Swap(pathOut->hexes[i], pathOut->hexes[len - i - 1]);
	}
	pathOut->len = (U32)len;
	return true;
}

//--------------------------------------------------------------------------------------------------
/*
static bool IsHexLandable(Hex const* hex) {
	return hex->terrain->moveCost && !hex->unitData;
}

// A* if we ever need, possibly for projectiles and spell one-off closest target type things.
// Keep commented out until battle module done, then delete if unused
bool FindPath(Hex* startHex, Hex* endHex, Unit::Side side, Path* pathOut) {
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
				goto FoundGoalHex;
			}
		}

		Hex* const entryHex = &hexes[entry.idx];
		for (U32 i = 0; i < entryHex->neighborsLen; i++) {
			Hex* const neighbor = entryHex->neighbors[i];
			if (pathVisited[neighbor->idx]) {
				continue;
			}
			if (!IsHexPassable(entryHex->neighbors[i], side)) {
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

	FoundGoalHex:
	Assert(foundGoalHex != startHex);
	Hex** iter = pathOut->hexes;
	for (Hex* hex = foundGoalHex; hex != startHex; hex = pathParent[hex->idx]) {
		*iter++ = hex;
	}
	U64 const len = (U64)(iter - pathOut->hexes);
	U64 const halfLen = len / 2;
	for (U64 i = 0; i < halfLen ; i++) {
		Swap(pathOut->hexes[i], pathOut->hexes[len - i - 1]);
	}
	pathOut->len = (U32)len;

	return true;
}
*/
//--------------------------------------------------------------------------------------------------

}	// namespace JC::Battle