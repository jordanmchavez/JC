#include "JC/File.h"
#include "JC/Unicode.h"
#include "JC/Win.h"

//--------------------------------------------------------------------------------------------------

Res<File> File_Open(s8 path) {
	Res<s16z> wpath  = Unicode_Utf8ToWtf16z(path);
	Err_Check(wpath, File_ErrBadPath, "path", path);
	HANDLE h = CreateFileW(
		(const wchar_t*)wpath.val.ptr,
		GENERIC_READ,
		FILE_SHARE_READ,
		nullptr,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		nullptr
	);
	if (h == INVALID_HANDLE_VALUE) {
		return Win_LastErr(File_ErrOpen, "CreateFileW", "path", path);
	}

	return File { .opaque = (u64)h };
}

//--------------------------------------------------------------------------------------------------

Res<File_Attrs> File_GetAttrs(File f) {
	BY_HANDLE_FILE_INFORMATION info;
	if (GetFileInformationByHandle((HANDLE)f.opaque, &info) == FALSE) {
		return Win_LastErr(File_ErrGetAttrs, "GetFileInformationByHandle");
	}

	return File_Attrs {
		.len      = (((u64)info.nFileSizeHigh) << 32) | (u64)info.nFileSizeLow,
		.dir      = (info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? true : false,
		.created  = Win_TicksSince_1_1_1601 + (((u64)info.ftCreationTime.dwHighDateTime << 32) | (u64)info.ftCreationTime.dwLowDateTime),
		.modified = Win_TicksSince_1_1_1601 + (((u64)info.ftLastWriteTime.dwHighDateTime << 32) | (u64)info.ftLastWriteTime.dwLowDateTime),
	};
}
//--------------------------------------------------------------------------------------------------

Res<u64> File_Read(File f, void* buf, u64 len) {
	Assert(len < 0xffffffffull);
	u64 bytesRead = 0;
	if (ReadFile((HANDLE)f.opaque, buf, (DWORD)len, (DWORD*)&bytesRead, nullptr) == FALSE) {
		return Win_LastErr(File_ErrRead, "ReadFile", "len", len);
	}
	Err_Check(bytesRead == len, File_ErrReadLen, "len", len);
	return bytesRead;
}

//--------------------------------------------------------------------------------------------------

void File_Close(File f) {
	CloseHandle((HANDLE)f.opaque);
}