#include "JC/Sys.h"

#include "JC/Unicode.h"
#include "JC/Sys_Win.h"

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

void* VirtualCommit(void* p, u64 size) {
	Assert(p);
	Assert((u64)p % 4096 == 0);
	Assert(size % 4096 == 0);
	if (::VirtualAlloc(p, size, MEM_COMMIT, PAGE_READWRITE) == nullptr) {
		Panic("VirtualAlloc failed with MEM_COMMIT", "lasterror", GetLastError(), "size", size, "ptr", p);
	}
	return (u8*)p + size;
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

Res<File> Open(Str path) {
	HANDLE h = CreateFileW(Unicode::Utf8ToWtf16z(tempAllocator, path).data, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
	if (!IsValidHandle(h)) {
		return Err_WinLast("CreateFileW", "path", path);
	}
	return File { .handle = (u64)h };
}

Res<u64> Len(File file) {
	LARGE_INTEGER fileSize;
	if (GetFileSizeEx((HANDLE)file.handle, &fileSize) == 0) {
		return Err_WinLast("GetFileSizeEx");
	}
	return fileSize.QuadPart;
}

Res<> Read(File file, void* out, u64 outLen) {
	u64 offset = 0;
	while (offset < outLen) {
		const u64 rem = outLen - offset;
		const u32 bytesToRead = rem > U32Max ? U32Max : (u32)rem;
		DWORD bytesRead = 0;
		if (ReadFile((HANDLE)file.handle, (u8*)out + offset, bytesToRead, &bytesRead, 0) == FALSE) {
			return Err_WinLast("ReadFile");
		}
		offset += bytesRead;
	}
	return Ok();
}

Res<Span<u8>> ReadAll(Str path) {
	File file;
	if (Res<> r = Open(path).To(file); !r) {
		return r.err;
	}
	Defer { Close(file); };

	u64 len = 0;
	if (Res<> r = Len(file).To(len); !r) {
		return r.err;
	}

	u8* buf = (u8*)tempAllocator->Alloc(len);
	if (Res<> r = Read(file, buf, len); !r) {
		return r.err;
	}

	return Span<u8>(buf, len);
}

void Close(File file) {
	if (file.handle) {
		CloseHandle((HANDLE)file.handle);
	}
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Sys