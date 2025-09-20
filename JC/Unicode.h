#pragma once

#include "JC/Core.h"

namespace JC::Unicode {

//--------------------------------------------------------------------------------------------------

constexpr U64 ConstExprWStrLen(const wchar_t* s) {
	if (s == nullptr) {
		return 0;
	}
	const wchar_t* p = s;
	while (*p) {
		++p;
	}
	return (U64)(p - s);
}

//--------------------------------------------------------------------------------------------------

struct WStrZ {
	const wchar_t* data = nullptr;
	U64            len  = 0;

	constexpr WStrZ() = default;
	constexpr WStrZ(const WStrZ&) = default;
	constexpr WStrZ(const wchar_t* sz)       { data = sz;                    len = ConstExprWStrLen(sz); }
	constexpr WStrZ(const wchar_t* p, U64 l) { JC_ASSERT(!l || p); data = p; len = l; }

	constexpr WStrZ& operator=(const WStrZ& s)    { data = s.data;  len = s.len;                return *this; }
	constexpr WStrZ& operator=(const wchar_t* sz) { data = sz;      len = ConstExprWStrLen(sz); return *this; }

	constexpr wchar_t operator[](U64 i) const { JC_ASSERT(i < len); return data[i]; }
};

inline Bool operator==(WStrZ s1,          WStrZ          s2) { return s1.len == s2.len && !memcmp(s1.data, s2.data, s1.len); }
inline Bool operator==(WStrZ s1,          const wchar_t* s2) { return !wcscmp(s1.data, s2); }
inline Bool operator==(const wchar_t* s1, WStrZ          s2) { return !wcscmp(s1,      s2.data);  }

inline Bool operator!=(WStrZ s1,          WStrZ          s2) { return s1.len != s2.len || memcmp(s1.data, s2.data, s1.len); }
inline Bool operator!=(WStrZ s1,          const wchar_t* s2) { return wcscmp(s1.data, s2); }
inline Bool operator!=(const wchar_t* s1, WStrZ          s2) { return wcscmp(s1,      s2.data);  }

//--------------------------------------------------------------------------------------------------

WStrZ Utf8ToWtf16z(Allocator* allocator, Str s);
Str   Wtf16zToUtf8(Allocator* allocator, WStrZ s);

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Unicode