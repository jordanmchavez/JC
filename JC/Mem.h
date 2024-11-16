#pragma once

#include "JC/Common.h"

namespace JC {

//--------------------------------------------------------------------------------------------------

struct Mem {
	u8* beg;
	u8* end;

	void* Alloc(u64 size);
	void* Realloc(void* p, u64 oldSize, u64 newSize);

	static void Init();
	static Mem  Perm();
	static Mem  Scratch();
};

//--------------------------------------------------------------------------------------------------

}	// namespace JC