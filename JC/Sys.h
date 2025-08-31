#pragma once

#include "JC/Core.h"

namespace JC::Sys {

//--------------------------------------------------------------------------------------------------

void Init(Mem::TempAllocator* tempAllocator);

void Abort();

void Print(Str msg);

#if defined JC_COMPILER_MSVC
	#define JC_DEBUGGER_BREAK() __debugbreak()
#endif	// JC_COMPILER

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
	#if defined JC_PLATFORM_WINDOWS
		U64 opaque = 0;
	#endif	// JC_PLATFORM
};

void InitMutex(Mutex* mutex);
void LockMutex(Mutex* mutex);
void UnlockMutex(Mutex* mutex);
void ShutdownMutex(Mutex* mutex);

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Sys