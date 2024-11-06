#pragma once

#include "JC/Common.h"

namespace JC {

//--------------------------------------------------------------------------------------------------

constexpr u64 ConstExprStrLen16(const char16_t* s) {
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
	const char16_t* ptr = nullptr;
	u64             len = 0;

	constexpr          s16z()                         { ptr = nullptr; len = 0; }
	constexpr          s16z(const s16z&s)             { ptr = s.ptr;   len = s.len; }
	constexpr          s16z(const char16_t* sz)       { ptr = sz;      len = ConstExprStrLen16(sz); }
	constexpr          s16z(const char16_t* p, u64 l) { ptr = p;       len = l; }
	constexpr s16z&    operator=(const s16z& s)       { ptr = s.ptr;   len = s.len;                 return *this;}
	constexpr s16z&    operator=(const char16_t* sz)  { ptr = sz;      len = ConstExprStrLen16(sz); return *this;}
	constexpr char16_t operator[](u64 i) const        { return ptr[i]; }
};

constexpr bool operator==(s16z str1, s16z str2) { return str1.len == str2.len && ConstExprMemCmp(str1.ptr, str2.ptr, str1.len * 2) == 0; }
constexpr bool operator!=(s16z str1, s16z str2) { return str1.len != str2.len && ConstExprMemCmp(str1.ptr, str2.ptr, str1.len * 2) != 0; }

//--------------------------------------------------------------------------------------------------

struct UnicodeApi {
	static constexpr ErrCode Err_Utf8BadByte             = { .ns = "unicode", .code = 1 };
	static constexpr ErrCode Err_Utf8MissingTrailingByte = { .ns = "unicode", .code = 2 };
	static constexpr ErrCode Err_Utf8BadTrailingByte     = { .ns = "unicode", .code = 3 };

	virtual Res<s16z> Utf8ToWtf16z(s8 s) = 0;
	virtual s8        Wtf16zToUtf8(s16z s) = 0;

}	// namespace Unicode

//--------------------------------------------------------------------------------------------------

}	// namespace JC