#pragma once

#include "JC/Common.h"

namespace JC {

//--------------------------------------------------------------------------------------------------

struct Mem {
	virtual void* Alloc(u64 size, SrcLoc sl = SrcLoc::Here()) = 0;
	virtual bool  Extend(void* p, u64 oldSize, u64 newSize, SrcLoc sl = SrcLoc::Here()) = 0;
	virtual void  Free(void* p, u64 size) = 0;
};

//--------------------------------------------------------------------------------------------------

struct TempMem : Mem {
	virtual u64  Mark() = 0;
	virtual void Reset(u64 mark) = 0;
	virtual void Free(void* p, u64 size) = 0;
};

//--------------------------------------------------------------------------------------------------

struct MemLeakReporter {
	virtual void LeakedScope(s8 name, u64 leakedBytes, u32 leakedAllocs, u32 leakedChildren) = 0;
	virtual void LeakedAlloc(SrcLoc sl, u64 leakedBytes, u32 leakedAllocs) = 0;
	virtual void LeakedChild(s8 name, u64 leakedBytes, u32 leakedAllocs) = 0;
};

struct MemApi {
	static MemApi* Get();

	virtual void     Init() = 0;
	virtual void     SetLeakReporter(MemLeakReporter* memLeakReporter) = 0;
	virtual void     Frame(u64 frame) = 0;
	virtual Mem*     Perm() = 0;
	virtual TempMem* Temp() = 0;
	virtual Mem*     CreateScope(s8 name, Mem* parent) = 0;
	virtual void     DestroyScope(Mem* mem) = 0;
};

//--------------------------------------------------------------------------------------------------

}	// namespace JC