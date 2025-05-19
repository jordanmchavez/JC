#pragma once

#include "JC/Core.h"

namespace JC::Sys {

//--------------------------------------------------------------------------------------------------

void Init(Mem::TempAllocator* tempAllocator);

void Abort();

void Print(Str msg);

#if defined Compiler_Msvc
	#define Sys_DebuggerBreak() __debugbreak()
#else // Compiler_
	#error("unsupported compiler")
#endif	// Compiler_

Bool  IsDebuggerPresent();
void  DebuggerPrint(const char* msg);

//--------------------------------------------------------------------------------------------------

void* VirtualAlloc(U64 size);
void* VirtualReserve(U64 size);
void* VirtualCommit(void* p, U64 size);
void  VirtualFree(void* p);
void  VirtualDecommit(void* p, U64 size);

//--------------------------------------------------------------------------------------------------

struct Mutex {
	#if defined Platform_Windows
		U64 opaque = 0;
	#endif	// Platform
};

void InitMutex(Mutex* mutex);
void LockMutex(Mutex* mutex);
void UnlockMutex(Mutex* mutex);
void ShutdownMutex(Mutex* mutex);

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Sys