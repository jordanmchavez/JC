#pragma once

#include "JC/Core.h"

namespace JC::Mem {

//--------------------------------------------------------------------------------------------------

constexpr u32 AllocFlag_NoZero   = 1 << 0;
constexpr u32 AllocFlag_NoMemcpy = 1 << 1;

struct Allocator {
	virtual void* Alloc(void* ptr, u64 size, u32 flags, SrcLoc sl = SrcLoc::Here()) = 0;

	void* Alloc  (           u64 size, SrcLoc sl = SrcLoc::Here()) { return Alloc(0,   size, 0, sl); }
	void* Realloc(void* ptr, u64 size, SrcLoc sl = SrcLoc::Here()) { return Alloc(ptr, size, 0, sl); }
	void  Free   (void* ptr,           SrcLoc sl = SrcLoc::Here()) {        Alloc(ptr, 0,    0, sl); }

	template <class T> T* AllocT  (        u64 n = 1, SrcLoc sl = SrcLoc::Here()) { return (T*)Alloc(0,   n * sizeof(T), 0, sl); }
	template <class T> T* ReallocT(T* ptr, u64 n,     SrcLoc sl = SrcLoc::Here()) { return (T*)Alloc(ptr, n * sizeof(T), 0, sl); }
};

struct TempAllocator : Allocator {
	virtual void Reset() = 0;
};

Allocator*     CreateAllocator(u64 reserveSize);
TempAllocator* CreateTempAllocator(u64 reserveSize);

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Mem