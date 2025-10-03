#pragma once

//--------------------------------------------------------------------------------------------------

#if defined _MSC_VER
	#define Platform_Windows
	#define Compiler_Msvc

	//__pragma(pack(push, _CRT_PACKING))
	extern "C" {
	using size_t = decltype(sizeof(0));
	void*  __cdecl memcpy(void* dst, const void* src, size_t size);
	void*  __cdecl memmove(void* dst, const void* src, size_t size);
	int    __cdecl memcmp(const void* p1, const void* p2, size_t size);
	void*  __cdecl memset(void* p, int val, size_t n);
	size_t __cdecl strlen(char const* s);
	int    __cdecl strcmp(char const* s1, char const* s2);
	}
	//__pragma(pack(pop))
#endif	// _MSC_VER

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

//--------------------------------------------------------------------------------------------------

#if defined Compiler_Msvc
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

	#define SrcLoc_File __builtin_FILE()
	#define SrcLoc_Line __builtin_LINE()
	#define Dbg_Break __debugbreak()
	#define IfConstEval if (__builtin_is_constant_evaluated())
#endif	// Compiler_Msvc

constexpr U32 U32Max = 0xffffffff;
constexpr U64 U64Max = (U64)0xffffffffffffffff;

#define MacroConcat2(x, y) x##y
#define MacroConcat(x, y)  MacroConcat2(x, y)
#define MacroUniqueName(x) MacroConcat(x, __LINE__)
#define MacroStringize2(x) #x
#define MacroStringize(x) MacroStringize(x)
#define MacroLineStr MacroStringize(__LINE__)
#define LenOf(a) (sizeof(a) / sizeof(a[0]))

constexpr U32 ConstExprStrLen(char const* s) {
	IfConstEval {
		char const* i = s; 
		for (; *i; i++) {}
		return (U32)(i - s);
	} else {
		return (U32)strlen(s);
	}
}

struct Mem;

template <class T> struct Array;
template <class K, class V> struct Map;

template <class T> constexpr T Min(T x, T y) { return x < y ? x : y; }
template <class T> constexpr T Max(T x, T y) { return x > y ? x : y; }
template <class T> constexpr T Clamp(T x, T lo, T hi) { return x < lo ? lo : (x > hi ? hi : x); }

//--------------------------------------------------------------------------------------------------

struct SrcLoc {
	char const* file;
	U32         line;

	static consteval SrcLoc Here(char const* file = SrcLoc_File, U32 line = (U32)SrcLoc_Line) {
		return SrcLoc { .file = file, .line = line };
	}
};

//--------------------------------------------------------------------------------------------------

enum struct ArgType { Bool, Char, I64, U64, F64, Str, Ptr };

struct Arg {
	ArgType type;
	union {
		Bool b;
		char c;
		I64 i;
		U64 u;
		F64 f;
		struct { char const* s; U32 l; } s;
		const void* p;
	};
};

constexpr Arg Arg_Make(bool               val) { return Arg { .type = ArgType::Bool, .b  = val }; }
constexpr Arg Arg_Make(char               val) { return Arg { .type = ArgType::Char, .c  = val }; }
constexpr Arg Arg_Make(signed char        val) { return Arg { .type = ArgType::I64,  .i  = val }; }
constexpr Arg Arg_Make(signed short       val) { return Arg { .type = ArgType::I64,  .i  = val }; }
constexpr Arg Arg_Make(signed int         val) { return Arg { .type = ArgType::I64,  .i  = val }; }
constexpr Arg Arg_Make(signed long        val) { return Arg { .type = ArgType::I64,  .i  = val }; }
constexpr Arg Arg_Make(signed long long   val) { return Arg { .type = ArgType::I64,  .i  = val }; }
constexpr Arg Arg_Make(unsigned char      val) { return Arg { .type = ArgType::U64,  .u  = val }; }
constexpr Arg Arg_Make(unsigned short     val) { return Arg { .type = ArgType::U64,  .u  = val }; }
constexpr Arg Arg_Make(unsigned int       val) { return Arg { .type = ArgType::U64,  .u  = val }; }
constexpr Arg Arg_Make(unsigned long      val) { return Arg { .type = ArgType::U64,  .u  = val }; }
constexpr Arg Arg_Make(unsigned long long val) { return Arg { .type = ArgType::U64,  .u  = val }; }
constexpr Arg Arg_Make(float              val) { return Arg { .type = ArgType::F64,  .f  = val }; }
constexpr Arg Arg_Make(double             val) { return Arg { .type = ArgType::F64,  .f  = val }; }
constexpr Arg Arg_Make(char*              val) { return Arg { .type = ArgType::Str,  .s  = { .s = val, .l = val ? ConstExprStrLen(val) : 0 } }; }
constexpr Arg Arg_Make(char const*        val) { return Arg { .type = ArgType::Str,  .s  = { .s = val, .l = val ? ConstExprStrLen(val) : 0 } }; }
constexpr Arg Arg_Make(const void*        val) { return Arg { .type = ArgType::Ptr,  .p  = val }; }
constexpr Arg Arg_Make(Arg                val) { return val; }
										      
