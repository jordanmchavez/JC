#include "JC/Sys.h"

#include "JC/MinimalWindows.h"

namespace JC {

//--------------------------------------------------------------------------------------------------

void Sys::Abort() {
	TerminateProcess(GetCurrentProcess(), 3);
}

bool Sys::IsDebuggerPresent() {
	return ::IsDebuggerPresent();
}

void Sys::DebuggerPrint(const char* msg) {
	OutputDebugStringA(msg);
}

void* Sys::VirtualReserve(u64 size) {
	void* p = VirtualAlloc(nullptr, size, MEM_RESERVE, PAGE_READWRITE);
	if (!p) {
		Panic("VirtualAlloc failed with MEM_RESERVE for {}", size);
	}
	return p;
}

void Sys::VirtualCommit(const void* p, u64 size) {
	Assert(p);
	if (VirtualAlloc((void*)p, size, MEM_COMMIT, PAGE_READWRITE) == nullptr) {
		Panic("VirtualAlloc failed with MEM_COMMIT for {} at {}", size, p);
	}
}

void VirtualFree(const void* p) {
	if (p) {
		::VirtualFree((void*)p, 0, MEM_RELEASE);
	}
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC