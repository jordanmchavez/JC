#pragma once

#include "JC/Common.h"

namespace JC {

struct MemTraceApi;

//--------------------------------------------------------------------------------------------------

struct Mem {
	virtual void* Alloc(u64 size, SrcLoc sl = SrcLoc::DefArg()) = 0;
	virtual bool  Extend(void* p, u64 size, SrcLoc sl = SrcLoc::DefArg()) = 0;
	virtual void  Free(void* p) = 0;

	template <class T> T*   AllocT(u64 n, SrcLoc sl = SrcLoc::DefArg())        { return (T*)Alloc(n * sizeof(T), sl); }
	template <class T> bool ExtendT(T* p, u64 n, SrcLoc sl = SrcLoc::DefArg()) { return Extend(p, n * sizeof(T), sl); }
};

//--------------------------------------------------------------------------------------------------

struct TempMem : Mem {
	virtual u64  Mark() = 0;
	virtual void Reset(u64 mark) = 0;

	void Free(void*) override {}
};

//--------------------------------------------------------------------------------------------------

struct MemApi {
	static MemApi* Get();

	virtual void     Init(MemTraceApi* memTraceApi) = 0;
	virtual Mem*     Perm() = 0;
	virtual TempMem* Temp() = 0;
	virtual Mem*     CreateScope(s8 name, Mem* parent) = 0;
	virtual void     DestroyScope(Mem* mem) = 0;
};

//--------------------------------------------------------------------------------------------------

}	// namespace JC