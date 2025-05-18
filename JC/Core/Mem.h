#pragma once

#include "JC/Core.h"

namespace JC::Mem {

//--------------------------------------------------------------------------------------------------

constexpr u32 AllocFlag_NoInit   = 1 << 0;	// memset for new regions, memcpy for realloc'd regions

struct Allocator {
	virtual void* Alloc(void* ptr, u64 ptrSize, u64 newSize, u32 flags, SrcLoc sl = SrcLoc::Here()) = 0;

	void* Alloc  (                        u64 newSize, SrcLoc sl = SrcLoc::Here()) { return Alloc(0,   0,       newSize, 0, sl); }
	void* Realloc(void* ptr, u64 ptrSize, u64 newSize, SrcLoc sl = SrcLoc::Here()) { return Alloc(ptr, ptrSize, newSize, 0, sl); }
	void  Free   (void* ptr, u64 ptrSize,              SrcLoc sl = SrcLoc::Here()) {        Alloc(ptr, ptrSize,    0,    0, sl); }

	template <class T> T* AllocT  (                  u64 n = 1, SrcLoc sl = SrcLoc::Here()) { return (T*)Alloc(0,   0,                n * sizeof(T), 0, sl); }
	template <class T> T* ReallocT(T* ptr, u64 oldN, u64 n,     SrcLoc sl = SrcLoc::Here()) { return (T*)Alloc(ptr, oldN * sizeof(T), n * sizeof(T), 0, sl); }
};

struct TempAllocator : Allocator {
	virtual void Reset() = 0;
};

void Init(u64 permCommitSize, u64 permReserveSize, u64 tempReserveSize);

extern Allocator*     permAllocator;
extern TempAllocator* tempAllocator;

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Mem