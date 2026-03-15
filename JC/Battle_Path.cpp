#include "JC/Battle_Internal.h"

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
bool FindPath(Unit const* unit, Hex const* end, Path* pathOut) {
	pathOut->len = 0;
	if (end == unit->hex) {
		return true;
	}

	Hex const* const* parents = unit->pathMap.parents;
	if (!parents[end->idx]) {
		return false;
	}

	for (Hex const* hex = end; hex != unit->hex; hex = parents[hex->idx]) {
		pathOut->hexes[pathOut->len++] = hex;
	}
	U16 const halfLen = pathOut->len / 2;
	for (U64 i = 0; i < halfLen; i++) {
		Swap(pathOut->hexes[i], pathOut->hexes[pathOut->len - i - 1]);
	}
	return true;
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Battle