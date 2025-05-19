#pragma once

#include "JC/Core.h"

namespace JC::Mem {

//--------------------------------------------------------------------------------------------------

constexpr U32 AllocFlag_NoInit   = 1 << 0;	// memset for new regions, memcpy for realloc'd regions

struct Allocator {
	virtual void* Alloc(void* ptr, U64 ptrSize, U64 newSize, U32 flags, SrcLoc sl = SrcLoc::Here()) = 0;

	void* Alloc  (                        U64 newSize, SrcLoc sl = SrcLoc::Here()) { return Alloc(0,   0,       newSize, 0, sl); }
	void* Realloc(void* ptr, U64 ptrSize, U64 newSize, SrcLoc sl = SrcLoc::Here()) { return Alloc(ptr, ptrSize, newSize, 0, sl); }
	void  Free   (void* ptr, U64 ptrSize,              SrcLoc sl = SrcLoc::Here()) {        Alloc(ptr, ptrSize,    0,    0, sl); }

	template <class T> T* AllocT  (                  U64 n = 1, SrcLoc sl = SrcLoc::Here()) { return (T*)Alloc(0,   0,                n * sizeof(T), 0, sl); }
	template <class T> T* ReallocT(T* ptr, U64 oldN, U64 n,     SrcLoc sl = SrcLoc::Here()) { return (T*)Alloc(ptr, oldN * sizeof(T), n * sizeof(T), 0, sl); }
};

struct TempAllocator : Allocator {
	virtual void Reset() = 0;
};

void Init(U64 permCommitSize, U64 permReserveSize, U64 tempReserveSize);

extern Allocator*     permAllocator;
extern TempAllocator* tempAllocator;

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Mem