#pragma once

#include "JC/Common.h"

namespace JC {

//--------------------------------------------------------------------------------------------------

struct Mem {
	u8* beg;
	u8* end;

	void* Alloc(u64 size);
	void* Realloc(void* p, u64 oldSize, u64 newSize);

	static Mem Create(u64 reserve, u64 commit);
};

//--------------------------------------------------------------------------------------------------

}	// namespace JC