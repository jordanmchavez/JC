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

bool  IsDebuggerPresent();
void  DebuggerPrint(const char* msg);

//--------------------------------------------------------------------------------------------------

void* VirtualAlloc(u64 size);
void* VirtualReserve(u64 size);
void* VirtualCommit(void* p, u64 size);
void  VirtualFree(void* p);
void  VirtualDecommit(void* p, u64 size);

//--------------------------------------------------------------------------------------------------

struct Mutex {
	#if defined Platform_Windows
		u64 opaque = 0;
	#endif	// Platform
};

void InitMutex(Mutex* mutex);
void LockMutex(Mutex* mutex);
void UnlockMutex(Mutex* mutex);
void ShutdownMutex(Mutex* mutex);

//--------------------------------------------------------------------------------------------------

struct File { u64 handle = 0; };

void          Init();
Res<File>     Open(Str path);
Res<u64>      Len(File file);
Res<>         Read(File file, void* out, u64 outLen);
Res<Span<u8>> ReadAll(Mem::Allocator* allocator, Str path);
void          Close(File file);

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Sys