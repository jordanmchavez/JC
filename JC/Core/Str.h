#pragma once

#include "JC/Core.h"

namespace JC {

//--------------------------------------------------------------------------------------------------

struct Str {
	const char* data = "";
	u64         len  = 0;

	constexpr Str() = default;
	constexpr Str(const Str& s) = default;
	constexpr Str(const char* s);
	constexpr Str(const char* d, u64 l);
	constexpr Str(const char* b, const char* e);

	constexpr Str& operator=(const Str& s) = default;
	constexpr Str& operator=(const char* s);

	constexpr char operator[](u64 i) const;
};

constexpr Arg MakeArg(Str val) { return { .type = ArgType::Str, .s = { .data = val.data, .len = val.len } }; }

constexpr Str::Str(const char* s)                {                  data = s; len = ConstExprStrLen(s); }
constexpr Str::Str(const char* d, u64 l)         { Assert(d || !l); data = d; len = l; }
constexpr Str::Str(const char* b, const char* e) { Assert(b <= e);  data = b; len = e - b; }

constexpr Str& Str::operator=(const char* s) { data = s; len = ConstExprStrLen(s); return *this; }

constexpr char Str::operator[](u64 i) const { Assert(i < len); return data[i]; }

inline bool operator==(Str s1,         Str s2)         { return s1.len == s2.len && !memcmp(s1.data, s2.data, s1.len); }
inline bool operator==(Str s1,         const char* s2) { return !strcmp(s1.data, s2); }
inline bool operator==(const char* s1, Str s2)         { return !strcmp(s1,      s2.data);  }

Str Copy(Mem::Allocator* allocator, Str s);

//--------------------------------------------------------------------------------------------------

}	// namespace JC