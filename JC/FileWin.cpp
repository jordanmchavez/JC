#include "JC/File.h"

#include "JC/Mem.h"
#include "JC/Unicode.h"

#include "JC/MinimalWindows.h"

namespace JC {

//--------------------------------------------------------------------------------------------------

struct FileApiObj : FileApi {
	TempMem* tempMem = 0;

	void Init(TempMem* tempMemIn) override {
		tempMem = tempMemIn;
	}

	Res<File> Open(s8 path) override {
		HANDLE h = CreateFileW(Utf8ToWtf16z(tempMem, path).data, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
		if (IsInvalidHandle(h)) {
			return MakeLastErr(CreateFileW, "path", path);
		}
		return File { .handle = (u64)h };
	}

	Res<u64> Len(File file) override {
		LARGE_INTEGER fileSize;
		if (GetFileSizeEx((HANDLE)file.handle, &fileSize) == 0) {
			return MakeLastErr(GetFileSizeEx);
		}
		return fileSize.QuadPart;
	}

	Res<> Read(File file, void* out, u64 outLen) override {
		u64 offset = 0;
		while (offset < outLen) {
			const u64 rem = outLen - offset;
			const u32 bytesToRead = rem > U32Max ? U32Max : (u32)rem;
			DWORD bytesRead = 0;
			if (ReadFile((HANDLE)file.handle, (u8*)out + offset, bytesToRead, &bytesRead, 0) == FALSE) {
				return MakeLastErr(ReadFile);
			}
			offset += bytesRead;
		}
		return Ok();
	}

	void Close(File file) override {
		if (file.handle) {
			CloseHandle((HANDLE)file.handle);
		}
	}
};

//--------------------------------------------------------------------------------------------------

Res<Span<u8>> FileApi::ReadAll(Mem* mem, s8 path) {
	File file;
	if (Res<> r = Open(path).To(file); !r) {
		return r.err;
	}
	Defer { Close(file); };

	u64 len = 0;
	if (Res<> r = Len(file).To(len); !r) {
		return r.err;
	}

	u8* buf = (u8*)mem->Alloc(len);
	if (Res<> r = Read(file, buf, len); !r) {
		return r.err;
	}

	return Span<u8>(buf, len);
}

//--------------------------------------------------------------------------------------------------

static FileApiObj fileApiObj;

FileApi* GetFileApi() {
	return &fileApiObj;
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC