#include "JC/Sys.h"

#include "JC/Mem.h"
#include "JC/MinimalWindows.h"
#include "JC/Unicode.h"

namespace JC {

//--------------------------------------------------------------------------------------------------

s8 MakeWinErrorDesc(u32 code) {
	const ErrCode ec = { .ns = "win", .code = (u64)code };
	wchar_t* desc = nullptr;
	DWORD descLen = FormatMessageW(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		nullptr,
		code,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPWSTR)&desc,
		0,
		nullptr
	);
	if (descLen == 0) {
		return "";
	}

	while (descLen > 0 && desc[descLen] == L'\n' || desc[descLen] == L'\r' || desc[descLen] == L'.') {
		--descLen;
	}
	s8 msg = Unicode::Wtf16zToUtf8(GetMemApi()->Temp(), s16z(desc, descLen));
	LocalFree(desc);
	return msg;
}

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

void* Sys::VirtualAlloc(u64 size) {
	Assert(size % 4096 == 0);
	void* p = ::VirtualAlloc(nullptr, size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
	if (!p) {
		Panic("VirtualAlloc failed with MEM_RESERVE for {}", size);
	}
	return p;
}

void* Sys::VirtualReserve(u64 size) {
	Assert(size % 65536 == 0);
	void* p = ::VirtualAlloc(nullptr, size, MEM_RESERVE, PAGE_READWRITE);
	if (!p) {
		Panic("VirtualAlloc failed with MEM_RESERVE for {}", size);
	}
	return p;
}

void Sys::VirtualCommit(void* p, u64 size) {
	Assert(p);
	Assert((u64)p % 4096 == 0);
	Assert(size % 4096 == 0);
	if (::VirtualAlloc(p, size, MEM_COMMIT, PAGE_READWRITE) == nullptr) {
		Panic("VirtualAlloc failed with MEM_COMMIT for {} at {}", size, p);
	}
}

void Sys::VirtualFree(void* p) {
	if (p) {
		::VirtualFree(p, 0, MEM_RELEASE);
	}
}

void Sys::VirtualDecommit(void* p, u64 size) {
	# pragma warning(push )
	# pragma warning(disable: 6250)
	::VirtualFree(p, size, MEM_DECOMMIT);
	# pragma warning(pop)
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC