#include "JC/Battle_Internal.h"

#include "JC/Math.h"
#include "JC/Unit.h"

namespace JC::Battle {

//--------------------------------------------------------------------------------------------------

static constexpr U32 MaxHeapEntries = MaxCols * MaxRows * 6;

struct HeapEntry {
	U32 dist;
	U32 hops;
	U32 idx;
};

struct Heap {
	HeapEntry* entries;
	U32        len;
	U32        cap;
};

//--------------------------------------------------------------------------------------------------

static bool* pathVisited;
static U32*  pathHops;
static Heap  pathHeap;

//--------------------------------------------------------------------------------------------------

void InitPath(Mem permMem) {
	pathVisited      = Mem::AllocT<bool>(permMem, MaxCols * MaxRows);
	pathHops         = Mem::AllocT<U32>(permMem, MaxCols * MaxRows);
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
		if (ep.dist < ei.dist || (ep.dist == ei.dist && ep.hops <= ei.hops)) { break; }
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
			       (heap->entries[a].dist == heap->entries[b].dist && heap->entries[a].hops < heap->entries[b].hops);
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

static bool IsHexPassable(Hex const* hex, Unit::Side side) {
	return hex->terrain->moveCost && (!hex->unitData || hex->unitData->side == side);
}

//--------------------------------------------------------------------------------------------------

// Djikstra flood fill
void BuildPathMap(Data const* data, Hex const* startHex, U32 move, Unit::Side side, PathMap* pathMap) {
	memset(pathMap->moveCosts, 0xff, sizeof(pathMap->moveCosts));
	memset(pathMap->parents,   0,   sizeof(pathMap->parents));
	memset(pathVisited,            0,   MaxCols * MaxRows * sizeof(pathVisited[0]));
	memset(pathHops,               0xff, MaxCols * MaxRows * sizeof(pathHops[0]));
	pathHeap.len = 0;

	pathMap->moveCosts[startHex->idx] = 0;
	pathHops[startHex->idx] = 0;
	HeapPush(&pathHeap, { .dist = 0, .hops = 0, .idx = startHex->idx });

	while (pathHeap.len > 0) {
		HeapEntry const entry = HeapPop(&pathHeap);
		if (entry.dist >= move) {
			break;
		}
		if (pathVisited[entry.idx]) {
			continue;
		}
		pathVisited[entry.idx] = true;

		Hex* const entryHex = &data->hexes[entry.idx];
		for (U32 i = 0; i < 6; i++) {
			Hex* const neighbor = entryHex->neighbors[i];
			if (!neighbor || pathVisited[neighbor->idx] || !IsHexPassable(neighbor, side)) {
				continue;
			}
			U32 const tentativeScore = pathMap->moveCosts[entry.idx] + neighbor->terrain->moveCost;
			U32 const tentativeHops  = entry.hops + 1;
			bool const betterCost = tentativeScore < pathMap->moveCosts[neighbor->idx];
			bool const fewerHops  = tentativeScore == pathMap->moveCosts[neighbor->idx] && tentativeHops < pathHops[neighbor->idx];
			if (betterCost || fewerHops) {
				pathMap->moveCosts[neighbor->idx] = tentativeScore;
				pathMap->parents[neighbor->idx]   = entryHex;
				pathHops[neighbor->idx]               = tentativeHops;
				HeapPush(&pathHeap, { .dist = tentativeScore, .hops = tentativeHops, .idx = neighbor->idx });
			}
		}
	}

	// TODO: this is pointless, find a way to make it work without this conversion
	for (U32 i = 0; i < MaxCols * MaxRows; i++) {
		if (pathMap->moveCosts[i] == U32Max) {
			pathMap->moveCosts[i] = 0;
			pathMap->hexCounts[i] = 0;
		} else {
			pathMap->hexCounts[i] = pathHops[i];
		}
	}
}

//--------------------------------------------------------------------------------------------------

// Simple back-traversal using `parents`
bool FindPathFromMoveCostMap(PathMap const* pathMap, Hex const* startHex, Hex const* end, Path* pathOut) {
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
static U32 HexDistance(Hex const* a, Hex const* b) {
	I32 const aq = a->c - (a->r - (a->r & 1)) / 2;
	I32 const as = a->r;
	I32 const bq = b->c - (b->r - (b->r & 1)) / 2;
	I32 const bs = b->r;
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

static bool IsHexLandable(Hex const* hex) {
	return hex->terrain->moveCost && !hex->unitData;
}

// A*
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