constexpr void Arg_Fill(Arg*) {}

template <class A1, class... A>
constexpr void Arg_Fill(Arg* out, A1 arg1, A... args) {
	*out = MakeArg(arg1);
	FillArgs(out + 1, args...);
}

//--------------------------------------------------------------------------------------------------

inline void FmtStr_BadPlaceholderSpecOrUnmatchedOpenBrace() {}
inline void FmtStr_NotEnoughArgs() {}
inline void FmtStr_CloseBraceNotEscaped() {}
inline void FmtStr_TooManyArgs() {}

consteval char const* FmtStr_CheckSpec(char const* i) {
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
	if (!*i) { FmtStr_BadPlaceholderSpecOrUnmatchedOpenBrace(); }

	switch (*i) {
		case 'x': i++; break;
		case 'X': i++; break;
		case 'b': i++; break;
		case 'f': i++; break;
		case 'e': i++; break;
		default:       break;
	}
	if (*i != '}') { FmtStr_BadPlaceholderSpecOrUnmatchedOpenBrace(); }
	i++;
	return i;
}

consteval void FmtStr_Check(char const* fmt, size_t argsLen) {
/*
	char const* i = fmt;
	U32 nextArg = 0;

	for (;;) {
		if (!*i) {
			if (nextArg != argsLen) { FmtStr_TooManyArgs(); }
			return;
		}

		if (*i == '{') {
			i++;
			if (*i == '{') {
				i++;
			} else {
				i = FmtStr_CheckSpec(i);
				if (nextArg >= argsLen) { FmtStr_NotEnoughArgs(); }
				nextArg++;
			}
		} else if (*i != '}') {
			i++;
		} else {
			i++;
			if (*i != '}') { FmtStr_CloseBraceNotEscaped(); }
			i++;
		}
	}*/
}

template <class T> struct TypeIdentity { using Type = T; };

template <class... A> struct _FmtStr {
	char const* fmt;
	consteval _FmtStr(char const* fmtIn) { FmtStr_Check(fmtIn, sizeof...(A)); fmt = fmtIn; }
	operator char const*() const { return fmt; }
};
template <class... A> using FmtStr = _FmtStr<typename TypeIdentity<A>::Type...>;

//--------------------------------------------------------------------------------------------------

[[noreturn]] void Panicv(SrcLoc sl, char const* expr, char const* fmt, Arg const* args, U32 argsLen);

template <class... A> [[noreturn]] void Panicf(SrcLoc sl, FmtStr<A...> fmt, A... args) {
	Arg argsArr[sizeof...(A)] = { Arg_Make(args)... };
	Panicv(sl, 0, fmt, argsArr, sizeof...(A));
}

#define Panic(fmt, ...) Panicf(SrcLoc::Here(), fmt, ##__VA_ARGS__)

[[noreturn]] inline void AssertFail(SrcLoc sl, char const* expr) {
	Panicv(sl, expr, 0, 0, 0);
}

