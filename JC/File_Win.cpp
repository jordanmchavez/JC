#include "JC/File.h"

#include "JC/Array.h"
#include "JC/Sys_Win.h"
#include "JC/Unicode.h"
#include "JC/UnitTest.h"

namespace JC::File {

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

void Close(File file) {
	if (file) {
		Assert(file.handle < MaxFiles);
		FileObj* const fileObj = &fileObjs[file.handle];
		CloseHandle(fileObj->hfile);
		fileObj->hfile = 0;
	}
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

Res<Span<U8>> ReadAllBytes(Mem mem, Str path) {
	File file; TryTo(Open(path), file);
	Defer { Close(file); };
	U64 len = 0; TryTo(Len(file), len);
	U8* buf = (U8*)Mem::Alloc(mem, len);
	if (Res<> r = Read(file, buf, len); !r) {
		return r.err;
	}
	return Span<U8>(buf, len);
}

//--------------------------------------------------------------------------------------------------

Res<Str> ReadAllStr(Mem mem, Str path) {
	Span<U8> bytes; TryTo(ReadAllBytes(mem, path), bytes);
	Assert(bytes.len <= (U64)U32Max);
	return Str((char const*)bytes.data, (U32)bytes.len);
}

//--------------------------------------------------------------------------------------------------

Res<Span<Str>> EnumFiles(Str dir, Str ext) {
	// Strip dir of any trailing slash
	if (dir.len > 0 && (dir[dir.len - 1] == '/' || dir[dir.len - 1] == '\\')) {
		dir.len--;
	}

	StrBuf sb(tempMem);
	if (dir.len) {
		sb.Add(dir);
		sb.Add('/');
	}
	sb.Add('*');
	if (ext.len) {
		if (ext[0] != '.') {
			sb.Add('.');
		}
		sb.Add(ext);
	}

	WIN32_FIND_DATAW findData;
	HANDLE hFind = FindFirstFileW(Unicode::Utf8ToWtf16z(tempMem, sb.ToStr()).data, &findData);
	if (!Sys::IsValidHandle(hFind)) {
		if (GetLastError() == ERROR_FILE_NOT_FOUND) {
			return Span<Str>();
		}
		return Win_LastErr("FindFirstFileW", "dir", dir, "ext", ext);
	}
	Defer { FindClose(hFind); };

	Array<Str> resultPaths(tempMem, 128);
	do {
		if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
			continue;
		}
		resultPaths.Add(SPrintf(tempMem, "%s/%s", dir, Unicode::Wtf16zToUtf8(tempMem, findData.cFileName)));
	} while (FindNextFileW(hFind, &findData));

	return Span<Str>(resultPaths.data, resultPaths.len);
}

//--------------------------------------------------------------------------------------------------

// Takes a file path and removes the extension, including the dot, if any
Str RemoveExt(Str path) {
	if (path.len == 0) {
		return path;
	}
	char const* iter = path.data + path.len - 1;
	char const* const begin = path.data;
	while (iter >= begin && *iter != '/' && *iter != '\\') {
		if (*iter == '.') {
			return Str(begin, (U32)(iter - begin));
		}
		iter--;
	}
	return path;
}

//--------------------------------------------------------------------------------------------------

// returns true if path1 and path2 are the same windows path
// both can be relative
// both can be absolute
// if one is relative and the other is absolute, then always false
bool PathsEq(Str path1, Str path2) {
	auto IsAbsolute = [](Str p) -> bool {
		if (p.len >= 3 && p[1] == ':' && (p[2] == '/' || p[2] == '\\')) {
			return true;  // drive-letter: C:\ or C:/
		}
		if (p.len >= 2 && (p[0] == '\\' || p[0] == '/') && (p[1] == '\\' || p[1] == '/')) {
			return true;  // UNC: \\ or //
		}
		return false;
	};

	if (IsAbsolute(path1) != IsAbsolute(path2)) {
		return false;
	}

	wchar_t buf1[MAX_PATH];
	wchar_t buf2[MAX_PATH];
	DWORD const len1 = GetFullPathNameW(Unicode::Utf8ToWtf16z(tempMem, path1).data, MAX_PATH, buf1, nullptr);
	DWORD const len2 = GetFullPathNameW(Unicode::Utf8ToWtf16z(tempMem, path2).data, MAX_PATH, buf2, nullptr);

	if (len1 == 0 || len2 == 0 || len1 >= MAX_PATH || len2 >= MAX_PATH) {
		return false;
	}

	return CompareStringOrdinal(buf1, (int)len1, buf2, (int)len2, TRUE) == CSTR_EQUAL;
}

//--------------------------------------------------------------------------------------------------

Unit_Test("File") {
	Unit_SubTest("RemoveExt") {
		Unit_CheckEq(RemoveExt(""), "");
		Unit_CheckEq(RemoveExt("a"), "a");
		Unit_CheckEq(RemoveExt("a."), "a");
		Unit_CheckEq(RemoveExt("a.a"), "a");
		Unit_CheckEq(RemoveExt("a.abc123"), "a");
		Unit_CheckEq(RemoveExt("a.foo/b.bar/c.qux/d"), "a.foo/b.bar/c.qux/d");
		Unit_CheckEq(RemoveExt("a.foo/b.bar/c.qux/d.bat"), "a.foo/b.bar/c.qux/d");
		Unit_CheckEq(RemoveExt("a.foo\\b.bar\\c.qux\\d"), "a.foo\\b.bar\\c.qux\\d");
		Unit_CheckEq(RemoveExt("a.foo\\b.bar\\c.qux\\d.bat"), "a.foo\\b.bar\\c.qux\\d");
		Unit_CheckEq(RemoveExt("file.tar.gz"), "file.tar");
		Unit_CheckEq(RemoveExt(".hidden"), "");
		Unit_CheckEq(RemoveExt("path/.hidden"), "path/");
	}

	Unit_SubTest("PathsEq") {
		Unit_CheckEq(PathsEq("foo/bar.txt",          "foo/bar.txt"),          true);
		Unit_CheckEq(PathsEq("foo/bar.txt",          "foo\\bar.txt"),         true);
		Unit_CheckEq(PathsEq("foo/./bar.txt",        "foo/bar.txt"),          true);
		Unit_CheckEq(PathsEq("foo/baz/../bar.txt",   "foo/bar.txt"),          true);
		Unit_CheckEq(PathsEq("foo/bar.txt",          "foo/BAR.TXT"),          true);
		Unit_CheckEq(PathsEq("foo/bar.txt",          "foo/baz.txt"),          false);
		Unit_CheckEq(PathsEq("C:/foo/bar.txt",       "C:\\foo\\bar.txt"),     true);
		Unit_CheckEq(PathsEq("C:/foo/bar.txt",       "c:\fOo/BaR.TXT"),       true);
		Unit_CheckEq(PathsEq("C:/foo/baz/../bar.txt","C:/foo/bar.txt"),       true);
		Unit_CheckEq(PathsEq("C:/foo/bar.txt",       "foo/bar.txt"),          false);  // abs vs rel
		Unit_CheckEq(PathsEq("C:",                   "c:/"),                  true); 
	}
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC::File