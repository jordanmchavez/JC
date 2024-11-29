#pragma once

#include "JC/Common.h"

namespace JC {

struct Mem;

//--------------------------------------------------------------------------------------------------

namespace Sys {
	void Abort();
	bool IsDebuggerPresent();
	void DebuggerPrint(const char* msg);

	void* VirtualAlloc(u64 size);
	void* VirtualReserve(u64 size);
	void  VirtualCommit(void* p, u64 size);
	void  VirtualFree(void* p);
	void  VirtualDecommit(void* p, u64 size);
	Mem*  VirtualMem();
};

#if defined Compiler_Msvc
	#define Sys_DebuggerBreak() __debugbreak()
#else // Compiler_
	#error("unsupported compiler")
#endif	// Compiler_

//--------------------------------------------------------------------------------------------------
}	// namespace JC