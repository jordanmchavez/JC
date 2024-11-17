#pragma once

#include "JC/Common.h"

namespace JC {

//--------------------------------------------------------------------------------------------------

struct MemLeakReporter {
	virtual void Begin(s8 name, u64 bytes, u32 allocs, u32 children) = 0;
	virtual void Alloc(SrcLoc sl, u64 bytes, u64 allocs) = 0;
	virtual void Child(s8 name, u64 bytes, u32 allocs) = 0;
	virtual void End() = 0;
};

struct Mem {
	virtual void* Alloc(u64 size, SrcLoc sl = SrcLoc::Here()) = 0;
	virtual void* Realloc(void* p, u64 oldSize, u64 newSize, SrcLoc sl = SrcLoc::Here()) = 0;
	virtual void  Free(void* p, u64 size) = 0;

	static  Mem*  Create(s8 name);
	static  void  Destroy(Mem* mem);

	static  Mem*  Temp();

	static  void  Frame();
};

//--------------------------------------------------------------------------------------------------

}	// namespace JC