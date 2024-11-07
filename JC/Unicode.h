#pragma once

#include "JC/Common.h"

struct Allocator;

namespace JC {

//--------------------------------------------------------------------------------------------------

constexpr u64 StrLen16(const char16_t* s) {
	if (s == nullptr) {
		return 0;
	}
	const char16_t* p = s;
	while (*p) {
		++p;
	}
	return (u64)(p - s);
}

//--------------------------------------------------------------------------------------------------

struct s16z {
	const char16_t* data = nullptr;
	u64             len  = 0;

	constexpr          s16z()                         { data = nullptr; len = 0; }
	constexpr          s16z(const s16z&s)             { data = s.data;  len = s.len; }
	constexpr          s16z(const char16_t* sz)       { data = sz;      len = StrLen16(sz); }
	constexpr          s16z(const char16_t* p, u64 l) { data = p;       len = l; }

	constexpr s16z&    operator=(const s16z& s)       { data = s.data;  len = s.len;        return *this;}
	constexpr s16z&    operator=(const char16_t* sz)  { data = sz;      len = StrLen16(sz); return *this;}

	constexpr char16_t operator[](u64 i) const        { return data[i]; }
};

constexpr bool operator==(s16z str1, s16z str2) { return str1.len == str2.len && JC_MEMCMP(str1.data, str2.data, str1.len * 2) == 0; }
constexpr bool operator!=(s16z str1, s16z str2) { return str1.len != str2.len && JC_MEMCMP(str1.data, str2.data, str1.len * 2) != 0; }

//--------------------------------------------------------------------------------------------------

namespace Unicode {
	static constexpr ErrCode Err_Utf8BadByte             = { .ns = "uni", .code = 1 };
	static constexpr ErrCode Err_Utf8MissingTrailingByte = { .ns = "uni", .code = 2 };
	static constexpr ErrCode Err_Utf8BadTrailingByte     = { .ns = "uni", .code = 3 };

	Res<s16z> Utf8ToWtf16z(TempAllocator* tempAllocator, s8 s);
	s8        Wtf16zToUtf8(TempAllocator* tempAllocator, s16z s);

}	// namespace Unicode

//--------------------------------------------------------------------------------------------------

}	// namespace JC