#include "JC/Sys.h"

#include "JC/MinimalWindows.h"
#include "JC/Unicode.h"

namespace JC {

//--------------------------------------------------------------------------------------------------

void Sys::Abort() {
	TerminateProcess(GetCurrentProcess(), 3);
}

bool Sys::IsDebuggerPresent() {
	return ::IsDebuggerPresent();
}

void Sys::DebuggerPrint(TempAllocator* tempAllocator, s8 msg) {
	if (Res<s16z> res = Unicode::Utf8ToWtf16z(tempAllocator, msg)) {
		OutputDebugStringW((LPCWSTR)res.val.data);
	} else {
		OutputDebugStringW(L"<bad UTF-8 string>");
	}
}

//--------------------------------------------------------------------------------------------------

struct VirtualMemoryApiImpl : VirtualMemoryApi {
	void* Map(u64 size) override {
		return VirtualAlloc(nullptr, size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
	}

	void Unmap(const void* ptr, u64 size) {
		VirtualFree((void*)ptr, size, MEM_RELEASE);
	}
};

static VirtualMemoryApiImpl virtualMemoryApiImpl;

VirtualMemoryApi* VirtualMemoryApi::Get() {
	return &virtualMemoryApiImpl;
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC