#pragma once

#if defined _MSC_VER
	#include <string.h>	// memset/memmove/memcpy
#endif

namespace JC {

//--------------------------------------------------------------------------------------------------

#if defined _MSC_VER
	#define Compiler_Msvc
	#define Platform_Windows

	using  I8  = signed char;
	using  U8  = unsigned char;
	using I16  = signed short;
	using U16  = unsigned short;
	using I32  = signed int;
	using U32  = unsigned int;
	using I64  = signed long long;
	using U64  = unsigned long long;
	using F32  = float;
	using F64  = double;
	using Bool = bool;

	#define BuiltinFile __builtin_FILE()
	#define BuiltinLine ((U32)__builtin_LINE())
	#define IfConsteval if (__builtin_is_constant_evaluated())
	#define BuiltinIsEnum(T) __is_enum(T)
#endif

constexpr U32 U32Max = 0xffffffff;
constexpr U64 U64Max = (U64)0xffffffffffffffff;

#define MacroConcat2(x, y) x##y
#define MacroConcat(x, y)  MacroConcat2(x, y)
#define MacroName(x) MacroConcat(x, __LINE__)
#define MacroStringize2(x) #x
#define MacroStringize(x) MacroStringize2(x)
#define LineStr MacroStringize(__LINE__)
#define LenOf(a) (U64)(sizeof(a) / sizeof(a[0]))

constexpr U64 KB = 1024;
constexpr U64 MB = 1024 * KB;
constexpr U64 GB = 1024 * MB;
constexpr U64 TB = 1024 * GB;

#define Defer \
	auto MacroName(Defer_) = DeferHelper() + [&]()

template <class F> struct DeferInvoker {
	F fn;
	DeferInvoker(F&& fn_) : fn(fn_) {}
	~DeferInvoker() { fn(); }
};

enum struct DeferHelper {};
template <class F> DeferInvoker<F> operator+(DeferHelper, F&& fn) { return DeferInvoker<F>((F&&)fn); }

//--------------------------------------------------------------------------------------------------

struct Rect {
	I32 x = 0;
	I32 y = 0;
	U32 w = 0;
	U32 h = 0;
};

struct Vec2 {
	F32 x = 0.0f;
	F32 y = 0.0f;
};

struct Vec3 {
	F32 x = 0.0f;
	F32 y = 0.0f;
	F32 z = 0.0f;
};

struct Vec4 {
	F32 x = 0.0f;
	F32 y = 0.0f;
	F32 z = 0.0f;
	F32 w = 0.0f;
};

struct Mat2 {
	F32 m[2][2] = {};
};

struct Mat3 {
	F32 m[3][3] = {};
};

struct Mat4 {
	F32 m[4][4] = {};
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

//--------------------------------------------------------------------------------------------------

namespace JC {

}	// namespace JC