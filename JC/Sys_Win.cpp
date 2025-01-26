#include "JC/Sys.h"
#include "JC/Sys_Win.h"
#include "JC/Unicode.h"

namespace JC {

//--------------------------------------------------------------------------------------------------

namespace Sys {

//--------------------------------------------------------------------------------------------------

void Abort() {
	TerminateProcess(GetCurrentProcess(), 3);
}

//--------------------------------------------------------------------------------------------------

void Print(Str msg) {
	WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), msg.data, (DWORD)msg.len, 0, 0);
}

//--------------------------------------------------------------------------------------------------

bool IsDebuggerPresent() {
	return ::IsDebuggerPresent();
}

void DebuggerPrint(const char* msg) {
	OutputDebugStringA(msg);
}

//--------------------------------------------------------------------------------------------------

void* VirtualAlloc(u64 size) {
	Assert(size % 4096 == 0);
	void* p = ::VirtualAlloc(nullptr, size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
	if (!p) {
		Panic("VirtualAlloc failed with MEM_RESERVE for {}: {}", size, GetLastError());
	}
	return p;
}

void* VirtualReserve(u64 size) {
	Assert(size % 65536 == 0);
	void* p = ::VirtualAlloc(nullptr, size, MEM_RESERVE, PAGE_READWRITE);
	if (!p) {
		Panic("VirtualAlloc failed with MEM_RESERVE for {}: {}", size, GetLastError());
	}
	return p;
}

void VirtualCommit(void* p, u64 size) {
	Assert(p);
	Assert((u64)p % 4096 == 0);
	Assert(size % 4096 == 0);
	if (::VirtualAlloc(p, size, MEM_COMMIT, PAGE_READWRITE) == nullptr) {
		Panic("VirtualAlloc failed with MEM_COMMIT for {} at {}: {}", size, p, GetLastError());
	}
}

void VirtualFree(void* p) {
	if (p) {
		::VirtualFree(p, 0, MEM_RELEASE);
	}
}

void VirtualDecommit(void* p, u64 size) {
	# pragma warning(push )
	# pragma warning(disable: 6250)
	::VirtualFree(p, size, MEM_DECOMMIT);
	# pragma warning(pop)
}

//--------------------------------------------------------------------------------------------------

void InitMutex(Mutex* mutex) {
	*(SRWLOCK*)mutex = SRWLOCK_INIT;
}

void LockMutex(Mutex* mutex) {
	AcquireSRWLockExclusive((SRWLOCK*)mutex);
}

void UnlockMutex(Mutex* mutex) {
	ReleaseSRWLockExclusive((SRWLOCK*)mutex);
}

void ShutdownMutex(Mutex* ) {
	// no-op on windows
}

//--------------------------------------------------------------------------------------------------

}	// namespace Sys
}	// namespace JC