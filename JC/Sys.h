#pragma once

#include "JC/Core.h"

namespace JC::Sys {

//--------------------------------------------------------------------------------------------------

void Abort();

void Print(Str msg);

#if defined Compiler_Msvc
	#define Sys_DebuggerBreak() __debugbreak()
#else // Compiler_
	#error("unsupported compiler")
#endif	// Compiler_

bool  IsDebuggerPresent();
void  DebuggerPrint(const char* msg);

void* VirtualAlloc(u64 size);
void* VirtualReserve(u64 size);
void  VirtualCommit(void* p, u64 size);
void  VirtualFree(void* p);
void  VirtualDecommit(void* p, u64 size);

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Sys