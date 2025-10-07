#include "JC/FS.h"
#include "JC/Unicode.h"
#include "JC/Sys_Win.h"

namespace JC::FS {

//--------------------------------------------------------------------------------------------------

Res<File> Open(Str path) {
	HANDLE h = CreateFileW(Unicode::Utf8ToWtf16z(tempAllocator, path).data, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
	if (!Sys::IsValidHandle(h)) {
		return Win_LastErr("CreateFileW", "path", path);
	}
	return File { .handle = (U64)h };
}

//--------------------------------------------------------------------------------------------------

Res<U64> Len(File file) {
	LARGE_INTEGER fileSize;
	if (GetFileSizeEx((HANDLE)file.handle, &fileSize) == 0) {
		return Win_LastErr("GetFileSizeEx");
	}
	return fileSize.QuadPart;
}

//--------------------------------------------------------------------------------------------------

Res<> Read(File file, void* out, U64 outLen) {
	U64 offset = 0;
	while (offset < outLen) {
		const U64 rem = outLen - offset;
		const U32 bytesToRead = rem > U32Max ? U32Max : (U32)rem;
		DWORD bytesRead = 0;
		if (ReadFile((HANDLE)file.handle, (U8*)out + offset, bytesToRead, &bytesRead, 0) == FALSE) {
			return Win_LastErr("ReadFile");
		}
		offset += bytesRead;
	}
	return Ok();
}

//--------------------------------------------------------------------------------------------------

Res<Span<U8>> ReadAll(Mem::Mem* mem, Str path) {
	File file;
	if (Res<> r = Open(path).To(file); !r) {
		return r.err;
	}

	U64 len = 0;
	if (Res<> r = Len(file).To(len); !r) {
		Close(file);
		return r.err;
	}

	U8* buf = (U8*)Mem::Alloc(mem, len);
	if (Res<> r = Read(file, buf, len); !r) {
		Close(file);
		return r.err;
	}

	Close(file);
	return Span<U8>(buf, len);
}

//--------------------------------------------------------------------------------------------------

void Close(File file) {
	if (file.handle) {
		CloseHandle((HANDLE)file.handle);
	}
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC::FS