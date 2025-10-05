#pragma once

#include "JC/Common.h"

//--------------------------------------------------------------------------------------------------

struct Sys_Mutex;

void  Sys_Abort();
Bool  Sys_DbgPresent();
void  Sys_DbgPrint(const char* msg);
void  Sys_Print(Str msg);
void* Sys_VirtualAlloc(U64 size);
void* Sys_VirtualReserve(U64 size);
void* Sys_VirtualCommit(void* p, U64 size);
void  Sys_VirtualFree(void* p);
void  Sys_InitMutex(Sys_Mutex* mutex);
void  Sys_LockMutex(Sys_Mutex* mutex);
void  Sys_UnlockMutex(Sys_Mutex* mutex);
void  Sys_ShutdownMutex(Sys_Mutex* mutex);