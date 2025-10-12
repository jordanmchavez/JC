#include "JC/FS.h"
#include "JC/Handle.h"
#include "JC/Sys_Win.h"
#include "JC/Unicode.h"

namespace JC::FS {

//--------------------------------------------------------------------------------------------------

static constexpr U64 MaxFiles = 1024;

struct FileObj {
	HANDLE hfile;
};

static Mem::Mem                   permMem;
static Mem::Mem                   tempMem;
static HandleArray<FileObj, File> files;

//--------------------------------------------------------------------------------------------------

void Init(Mem::Mem permMem_, Mem::Mem tempMem_) {
	permMem = permMem_;
	tempMem = tempMem_;
	files.Init(permMem, MaxFiles);
}

//--------------------------------------------------------------------------------------------------

Res<File> Open(Str path) {
	HANDLE h = CreateFileW(Unicode::Utf8ToWtf16z(tempMem, path).data, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
	if (!Sys::IsValidHandle(h)) {
		return Win_LastErr("CreateFileW", "path", path);
	}
	auto entry = files.Alloc();
	entry->obj.hfile = h;
	return entry->Handle();
}

//--------------------------------------------------------------------------------------------------

Res<U64> Len(File file) {
	FileObj* const fileObj = files.Get(file);
	LARGE_INTEGER fileSize;
	if (GetFileSizeEx(fileObj->hfile, &fileSize) == 0) {
		return Win_LastErr("GetFileSizeEx");
	}
	return fileSize.QuadPart;
}

//--------------------------------------------------------------------------------------------------

Res<> Read(File file, void* out, U64 outLen) {
	FileObj* const fileObj = files.Get(file);
	U64 offset = 0;
	while (offset < outLen) {
		const U64 rem = outLen - offset;
		const U32 bytesToRead = rem > U32Max ? U32Max : (U32)rem;
		DWORD bytesRead = 0;
		if (ReadFile(fileObj->hfile, (U8*)out + offset, bytesToRead, &bytesRead, 0) == FALSE) {
			return Win_LastErr("ReadFile");
		}
		offset += bytesRead;
	}
	return Ok();
}

//--------------------------------------------------------------------------------------------------

Res<Span<U8>> ReadAll(Mem::Mem mem, Str path) {
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
	if (file) {
		FileObj* const fileObj = files.Get(file);
		CloseHandle(fileObj->hfile);
		files.Free(file);
	}
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC::FS