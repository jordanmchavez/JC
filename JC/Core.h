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

//--------------------------------------------------------------------------------------------------

[[noreturn]] void VPanic(SrcLoc sl, const char* expr, const char* fmt, Span<const Arg> args);


template <class... A> [[noreturn]] void Panic(SrcLoc sl, FmtStr<A...> fmt, A... args) {
	VPanic(sl, 0, fmt, { MakeArg(args)... });
}

using PanicFn = void (SrcLoc sl, const char* expr, const char* msg);
PanicFn* SetPanicFn(PanicFn* panicFn);

[[noreturn]] inline void AssertFail(SrcLoc sl, const char* expr) {
	VPanic(sl, expr, 0, {});
}

template <class... A> [[noreturn]] void AssertFail(SrcLoc sl, const char* expr, FmtStr<A...> fmt, A... args) {
	VPanic(sl, expr, fmt, { MakeArg<A>(args)... });
}

#define JC_PANIC(fmt, ...) JC::Panic(SrcLoc::Here(), fmt, ##__VA_ARGS__)

//--------------------------------------------------------------------------------------------------

} namespace JC::Mem {

constexpr U32 AllocFlag_NoInit   = 1 << 0;	// memset for new regions, memcpy for realloc'd regions

struct Allocator {
	virtual void* Alloc(void* ptr, U64 ptrSize, U64 newSize, U32 flags, SrcLoc sl = SrcLoc::Here()) = 0;

	void* Alloc  (                        U64 newSize, SrcLoc sl = SrcLoc::Here()) { return Alloc(0,   0,       newSize, 0, sl); }
	void* Realloc(void* ptr, U64 ptrSize, U64 newSize, SrcLoc sl = SrcLoc::Here()) { return Alloc(ptr, ptrSize, newSize, 0, sl); }
	void  Free   (void* ptr, U64 ptrSize,              SrcLoc sl = SrcLoc::Here()) {        Alloc(ptr, ptrSize,    0,    0, sl); }

	template <class T> T* AllocT  (                  U64 n = 1, SrcLoc sl = SrcLoc::Here()) { return (T*)Alloc(0,   0,                n * sizeof(T), 0, sl); }
	template <class T> T* ReallocT(T* ptr, U64 oldN, U64 n,     SrcLoc sl = SrcLoc::Here()) { return (T*)Alloc(ptr, oldN * sizeof(T), n * sizeof(T), 0, sl); }
};

struct TempAllocator : Allocator {
	virtual void Reset() = 0;
};

void Init(U64 permCommitSize, U64 permReserveSize, U64 tempReserveSize);

extern Allocator*     permAllocator;
extern TempAllocator* tempAllocator;

} namespace JC {

//--------------------------------------------------------------------------------------------------

#define JC_ASSERT(expr, ...) \
	do { \
		if (!(expr)) { \
			JC::AssertFail(SrcLoc::Here(), #expr, ##__VA_ARGS__); \
		} \
	} while (false)

constexpr U64 ConstExprStrLen(const char* s) {
	JC_IF_CONSTEVAL {
		const char* i = s; 
		for (; *i; i++) {}
		return (U64)(i - s);
	} else {
		return strlen(s);
	}
}

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

struct Str {
	const char* data = "";
	U64         len  = 0;

	constexpr Str() = default;
	constexpr Str(const Str&) = default;
	constexpr Str(const char* s)                {                  data = s; len = ConstExprStrLen(s); }
	constexpr Str(const char* d, U64 l)         { JC_ASSERT(d || !l); data = d; len = l; }
	constexpr Str(const char* b, const char* e) { JC_ASSERT(b <= e);  data = b; len = e - b; }

	constexpr Str& operator=(const Str&) = default;
	constexpr Str& operator=(const char* s) { data = s; len = ConstExprStrLen(s); return *this; }

	constexpr char operator[](U64 i) const { JC_ASSERT(i < len); return data[i]; }
};

inline Bool operator==(Str         s1, Str         s2) { return s1.len == s2.len && !memcmp(s1.data, s2.data, s1.len); }
inline Bool operator==(const char* s1, Str         s2) { return !strcmp(s1,      s2.data); }
inline Bool operator==(Str         s1, const char* s2) { return !strcmp(s1.data, s2); }
inline Bool operator!=(Str         s1, Str         s2) { return s1.len != s2.len || memcmp(s1.data, s2.data, s1.len); }
inline Bool operator!=(const char* s1, Str         s2) { return strcmp(s1,      s2.data); }
inline Bool operator!=(Str         s1, const char* s2) { return strcmp(s1.data, s2); }

constexpr Arg MakeArg(Str val) { return { .type = ArgType::Str, .s = { .data = val.data, .len = val.len } }; }

Str Copy(Mem::Allocator* allocator, Str s);

//--------------------------------------------------------------------------------------------------

struct WStrZ {
	const wchar_t* data = nullptr;
	U64            len  = 0;

	constexpr WStrZ() = default;
	constexpr WStrZ(const WStrZ&) = default;
	constexpr WStrZ(const wchar_t* sz)       { data = sz;                  len = ConstExprWStrLen(sz); }
	constexpr WStrZ(const wchar_t* p, U64 l) { JC_ASSERT(!l || p); data = p;  len = l; }

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

struct ErrCode {
	Str ns;
	Str code;
};

struct [[nodiscard]] Err {
	static constexpr U32 MaxArgs = 32;

	struct Data {
		Data*    prev               = {};
		SrcLoc   sl                 = {};
		Str      ns                 = {};
		Str      code               = {};
		NamedArg namedArgs[MaxArgs] = {};
		U32      namedArgsLen       = 0;
	};

	Data* data = 0;

	static void Init(Mem::TempAllocator* tempAllocator);

	Err() = default;

	template <class...A> Err(Str ns, Str code, A... args, SrcLoc sl) {
		NamedArg namedArgs[1 + (sizeof...(A) / 2)];
		BuildNamedArgs(namedArgs, args...);
		Init(ns, code, Span<const NamedArg>(namedArgs, sizeof...(A) / 2), sl);
	}

	void Init(Str ns, Str code, Span<const NamedArg> namedArgs, SrcLoc sl);
	void Init(Str ns, U64 code, Span<const NamedArg> namedArgs, SrcLoc sl);

	Err Push(Err err);
	
	Str GetStr();
};

constexpr bool operator==(ErrCode ec, Err err) { return err.data && ec.ns == err.data->ns && ec.code == err.data->code; }
constexpr bool operator==(Err err, ErrCode ec) { return err.data && ec.ns == err.data->ns && ec.code == err.data->code; }

static_assert(sizeof(Err) == 8);

#define DefErr(InNs, InCode) \
	constexpr ErrCode EC_##InCode = { .ns = #InNs, .code = #InCode }; \
	template <class... A> struct Err_##InCode : JC::Err { \
		static inline U8 Sig = 0; \
		Err_##InCode(A... args, SrcLoc sl = SrcLoc::Here()) { \
			NamedArg namedArgs[1 + (sizeof...(A) / 2)]; \
			BuildNamedArgs(namedArgs, args...); \
			Init(#InNs, #InCode, Span<const NamedArg>(namedArgs, sizeof...(A) / 2), sl); \
		} \
	}; \
	template <class...A> Err_##InCode(A...) -> Err_##InCode<A...>

//--------------------------------------------------------------------------------------------------

template <class T = void> struct [[nodiscard]] Res;

template <> struct [[nodiscard]] Res<void> {
	Err err = {};

	constexpr Res() = default;
	constexpr Res(Err e) { err = e; }	// implicit
	constexpr Res(const Res<>&) = default;
	constexpr operator Bool() const { return !err.data; }
};

static_assert(sizeof(Res<>) == 8);

template <class T> struct [[nodiscard]] Res {
	union {
		T   val;
		Err err;
	};
	Bool hasVal = false;

	constexpr Res() = default;
	constexpr Res(T v)   { val = v; hasVal = true;  }	// implicit
	constexpr Res(Err e) { err = e; hasVal = false; }	// implicit
	constexpr Res(const Res<T>&) = default;
	constexpr operator Bool() const { return hasVal; }
	constexpr Res<> To(T& out) { if (hasVal) { out = val; return Res<>{}; } return Res<>(err); }
	constexpr T Or(T def) { return hasVal ? val : def; }
	template <class T> constexpr bool Is() { return !hasVal && err.Is<T>(); }
};

constexpr Res<> Ok() { return Res<>(); }

#define CheckRes(Code) \
	if (Res<> _r = Code; !_r) { \
		return _r.err.Push(Err("", "", SrcLoc::Here())); \
	}

//--------------------------------------------------------------------------------------------------

}	// namespace JC