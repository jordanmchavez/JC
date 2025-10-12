#pragma once

#include "JC/Common_Assert.h"
#include "JC/Common_Std.h"

namespace JC {

//--------------------------------------------------------------------------------------------------

// H must be declared using DefHandle(H)
template <class T, class H> struct HandlePool {
	struct Entry {
		T   obj = {};
		U32 gen = 0;
		U32 idx = 0;

		H Handle() const { return H { .idx = idx, .gen = gen }; }
	};

	Entry* entries;
	U32    len;
	U32    cap;
	U32    gen;
	U32    free;

	void Init(Mem::Mem mem, U32 cap_) {
		entries = Mem::AllocT<Entry>(mem, cap_);
		len     = 0;
		cap     = cap_;
		gen     = 1;
		free    = 0;
	}

	void Init(Entry* entries_, U32 cap_) {
		entries = entries_;
		len     = 0;
		cap     = cap_;
		gen     = 1;
		free    = 0;
	}

	Entry* GetEntry(H h) {
		const U32 i = h.idx;
		const U32 g = h.gen;
		Assert(i < len);
		Assert(g > 0);
		Entry* const entry = &entries[i];
		Assert(entry->gen == g);
		Assert(entry->idx == i);
		return entry;
	}

	T* Get(H h) {
		return &GetEntry(h)->obj;
	}

	Entry* Alloc() {
		U32 i = 0;
		if (free) {
			i = free;
			free = entries[free].idx;	// next
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