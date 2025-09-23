#pragma once

//--------------------------------------------------------------------------------------------------

#if defined _MSC_VER
	#define JC_PLATFORM_WINDOWS
	#define JC_COMPILER_MSVC

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

namespace JC {

//--------------------------------------------------------------------------------------------------

#if defined JC_COMPILER_MSVC
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

	#define JC_SRC_LOC_FILE __builtin_FILE()
	#define JC_SRC_LOC_LINE __builtin_LINE()
	#define JC_DEBUGGER_BREAK __debugbreak()
	#define JC_IF_CONSTEVAL if (__builtin_is_constant_evaluated())
#endif	// JC_COMPILER_MSVC

//--------------------------------------------------------------------------------------------------

struct SrcLoc {
	const char* file;
	U32         line;

	static consteval SrcLoc Here(const char* file = JC_SRC_LOC_FILE, U32 line = (U32)JC_SRC_LOC_LINE) {
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
		struct { const char* s; U32 l; } s;
		const void* p;
	};
};

constexpr Arg MakeArg(bool               val) { return Arg { .type = ArgType::Bool, .b  = val }; }
constexpr Arg MakeArg(char               val) { return Arg { .type = ArgType::Char, .c  = val }; }
constexpr Arg MakeArg(signed char        val) { return Arg { .type = ArgType::I64,  .i  = val }; }
constexpr Arg MakeArg(signed short       val) { return Arg { .type = ArgType::I64,  .i  = val }; }
constexpr Arg MakeArg(signed int         val) { return Arg { .type = ArgType::I64,  .i  = val }; }
constexpr Arg MakeArg(signed long        val) { return Arg { .type = ArgType::I64,  .i  = val }; }
constexpr Arg MakeArg(signed long long   val) { return Arg { .type = ArgType::I64,  .i  = val }; }
constexpr Arg MakeArg(unsigned char      val) { return Arg { .type = ArgType::U64,  .u  = val }; }
constexpr Arg MakeArg(unsigned short     val) { return Arg { .type = ArgType::U64,  .u  = val }; }
constexpr Arg MakeArg(unsigned int       val) { return Arg { .type = ArgType::U64,  .u  = val }; }
constexpr Arg MakeArg(unsigned long      val) { return Arg { .type = ArgType::U64,  .u  = val }; }
constexpr Arg MakeArg(unsigned long long val) { return Arg { .type = ArgType::U64,  .u  = val }; }
constexpr Arg MakeArg(float              val) { return Arg { .type = ArgType::F64,  .f  = val }; }
constexpr Arg MakeArg(double             val) { return Arg { .type = ArgType::F64,  .f  = val }; }
constexpr Arg MakeArg(char*              val) { return Arg { .type = ArgType::Str,  .s  = { .s = val, .l = val ? ConstExprStrLen(val) : 0 } }; }
constexpr Arg MakeArg(const char*        val) { return Arg { .type = ArgType::Str,  .s  = { .s = val, .l = val ? ConstExprStrLen(val) : 0 } }; }
constexpr Arg MakeArg(const void*        val) { return Arg { .type = ArgType::Ptr,  .p  = val }; }
constexpr Arg MakeArg(Arg                val) { return val; }
										      
constexpr void FillArgs(Arg*) {}

template <class A1, class... A>
constexpr void FillArgs(Arg* out, A1 arg1, A... args) {
	*out = MakeArg(arg1);
	FillArgs(out + 1, args...);
}

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

struct Allocator {
	virtual void* Alloc(void* ptr, U64 ptrSize, U64 newSize, SrcLoc sl = SrcLoc::Here()) = 0;

	void* Alloc  (                        U64 newSize, SrcLoc sl = SrcLoc::Here()) { return Alloc(0,   0,       newSize, sl); }
	void* Realloc(void* ptr, U64 ptrSize, U64 newSize, SrcLoc sl = SrcLoc::Here()) { return Alloc(ptr, ptrSize, newSize, sl); }
	void  Free   (void* ptr, U64 ptrSize,              SrcLoc sl = SrcLoc::Here()) {        Alloc(ptr, ptrSize, 0,       sl); }

