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

//--------------------------------------------------------------------------------------------------

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

template <class T>            struct                TypeIdentity                { using Type = T; };
template <class T>            struct                RemoveRef                   { using Type = T; };
template <class T>            struct                RemoveRef<T&>               { using Type = T; };
template <class T>            struct                RemoveRef<T&&>              { using Type = T; };
template <class T>            struct                RemoveConst                 { using Type = T; };
template <class T>            struct                RemoveConst<const T>        { using Type = T; };
template <class T>            struct                RemoveVolatile              { using Type = T; };
template <class T>            struct                RemoveVolatile<volatile T>  { using Type = T; };
template <class T1, class T2> struct                IsSameTypeT                 { static constexpr bool Val = false; };
template <class T>            struct                IsSameTypeT<T, T>           { static constexpr bool Val = true;  };
template <class T1, class T2> inline constexpr bool IsSameType                  = IsSameTypeT<T1, T2>::Val;
template <class T>            struct                IsPointerT                  { static constexpr bool Val = false; };
template <class T>            struct                IsPointerT<T*>              { static constexpr bool Val = true; };
template <class T>            inline constexpr bool IsPointer                   = IsPointerT<T>::Val;
template <class T>            inline constexpr bool IsEnum                      = BuiltinIsEnum(T);
template <class...>           inline constexpr bool AlwaysFalse                 = false; 

//--------------------------------------------------------------------------------------------------
/*

} namespace std {
	template <class T> struct initializer_list {
		const T* _begin = 0;
		const T* _end   = 0;

		constexpr initializer_list(const T* b, const T* e) { _begin = b; _end = e; }
		constexpr const T* begin() const { return _begin; }
		constexpr const T* end() const { return _end; }
		constexpr size_t size() const { return _end - _begin; }
	};
} namespace JC {

//--------------------------------------------------------------------------------------------------

template <class T> struct Span {
	const T* data = 0;
	u64      len  = 0;

	constexpr Span() { data = nullptr; len = 0; }
	constexpr Span(const T* d, u64 l) { data = d; len = l; }
	template <u64 N> constexpr Span(T (&a)[N]) { data = a; len = N; }
	constexpr Span(std::initializer_list<T> il) { data = il.begin(), len = il.size(); }
	constexpr Span(const Span&) = default;

	constexpr Span& operator=(const Span&) = default;
	
	constexpr const T& operator[](u64 i) const { Assert(i < len); return data[i]; }

	constexpr const T* Begin() const { return data; }
	constexpr const T* End()   const { return data + len; }
};

//--------------------------------------------------------------------------------------------------

struct ErrData;
ErrData* MakeErrData(Str ns, Str code, VArgs vargs, SrcLoc sl);

struct Err { ErrData* data = 0; };

#define DefErr(Ns, Code) \
	struct Err_##Code : Err { \
		template <class... A> Err_##Code(A... args, SrcLoc sl = SrcLoc::Here()) { \
			static_assert(sizeof...(A) % 2 == 0); \
			data = MakeErrData("MyNs", "MyCode", MakeVArgs(args...), sl); \
		} \
	}; \
	template <class...A> Err_##Code(A...) -> Err_##Code<A...>

//--------------------------------------------------------------------------------------------------

template <class T = void> struct [[nodiscard]] Res;

template <> struct [[nodiscard]] Res<void> {
	Err err = {};

	constexpr Res() = default;
	constexpr Res(Err e) { err = e; }	// implicit
	template <class T> constexpr Res(const Res<T>& r) {
		if constexpr (IsSameType<T, void>) {
			err = r.err;
		} else {
			Assert(!r.hasVal);
			err = r.err;
		}
	}

	constexpr operator bool() const { return err.data; }

	constexpr void Ignore() const {}
};

template <class T> struct [[nodiscard]] Res {
	union {
		T   val;
		Err err;
	};
	bool hasVal = false;

	constexpr Res()      {          hasVal = false; }
	constexpr Res(T v)   { val = v; hasVal = true;  }	// implicit
	constexpr Res(Err e) { err = e; hasVal = false; }	// implicit
	template <class U> constexpr Res(const Res<U>& r) {
		if constexpr (IsSameType<U, T>) {
			hasVal = r.hasVal;
			if (r.hasVal) {
				val = r.val;
			} else {
				err = r.err;
			}
		} else if constexpr (IsSameType<U, void>) {
			Assert(r.err.handle);
			err = r.err;
			hasVal = false;
			
		} else {
			Assert(!r.hasVal);
			err = r.err;
			hasVal = false;
		}
	}

	constexpr operator bool() const { return hasVal; }

	constexpr Res<> To(T& out) { if (hasVal) { out = val; return Res<>{}; } return Res<>(err); }
	constexpr T     Or(T def) { return hasVal ? val : def; }

	constexpr void Ignore() const {}
};

constexpr Res<> Ok() { return Res<>(); }

//--------------------------------------------------------------------------------------------------

template <class T> constexpr T Min(T x, T y) { return x < y ? x : y; }
template <class T> constexpr T Max(T x, T y) { return x > y ? x : y; }
template <class T> constexpr T Clamp(T x, T lo, T hi) { return x < lo ? lo : (x > hi ? hi : x); }

//--------------------------------------------------------------------------------------------------

struct Rect {
	i32 x = 0;
	i32 y = 0;
	u32 w = 0;
	u32 h = 0;
};

//--------------------------------------------------------------------------------------------------
*/
}	// namespace JC

#include "JC/Common/Str.h"
#include "JC/Common/SrcLoc.h"
#include "JC/Common/Mem.h"
#include "JC/Common/VArg.h"
#include "JC/Common/Fmt.h"
#include "JC/Common/Panic.h"
