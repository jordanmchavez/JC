#pragma once

#include "JC/Common.h"

namespace JC {

//--------------------------------------------------------------------------------------------------

namespace Sys {
	void Abort();
	bool IsDebuggerPresent();
	void DebuggerPrint(const char* msg);

	void* VirtualReserve(u64 size);
	void  VirtualCommit(const void* p, u64 size);
	void  VirtualFree(const void* p);
};

#if defined Compiler_Msvc
	#define Sys_DebuggerBreak() __debugbreak()
#else // Compiler_
	#error("unsupported compiler")
#endif	// Compiler_

//--------------------------------------------------------------------------------------------------
}	// namespace JC