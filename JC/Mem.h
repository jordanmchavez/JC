#pragma once

#include "JC/Common.h"

namespace JC {

struct MemTraceApi;

//--------------------------------------------------------------------------------------------------

struct Mem {
	virtual void* Alloc(u64 size, SrcLoc sl = SrcLoc::DefArg()) = 0;
	virtual bool  Extend(void* p, u64 size, SrcLoc sl = SrcLoc::DefArg()) = 0;
	virtual void  Free(void* p) = 0;
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
	virtual void     Frame(u64 frame) = 0;
	virtual Mem*     Perm() = 0;
	virtual TempMem* Temp() = 0;
	virtual Mem*     CreateScope(s8 name, Mem* parent) = 0;
	virtual void     DestroyScope(Mem* mem) = 0;
};

//--------------------------------------------------------------------------------------------------

}	// namespace JC