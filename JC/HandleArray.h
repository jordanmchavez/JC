#pragma once

#include "JC/Common.h"

namespace JC {

//--------------------------------------------------------------------------------------------------

template <class T, class H> struct HandleArray {
	struct Entry {
		T   val = {};
		u32 gen = 0;	// bottom 32 = index, top 32 = gen, doubles as freelist next
		u32 idx = 0;
	};

	Mem* mem  = 0;
	T*   data = 0;
	u32  len  = 0;
	u32  cap  = 0;
	u32  gen  = 0;
	u32  free = 0;

	void Init(Mem* memIn) {
		mem  = memIn;
		data = (T*)mem->Alloc(16 * sizeof(T));
		len  = 1;	// rserve index 0 for invalid
		cap  = 16;
		gen  = 0;
		free = 0;
	}

	T* Get(H h) {
		Assert(h.handle > 0 && h.handle < len);
		T* const val = &data[h.handle];
		Assert(val->handle == handle);
		return val;
	}

	H Alloc(SrcLoc sl = Here()) {
		u32 index = 0;
		if (free) {
			index = free;
			free = (u32)val->handle;	// next
		} else {
			if (len >= cap) {
				const u32 newCap = cap * 2;
				if (!mem->Extend(data, cap * sizeof(T), newCap * sizeof(T), sl)) {
					T* newData = (T*)mem->Alloc(newCap * sizeof(T), sl);
					MemCpy(newData, data, len * sizeof(T));
					mem->Free(data, cap * sizeof(T));
					data = newData;
				}
				cap = newCap;
			}
			index = len;
			len++;
		}

		T* const val = data[index];
		const H handle = H { .handle = ((u64)gen << 32) | (u64)index };
		val->handle = handle;

		gen++;
		if (gen == U32Max) {
			gen = 1;
		}

		return handle;
	}

	void Free(H h) {
		Assert(h.handle > 0 && h.handle < len);
		const u32 idx = h.handle & 0xffffffff;
		const u32 gen = h.handle >> 32;
		T* const val = &data[idx];
		Assert(val->gen == gen);
		val->handle = 0;
		TODO: we dont get the benefit of handle==0 if were also using it as the next free index
		val->handle = free;
		free = va
	}

	void Shutdown() {
		if (mem && data) {
			mem->Free(data, cap * sizeof(T));
		}
	}
};

//--------------------------------------------------------------------------------------------------

}	// namespace JC