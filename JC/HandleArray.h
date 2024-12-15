#pragma once

#include "JC/Common.h"

namespace JC {

struct Mem;

//--------------------------------------------------------------------------------------------------

template <class T, class H> struct HandleArray {
	struct Entry {
		T   val = {};
		u32 gen = 0;
		u32 idx = 0;
	};

	Mem*   mem     = 0;
	Entry* entries = 0;
	u32    len     = 0;
	u32    cap     = 0;
	u32    gen     = 1;
	u32    free    = 0;

	void Init(Mem* memIn) {
		mem     = memIn;
		entries = (Entry*)mem->Alloc(16 * sizeof(Entry));
		len     = 1;	// rserve index 0 for invalid
		cap     = 16;
		gen     = 1;
		free    = 0;
	}

	T* Get(H h) {
		const u32 i = (u32)(h.handle & 0xffffffff);
		const u32 g = (u32)(h.handle >> 32);
		Assert(i > 0 && i < len);
		Assert(g);
		Entry* const entry = &entries[i];
		Assert(entry->gen == g);
		Assert(entry->idx == i);
		return &entry->val;
	}

	T* Alloc(SrcLoc sl = SrcLoc::Here()) {
		u32 i = 0;
		if (free) {
			i = free;
			free = (u32)entries[free].idx;	// next
		} else {
			if (len >= cap) {
				const u32 newCap = cap * 2;
				if (!mem->Extend(entries, cap * sizeof(Entry), newCap * sizeof(Entry), sl)) {
					Entry* newEntries = (Entry*)mem->Alloc(newCap * sizeof(Entry), sl);
					MemCpy(newEntries, entries, len * sizeof(Entry));
					mem->Free(entries, cap * sizeof(Entry));
					entries = newEntries;
				}
				cap = newCap;
			}
			i = len;
			len++;
			Assert(len <= cap);
		}

		Entry* const entry = &entries[i];
		entry->gen = gen;
		entry->idx = i;

		gen++;
		if (gen == U32Max) {
			gen = 1;
		}

		return &entry->val;
	}

	void Free(T* val) {
		Entry* const entry = (Entry*)val;
		Assert(entry > entries && entry < entries + len);
		Assert(entry->gen);
		entry->gen = 0;
		entry->idx = free;
		free = (u32)(entry - entries);
	}

	H GetHandle(const T* val) {
		const Entry* const entry = (const Entry*)val;
		return H { .handle = ((u64)entry->gen << 32) | entry->idx };
	}

	void Shutdown() {
		if (mem && entries) {
			mem->Free(entries, cap * sizeof(Entry));
		}
		entries = 0;
		len     = 0;
		cap     = 0;
		gen     = 1;
		free    = 0;
	}
};

//--------------------------------------------------------------------------------------------------

}	// namespace JC