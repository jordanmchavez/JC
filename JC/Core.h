#pragma once

#if defined _MSC_VER
	#include <string.h>	// memset/memmove/memcpy
#endif

namespace JC {

//--------------------------------------------------------------------------------------------------

#if defined _MSC_VER
	#define JC_COMPILER_MSVC
	#define JC_PLATFORM_WINDOWS

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

	#define JC_IF_CONSTEVAL if (__builtin_is_constant_evaluated())
	#define JC_BUILTIN_IS_ENUM(T) __is_enum(T)
#endif

constexpr U32 U32Max = 0xffffffff;
constexpr U64 U64Max = (U64)0xffffffffffffffff;

#define JC_MACRO_CONCAT_2(x, y) x##y
#define JC_MACRO_CONCAT(x, y)  JC_MACRO_CONCAT_2(x, y)
#define JC_MACRO_UNIQUE_NAME(x) JC_MACRO_CONCAT(x, __LINE__)
#define JC_MACRO_STRINGIZE_2(x) #x
#define JC_MACRO_STRINGIZE(x) JC_MACRO_STRINGIZE_2(x)
#define JC_MACRO_LINE_STR JC_MACRO_STRINGIZE(__LINE__)
#define JC_LENOF(a) ((U64)(sizeof(a) / sizeof(a[0])))

consteval const char* FileNameOnlyImpl(const char* path) {
	const char* fileName = path;
	const char* i = path;
	while (*i) {
		if (*i == '\\' || *i == '/') {
			fileName = i + 1;
		}
		i++;
	}
	return fileName;
}

#define JC_FILE_NAME_ONLY() FileNameOnlyImpl(__FILE__)

constexpr U64 KB = 1024;
constexpr U64 MB = 1024 * KB;
constexpr U64 GB = 1024 * MB;
constexpr U64 TB = 1024 * GB;

#define JC_DEFER \
	auto JC_MACRO_UNIQUE_NAME(Defer_) = DeferHelper() + [&]()

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

struct SrcLoc {
	const char* file = {};
	U32         line = 0;

