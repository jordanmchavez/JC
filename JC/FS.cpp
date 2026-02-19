#include "JC/FS.h"
#include "JC/Sys_Win.h"
#include "JC/Unicode.h"

namespace JC::FS {

//--------------------------------------------------------------------------------------------------

static constexpr U32 MaxFiles = 64;

struct FileObj {
	HANDLE hfile;
};

static Mem     tempMem;
static FileObj fileObjs[MaxFiles];

//--------------------------------------------------------------------------------------------------

void Init(Mem tempMemIn) {
	tempMem = tempMemIn;
}

//--------------------------------------------------------------------------------------------------

Res<File> Open(Str path) {
	FileObj* fileObj = 0;
	for (U32 i = 1; i < MaxFiles; i++) {	// reserve 0 for invalid
		if (!fileObjs[i].hfile) {
			fileObj = &fileObjs[i];
			break;
		}
	}
	Assert(fileObj);
	HANDLE h = CreateFileW(Unicode::Utf8ToWtf16z(tempMem, path).data, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
	if (!Sys::IsValidHandle(h)) {
		return Win_LastErr("CreateFileW", "path", path);
	}
	fileObj->hfile = h;
	return File { .handle = (U64)(fileObj - fileObjs) };
}

//--------------------------------------------------------------------------------------------------

Res<U64> Len(File file) {
	Assert(file.handle);
	Assert(file.handle < MaxFiles);
	FileObj* const fileObj = &fileObjs[file.handle];
	Assert(fileObj->hfile);
	LARGE_INTEGER fileSize;
	if (GetFileSizeEx(fileObj->hfile, &fileSize) == 0) {
		return Win_LastErr("GetFileSizeEx");
	}
	return fileSize.QuadPart;
}

//--------------------------------------------------------------------------------------------------

Res<> Read(File file, void* out, U64 outLen) {
	Assert(file.handle);
	Assert(file.handle < MaxFiles);
	FileObj* const fileObj = &fileObjs[file.handle];
	Assert(fileObj->hfile);
	U64 offset = 0;
	while (offset < outLen) {
		U64 const rem = outLen - offset;
		U32 const bytesToRead = rem > U32Max ? U32Max : (U32)rem;
		DWORD bytesRead = 0;
		if (ReadFile(fileObj->hfile, (U8*)out + offset, bytesToRead, &bytesRead, 0) == FALSE) {
			return Win_LastErr("ReadFile");
		}
		offset += bytesRead;
	}
	return Ok();
}

//--------------------------------------------------------------------------------------------------

Res<Span<U8>> ReadAll(Mem mem, Str path) {
	File file; TryTo(Open(path), file);
	Defer { Close(file); };
	U64 len = 0; TryTo(Len(file), len);
	MemMark mark = Mem::Mark(mem);
	U8* buf = (U8*)Mem::Alloc(mem, len);
	if (Res<> r = Read(file, buf, len); !r) {
		Mem::Reset(mem, mark);
		return r.err;
	}
	return Span<U8>(buf, len);
}

//--------------------------------------------------------------------------------------------------

Res<Span<char>> ReadAllZ(Mem mem, Str path) {
	File file; TryTo(Open(path), file);
	Defer { Close(file); };
	U64 len = 0; TryTo(Len(file), len);
	MemMark mark = Mem::Mark(mem);
	char* buf = (char*)Mem::Alloc(mem, len + 1);
	if (Res<> r = Read(file, buf, len); !r) {
		Mem::Reset(mem, mark);
		return r.err;
	}
	buf[len] = '\0';
	return Span<char>(buf, len);
}

//--------------------------------------------------------------------------------------------------

void Close(File file) {
	if (file) {
		Assert(file.handle < MaxFiles);
		FileObj* const fileObj = &fileObjs[file.handle];
		CloseHandle(fileObj->hfile);
		fileObj->hfile = 0;
	}
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC::FS