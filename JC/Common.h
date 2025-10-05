#pragma once

//--------------------------------------------------------------------------------------------------

#define BreakOnError

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

constexpr U64 KB = 1024;
constexpr U64 MB = 1024 * KB;
constexpr U64 GB = 1024 * MB;
constexpr U64 TB = 1024 * GB;

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
};

consteval SrcLoc SrcLoc_Here(char const* file = SrcLoc_File, U32 line = (U32)SrcLoc_Line) {
	return SrcLoc { .file = file, .line = line };
}
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
constexpr Arg Arg_Make(decltype(nullptr)  val) { return Arg { .type = ArgType::Ptr,  .p  = val }; }
constexpr Arg Arg_Make(Arg                val) { return val; }
										      
constexpr void Arg_Fill(Arg*) {}

template <class A1, class... A>
constexpr void Arg_Fill(Arg* out, A1 arg1, A... args) {
	*out = MakeArg(arg1);
	FillArgs(out + 1, args...);
}

//--------------------------------------------------------------------------------------------------

inline void FmtStr_TooManyArgs() {}
inline void FmtStr_NotEnoughArgs() {}
inline void FmtStr_t_Arg_NotBool() {}
inline void FmtStr_c_Arg_NotChar() {}
inline void FmtStr_s_Arg_NotStr() {}
inline void FmtStr_i_Arg_NotI64() {}
inline void FmtStr_u_Arg_NotU64() {}
inline void FmtStr_x_Arg_NotU64() {}
inline void FmtStr_X_Arg_NotU64() {}
inline void FmtStr_b_Arg_NotU64() {}
inline void FmtStr_f_Arg_NotF64() {}
inline void FmtStr_e_Arg_NotF64() {}
inline void FmtStr_E_Arg_NotF64() {}
inline void FmtStr_g_Arg_NotF64() {}
inline void FmtStr_P_Arg_NotPtr() {}
inline void FmtStr_UnknownArgType() {}

template <class... A> consteval void FmtStr_Check(char const* fmt) {
	constexpr ArgType argTypes[sizeof...(A) + 1] = { Arg_Make(A()).type... };

	U32 argIdx = 0;

	char const* f = fmt;
	for (;;) {
		while (*f != '%') {
			if (*f == 0) {
				if (argIdx < sizeof...(A)) { FmtStr_TooManyArgs(); }
				return;
			}
			f++;
		}
		f++;

		if (*f == '%') {
			f++;
			continue;
		}

		for (;;) {
			switch (*f) {
				case '-': f++; continue;
				case '+': f++; continue;
				case ' ': f++; continue;
				case '0': f++; continue;
			}
			break;
		}

		while (*f >= '0' && *f <= '9') {
			f++;
		}

		if (*f == '.') {
			f++;
			while (*f >= '0' && *f <= '9') {
				f++;
			}
		}

		if (argIdx >= sizeof...(A)) { FmtStr_NotEnoughArgs(); }
		switch (*f) {
			case 't': if (argTypes[argIdx] != ArgType::Bool) { FmtStr_t_Arg_NotBool(); } break;
			case 'c': if (argTypes[argIdx] != ArgType::Char) { FmtStr_c_Arg_NotChar(); } break;
			case 's': if (argTypes[argIdx] != ArgType::Str ) { FmtStr_s_Arg_NotStr(); } break;
			case 'i': if (argTypes[argIdx] != ArgType::I64 ) { FmtStr_i_Arg_NotI64(); } break;
			case 'u': if (argTypes[argIdx] != ArgType::U64 ) { FmtStr_u_Arg_NotU64(); } break;
			case 'x': if (argTypes[argIdx] != ArgType::U64 ) { FmtStr_x_Arg_NotU64(); } break;
			case 'X': if (argTypes[argIdx] != ArgType::U64 ) { FmtStr_X_Arg_NotU64(); } break;
			case 'b': if (argTypes[argIdx] != ArgType::U64 ) { FmtStr_b_Arg_NotU64(); } break;
			case 'f': if (argTypes[argIdx] != ArgType::F64 ) { FmtStr_f_Arg_NotF64(); } break;
			case 'e': if (argTypes[argIdx] != ArgType::F64 ) { FmtStr_e_Arg_NotF64(); } break;
			case 'E': if (argTypes[argIdx] != ArgType::F64 ) { FmtStr_E_Arg_NotF64(); } break;
			case 'g': if (argTypes[argIdx] != ArgType::F64 ) { FmtStr_g_Arg_NotF64(); } break;
			case 'p': if (argTypes[argIdx] != ArgType::Ptr ) { FmtStr_P_Arg_NotPtr(); } break;
			case 'a': break;
			default: FmtStr_UnknownArgType(); break;
		}
		argIdx++;
		f++;
	}
}

template <class T> struct TypeIdentity { using Type = T; };

template <class... A> struct _FmtStr {
	char const* fmt;

	consteval _FmtStr(char const* fmt_) {
		FmtStr_Check<A...>(fmt_);
		fmt = fmt_;
	}

	operator char const*() const { return fmt; }
};
template <class... A> using FmtStr = _FmtStr<typename TypeIdentity<A>::Type...>;

//--------------------------------------------------------------------------------------------------

[[noreturn]] void Panicv(SrcLoc sl, char const* expr, char const* fmt, Arg const* args, U32 argsLen);

template <class... A> [[noreturn]] void Panicf(SrcLoc sl, FmtStr<A...> fmt, A... args) {
	Arg argsArr[sizeof...(A)] = { Arg_Make(args)... };
	Panicv(sl, 0, fmt, argsArr, sizeof...(A));
}

#define Panic(fmt, ...) Panicf(SrcLoc_Here(), fmt, ##__VA_ARGS__)

