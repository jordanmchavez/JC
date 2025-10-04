#include "JC/Sys.h"
#include "JC/Sys_Win.h"

//--------------------------------------------------------------------------------------------------

void Sys_Abort() {
	TerminateProcess(GetCurrentProcess(), 3);
}

void Sys_Print(Str msg) {
	WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), msg.data, (DWORD)msg.len, 0, 0);
}

Bool Sys_DbgPresent() {
	return ::IsDebuggerPresent();
}

void Sys_DbgPrint(const char* msg) {
	OutputDebugStringA(msg);
}

void* Sys_VirtualAlloc(U64 size) {
	Assert(size % 4096 == 0);
	void* p = ::VirtualAlloc(nullptr, size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
	if (!p) {
		Panic("VirtualAlloc failed with MEM_RESERVE: lasterror=%u, size=%u", GetLastError(), size);
	}
	return p;
}

void* Sys_VirtualReserve(U64 size) {
	Assert(size % 65536 == 0);
	void* p = ::VirtualAlloc(nullptr, size, MEM_RESERVE, PAGE_READWRITE);
	if (!p) {
		Panic("VirtualAlloc failed with MEM_RESERVE: lasterror=%u, size=%u", GetLastError(), size);
	}
	return p;
}

void* Sys_VirtualCommit(void* p, U64 size) {
	Assert(p);
	Assert((U64)p % 4096 == 0);
	Assert(size % 4096 == 0);
	if (::VirtualAlloc(p, size, MEM_COMMIT, PAGE_READWRITE) == nullptr) {
		Panic("VirtualAlloc failed with MEM_COMMIT: lasterror=%u, size=%u, ptr=%p", GetLastError(), size, p);
	}
	return (U8*)p + size;
}

void Sys_VirtualFree(void* p) {
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