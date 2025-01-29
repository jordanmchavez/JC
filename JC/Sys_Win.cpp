#include "JC/Sys.h"

#define WIN32_LEAN_AND_MEAN
#define VC_EXTRALEAN
#include <Windows.h>


namespace JC::Sys {

//--------------------------------------------------------------------------------------------------

void Abort() {
	TerminateProcess(GetCurrentProcess(), 3);
}

void Print(Str msg) {
	WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), msg.data, (DWORD)msg.len, 0, 0);
}

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
		Panic("VirtualAlloc failed with MEM_RESERVE", "lasterror", GetLastError(), "size", size);
	}
	return p;
}

void* VirtualReserve(u64 size) {
	Assert(size % 65536 == 0);
	void* p = ::VirtualAlloc(nullptr, size, MEM_RESERVE, PAGE_READWRITE);
	if (!p) {
		Panic("VirtualAlloc failed with MEM_RESERVE", "lasterror", GetLastError(), "size", size);
	}
	return p;
}

void VirtualCommit(void* p, u64 size) {
	Assert(p);
	Assert((u64)p % 4096 == 0);
	Assert(size % 4096 == 0);
	if (::VirtualAlloc(p, size, MEM_COMMIT, PAGE_READWRITE) == nullptr) {
		Panic("VirtualAlloc failed with MEM_COMMIT", "lasterror", GetLastError(), "size", size, "ptr", p);
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

}	// namespace JC::Sys