#pragma once

#include "JC/Common.h"

namespace JC {

//--------------------------------------------------------------------------------------------------

struct Mem {
	virtual void* Alloc(u64 size, SrcLoc sl = SrcLoc::DefArg()) = 0;
	virtual void* Realloc(void* p, u64 oldSize, u64 newSize, SrcLoc sl = SrcLoc::DefArg()) = 0;
	virtual void  Free(void* p, u64 size) = 0;

	template <class T> T*   Alloc(u64 n, SrcLoc sl = SrcLoc::DefArg())                      { return (T*)Alloc(n * sizeof(T), sl); }
	template <class T> T*   Realloc(T* p, u64 oldN, u64 newN, SrcLoc sl = SrcLoc::DefArg()) { return (T*)Realloc(p, oldN * sizeof(T), newN * sizeof(T), sl); }
	template <class T> void Free(T* p, u64 n)                                             { Free((void*)p, n * sizeof(T)); }
};

//--------------------------------------------------------------------------------------------------

struct TempMem : Mem {
	virtual u64  Mark() = 0;
	virtual void Reset(u64 mark) = 0;

	void Free(void*, u64) override {}
};

//--------------------------------------------------------------------------------------------------

struct MemLeakReporter {
	virtual void Begin(s8 name, u64 bytes, u32 allocs, u32 children) = 0;
	virtual void Alloc(SrcLoc sl, u64 bytes, u64 allocs) = 0;
	virtual void Child(s8 name, u64 bytes, u32 allocs) = 0;
	virtual void End() = 0;
};

struct MemApi {
	static MemApi* Get();

	virtual void     Init() = 0;
	virtual void     SetLeakReporter(MemLeakReporter* r) = 0;
	virtual Mem*     Create(u64 size) = 0;
	virtual Mem*     CreateScope(s8 name, Mem* parent) = 0;
	virtual Mem*     DestroyScope(Mem* mem) = 0;
	virtual TempMem* CreateTemp(u64 size) = 0;
};

//--------------------------------------------------------------------------------------------------

}	// namespace JC