	template <class T> T* AllocT  (                  U64 n = 1, SrcLoc sl = SrcLoc::Here()) { return (T*)Alloc(0,   0,                n * sizeof(T), sl); }
	template <class T> T* ReallocT(T* ptr, U64 oldN, U64 n,     SrcLoc sl = SrcLoc::Here()) { return (T*)Alloc(ptr, oldN * sizeof(T), n * sizeof(T), sl); }
};

struct TempAllocator : Allocator {
	virtual void Reset() = 0;
};

//--------------------------------------------------------------------------------------------------

[[noreturn]] void VPanic(SrcLoc sl, const char* expr, const char* fmt, Span<const Arg> args);

template <class... A> [[noreturn]] void APanic(SrcLoc sl, FmtStr<A...> fmt, A... args) {
	VPanic(sl, 0, fmt, { MakeArg(args)... });
}

#define Panic(fmt, ...) JC::APanic(SrcLoc::Here(), fmt, ##__VA_ARGS__)

[[noreturn]] inline void AssertFail(SrcLoc sl, const char* expr) {
	VPanic(sl, expr, 0, {});
}

template <class... A> [[noreturn]] void AssertFail(SrcLoc sl, const char* expr, FmtStr<A...> fmt, A... args) {
	VPanic(sl, expr, fmt, { MakeArg<A>(args)... });
}

#define Assert(expr, ...) \
	do { if (!(expr)) { \
		JC::AssertFail(SrcLoc::Here(), #expr, ##__VA_ARGS__); \
	} } while (false)

using PanicFn = void (SrcLoc sl, const char* expr, const char* msg);
PanicFn* SetPanicFn(PanicFn* panicFn);

//--------------------------------------------------------------------------------------------------

constexpr U32 ConstExprStrLen(const char* s) {
	JC_IF_CONSTEVAL {
		const char* i = s; 
		for (; *i; i++) {}
		return (U32)(i - s);
	} else {
		return (U32)strlen(s);
	}
}

struct Str {
	const char* data = 0;
	U32         len  = 0;

	constexpr      Str() = default;
	constexpr      Str(const Str&) = default;
	constexpr      Str(const char* s) { data = s; len = s ? ConstExprStrLen(s) : 0; }
	constexpr      Str(const char* s, U32 l) { JC_ASSERT(s || l == 0); data = s; len = l; }
	constexpr Str& operator=(const Str&) = default;
	constexpr Str& operator=(const char* s) { data = s; len = s ? ConstExprStrLen(s) : 0; return *this; }
	constexpr char operator[](U32 i) const { JC_ASSERT(i < len); return data[i]; }
};

inline Bool operator==(Str         s1, Str         s2) { return s1.len == s2.len && !memcmp(s1.data, s2.data, s1.len); }
inline Bool operator==(const char* s1, Str         s2) { JC_ASSERT(s1); return !strcmp(s1, s2.data); }
inline Bool operator==(Str         s1, const char* s2) { JC_ASSERT(s2); return !strcmp(s1.data, s2); }
inline Bool operator!=(Str         s1, Str         s2) { return s1.len != s2.len || memcmp(s1.data, s2.data, s1.len); }
inline Bool operator!=(const char* s1, Str         s2) { JC_ASSERT(s1); return strcmp(s1,s2.data); }
inline Bool operator!=(Str         s1, const char* s2) { JC_ASSERT(s2); return strcmp(s1.data, s2); }

constexpr Arg MakeArg(Str val) { return Arg { .type = ArgType::Str, .s = { .s = val.data, .l = val.len } }; }

//--------------------------------------------------------------------------------------------------

template <class T> struct Span {
	T*  data = 0;
	U64 len  = 0;

	constexpr Span() = default;
	constexpr Span(T* d, U64 l) { data = d; len = l; }
	constexpr Span(std::initializer_list<T> il) { data = const_cast<T*>(il.begin()); len = il.size(); }
	template <U64 N> Span(T (&arr)[N]) { data = arr; len = N; }
	constexpr Span(const Span&) = default;
	constexpr Span& operator=(const Span&) = default;
	constexpr       T& operator[](U64 i)       { JC_ASSERT(i < len); return data[i]; }
	constexpr const T& operator[](U64 i) const { JC_ASSERT(i < len); return data[i]; }
};

//--------------------------------------------------------------------------------------------------

namespace Err {
	struct [[nodiscard]] Err { U64 handle = 0; };

	Err Make(Err prev, SrcLoc sl, Str ns, Str code, Span<const Str> names, Span<const Arg> args);

	struct Code {
		const Str ns;
		const Str code;

																								  Err operator()(                                                                                                                        SrcLoc sl = SrcLoc::Here()) const { return Make(Err(), sl, ns, code, {},                                 {}); }
		template <class A1                                                                      > Err operator()(Str n1, A1 a1,                                                                                                          SrcLoc sl = SrcLoc::Here()) const { return Make(Err(), sl, ns, code, { n1 },                             { MakeArg(a1) }); }
		template <class A1, class A2                                                            > Err operator()(Str n1, A1 a1, Str n2, A2 a2,                                                                                           SrcLoc sl = SrcLoc::Here()) const { return Make(Err(), sl, ns, code, { n1, n2 },                         { MakeArg(a1), MakeArg(a2) }); }
		template <class A1, class A2, class A3                                                  > Err operator()(Str n1, A1 a1, Str n2, A2 a2, Str n3, A3 a3,                                                                            SrcLoc sl = SrcLoc::Here()) const { return Make(Err(), sl, ns, code, { n1, n2, n3 },                     { MakeArg(a1), MakeArg(a2), MakeArg(a3) }); }
		template <class A1, class A2, class A3, class A4                                        > Err operator()(Str n1, A1 a1, Str n2, A2 a2, Str n3, A3 a3, Str n4, A4 a4,                                                             SrcLoc sl = SrcLoc::Here()) const { return Make(Err(), sl, ns, code, { n1, n2, n3, n4 },                 { MakeArg(a1), MakeArg(a2), MakeArg(a3), MakeArg(a4) }); }
		template <class A1, class A2, class A3, class A4, class A5                              > Err operator()(Str n1, A1 a1, Str n2, A2 a2, Str n3, A3 a3, Str n4, A4 a4, Str n5, A5 a5,                                              SrcLoc sl = SrcLoc::Here()) const { return Make(Err(), sl, ns, code, { n1, n2, n3, n4, n5 },             { MakeArg(a1), MakeArg(a2), MakeArg(a3), MakeArg(a4), MakeArg(a5) }); }
		template <class A1, class A2, class A3, class A4, class A5, class A6                    > Err operator()(Str n1, A1 a1, Str n2, A2 a2, Str n3, A3 a3, Str n4, A4 a4, Str n5, A5 a5, Str n6, A6 a6,                               SrcLoc sl = SrcLoc::Here()) const { return Make(Err(), sl, ns, code, { n1, n2, n3, n4, n5, n6 },         { MakeArg(a1), MakeArg(a2), MakeArg(a3), MakeArg(a4), MakeArg(a5), MakeArg(a6) }); }
		template <class A1, class A2, class A3, class A4, class A5, class A6, class A7          > Err operator()(Str n1, A1 a1, Str n2, A2 a2, Str n3, A3 a3, Str n4, A4 a4, Str n5, A5 a5, Str n6, A6 a6, Str n7, A7 a7,                SrcLoc sl = SrcLoc::Here()) const { return Make(Err(), sl, ns, code, { n1, n2, n3, n4, n5, n6, n7},      { MakeArg(a1), MakeArg(a2), MakeArg(a3), MakeArg(a4), MakeArg(a5), MakeArg(a6), MakeArg(a7) }); }
		template <class A1, class A2, class A3, class A4, class A5, class A6, class A7, class A8> Err operator()(Str n1, A1 a1, Str n2, A2 a2, Str n3, A3 a3, Str n4, A4 a4, Str n5, A5 a5, Str n6, A6 a6, Str n7, A7 a7, Str n8, A8 a8, SrcLoc sl = SrcLoc::Here()) const { return Make(Err(), sl, ns, code, { n1, n2, n3, n4, n5, n6, n7, n8 }, { MakeArg(a1), MakeArg(a2), MakeArg(a3), MakeArg(a4), MakeArg(a5), MakeArg(a6), MakeArg(a7), MakeArg(a8) }); }
	};
}


#define DefErrCode(InNs, InCode) constexpr Err::Code EC_##InCode = { .ns = #InNs, .code = #InCode }

//--------------------------------------------------------------------------------------------------

template <class T = void> struct [[nodiscard]] Res;

template <> struct [[nodiscard]] Res<void> {
	Err::Err err;

	constexpr Res() = default;
	constexpr Res(Err::Err e) { err = e; }	// implicit
	constexpr Res(const Res<>&) = default;
	constexpr operator Bool() const { return !err.handle; }
};

static_assert(sizeof(Res<>) == 8);

template <class T> struct [[nodiscard]] Res {
	union {
		T        val;
		Err::Err err;
	};
	Bool hasVal = false;

	constexpr Res() = default;
	constexpr Res(T v) { val = v; hasVal = true;  }	// implicit
	constexpr Res(Err::Err e) { err = e; hasVal = false; }	// implicit
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

//--------------------------------------------------------------------------------------------------

}	// namespace JC