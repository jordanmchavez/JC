#pragma once

#include "JC/Common.h"

namespace JC {

//--------------------------------------------------------------------------------------------------

struct Allocator {
	virtual void* Alloc(u64 size, SrcLoc srcLoc = SrcLoc::Here()) = 0;
	virtual void* Realloc(void* p, u64 size, u64 newSize, SrcLoc srcLoc = SrcLoc::Here()) = 0;
	virtual void  Free(void* p, u64 size) = 0;
};

struct AllocatorApi {
	static AllocatorApi* Get();

	virtual void       Init() = 0;
	virtual void       Shutdown() = 0;

	virtual Allocator* Create(s8 name, Allocator* parent) = 0;
	virtual void       Destroy(Allocator* allocator) = 0;

	virtual Allocator* Temp() = 0;
	virtual void       ResetTemp() = 0;
};

//--------------------------------------------------------------------------------------------------

}	// namespace JC