	static consteval SrcLoc Here(
		#if defined JC_COMPILER_MSVC
			const char* file = __builtin_FILE(),
			U32 line         =  (U32)__builtin_LINE()
		#endif	// JC_COMPILER
	) {
		return SrcLoc { .file = file, .line = line };
	}
};

template <class T> struct SrcLocWrapper {
	T      val;
	SrcLoc sl;

	SrcLocWrapper(T v, SrcLoc slIn = SrcLoc::Here()) { val = v; sl = slIn; }
};

//--------------------------------------------------------------------------------------------------

} namespace std {
	template <class T> struct initializer_list {
		const T* _begin = 0;
		const T* _end   = 0;

		constexpr initializer_list() = default;
		constexpr initializer_list(const T* b, const T* e) { _begin = b; _end = e; }
		constexpr const T* begin() const { return _begin; }
		constexpr const T* end() const { return _end; }
		constexpr size_t size() const { return _end - _begin; }
	};
} namespace JC {

//--------------------------------------------------------------------------------------------------

template <class T> struct Span {
	T*  data = 0;
	U64 len  = 0;

	constexpr Span() = default;
	constexpr Span(T* d, U64 l) { data = d; len = l; }
	constexpr Span(std::initializer_list<T> il) { data = const_cast<T*>(il.begin()); len = il.size(); }
	constexpr Span(const Span&) = default;

	constexpr Span& operator=(const Span&) = default;
	
	constexpr       T& operator[](U64 i)       { return data[i]; }
	constexpr const T& operator[](U64 i) const { return data[i]; }

	constexpr       T* Begin()       { return data; }
	constexpr const T* Begin() const { return data; }
	constexpr       T* End()         { return data + len; }
	constexpr const T* End()   const { return data + len; }
};

//--------------------------------------------------------------------------------------------------

template <class T>            struct                TypeIdentity               { using Type = T; };
template <class T>            struct                RemovePtr                  { using Type = T; };
template <class T>            struct                RemovePtr<T*>              { using Type = T; };
template <class T>            struct                RemoveRef                  { using Type = T; };
template <class T>            struct                RemoveRef<T&>              { using Type = T; };
template <class T>            struct                RemoveRef<T&&>             { using Type = T; };
template <class T>            struct                RemoveConst                { using Type = T; };
template <class T>            struct                RemoveConst<const T>       { using Type = T; };
template <class T>            struct                RemoveVolatile             { using Type = T; };
template <class T>            struct                RemoveVolatile<volatile T> { using Type = T; };
template <class T1, class T2> struct                IsSameTypeT                { static constexpr Bool Val = false; };
template <class T>            struct                IsSameTypeT<T, T>          { static constexpr Bool Val = true;  };
template <class T1, class T2> inline constexpr Bool IsSameType                 = IsSameTypeT<T1, T2>::Val;
template <class T>            struct                IsPointerT                 { static constexpr Bool Val = false; };
template <class T>            struct                IsPointerT<T*>             { static constexpr Bool Val = true; };
template <class T>            inline constexpr Bool IsPointer                  = IsPointerT<T>::Val;
template <class T>            inline constexpr Bool IsEnum                     = JC_BUILTIN_IS_ENUM(T);
template <class...>           inline constexpr Bool AlwaysFalse                = false; 

//--------------------------------------------------------------------------------------------------

enum struct ArgType {
	Bool,
	Char,
	I64,
	U64,
	F64,
	Ptr,
	Str,
};

struct ArgStr {
	const char* data;
	U64         len;
};

struct Arg {
	ArgType type;
	union {
		Bool        b;
		char        c;
		I64         i;
		U64         u;
		F64         f;
		ArgStr      s;
		const void* p;
	};
};

template <class T>
Arg MakeArg(T val) {
	using Underlying = typename RemoveConst<typename RemoveVolatile<typename RemoveRef<T>::Type>::Type>::Type;
	     if constexpr (IsSameType<Underlying, Bool>)               { return { .type = ArgType::Bool, .b = val }; }
	else if constexpr (IsSameType<Underlying, char>)               { return { .type = ArgType::Char, .c = val }; }
	else if constexpr (IsSameType<Underlying, signed char>)        { return { .type = ArgType::I64,  .i = val }; }
	else if constexpr (IsSameType<Underlying, signed short>)       { return { .type = ArgType::U64,  .i = val }; }
	else if constexpr (IsSameType<Underlying, signed int>)         { return { .type = ArgType::I64,  .i = val }; }
	else if constexpr (IsSameType<Underlying, signed long>)        { return { .type = ArgType::I64,  .i = val }; }
	else if constexpr (IsSameType<Underlying, signed long long>)   { return { .type = ArgType::I64,  .i = val }; }
	else if constexpr (IsSameType<Underlying, unsigned char>)      { return { .type = ArgType::U64,  .u = val }; }
	else if constexpr (IsSameType<Underlying, unsigned short>)     { return { .type = ArgType::U64,  .u = val }; }
	else if constexpr (IsSameType<Underlying, unsigned int>)       { return { .type = ArgType::U64,  .u = val }; }
	else if constexpr (IsSameType<Underlying, unsigned long>)      { return { .type = ArgType::U64,  .u = val }; }
	else if constexpr (IsSameType<Underlying, unsigned long long>) { return { .type = ArgType::U64,  .u = val }; }
	else if constexpr (IsSameType<Underlying, float>)              { return { .type = ArgType::F64,  .f = val }; }
	else if constexpr (IsSameType<Underlying, double>)             { return { .type = ArgType::F64,  .f = val }; }
	else if constexpr (IsSameType<Underlying, char*>)              { return { .type = ArgType::Str,  .s = { .data = val, .len = strlen(val) } }; }
	else if constexpr (IsSameType<Underlying, const char*>)        { return { .type = ArgType::Str,  .s = { .data = val, .len = strlen(val) } }; }
	else if constexpr (IsPointer<Underlying>)                      { return { .type = ArgType::Ptr,  .p = val }; }
	else if constexpr (IsSameType<Underlying, decltype(nullptr)>)  { return { .type = ArgType::Ptr,  .p = nullptr }; }
	else if constexpr (IsEnum<Underlying>)                         { return { .type = ArgType::U64,  .u = (U64)val }; }
	else if constexpr (IsSameType<Underlying, Arg>)                { return val; }
	else if constexpr (IsSameType<Underlying, Span<Arg>>)          { static_assert(AlwaysFalse<T>, "You passed Span<Arg> as a placeholder variable: you probably meant to call VFmt() instead of Fmt()"); }
	else                                                           { static_assert(AlwaysFalse<T>, "Unsupported arg type"); }
}

template <U64 N> constexpr Arg MakeArg(char (&val)[N])       { return { .type = ArgType::Str, .s = { .data = val, .len = ConstExprStrLen(val) } }; }
template <U64 N> constexpr Arg MakeArg(const char (&val)[N]) { return { .type = ArgType::Str, .s = { .data = val, .len = ConstExprStrLen(val) } }; }

//--------------------------------------------------------------------------------------------------

struct NamedArg {
	const char* name = {};
	Arg         arg  = {};
};

template <class A, class... As> void BuildNamedArgs(NamedArg* namedArgs, const char* name, A arg, As... args) {
	namedArgs->name = name;
	namedArgs->arg  = MakeArg(arg);
	if constexpr (sizeof...(As) > 0) {
		BuildNamedArgs(namedArgs + 1, args...);
	}
}

inline void BuildNamedArgs(NamedArg*) {}

//--------------------------------------------------------------------------------------------------

inline void BadFmtStr_BadPlaceholderSpecOrUnmatchedOpenBrace() {}
inline void BadFmtStr_NotEnoughArgs() {}
inline void BadFmtStr_CloseBraceNotEscaped() {}
inline void BadFmtStr_TooManyArgs() {}

consteval const char* CheckFmtSpec(const char* i) {
	for (Bool flagsDone = false; !flagsDone; ) {
		switch (*i) {
			case '}': i++; return i;
			case '<': i++; break;
			case '+': i++; break;
			case ' ': i++; break;
			case '0': i++; flagsDone = true; break;
			default:       flagsDone = true; break;
		}
	}
	while (*i >= '0' && *i <= '9') {
		i++;
	}
	if (*i == '.') {
		i++;
		while (*i >= '0' && *i <= '9') {
			i++;
		}
	}
	if (!*i) { BadFmtStr_BadPlaceholderSpecOrUnmatchedOpenBrace(); }

	switch (*i) {
		case 'x': i++; break;
		case 'X': i++; break;
		case 'b': i++; break;
		case 'f': i++; break;
		case 'e': i++; break;
		default:       break;
	}
	if (*i != '}') { BadFmtStr_BadPlaceholderSpecOrUnmatchedOpenBrace(); }
	i++;
	return i;
}

consteval void CheckFmtStr(const char* fmt, size_t argsLen) {
	const char* i = fmt;
	U32 nextArg = 0;

	for (;;) {
		if (!*i) {
			if (nextArg != argsLen) { BadFmtStr_TooManyArgs(); }
			return;
		}

		if (*i == '{') {
			i++;
			if (*i == '{') {
				i++;
			} else {
				i = CheckFmtSpec(i);
				if (nextArg >= argsLen) { BadFmtStr_NotEnoughArgs(); }
				nextArg++;
			}
		} else if (*i != '}') {
			i++;
		} else {
			i++;
			if (*i != '}') { BadFmtStr_CloseBraceNotEscaped(); }
			i++;
		}
	}
}

template <class... A> struct _FmtStr {
	const char* fmt;
	consteval _FmtStr(const char* fmtIn) { CheckFmtStr(fmtIn, sizeof...(A)); fmt = fmtIn; }
	operator const char*() const { return fmt; }
};
template <class... A> using FmtStr = _FmtStr<typename TypeIdentity<A>::Type...>;

}	// namespace JC

#include "JC/Core/Panic.h"
#include "JC/Core/Mem.h"
#include "JC/Core/Str.h"
#include "JC/Core/Err.h"
#include "JC/Core/Res.h"