[[noreturn]] inline void AssertFail(SrcLoc sl, char const* expr) {
	Panicv(sl, expr, 0, 0, 0);
}

template <class... A> [[noreturn]] void AssertFail(SrcLoc sl, char const* expr, FmtStr<A...> fmt, A... args) {
	Arg argsArr[sizeof...(A)] = { Arg_Make(args)... };
	Panicv(sl, expr, fmt, argsArr, sizeof...(A));
}

#define Assert(expr, ...) \
	do { if (!(expr)) { \
		AssertFail(SrcLoc_Here(), #expr, ##__VA_ARGS__); \
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

inline Bool operator==(Str s1, Str s2) { return s1.len == s2.len && !memcmp(s1.data, s2.data, s1.len); }
inline Bool operator!=(Str s1, Str s2) { return s1.len != s2.len ||  memcmp(s1.data, s2.data, s1.len); }

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

void Err_AddArg(Err err, Str name, Arg arg);

template <class A1, class... A> void Err_AddArgs(Err err, Str name1, A1 arg1, A... args) {
	Err_AddArg(err, name1, Arg_Make(arg1));
	if constexpr (sizeof...(A) > 0) {
		Err_AddArgs(err, args...);
	}
}

Err Err_Make(Err prev, SrcLoc sl, Str ns, Str code);
Err Err_Make(Err prev, SrcLoc sl, Str ns, U64 code);

template <class... A> Err Err_Make(Err prev, SrcLoc sl, Str ns, Str code, A... args) {
	Err err = Err_Make(prev, sl, ns, code);
	Err_AddArgs(err, args...);
	return err;
}

template <class... A> Err Err_Make(Err prev, SrcLoc sl, Str ns, U64 code, A... args) {
	Err err = Err_Make(prev, sl, ns, code);
	Err_AddArgs(err, args...);
	return err;
}

struct Err_Code {
	Str const ns;
	Str const code;

                                                                                              Err operator()(                                                                                                                        SrcLoc sl = SrcLoc_Here()) const { return Err_Make(Err(), sl, ns, code); }
	template <class A1                                                                      > Err operator()(Str n1, A1 a1,                                                                                                          SrcLoc sl = SrcLoc_Here()) const { return Err_Make(Err(), sl, ns, code, n1, a1); }
	template <class A1, class A2                                                            > Err operator()(Str n1, A1 a1, Str n2, A2 a2,                                                                                           SrcLoc sl = SrcLoc_Here()) const { return Err_Make(Err(), sl, ns, code, n1, a1, n2, a2); }
	template <class A1, class A2, class A3                                                  > Err operator()(Str n1, A1 a1, Str n2, A2 a2, Str n3, A3 a3,                                                                            SrcLoc sl = SrcLoc_Here()) const { return Err_Make(Err(), sl, ns, code, n1, a1, n2, a2, n3, a3); }
	template <class A1, class A2, class A3, class A4                                        > Err operator()(Str n1, A1 a1, Str n2, A2 a2, Str n3, A3 a3, Str n4, A4 a4,                                                             SrcLoc sl = SrcLoc_Here()) const { return Err_Make(Err(), sl, ns, code, n1, a1, n2, a2, n3, a3, n4); }
	template <class A1, class A2, class A3, class A4, class A5                              > Err operator()(Str n1, A1 a1, Str n2, A2 a2, Str n3, A3 a3, Str n4, A4 a4, Str n5, A5 a5,                                              SrcLoc sl = SrcLoc_Here()) const { return Err_Make(Err(), sl, ns, code, n1, a1, n2, a2, n3, a3, n4, a4, n5, a5); }
	template <class A1, class A2, class A3, class A4, class A5, class A6                    > Err operator()(Str n1, A1 a1, Str n2, A2 a2, Str n3, A3 a3, Str n4, A4 a4, Str n5, A5 a5, Str n6, A6 a6,                               SrcLoc sl = SrcLoc_Here()) const { return Err_Make(Err(), sl, ns, code, n1, a1, n2, a2, n3, a3, n4, a4, n5, a5, n6, a6); }
	template <class A1, class A2, class A3, class A4, class A5, class A6, class A7          > Err operator()(Str n1, A1 a1, Str n2, A2 a2, Str n3, A3 a3, Str n4, A4 a4, Str n5, A5 a5, Str n6, A6 a6, Str n7, A7 a7,                SrcLoc sl = SrcLoc_Here()) const { return Err_Make(Err(), sl, ns, code, n1, a1, n2, a2, n3, a3, n4, a4, n5, a5, n6, n7, a7); }
	template <class A1, class A2, class A3, class A4, class A5, class A6, class A7, class A8> Err operator()(Str n1, A1 a1, Str n2, A2 a2, Str n3, A3 a3, Str n4, A4 a4, Str n5, A5 a5, Str n6, A6 a6, Str n7, A7 a7, Str n8, A8 a8, SrcLoc sl = SrcLoc_Here()) const { return Err_Make(Err(), sl, ns, code, n1, a1, n2, a2, n3, a3, n4, a4, n5, a5, n6, n7, a7, n8, a8); }
};

#define Err_Def(Ns_, Code_) constexpr Err_Code Err_##Code_ = { .ns = #Ns_, .code = #Code_ }

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

//--------------------------------------------------------------------------------------------------

struct IRect { I32 x, y; U32 width, height; };
struct Vec2 { F32 x, y; };
struct Vec3 { F32 x, y, z; };
struct Vec4 { F32 x, y, z, w; };
struct Mat2 { F32 m[2][2]; };
struct Mat3 { F32 m[3][3]; };
struct Mat4 { F32 m[4][4]; };