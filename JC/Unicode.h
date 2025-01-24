#pragma once

#include "JC/Common.h"

namespace JC {

//--------------------------------------------------------------------------------------------------

constexpr u64 StrLen16(const wchar_t* s) {
	if (s == nullptr) {
		return 0;
	}
	const wchar_t* p = s;
	while (*p) {
		++p;
	}
	return (u64)(p - s);
}

//--------------------------------------------------------------------------------------------------

struct s16z {
	const wchar_t* data = nullptr;
	u64            len  = 0;

	constexpr          s16z()                        { data = nullptr; len = 0; }
	constexpr          s16z(const s16z&s)            { data = s.data;  len = s.len; }
	constexpr          s16z(const wchar_t* sz)       { data = sz;      len = StrLen16(sz); }
	constexpr          s16z(const wchar_t* p, u64 l) { data = p;       len = l; }

	constexpr s16z&    operator=(const s16z& s)      { data = s.data;  len = s.len;        return *this;}
	constexpr s16z&    operator=(const wchar_t* sz)  { data = sz;      len = StrLen16(sz); return *this;}

	constexpr char16_t operator[](u64 i) const        { return data[i]; }
};

constexpr bool operator==(s16z str1, s16z str2) { return str1.len == str2.len && memcmp(str1.data, str2.data, str1.len * 2) == 0; }
constexpr bool operator!=(s16z str1, s16z str2) { return str1.len != str2.len && memcmp(str1.data, str2.data, str1.len * 2) != 0; }

//--------------------------------------------------------------------------------------------------

s16z Utf8ToWtf16z(Arena* arena, s8 s);
s8   Wtf16zToUtf8(Arena* arena, s16z s);

//--------------------------------------------------------------------------------------------------

}	// namespace JC