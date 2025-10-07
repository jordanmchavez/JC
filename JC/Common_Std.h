#pragma once

#include "JC/Common_Platform.h"

//--------------------------------------------------------------------------------------------------

#if defined Compiler_Msvc
	extern "C" {
		using size_t = decltype(sizeof(0));
		void*  __cdecl memcpy(void* dst, const void* src, size_t size);
		void*  __cdecl memmove(void* dst, const void* src, size_t size);
		int    __cdecl memcmp(const void* p1, const void* p2, size_t size);
		void*  __cdecl memset(void* p, int val, size_t n);
		size_t __cdecl strlen(char const* s);
		int    __cdecl strcmp(char const* s1, char const* s2);
	}
#endif	// Compiler

namespace std {
	template <class T> struct initializer_list {
		const T* _begin = 0;
		const T* _end   = 0;

		constexpr initializer_list() = default;
		constexpr initializer_list(const T* b, const T* e) { _begin = b; _end = e; }
		constexpr const T* begin() const { return _begin; }
		constexpr const T* end() const { return _end; }
		constexpr size_t size() const { return _end - _begin; }
	};
}	// namespace std

namespace JC {

//--------------------------------------------------------------------------------------------------

#if defined Compiler_Msvc
	using I8  = signed char;
	using I16 = signed short;
	using I32 = signed int;
	using I64 = signed long long;

	using U8  = unsigned char;
	using U16 = unsigned short;
	using U32 = unsigned int;
	using U64 = unsigned long long;

	using F32 = float;
	using F64 = double;

	#define Sys_DbgBreak __debugbreak()
#endif	// Compiler

constexpr U32 U32Max = 0xffffffff;
constexpr U64 U64Max = (U64)0xffffffffffffffff;

constexpr U64 KB = 1024;
constexpr U64 MB = 1024 * KB;
constexpr U64 GB = 1024 * MB;
constexpr U64 TB = 1024 * GB;

//--------------------------------------------------------------------------------------------------

struct IRect { I32 x, y; U32 width, height; };
struct Vec2 { F32 x, y; };
struct Vec3 { F32 x, y, z; };
struct Vec4 { F32 x, y, z, w; };
struct Mat2 { F32 m[2][2]; };
struct Mat3 { F32 m[3][3]; };
struct Mat4 { F32 m[4][4]; };

template <class T> struct Array;

//--------------------------------------------------------------------------------------------------

#define MacroConcat2(x, y) x##y
#define MacroConcat(x, y)  MacroConcat2(x, y)
#define MacroUniqueName(x) MacroConcat(x, __LINE__)
#define MacroStringize2(x) #x
#define MacroStringize(x) MacroStringize(x)
#define MacroLineStr MacroStringize(__LINE__)
#define LenOf(a) (sizeof(a) / sizeof(a[0]))

//--------------------------------------------------------------------------------------------------

constexpr U32 ConstExprStrLen(char const* s) {
	IfConstEval {
		char const* i = s; 
		for (; *i; i++) {}
		return (U32)(i - s);
	} else {
		return (U32)strlen(s);
	}
}

struct Str {
	char const* data = 0;
	U64         len  = 0;

	constexpr Str() = default;
	constexpr Str(Str const&) = default;
	constexpr Str(char const* s) { data = s; len = s ? ConstExprStrLen(s) : 0; }
	Str(char const* s, U64 l);
	constexpr Str& operator=(Str const&) = default;
	constexpr Str& operator=(char const* s) { data = s; len = s ? ConstExprStrLen(s) : 0; return *this; }
	char operator[](U64 i) const { return data[i]; }
};

bool operator==(Str s1, Str s2);
bool operator!=(Str s1, Str s2);

//--------------------------------------------------------------------------------------------------

template <class T> struct Span {
	T*  data = 0;
	U64 len  = 0;

	constexpr Span() = default;
	constexpr Span(T* d, U64 l) { data = d; len = l; }
	constexpr Span(std::initializer_list<T> il) { data = const_cast<T*>(il.begin()); len = il.size(); }
	template <U64 N> Span(T (&arr)[N]) { data = arr; len = N; }
	constexpr Span(const Span&) = default;
	constexpr Span& operator=(Span const&) = default;
	T& operator[](U64 i)       { return data[i]; }
	T  operator[](U64 i) const { return data[i]; }
};

//--------------------------------------------------------------------------------------------------

template <class T> constexpr T Min(T x, T y) { return x < y ? x : y; }
template <class T> constexpr T Max(T x, T y) { return x > y ? x : y; }
template <class T> constexpr T Clamp(T x, T lo, T hi) { return x < lo ? lo : (x > hi ? hi : x); }

//--------------------------------------------------------------------------------------------------

}	// namespace JC