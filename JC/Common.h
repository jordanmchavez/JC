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

//--------------------------------------------------------------------------------------------------

struct SrcLoc {
	Str file = {};
	u32 line = 0;

	static consteval SrcLoc Here(Str file = BuiltinFile, u32 line = BuiltinLine) {
		return SrcLoc { .file = file, .line = line };
	}
};

//--------------------------------------------------------------------------------------------------

enum struct VArgType {
	Bool,
	Char,
	I64,
	U64,
	F64,
	Ptr,
	Str,
};

struct VArgStr {
	const char* data;
	u64         len;
};

struct VArgs;

struct VArg {
	VArgType type;
	union {
		bool        b;
		char        c;
		i64         i;
		u64         u;
		f64         f;
		VArgStr     s;
		const void* p;
	};
};

template <class T>
static VArg MakeVArg(T val) {
	using Underlying = typename RemoveConst<typename RemoveVolatile<typename RemoveRef<T>::Type>::Type>::Type;
	     if constexpr (IsSameType<Underlying, bool>)               { return { .type = VArgType::Bool, .b = val }; }
	else if constexpr (IsSameType<Underlying, char>)               { return { .type = VArgType::Char, .c = val }; }
	else if constexpr (IsSameType<Underlying, signed char>)        { return { .type = VArgType::I64,  .i = val }; }
	else if constexpr (IsSameType<Underlying, signed short>)       { return { .type = VArgType::U64,  .i = val }; }
	else if constexpr (IsSameType<Underlying, signed int>)         { return { .type = VArgType::I64,  .i = val }; }
	else if constexpr (IsSameType<Underlying, signed long>)        { return { .type = VArgType::I64,  .i = val }; }
	else if constexpr (IsSameType<Underlying, signed long long>)   { return { .type = VArgType::I64,  .i = val }; }
	else if constexpr (IsSameType<Underlying, unsigned char>)      { return { .type = VArgType::U64,  .u = val }; }
	else if constexpr (IsSameType<Underlying, unsigned short>)     { return { .type = VArgType::U64,  .u = val }; }
	else if constexpr (IsSameType<Underlying, unsigned int>)       { return { .type = VArgType::U64,  .u = val }; }
	else if constexpr (IsSameType<Underlying, unsigned long>)      { return { .type = VArgType::U64,  .u = val }; }
	else if constexpr (IsSameType<Underlying, unsigned long long>) { return { .type = VArgType::U64,  .u = val }; }
	else if constexpr (IsSameType<Underlying, float>)              { return { .type = VArgType::F64,  .f = val }; }
	else if constexpr (IsSameType<Underlying, double>)             { return { .type = VArgType::F64,  .f = val }; }
	else if constexpr (IsSameType<Underlying, Str>)                { return { .type = VArgType::Str,  .s = { .data = val.data, .len = val.len } }; }
	else if constexpr (IsSameType<Underlying, char*>)              { return { .type = VArgType::Str,  .s = { .data = val,      .len = ConstExprStrLen(val) } }; }
	else if constexpr (IsSameType<Underlying, const char*>)        { return { .type = VArgType::Str,  .s = { .data = val,      .len = ConstExprStrLen(val) } }; }
	else if constexpr (IsPointer<Underlying>)                      { return { .type = VArgType::Ptr,  .p = val }; }
	else if constexpr (IsSameType<Underlying, decltype(nullptr)>)  { return { .type = VArgType::Ptr,  .p = nullptr }; }
	else if constexpr (IsEnum<Underlying>)                         { return { .type = VArgType::U64,  .u = (u64)val }; }
	else if constexpr (IsSameType<Underlying, VArg>)               { return val; }
	else if constexpr (IsSameType<Underlying, VArgs>)              { static_assert(AlwaysFalse<T>, "You passed VArgs as a placeholder variable: you probably meant to call VFmt() instead of Fmt()"); }
	else                                                           { static_assert(AlwaysFalse<T>, "Unsupported arg type"); }
}
template <u64 N> static constexpr VArg MakeVArg(char (&val)[N])       { return { .type = VArgType::Str, .s = { .data = val, .len = ConstExprStrLen(val) } }; }
template <u64 N> static constexpr VArg MakeVArg(const char (&val)[N]) { return { .type = VArgType::Str, .s = { .data = val, .len = ConstExprStrLen(val) } }; }

template <u32 N> struct VArgStore {
	VArg vargs[N > 0 ? N : 1] = {};
};

struct VArgs {
	const VArg* vargs = 0;
	u32         len  = 0;

	template <u32 N> constexpr VArgs(VArgStore<N> argStore) {
		vargs = argStore.vargs;
		len  = N;
	}
};

template <class... A> constexpr VArgStore<sizeof...(A)> MakeVArgs(A... args) {
	return VArgStore<sizeof...(A)> { MakeVArg(args)... };
}

//--------------------------------------------------------------------------------------------------

template <class... A> struct _FmtStr {
	Str fmt;
	consteval _FmtStr(Str         fmtIn);
	consteval _FmtStr(const char* fmtIn);
	operator Str() const { return fmt; }
};

template <class... A> using FmtStr = _FmtStr<typename TypeIdentity<A>::Type...>;

template <class... A> struct _FmtStrSrcLoc {
	Str    fmt;
	SrcLoc sl;
	consteval _FmtStrSrcLoc(Str         fmtIn, SrcLoc slIn = SrcLoc::Here());
	consteval _FmtStrSrcLoc(const char* fmtIn, SrcLoc slIn = SrcLoc::Here());
	operator Str() const { return fmt; }
};