template <class... A> [[noreturn]] void AssertFail(SrcLoc sl, char const* expr, FmtStr<A...> fmt, A... args) {
	Arg argsArr[sizeof...(A)] = { Arg_Make(args)... };
	Panicv(sl, expr, fmt, argsArr, sizeof...(A));
}

#define Assert(expr, ...) \
	do { if (!(expr)) { \
		AssertFail(SrcLoc::Here(), #expr, ##__VA_ARGS__); \
	} } while (false)

using PanicFn = void (SrcLoc sl, char const* expr, char const* msg);
PanicFn* SetPanicFn(PanicFn* panicFn);

//--------------------------------------------------------------------------------------------------

struct Str {
	char const* data = 0;
	U32         len  = 0;

	constexpr      Str() = default;
	constexpr      Str(Str const&) = default;
	constexpr      Str(char const* s) { data = s; len = s ? ConstExprStrLen(s) : 0; }
	constexpr      Str(char const* s, U32 l) { Assert(s || l == 0); data = s; len = l; }
	constexpr Str& operator=(Str const&) = default;
	constexpr Str& operator=(char const* s) { data = s; len = s ? ConstExprStrLen(s) : 0; return *this; }
	constexpr char operator[](U32 i) const { Assert(i < len); return data[i]; }
};

inline Bool operator==(Str         s1, Str         s2) { return s1.len == s2.len && !memcmp(s1.data, s2.data, s1.len); }
inline Bool operator==(char const* s1, Str         s2) { Assert(s1); return !strcmp(s1, s2.data); }
inline Bool operator==(Str         s1, char const* s2) { Assert(s2); return !strcmp(s1.data, s2); }
inline Bool operator!=(Str         s1, Str         s2) { return s1.len != s2.len || memcmp(s1.data, s2.data, s1.len); }
inline Bool operator!=(char const* s1, Str         s2) { Assert(s1); return strcmp(s1,s2.data); }
inline Bool operator!=(Str         s1, char const* s2) { Assert(s2); return strcmp(s1.data, s2); }

constexpr Arg Arg_Make(Str val) { return Arg { .type = ArgType::Str, .s = { .s = val.data, .l = val.len } }; }

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
	constexpr       T& operator[](U64 i)       { Assert(i < len); return data[i]; }
	constexpr const T& operator[](U64 i) const { Assert(i < len); return data[i]; }
};

//--------------------------------------------------------------------------------------------------

struct [[nodiscard]] Err { U64 handle = 0; };

Err Err_Make(Err prev, SrcLoc sl, Str ns, Str code, Span<Str const> names, Span<Arg const> args);

struct ErrCode {
	Str const ns;
	Str const code;

