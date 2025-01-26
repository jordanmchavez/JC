#pragma once

#include "JC/Common.h"

namespace JC {

//--------------------------------------------------------------------------------------------------

constexpr u64 ConstExprStrLen16(const wchar_t* s) {
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

struct Str16z {
	const wchar_t* data = nullptr;
	u64            len  = 0;

	constexpr Str16z()                        { data = 0;      len = 0; }
	constexpr Str16z(const Str16z&s )         { data = s.data; len = s.len; }
	constexpr Str16z(const wchar_t* sz)       { data = sz;     len = ConstExprStrLen16(sz); }
	constexpr Str16z(const wchar_t* p, u64 l) { Assert(!l || p); data = p;      len = l; }

	constexpr Str16z& operator=(const Str16z& s)    { data = s.data;  len = s.len;                 return *this; }
	constexpr Str16z& operator=(const wchar_t* sz)  { data = sz;      len = ConstExprStrLen16(sz); return *this; }

	constexpr char16_t operator[](u64 i) const { Assert(i < len); return data[i]; }
};

constexpr bool operator==(Str16z s1, Str16z s2) { return s1.len == s2.len && memcmp(s1.data, s2.data, s1.len * 2) == 0; }
constexpr bool operator!=(Str16z s1, Str16z s2) { return s1.len != s2.len && memcmp(s1.data, s2.data, s1.len * 2) != 0; }

//--------------------------------------------------------------------------------------------------

Str16z Utf8ToWtf16z(Mem::Allocator* allocator, Str s);
Str    Wtf16zToUtf8(Mem::Allocator* allocator, Str16z s);

//--------------------------------------------------------------------------------------------------

}	// namespace JC