#include "JC/Battle_Internal.h"

namespace JC::Battle {

//--------------------------------------------------------------------------------------------------

static constexpr U32 MaxHeapEntries = MaxCols * MaxRows * 6;

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
static U16 moveCosts[MaxHexes];
static U16 pathLens[MaxHexes];

void BuildPathMap(Hex const* hexes, Unit* unit) {
	Hex** const pathMap = unit->pathMap;
	memset(moveCosts,   0xff, sizeof(moveCosts));
	memset(pathLens,    0xff, sizeof(pathLens));
	memset(pathMap,     0,    sizeof(pathMap));
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

		Hex const* const entryHex = &hexes[entry.idx];
		for (U32 i = 0; i < 6; i++) {
			Hex* const neighbor = entryHex->neighbors[i];
			if (!neighbor || pathVisited[neighbor->idx] || !IsHexPassable(neighbor, side)) {
				continue;
			}
			U16 const tentativeScore   = moveCosts[entry.idx] + neighbor->terrain->moveCost;
			U16 const tentativePathLen = entry.pathLen + 1;
			bool const betterCost      = tentativeScore < moveCosts[neighbor->idx];
			bool const fewerHops       = tentativeScore == moveCosts[neighbor->idx] && tentativePathLen < pathLens[neighbor->idx];
			if (betterCost || fewerHops) {
				moveCosts[neighbor->idx] = tentativeScore;
				pathLens[neighbor->idx]  = tentativePathLen;
				pathMap[neighbor->idx]   = neighbor;
				HeapPush(&pathHeap, { .dist = tentativeScore, .pathLen = tentativePathLen, .idx = neighbor->idx });
			}
		}
	}
}

//--------------------------------------------------------------------------------------------------

// Simple back-traversal using `parents`
bool FindPath(Unit const* unit, Hex const* end, Path* pathOut) {
	if (end == unit->hex) {
		pathOut->len = 0;
		return true;
	}

	Hex const* const* pathMap = unit->pathMap;
	if (!pathMap[end->idx]) {
		pathOut->len = 0;
		return false;
	}

	Hex const** iter = pathOut->hexes;
	for (Hex const* hex = end; hex != unit->hex; hex = pathMap[hex->idx]) {
		*iter++ = hex;
	}
	U16 const len     = (U16)(iter - pathOut->hexes);
	U16 const halfLen = len / 2;
	for (U64 i = 0; i < halfLen; i++) {
		Swap(pathOut->hexes[i], pathOut->hexes[len - i - 1]);
	}
	pathOut->len = len;
	return true;
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Battle