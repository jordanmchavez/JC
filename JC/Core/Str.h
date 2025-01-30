#pragma once

#include "JC/Core.h"

namespace JC {

//--------------------------------------------------------------------------------------------------

constexpr u64 ConstExprStrLen(const char* s) {
	IfConsteval {
		const char* i = s; 
		for (; *i; i++) {}
		return (u64)(i - s);
	} else {
		return strlen(s);
	}
}

constexpr u64 ConstExprWStrLen(const wchar_t* s) {
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

struct Str {
	const char* data = "";
	u64         len  = 0;

	constexpr Str() = default;
	constexpr Str(const Str&) = default;
	constexpr Str(const char* s)                {                  data = s; len = ConstExprStrLen(s); }
	constexpr Str(const char* d, u64 l)         { Assert(d || !l); data = d; len = l; }
	constexpr Str(const char* b, const char* e) { Assert(b <= e);  data = b; len = e - b; }

	constexpr Str& operator=(const Str&) = default;
	constexpr Str& operator=(const char* s) { data = s; len = ConstExprStrLen(s); return *this; }

	constexpr char operator[](u64 i) const { Assert(i < len); return data[i]; }
};

inline bool operator==(Str         s1, Str         s2) { return s1.len == s2.len && !memcmp(s1.data, s2.data, s1.len); }
inline bool operator==(Str         s1, const char* s2) { return !strcmp(s1.data, s2); }
inline bool operator==(const char* s1, Str         s2) { return !strcmp(s1,      s2.data);  }

inline bool operator!=(Str         s1, Str         s2) { return s1.len != s2.len || memcmp(s1.data, s2.data, s1.len); }
inline bool operator!=(Str         s1, const char* s2) { return strcmp(s1.data, s2); }
inline bool operator!=(const char* s1, Str         s2) { return strcmp(s1,      s2.data);  }

constexpr Arg MakeArg(Str val) { return { .type = ArgType::Str, .s = { .data = val.data, .len = val.len } }; }

Str Copy(Mem::Allocator* allocator, Str s);

//--------------------------------------------------------------------------------------------------

struct WStrZ {
	const wchar_t* data = nullptr;
	u64            len  = 0;

	constexpr WStrZ() = default;
	constexpr WStrZ(const WStrZ&) = default;
	constexpr WStrZ(const wchar_t* sz)       { data = sz;     len = ConstExprWStrLen(sz); }
	constexpr WStrZ(const wchar_t* p, u64 l) { Assert(!l || p); data = p;      len = l; }

	constexpr WStrZ& operator=(const WStrZ& s)    { data = s.data;  len = s.len;                 return *this; }
	constexpr WStrZ& operator=(const wchar_t* sz) { data = sz;      len = ConstExprWStrLen(sz); return *this; }

	constexpr wchar_t operator[](u64 i) const { Assert(i < len); return data[i]; }
};

inline bool operator==(WStrZ s1,          WStrZ          s2) { return s1.len == s2.len && !memcmp(s1.data, s2.data, s1.len); }
inline bool operator==(WStrZ s1,          const wchar_t* s2) { return !wcscmp(s1.data, s2); }
inline bool operator==(const wchar_t* s1, WStrZ          s2) { return !wcscmp(s1,      s2.data);  }

inline bool operator!=(WStrZ s1,          WStrZ          s2) { return s1.len != s2.len || memcmp(s1.data, s2.data, s1.len); }
inline bool operator!=(WStrZ s1,          const wchar_t* s2) { return wcscmp(s1.data, s2); }
inline bool operator!=(const wchar_t* s1, WStrZ          s2) { return wcscmp(s1,      s2.data);  }

//--------------------------------------------------------------------------------------------------

}	// namespace JC