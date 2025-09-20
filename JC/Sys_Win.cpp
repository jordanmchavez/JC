#include "JC/Sys.h"
#include "JC/Sys_Win.h"

#include "JC/Mem.h"

namespace JC::Sys {

//--------------------------------------------------------------------------------------------------

static Mem::TempAllocator* tempAllocator;

//--------------------------------------------------------------------------------------------------

void Init(Mem::TempAllocator* tempAllocatorIn) {
	tempAllocator = tempAllocatorIn;
}

void Abort() {
	TerminateProcess(GetCurrentProcess(), 3);
}

void Print(Str msg) {
	WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), msg.data, (DWORD)msg.len, 0, 0);
}

Bool IsDebuggerPresent() {
	return ::IsDebuggerPresent();
}

void DebuggerPrint(const char* msg) {
	OutputDebugStringA(msg);
}

//--------------------------------------------------------------------------------------------------

void* VirtualAlloc(U64 size) {
	JC_ASSERT(size % 4096 == 0);
	void* p = ::VirtualAlloc(nullptr, size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
	if (!p) {
		JC_PANIC("VirtualAlloc failed with MEM_RESERVE: lasterror={}, size={}", GetLastError(), size);
	}
	return p;
}

void* VirtualReserve(U64 size) {
	JC_ASSERT(size % 65536 == 0);
	void* p = ::VirtualAlloc(nullptr, size, MEM_RESERVE, PAGE_READWRITE);
	if (!p) {
		JC_PANIC("VirtualAlloc failed with MEM_RESERVE: lasterror={}, size={}", GetLastError(), size);
	}
	return p;
}

void* VirtualCommit(void* p, U64 size) {
	JC_ASSERT(p);
	JC_ASSERT((U64)p % 4096 == 0);
	JC_ASSERT(size % 4096 == 0);
	if (::VirtualAlloc(p, size, MEM_COMMIT, PAGE_READWRITE) == nullptr) {
		JC_PANIC("VirtualAlloc failed with MEM_COMMIT: lasterror={}, size={}, ptr={}", GetLastError(), size, p);
	}
	return (U8*)p + size;
}

void VirtualFree(void* p) {
	if (p) {
		::VirtualFree(p, 0, MEM_RELEASE);
	}
}

void VirtualDecommit(void* p, U64 size) {
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

}	// namespace JC::Sys