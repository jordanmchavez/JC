#include "JC/Sys.h"
#include "JC/Sys_Win.h"
#include "JC/Common_Assert.h"

namespace JC::Sys {

//--------------------------------------------------------------------------------------------------

void Abort() {
	TerminateProcess(GetCurrentProcess(), 3);
}

void Print(Str msg) {
	WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), msg.data, (DWORD)msg.len, 0, 0);
}

bool DbgPresent() {
	return ::IsDebuggerPresent();
}

void DbgPrint(const char* msg) {
	OutputDebugStringA(msg);
}

void* VirtualAlloc(U64 size) {
	Assert(size % 4096 == 0);
	void* p = ::VirtualAlloc(nullptr, size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
	if (!p) {
		Panic("VirtualAlloc failed with MEM_RESERVE: lasterror=%u, size=%u", GetLastError(), size);
	}
	return p;
}

void* VirtualReserve(U64 size) {
	Assert(size % 65536 == 0);
	void* p = ::VirtualAlloc(nullptr, size, MEM_RESERVE, PAGE_READWRITE);
	if (!p) {
		Panic("VirtualAlloc failed with MEM_RESERVE: lasterror=%u, size=%u", GetLastError(), size);
	}
	return p;
}

void* VirtualCommit(void* p, U64 size) {
	Assert(p);
	Assert((U64)p % 4096 == 0);
	Assert(size % 4096 == 0);
	if (::VirtualAlloc(p, size, MEM_COMMIT, PAGE_READWRITE) == nullptr) {
		Panic("VirtualAlloc failed with MEM_COMMIT: lasterror=%u, size=%u, ptr=%p", GetLastError(), size, p);
	}
	return (U8*)p + size;
}

void VirtualFree(void* p) {
	if (p) {
		::VirtualFree(p, 0, MEM_RELEASE);
	}
}

void InitMutex(Mutex* mutex) {
	*(SRWLOCK*)mutex = SRWLOCK_INIT;
}

void LockMutex(Mutex* mutex) {
	AcquireSRWLockExclusive((SRWLOCK*)mutex);
}

void UnlockMutex(Mutex* mutex) {
	ReleaseSRWLockExclusive((SRWLOCK*)mutex);
}

void ShutdownMutex(Mutex*) {
	// no-op on windows
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Sys