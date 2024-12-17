#include "JC/File.h"

#include "JC/Unicode.h"
#include "JC/MinimalWindows.h"

namespace JC::File {

//--------------------------------------------------------------------------------------------------

struct ApiObj : Api {
	Arena* temp = 0;

	void Init(Arena* tempIn) override {
		temp = tempIn;
	}

	Res<File> Open(s8 path) override {
		HANDLE h = CreateFileW(Utf8ToWtf16z(temp, path).data, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
		if (IsInvalidHandle(h)) {
			return MakeLastErr(temp, CreateFileW, "path", path);
		}
		return File { .handle = (u64)h };
	}

	Res<u64> Len(File file) override {
		LARGE_INTEGER fileSize;
		if (GetFileSizeEx((HANDLE)file.handle, &fileSize) == 0) {
			return MakeLastErr(temp, GetFileSizeEx);
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
				return MakeLastErr(temp, ReadFile);
			}
			offset += bytesRead;
		}
		return Ok();
	}

	Res<Span<u8>> ReadAll(s8 path) override {
		File file;
		if (Res<> r = Open(path).To(file); !r) {
			return r.err;
		}
		Defer { Close(file); };

		u64 len = 0;
		if (Res<> r = Len(file).To(len); !r) {
			return r.err;
		}

		u8* buf = (u8*)temp->Alloc(len);
		if (Res<> r = Read(file, buf, len); !r) {
			return r.err;
		}

		return Span<u8>(buf, len);
	}

	void Close(File file) override {
		if (file.handle) {
			CloseHandle((HANDLE)file.handle);
		}
	}
};

//--------------------------------------------------------------------------------------------------

static ApiObj apiObj;

Api* GetApi() {
	return &apiObj;
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC::File