#pragma once

#include "JC/Common.h"

namespace JC {

//--------------------------------------------------------------------------------------------------

// H must be declared using DefHandle(H)
template <class T, class H> struct HandlePool {
	struct Entry {
		T   obj;
		U32 gen;
		U32 idx;	// doubles as next free index on the free list

		H Handle() const { return H { .handle = (((U64)gen) << 32) | ((U64)idx) }; }
	};

	Entry* entries;
	U32    len;
	U32    cap;
	U32    gen;
	U32    free;

	void Init(Mem mem, U32 capIn) {
		entries = Mem::AllocT<Entry>(mem, capIn);
		len     = 1;	// Reserve index 0 for invalid
		cap     = capIn;
		gen     = 1;
		free    = 0;	// free=0 -> unambiguously means no free elements: idx 0 is never allocated
	}

	Entry* GetEntry(H h) {
		U32 const i = (U32)h.handle;
		U32 const g = (U32)(h.handle >> 32);
		Assert(i > 0 && i < len);
		Assert(g > 0);
		Entry* const entry = &entries[i];
		Assert(entry->gen == g);
		return entry;
	}

	T* Get(H h) {
		return &GetEntry(h)->obj;
	}

	T* TryGet(H h) {
		U32 const i = (U32)h.handle;
		U32 const g = (U32)(h.handle >> 32);
		if (g == 0 || i == 0 || i >= len) { return nullptr; }
		Entry* const entry = &entries[i];
		if (entry->gen != g) { return nullptr; }
		return &entry->obj;
	}

	Entry* Alloc() {
		U32 i = 0;
		if (free) {
			i = free;
			free = entries[free].idx;	// next free
		} else {
			Assert(len < cap);
			i = len;
			len++;
		}

		Entry* const entry = &entries[i];
		memset(&entry->obj, 0, sizeof(entry->obj));
		entry->gen = gen;
		entry->idx = i;

		gen++;
		if (gen == U32Max) {
			gen = 1;
		}

		return entry;
	}

	void Free(H h) {
		Entry* const entry = GetEntry(h);
		entry->gen = 0;
		entry->idx = free;
		free = (U32)(entry - entries);
	}
};

//--------------------------------------------------------------------------------------------------

}	// namespace JC