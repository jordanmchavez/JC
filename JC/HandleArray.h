#pragma once

#include "JC/Core.h"

namespace JC {

//--------------------------------------------------------------------------------------------------

template <class T, class H> struct HandleArray {
	struct Entry {
		T   obj = {};
		U32 gen = 0;
		U32 idx = 0;
	};

	Mem::Allocator* allocator = 0;
	Entry*          entries   = 0;
	U32             len       = 0;
	U32             cap       = 0;
	U32             gen       = 1;
	U32             free      = 0;

	void Init(Mem::Allocator* allocatorIn) {
		allocator = allocatorIn;
		entries   = allocator->AllocT<Entry>(16);
		len       = 1;	// rserve index 0 for invalid
		cap       = 16;
		gen       = 1;
		free      = 0;
	}

	Entry* GetEntry(H h) {
		const U32 i = (U32)(h.handle & 0xffffffff);
		const U32 g = (U32)(h.handle >> 32);
		Assert(i > 0 && i < len);
		Assert(g);
		Entry* const entry = &entries[i];
		Assert(entry->gen == g);
		Assert(entry->idx == i);
		return entry;
	}

	T* Get(H h) {
		return &GetEntry(h)->obj;
	}

	H Alloc(SrcLoc sl = SrcLoc::Here()) {
		U32 i = 0;
		if (free) {
			i = free;
			free = (U32)entries[free].idx;	// next
		} else {
			if (len >= cap) {
				const U32 newCap = cap * 2;
				entries = allocator->ReallocT<Entry>(entries, cap, newCap, sl);
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

		return H { .handle = ((U64)entry->gen << 32) | i };
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