#pragma once

#include "JC/Common.h"

namespace JC::Mem {

//--------------------------------------------------------------------------------------------------

struct Allocator {
	virtual void* Alloc(u64 size, SrcLoc sl = SrcLoc::Here()) = 0;
	virtual bool  Extend(void* ptr, u64 size, u64 newSize, SrcLoc sl = SrcLoc::Here()) = 0;
		    void* Realloc(void* ptr, u64 size, u64 newSize, SrcLoc sl = SrcLoc::Here());
	virtual void  Free(void* ptr, u64 size, SrcLoc sl = SrcLoc::Here()) = 0;

	template <class T> T*    AllocT(u64 n = 1, SrcLoc sl = SrcLoc::Here()) { return (T*)Alloc(n * sizeof(T),sl); }
	template <class T> bool  ExtendT(T* ptr, u64 n, u64 newN, SrcLoc sl = SrcLoc::Here()) { return Extend(ptr, n * sizeof(T), newN * sizeof(T), sl); }
	template <class T> T*    ReallocT(T* ptr, u64 n, u64 newN, SrcLoc sl = SrcLoc::Here()) { return (T*)Realloc(ptr, n * sizeof(T), newN * sizeof(T), sl); }
	template <class T> void  FreeT(T* ptr, u64 n, SrcLoc sl = SrcLoc::Here()) { Free(ptr, n * sizeof(T), sl); }
};

struct TempAllocator : Allocator {
	void Free(void*, u64, SrcLoc) override {}
	virtual void Reset() = 0;
};

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Mem