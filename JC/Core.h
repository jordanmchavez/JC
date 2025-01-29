#pragma once

#if defined _MSC_VER
	#include <string.h>	// memset/memmove/memcpy
#endif

namespace JC {

//--------------------------------------------------------------------------------------------------

#if defined _MSC_VER
	#define Compiler_Msvc
	#define Platform_Windows

	using  i8 = signed char;
	using  u8 = unsigned char;
	using i16 = signed short;
	using u16 = unsigned short;
	using i32 = signed int;
	using u32 = unsigned int;
	using i64 = signed long long;
	using u64 = unsigned long long;
	using f32 = float;
	using f64 = double;

	#define BuiltinFile __builtin_FILE()
	#define BuiltinLine ((u32)__builtin_LINE())
	#define IfConsteval if (__builtin_is_constant_evaluated())
	#define BuiltinIsEnum(T) __is_enum(T)
#endif

constexpr u32 U32Max = 0xffffffff;
constexpr u64 U64Max = (u64)0xffffffffffffffff;

#define MacroConcat2(x, y) x##y
#define MacroConcat(x, y)  MacroConcat2(x, y)
#define MacroName(x) MacroConcat(x, __LINE__)
#define LenOf(a) (u64)(sizeof(a) / sizeof(a[0]))

#define Defer \
	auto MacroName(Defer_) = DeferHelper() + [&]()

template <class F> struct DeferInvoker {
	F fn;
	DeferInvoker(F&& fn_) : fn(fn_) {}
	~DeferInvoker() { fn(); }
};

enum struct DeferHelper {};
template <class F> DeferInvoker<F> operator+(DeferHelper, F&& fn) { return DeferInvoker<F>((F&&)fn); }

constexpr u64 ConstExprStrLen(const char* s) {
	IfConsteval {
		const char* i = s; 
		for (; *i; i++) {}
		return (u64)(i - s);
	} else {
		return strlen(s);
	}
}

//--------------------------------------------------------------------------------------------------

struct Rect {
	i32 x = 0;
	i32 y = 0;
	u32 w = 0;
	u32 h = 0;
};

//--------------------------------------------------------------------------------------------------

template <class T> constexpr T Min(T x, T y) { return x < y ? x : y; }
template <class T> constexpr T Max(T x, T y) { return x > y ? x : y; }
template <class T> constexpr T Clamp(T x, T lo, T hi) { return x < lo ? lo : (x > hi ? hi : x); }

//--------------------------------------------------------------------------------------------------

}	// namespace JC

#include "JC/Core/SrcLoc.h"
#include "JC/Core/Span.h"
#include "JC/Core/TypeTraits.h"
#include "JC/Core/Arg.h"
#include "JC/Core/Panic.h"
#include "JC/Core/Mem.h"
#include "JC/Core/Str.h"
#include "JC/Core/Err.h"
#include "JC/Core/Res.h"
#include "JC/Core/FmtStr.h"

namespace JC {

template <class T> constexpr const T& Span<T>::operator[](u64 i) const {
	Assert(i < len);
	return data[i];
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC