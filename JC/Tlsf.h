#pragma once

#include "JC/Core.h"

namespace JC::TLSF {

//--------------------------------------------------------------------------------------------------

struct Allocator : JC::Mem::Allocator {
	virtual void AddMem(void* ptr, u64 size) = 0;
};

Allocator* CreateAllocator(void* ptr, u64 size);

//--------------------------------------------------------------------------------------------------

}	// namespace JC:TLSF