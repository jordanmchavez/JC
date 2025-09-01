#pragma once

#include "JC/Core.h"

namespace JC {

//--------------------------------------------------------------------------------------------------

// Intrusive, assumes T.free is a T*
template <class T, U32 N> struct ObjPool {
	T   objs[N];
	U32 len;
	T*  free;

	T* Alloc() {
		if (free) {
			T* obj = free;
			free = free->next;
			return obj;
		}
		JC_ASSERT(len < N);
		return &objs[len++];
	}

	void Free(T* obj) {
		obj->next = free;
		free = obj;
	}
};

//--------------------------------------------------------------------------------------------------

// H must be of the form 
// struct { U64 handle = 0; };
template <class T, class H, U32 N> struct HandlePool {
	struct Entry {
		T   obj = {};
		U32 gen = 0;
		U32 idx = 0;

		H Handle() const { return H { .handle = ((U64)gen << 32) | idx }; }
	};

	Entry entries[N];
	U32   len  = 1;
	U32   gen  = 1;
	U32   free = 0;

	Entry* GetEntry(H h) {
		const U32 i = (U32)(h.handle & 0xffffffff);
		const U32 g = (U32)(h.handle >> 32);
		JC_ASSERT(i > 0 && i < len);
		JC_ASSERT(g);
		Entry* const entry = &entries[i];
		JC_ASSERT(entry->gen == g);
		JC_ASSERT(entry->idx == i);
		return entry;
	}

	T* Get(H h) {
		return &GetEntry(h)->obj;
	}

	Entry* Alloc() {
		U32 i = 0;
		if (free) {
			i = free;
			free = (U32)entries[free].idx;	// next
		} else {
			JC_ASSERT(len < N);
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