                                                                                              Err operator()(                                                                                                                        SrcLoc sl = SrcLoc::Here()) const { return Err_Make(Err(), sl, ns, code, {},                                 {}); }
	template <class A1                                                                      > Err operator()(Str n1, A1 a1,                                                                                                          SrcLoc sl = SrcLoc::Here()) const { return Err_Make(Err(), sl, ns, code, { n1 },                             { MakeArg(a1) }); }
	template <class A1, class A2                                                            > Err operator()(Str n1, A1 a1, Str n2, A2 a2,                                                                                           SrcLoc sl = SrcLoc::Here()) const { return Err_Make(Err(), sl, ns, code, { n1, n2 },                         { MakeArg(a1), MakeArg(a2) }); }
	template <class A1, class A2, class A3                                                  > Err operator()(Str n1, A1 a1, Str n2, A2 a2, Str n3, A3 a3,                                                                            SrcLoc sl = SrcLoc::Here()) const { return Err_Make(Err(), sl, ns, code, { n1, n2, n3 },                     { MakeArg(a1), MakeArg(a2), MakeArg(a3) }); }
	template <class A1, class A2, class A3, class A4                                        > Err operator()(Str n1, A1 a1, Str n2, A2 a2, Str n3, A3 a3, Str n4, A4 a4,                                                             SrcLoc sl = SrcLoc::Here()) const { return Err_Make(Err(), sl, ns, code, { n1, n2, n3, n4 },                 { MakeArg(a1), MakeArg(a2), MakeArg(a3), MakeArg(a4) }); }
	template <class A1, class A2, class A3, class A4, class A5                              > Err operator()(Str n1, A1 a1, Str n2, A2 a2, Str n3, A3 a3, Str n4, A4 a4, Str n5, A5 a5,                                              SrcLoc sl = SrcLoc::Here()) const { return Err_Make(Err(), sl, ns, code, { n1, n2, n3, n4, n5 },             { MakeArg(a1), MakeArg(a2), MakeArg(a3), MakeArg(a4), MakeArg(a5) }); }
	template <class A1, class A2, class A3, class A4, class A5, class A6                    > Err operator()(Str n1, A1 a1, Str n2, A2 a2, Str n3, A3 a3, Str n4, A4 a4, Str n5, A5 a5, Str n6, A6 a6,                               SrcLoc sl = SrcLoc::Here()) const { return Err_Make(Err(), sl, ns, code, { n1, n2, n3, n4, n5, n6 },         { MakeArg(a1), MakeArg(a2), MakeArg(a3), MakeArg(a4), MakeArg(a5), MakeArg(a6) }); }
	template <class A1, class A2, class A3, class A4, class A5, class A6, class A7          > Err operator()(Str n1, A1 a1, Str n2, A2 a2, Str n3, A3 a3, Str n4, A4 a4, Str n5, A5 a5, Str n6, A6 a6, Str n7, A7 a7,                SrcLoc sl = SrcLoc::Here()) const { return Err_Make(Err(), sl, ns, code, { n1, n2, n3, n4, n5, n6, n7},      { MakeArg(a1), MakeArg(a2), MakeArg(a3), MakeArg(a4), MakeArg(a5), MakeArg(a6), MakeArg(a7) }); }
	template <class A1, class A2, class A3, class A4, class A5, class A6, class A7, class A8> Err operator()(Str n1, A1 a1, Str n2, A2 a2, Str n3, A3 a3, Str n4, A4 a4, Str n5, A5 a5, Str n6, A6 a6, Str n7, A7 a7, Str n8, A8 a8, SrcLoc sl = SrcLoc::Here()) const { return Err_Make(Err(), sl, ns, code, { n1, n2, n3, n4, n5, n6, n7, n8 }, { MakeArg(a1), MakeArg(a2), MakeArg(a3), MakeArg(a4), MakeArg(a5), MakeArg(a6), MakeArg(a7), MakeArg(a8) }); }
};


#define Err_Def(InNs, InCode) constexpr Err::Code EC_##InCode = { .ns = #InNs, .code = #InCode }

//--------------------------------------------------------------------------------------------------

template <class T = void> struct [[nodiscard]] Res;

template <> struct [[nodiscard]] Res<void> {
	Err err;

	constexpr Res() = default;
	constexpr Res(Err e) { err = e; }	// implicit
	constexpr Res(const Res<>&) = default;
	constexpr operator Bool() const { return !err.handle; }
};

static_assert(sizeof(Res<>) == 8);

template <class T> struct [[nodiscard]] Res {
	union {
		T   val;
		Err err;
	};
	Bool hasVal = false;

	constexpr Res() = default;
	constexpr Res(T v) { val = v; hasVal = true;  }	// implicit
	constexpr Res(Err e) { err = e; hasVal = false; }	// implicit
	constexpr Res(const Res<T>&) = default;
	constexpr operator Bool() const { return hasVal; }
	constexpr Res<> To(T& out) { if (hasVal) { out = val; return Res<>{}; } return Res<>(err); }
	constexpr T Or(T def) { return hasVal ? val : def; }
};

constexpr Res<> Ok() { return Res<>(); }

#define Try(Expr) \
	do { if (Res<> r = (Expr); !r) { \
		return r.err; \
	} } while (false)

#define TryTo(Expr, Out) \
	do { if (Res<> r = (Expr).To(Out); !r) { \
		return r.err; \
	} } while (false)