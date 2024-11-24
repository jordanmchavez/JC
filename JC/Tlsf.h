#pragma once

#include "JC/Common.h"

namespace JC {

//--------------------------------------------------------------------------------------------------

struct Tlsf {
	u64 opaque = 0;
	
	void  Init(void* ptr, u64 size);
	void  AddChunk(void* ptr, u64 size);
	void* Alloc(u64 size);
	bool  Extend(void* ptr, u64 size);
	void  Free(void* ptr);
	bool  CheckIntegrity();
};

//--------------------------------------------------------------------------------------------------

}	// namespace JC