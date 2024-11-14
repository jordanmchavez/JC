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
	void* ReserveCommit(u64 size) override {
		void *p = VirtualAlloc(nullptr, size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
		if (!p) {
			JC_PANIC("VirtualAlloc failed with  MEM_RESERVE | MEM_COMMIT for {}", size);
		}
		return p;
	}

	void* Reserve(u64 size) override {
		void* p = VirtualAlloc(nullptr, size, MEM_RESERVE, PAGE_READWRITE);
		if (!p) {
			JC_PANIC("VirtualAlloc failed with MEM_RESERVE for {}", size);
		}
		return p;
	}

	void Commit(const void* p, u64 size) override {
		JC_ASSERT(p);
		if (VirtualAlloc((void*)p, size, MEM_COMMIT, PAGE_READWRITE) == nullptr) {
			JC_PANIC("VirtualAlloc failed with MEM_COMMIT for {} at {}", size, p);
		}
	}

	void Free(const void* p) override {
		if (p) {
			VirtualFree((void*)p, 0, MEM_RELEASE);
		}
	}
};

static VirtualMemoryApiImpl virtualMemoryApiImpl;

VirtualMemoryApi* VirtualMemoryApi::Get() {
	return &virtualMemoryApiImpl;
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC