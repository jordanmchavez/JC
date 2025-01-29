#pragma once

#include "JC/Common.h"

namespace JC::TLSF {

//--------------------------------------------------------------------------------------------------

struct Allocator : JC::Allocator {
	virtual void AddMem(void* ptr, u64 size) = 0;
};

Allocator* Create(void* ptr, u64 size);

//--------------------------------------------------------------------------------------------------

}	// namespace JC:TLSF