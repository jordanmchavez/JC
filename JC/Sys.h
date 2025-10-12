#pragma once

#include "JC/Common_Std.h"

namespace JC::Sys {

//--------------------------------------------------------------------------------------------------

#if defined Platform_Windows
	constexpr U32 MaxPath = 256;
	constexpr U64 VirtualPageSize = 4096;
#endif	// Platform

struct Mutex {
	#if defined Platform_Windows
		U64 opaque = 0;
	#endif	// Platform
};

void  Abort();
bool  DbgPresent();
void  DbgPrint(const char* msg);
void  Print(Str msg);
void* VirtualAlloc(U64 size);
void* VirtualReserve(U64 size);
void* VirtualCommit(void* p, U64 size);
void  VirtualFree(void* p);
void  InitMutex(Mutex* mutex);
void  LockMutex(Mutex* mutex);
void  UnlockMutex(Mutex* mutex);
void  ShutdownMutex(Mutex* mutex);

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Sys