template <class... A> using FmtStrSrcLoc = _FmtStrSrcLoc<typename TypeIdentity<A>::Type...>;

//--------------------------------------------------------------------------------------------------

[[noreturn]] void VPanic(SrcLoc sl, Str expr, Str fmt, VArgs vargs);
template <class... A> [[noreturn]] void Panic(FmtStrSrcLoc<A...> fmtSl, A... args);

[[noreturn]] inline void _AssertFail(SrcLoc sl, Str expr);
template <class... A> [[noreturn]] void _AssertFail(SrcLoc sl, Str expr, FmtStr<A...> fmt, A... args);

using PanicFn = void (SrcLoc sl, Str expr, Str fmt, VArgs vargs);

PanicFn* SetPanicFn(PanicFn* panicFn);

#define Assert(expr, ...) \
	do { \
		if (!(expr)) { \
			_AssertFail(SrcLoc::Here(), #expr, ##__VA_ARGS__); \
		} \
	} while (false)

//--------------------------------------------------------------------------------------------------

struct Allocator {
	virtual void* Alloc(u64 size, SrcLoc sl = SrcLoc::Here()) = 0;
	virtual bool  Extend(void* ptr, u64 size, u64 newSize, SrcLoc sl = SrcLoc::Here()) = 0;
		    void* Realloc(void* ptr, u64 size, u64 newSize, SrcLoc sl = SrcLoc::Here());
	virtual void  Free(void* ptr, u64 size, SrcLoc sl = SrcLoc::Here()) = 0;

	template <class T> T*    AllocT(u64 n = 1, SrcLoc sl = SrcLoc::Here()) { return (T*)Alloc(n * sizeof(T),sl); }
	template <class T> bool  ExtendT(T* ptr, u64 n, u64 newN, SrcLoc sl = SrcLoc::Here()) { return Extend(ptr, n * sizeof(T), newN * sizeof(T), sl); }
	template <class T> T*    ReallocT(T* ptr, u64 n, u64 newN, SrcLoc sl = SrcLoc::Here()) { return (T*)Realloc(ptr, n * sizeof(T), newN * sizeof(T), sl); }
	template <class T> void  FreeT(T* ptr, u64 n, SrcLoc sl = SrcLoc::Here()) { Free(ptr, n * sizeof(T), sl); }
};

struct TempAllocator : Allocator {
	void Free(void*, u64, SrcLoc) override {}
	virtual void Reset() = 0;
};

//--------------------------------------------------------------------------------------------------

enum struct LogLevel {
	Log,
	Err,
};

struct Logger {
	virtual               void VPrintf(SrcLoc sl, LogLevel level, Str          fmt, VArgs args) = 0;
	template <class... A> void  Printf(SrcLoc sl, LogLevel level, FmtStr<A...> fmt, A...  args);
};

#define Logf(fmt, ...) logger->Printf(SrcLoc::Here(), LogLevel::Log, fmt, __VA_ARGS__)
#define Errf(fmt, ...) logger->Printf(SrcLoc::Here(), LogLevel::Err, fmt, __VA_ARGS__)

//--------------------------------------------------------------------------------------------------

template <class T> struct Array;
template <class K, class V> struct Map;

//--------------------------------------------------------------------------------------------------

char* VFmt(char* outBegin, char* outEnd, Str fmt, VArgs args);
void  VFmt(Array<char>* out,             Str fmt, VArgs args);
Str   VFmt(Allocator* allocator,         Str fmt, VArgs args);

template <class... A> char* Fmt(char* outBegin, char* outEnd, FmtStr<A...> fmt, A... args);
template <class... A> void  Fmt(Array<char>* out,             FmtStr<A...> fmt, A... args);
template <class... A> Str   Fmt(Allocator* allocator,         FmtStr<A...> fmt, A... args);

//--------------------------------------------------------------------------------------------------

struct [[nodiscard]] Err {
	static constexpr u32 MaxArgs = 32;

	struct Arg {
		Str  name = {};
		VArg varg  = {};
	};

	struct Data {
		Data*  prev               = {};
		SrcLoc sl                 = {};
		Str    ns                 = {};
		Str    code               = {};
		Arg    namedArgs[MaxArgs] = {};
		u32    namedArgsLen       = 0;
	};

	Data* data = 0;

	Err() = default;
	template <class...A> Err(SrcLoc sl, Str ns, Str code, A... args);
	void Init(SrcLoc sl, Str ns, Str code, VArgs vargs);
	Err Push(Err err);
};

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
	template <class T> constexpr Res(const Res<T>& r);
	constexpr operator bool() const;
};

template <class T> struct [[nodiscard]] Res {
	union {
		T   val;
		Err err;
	};
	bool hasVal = false;

	constexpr Res();
	constexpr Res(T v);
	constexpr Res(Err e);
	template <class U> constexpr Res(const Res<U>& r);
	constexpr operator bool() const;
	constexpr Res<> To(T& out);
	constexpr T  Or(T def);
};

constexpr Res<> Ok();

//--------------------------------------------------------------------------------------------------

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
	
	//constexpr const T& operator[](u64 i) const { Assert(i < len); return data[i]; }
	constexpr const T& operator[](u64 i) const {
		if (i >= len) {
			_AssertFail(SrcLoc::Here(), "");
		}
		return data[i];
	}

	constexpr const T* Begin() const { return data; }
	constexpr const T* End()   const { return data + len; }
};

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

#include "JC/Common/Err.inl.h"
#include "JC/Common/Fmt.inl.h"
#include "JC/Common/Panic.inl.h"
#include "JC/Common/Str.inl.h"
