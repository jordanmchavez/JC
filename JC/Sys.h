#pragma once

#include "JC/Common.h"

namespace JC {

//--------------------------------------------------------------------------------------------------

namespace Sys {
	void Abort();
	bool IsDebuggerPresent();
	void DebuggerPrint(TempAllocator* tempAllocator, s8 msg);
};

#if defined JC_COMPILER_MSVC
	#define JC_DEBUGGER_BREAK __debugbreak()
#else // JC_COMPILER
	#error("unsupported compiler")
#endif	// JC_COMPILER

//--------------------------------------------------------------------------------------------------

struct VirtualMemoryApi {
	static VirtualMemoryApi* Get();

	virtual void* Map(u64 size) = 0;
	virtual void  Unmap(const void* ptr, u64 size) = 0;
};

//--------------------------------------------------------------------------------------------------

}	// namespace JC