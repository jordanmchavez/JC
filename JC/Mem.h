#pragma once

#include "JC/Core.h"

namespace JC::Mem {

//--------------------------------------------------------------------------------------------------

struct Allocator {
	virtual void* Alloc(void* ptr, U64 ptrSize, U64 newSize, SrcLoc sl = SrcLoc::Here()) = 0;

	void* Alloc  (                        U64 newSize, SrcLoc sl = SrcLoc::Here()) { return Alloc(0,   0,       newSize, sl); }
	void* Realloc(void* ptr, U64 ptrSize, U64 newSize, SrcLoc sl = SrcLoc::Here()) { return Alloc(ptr, ptrSize, newSize, sl); }
	void  Free   (void* ptr, U64 ptrSize,              SrcLoc sl = SrcLoc::Here()) {        Alloc(ptr, ptrSize, 0,       sl); }

	template <class T> T* AllocT  (                  U64 n = 1, SrcLoc sl = SrcLoc::Here()) { return (T*)Alloc(0,   0,                n * sizeof(T), sl); }
	template <class T> T* ReallocT(T* ptr, U64 oldN, U64 n,     SrcLoc sl = SrcLoc::Here()) { return (T*)Alloc(ptr, oldN * sizeof(T), n * sizeof(T), sl); }
};

struct TempAllocator : Allocator {
	virtual void Reset() = 0;
};

Allocator*     InitAllocator(U64 commitSize, U64 reserveSize);
TempAllocator* InitTempAllocator(U64 reserveSize);

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Mem