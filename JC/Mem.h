#pragma once

#include "JC/Common.h"

namespace JC {

//--------------------------------------------------------------------------------------------------

struct Mem {
	virtual void* Alloc(u64 size, SrcLoc sl = SrcLoc::Here()) = 0;
	virtual void* Realloc(void* p, u64 oldSize, u64 newSize, SrcLoc sl = SrcLoc::Here()) = 0;
	virtual void  Free(void* p, u64 size) = 0;

	template <class T> T*   Alloc(u64 n, SrcLoc sl = SrcLoc::Here())                      { return (T*)Alloc(n * sizeof(T), sl); }
	template <class T> T*   Realloc(T* p, u64 oldN, u64 newN, SrcLoc sl = SrcLoc::Here()) { return (T*)Realloc(p, oldN * sizeof(T), newN * sizeof(T), sl); }
	template <class T> void Free(T* p, u64 n)                                             { Free((void*)p, n * sizeof(T)); }
};

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

	virtual void Init() = 0;
	virtual void SetLeakReporter(MemLeakReporter* r) = 0;
	virtual Mem* Create(s8 name) = 0;
	virtual void Destroy(Mem* mem) = 0;
	virtual Mem* Temp() = 0;
	virtual void Frame() = 0;
};

//--------------------------------------------------------------------------------------------------

}	// namespace JC