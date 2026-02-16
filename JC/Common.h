#pragma once

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

#if defined _MSC_VER
	#define Platform_Windows
	#define Compiler_Msvc
#endif	// _MSC_VER

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

	extern "C" {
		using size_t = decltype(sizeof(0));
		void*  __cdecl memcpy(void* dst, const void* src, size_t size);
		void*  __cdecl memmove(void* dst, const void* src, size_t size);
		int    __cdecl memcmp(const void* p1, const void* p2, size_t size);
		void*  __cdecl memset(void* p, int val, size_t n);
		size_t __cdecl strlen(char const* s);
		int    __cdecl strcmp(char const* s1, char const* s2);

	}
	#define IfConstEval if (__builtin_is_constant_evaluated())
	#define DbgBreak __debugbreak()
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

template <class F> struct DeferInvoker {
	F fn;
	DeferInvoker(F&& fn_) : fn(fn_) {}
	~DeferInvoker() { fn(); }
};

enum struct DeferHelper {};
template <class F> DeferInvoker<F> operator+(DeferHelper, F&& fn) { return DeferInvoker<F>((F&&)fn); }

#define Defer \
	auto MacroUniqueName(Defer_) = DeferHelper() + [&]()

//--------------------------------------------------------------------------------------------------

constexpr U32 StrLen(char const* s) {
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
	constexpr Str(char const* s) { data = s; len = s ? StrLen(s) : 0; }
	Str(char const* s, U64 l);
	constexpr Str& operator=(Str const&) = default;
	constexpr Str& operator=(char const* s) { data = s; len = s ? StrLen(s) : 0; return *this; }
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

#define DefHandle(Type) \
	struct Type { \
		U64 handle = 0; \
		operator bool() const { return handle != 0; } \
	}

//--------------------------------------------------------------------------------------------------

struct Arg {
	enum struct Type { Bool, Char, I64, U64, F64, Str, Ptr };

	Type type;
	union {
		bool b;
		char c;
		I64 i;
		U64 u;
		F64 f;
		struct { char const* data; U64 len; } s;
		const void* p;
	};

	static constexpr Arg Make(bool               val) { return Arg { .type = Type::Bool, .b  = val }; }
	static constexpr Arg Make(char               val) { return Arg { .type = Type::Char, .c  = val }; }
	static constexpr Arg Make(signed char        val) { return Arg { .type = Type::I64,  .i  = val }; }
	static constexpr Arg Make(signed short       val) { return Arg { .type = Type::I64,  .i  = val }; }
	static constexpr Arg Make(signed int         val) { return Arg { .type = Type::I64,  .i  = val }; }
	static constexpr Arg Make(signed long        val) { return Arg { .type = Type::I64,  .i  = val }; }
	static constexpr Arg Make(signed long long   val) { return Arg { .type = Type::I64,  .i  = val }; }
	static constexpr Arg Make(unsigned char      val) { return Arg { .type = Type::U64,  .u  = val }; }
	static constexpr Arg Make(unsigned short     val) { return Arg { .type = Type::U64,  .u  = val }; }
	static constexpr Arg Make(unsigned int       val) { return Arg { .type = Type::U64,  .u  = val }; }
	static constexpr Arg Make(unsigned long      val) { return Arg { .type = Type::U64,  .u  = val }; }
	static constexpr Arg Make(unsigned long long val) { return Arg { .type = Type::U64,  .u  = val }; }
	static constexpr Arg Make(float              val) { return Arg { .type = Type::F64,  .f  = val }; }
	static constexpr Arg Make(double             val) { return Arg { .type = Type::F64,  .f  = val }; }
	static constexpr Arg Make(char*              val) { return Arg { .type = Type::Str,  .s  = { .data = val,      .len = val ? StrLen(val) : 0 } }; }
	static constexpr Arg Make(char const*        val) { return Arg { .type = Type::Str,  .s  = { .data = val,      .len = val ? StrLen(val) : 0 } }; }
	static constexpr Arg Make(Str                val) { return Arg { .type = Type::Str,  .s  = { .data = val.data, .len = val.len } }; }
	static constexpr Arg Make(const void*        val) { return Arg { .type = Type::Ptr,  .p  = val }; }
	static constexpr Arg Make(decltype(nullptr)  val) { return Arg { .type = Type::Ptr,  .p  = val }; }
	static constexpr Arg Make(Arg                val) { return val; }
							      
	static constexpr void Fill(Arg*) {}

	template <class A1, class... A>
	static constexpr void Fill(Arg* out, A1 arg1, A... args) {
		*out = Make(arg1);
		Fill(out + 1, args...);
	}
};

//--------------------------------------------------------------------------------------------------

#if defined Compiler_Msvc
	#define SrcLoc_File __builtin_FILE()
	#define SrcLoc_Line ((U32)__builtin_LINE())
#endif	// Compiler

struct SrcLoc {
	char const* file;
	U32         line;

	static consteval SrcLoc Here(char const* file = SrcLoc_File, U32 line = SrcLoc_Line) {
		return SrcLoc { .file = file, .line = line };
	}
};

//--------------------------------------------------------------------------------------------------

struct Mem {
	U64 handle = 0;
	operator bool() const { return handle != 0; }

	static Mem                        Create(U64 reserveSize);
	static void                       Destroy(Mem mem);
	static void*                      Alloc(Mem mem, U64 size, SrcLoc sl = SrcLoc::Here());
	static void*                      Extend(Mem mem, void* ptr, U64 newSize, SrcLoc sl = SrcLoc::Here());
	static U64                        Mark(Mem mem);
	static void                       Reset(Mem mem, U64 mark);
	template <class T> static T*      AllocT(Mem mem, U64 n, SrcLoc sl = SrcLoc::Here()) { return (T*)Mem::Alloc(mem, n * sizeof(T), sl); }
	template <class T> static Span<T> AllocSpan(Mem mem, U64 n, SrcLoc sl = SrcLoc::Here()) { return Span<T>((T*)Mem::Alloc(mem, n * sizeof(T), sl), n); }
	template <class T> static T*      ExtendT(Mem mem, T* ptr, U64 newN, SrcLoc sl = SrcLoc::Here()) { return (T*)Mem::Extend(mem, ptr , newN * sizeof(T), sl); }
};

struct MemScope {
	Mem mem;
	U64 mark;
	MemScope(Mem mem_) { mem = mem_; mark = Mem::Mark(mem); }
	~MemScope() { Mem::Reset(mem, mark); }
};

//--------------------------------------------------------------------------------------------------

struct NamedArg {
	Str name;
	Arg arg;
};

struct [[nodiscard]] Err {
	static constexpr U32 MaxNamedArgs = 32;

	U64        frame;
	Err const* prev;
	SrcLoc     sl;
	Str        ns;
	Str        sCode;
	U64        uCode;
	NamedArg   namedArgs[MaxNamedArgs];
	U32        namedArgsLen;

	static void FillNamedArgs(NamedArg*) {}

	template <class A1, class... A> static void FillNamedArgs(NamedArg* namedArgs, Str name1, A1 arg1, A... args) {
		*namedArgs = { .name = name1, .arg = Arg::Make(arg1) };
		FillNamedArgs(namedArgs + 1, args...);
	}

	static Err const* Makev(Err const* prev, SrcLoc sl, Str ns, Str sCode, U64 uCode, Span<NamedArg const> namedArgs);

	template <class... A> static Err const* Make(Err const* prev, SrcLoc sl, Str ns, Str sCode, U64 uCode, A... args) {
		static_assert(sizeof...(A) / 2 < MaxNamedArgs);
		static_assert((sizeof...(A) & 1) == 0);

		#if defined Cfg_BreakOnErr
			DbgBreak;
		#endif	// Cfg_BreakOnErr

		NamedArg namedArgs[sizeof...(A) + 1];
		FillNamedArgs(namedArgs, args...);
		return Makev(prev, sl, ns, sCode, uCode, namedArgs);
	}

	static void Frame(U64 frame);
};


struct ErrCode {
	Str const ns;
	Str const code;
                                                                                              Err const * operator()(                                                                                                                        SrcLoc sl = SrcLoc::Here()) const { return Err::Make(nullptr, sl, ns, code, 0); }
	template <class A1                                                                      > Err const * operator()(Str n1, A1 a1,                                                                                                          SrcLoc sl = SrcLoc::Here()) const { return Err::Make(nullptr, sl, ns, code, 0, n1, a1); }
	template <class A1, class A2                                                            > Err const * operator()(Str n1, A1 a1, Str n2, A2 a2,                                                                                           SrcLoc sl = SrcLoc::Here()) const { return Err::Make(nullptr, sl, ns, code, 0, n1, a1, n2, a2); }
	template <class A1, class A2, class A3                                                  > Err const * operator()(Str n1, A1 a1, Str n2, A2 a2, Str n3, A3 a3,                                                                            SrcLoc sl = SrcLoc::Here()) const { return Err::Make(nullptr, sl, ns, code, 0, n1, a1, n2, a2, n3, a3); }
	template <class A1, class A2, class A3, class A4                                        > Err const * operator()(Str n1, A1 a1, Str n2, A2 a2, Str n3, A3 a3, Str n4, A4 a4,                                                             SrcLoc sl = SrcLoc::Here()) const { return Err::Make(nullptr, sl, ns, code, 0, n1, a1, n2, a2, n3, a3, n4, a4); }
	template <class A1, class A2, class A3, class A4, class A5                              > Err const * operator()(Str n1, A1 a1, Str n2, A2 a2, Str n3, A3 a3, Str n4, A4 a4, Str n5, A5 a5,                                              SrcLoc sl = SrcLoc::Here()) const { return Err::Make(nullptr, sl, ns, code, 0, n1, a1, n2, a2, n3, a3, n4, a4, n5, a5); }
	template <class A1, class A2, class A3, class A4, class A5, class A6                    > Err const * operator()(Str n1, A1 a1, Str n2, A2 a2, Str n3, A3 a3, Str n4, A4 a4, Str n5, A5 a5, Str n6, A6 a6,                               SrcLoc sl = SrcLoc::Here()) const { return Err::Make(nullptr, sl, ns, code, 0, n1, a1, n2, a2, n3, a3, n4, a4, n5, a5, n6, a6); }
	template <class A1, class A2, class A3, class A4, class A5, class A6, class A7          > Err const * operator()(Str n1, A1 a1, Str n2, A2 a2, Str n3, A3 a3, Str n4, A4 a4, Str n5, A5 a5, Str n6, A6 a6, Str n7, A7 a7,                SrcLoc sl = SrcLoc::Here()) const { return Err::Make(nullptr, sl, ns, code, 0, n1, a1, n2, a2, n3, a3, n4, a4, n5, a5, n6, a6, n7, a7); }
	template <class A1, class A2, class A3, class A4, class A5, class A6, class A7, class A8> Err const * operator()(Str n1, A1 a1, Str n2, A2 a2, Str n3, A3 a3, Str n4, A4 a4, Str n5, A5 a5, Str n6, A6 a6, Str n7, A7 a7, Str n8, A8 a8, SrcLoc sl = SrcLoc::Here()) const { return Err::Make(nullptr, sl, ns, code, 0, n1, a1, n2, a2, n3, a3, n4, a4, n5, a5, n6, a6, n7, a7, n8, a8); }
};

bool operator==(ErrCode ec, Err const* err);
bool operator==(Err const* err, ErrCode ec);

#define DefErr(NsIn, CodeIn) constexpr ErrCode Err_##CodeIn = { .ns = #NsIn, .code = #CodeIn }

//--------------------------------------------------------------------------------------------------

template <class T = void> struct [[nodiscard]] Res;

template <> struct [[nodiscard]] Res<void> {
	Err const* err;

	constexpr Res() = default;
	constexpr Res(Err const* e) { err = e; }	// implicit
	constexpr Res(const Res<>&) = default;
	constexpr operator bool() const { return !err; }
};

static_assert(sizeof(Res<>) == 8);

template <class T> struct [[nodiscard]] Res {
	union {
		T          val;
		Err const* err;
	};
	bool hasVal = false;

	constexpr Res() = default;
	constexpr Res(T v) { val = v; hasVal = true;  }	// implicit
	constexpr Res(Err const* e) { err = e; hasVal = false; }	// implicit
	constexpr Res(const Res<T>&) = default;
	constexpr operator bool() const { return hasVal; }
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

inline void CheckFmtStr_TooManyArgs() {}
inline void CheckFmtStr_NotEnoughArgs() {}
inline void CheckFmtStr_t_Arg_NotBool() {}
inline void CheckFmtStr_c_Arg_NotChar() {}
inline void CheckFmtStr_s_Arg_NotStr() {}
inline void CheckFmtStr_i_Arg_NotI64() {}
inline void CheckFmtStr_u_Arg_NotU64() {}
inline void CheckFmtStr_x_Arg_NotU64() {}
inline void CheckFmtStr_X_Arg_NotU64() {}
inline void CheckFmtStr_b_Arg_NotU64() {}
inline void CheckFmtStr_f_Arg_NotF64() {}
inline void CheckFmtStr_e_Arg_NotF64() {}
inline void CheckFmtStr_E_Arg_NotF64() {}
inline void CheckFmtStr_g_Arg_NotF64() {}
inline void CheckFmtStr_p_Arg_NotPtr() {}
inline void CheckFmtStr_UnknownArgType() {}

template <class T> struct TypeIdentity { using Type = T; };

template <class... A> struct _CheckFmtStr {
	char const* fmt;

	consteval _CheckFmtStr(char const* fmt_) {
		fmt = fmt_;
		Check<A...>();
	}

	operator char const*() const { return fmt; }

	template <class... A> consteval void Check() {
		constexpr Arg::Type argTypes[sizeof...(A) + 1] = { Arg::Make(A()).type... };

		U32 argIdx = 0;

		char const* f = fmt;
		for (;;) {
			while (*f != '%') {
				if (*f == 0) {
					if (argIdx < sizeof...(A)) { CheckFmtStr_TooManyArgs(); }
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

			if (argIdx >= sizeof...(A)) { CheckFmtStr_NotEnoughArgs(); }
			switch (*f) {
				case 't': if (argTypes[argIdx] != Arg::Type::Bool) { CheckFmtStr_t_Arg_NotBool(); } break;
				case 'c': if (argTypes[argIdx] != Arg::Type::Char) { CheckFmtStr_c_Arg_NotChar(); } break;
				case 's': if (argTypes[argIdx] != Arg::Type::Str ) { CheckFmtStr_s_Arg_NotStr(); } break;
				case 'i': if (argTypes[argIdx] != Arg::Type::I64 ) { CheckFmtStr_i_Arg_NotI64(); } break;
				case 'u': if (argTypes[argIdx] != Arg::Type::U64 ) { CheckFmtStr_u_Arg_NotU64(); } break;
				case 'x': if (argTypes[argIdx] != Arg::Type::U64 ) { CheckFmtStr_x_Arg_NotU64(); } break;
				case 'X': if (argTypes[argIdx] != Arg::Type::U64 ) { CheckFmtStr_X_Arg_NotU64(); } break;
				case 'b': if (argTypes[argIdx] != Arg::Type::U64 ) { CheckFmtStr_b_Arg_NotU64(); } break;
				case 'f': if (argTypes[argIdx] != Arg::Type::F64 ) { CheckFmtStr_f_Arg_NotF64(); } break;
				case 'e': if (argTypes[argIdx] != Arg::Type::F64 ) { CheckFmtStr_e_Arg_NotF64(); } break;
				case 'E': if (argTypes[argIdx] != Arg::Type::F64 ) { CheckFmtStr_E_Arg_NotF64(); } break;
				case 'g': if (argTypes[argIdx] != Arg::Type::F64 ) { CheckFmtStr_g_Arg_NotF64(); } break;
				case 'p': if (argTypes[argIdx] != Arg::Type::Ptr ) { CheckFmtStr_p_Arg_NotPtr(); } break;
				case 'a': break;
				default: CheckFmtStr_UnknownArgType(); break;
			}
			argIdx++;
			f++;
		}
	}
};
template <class... A> using CheckFmtStr = _CheckFmtStr<typename TypeIdentity<A>::Type...>;

//--------------------------------------------------------------------------------------------------

Str SPrintv(Mem mem, char const* fmt, Span<Arg const> args);
char* SPrintv(char* outBegin,  char* outEnd, char const* fmt, Span<Arg const> args);

template <class... A> Str SPrintf(Mem mem, CheckFmtStr<A...> fmt, A... args) {
	return SPrintv(mem, fmt.fmt, { Arg::Make(args)... });
}

template <class... A> char* SPrintf(char* outBegin, char* outEnd, CheckFmtStr<A...> fmt, A... args) {
	return SPrintv(outBegin, outEnd, fmt.fmt, { Arg::Make(args)... });
}

//--------------------------------------------------------------------------------------------------

struct PrintBuf {
	Mem   mem;
	char* data;
	U64   len;
	U64   cap;

	PrintBuf(Mem mem);

	void Add(char c, SrcLoc sl = SrcLoc::Here());
	void Add(char c, U64 n, SrcLoc sl = SrcLoc::Here());
	void Add(char const* str, U64 strLen, SrcLoc sl = SrcLoc::Here());
	void Add(Str s, SrcLoc sl = SrcLoc::Here());
	void Remove();
	void Remove(U64 n);

	void Printv(char const* fmt, Span<Arg const> args);
	template <class... A> void Printf(CheckFmtStr<A...> fmt, A... args) { Printv(fmt, { Arg::Make(args)... }); }

	inline Str ToStr() const { return Str(data, len); }
};

//--------------------------------------------------------------------------------------------------

[[noreturn]] void Panicv(SrcLoc sl, char const* expr, char const* fmt, Span<Arg const> args);

[[noreturn]] inline void Panicf(SrcLoc sl, char const* expr) {
	Panicv(sl, expr, nullptr, {});
}

template <class... A> [[noreturn]] void Panicf(SrcLoc sl, char const* expr, CheckFmtStr<A...> fmt, A... args) {
	Panicv(sl, expr, fmt, { Arg::Make(args)... });
}

using PanicFn = void (SrcLoc sl, char const* expr, char const* msg);
PanicFn* SetPanicFn(PanicFn* panicFn);

//--------------------------------------------------------------------------------------------------

#define Panic(fmt, ...) Panicf(SrcLoc::Here(), nullptr, fmt, ##__VA_ARGS__)

#define Assert(expr, ...) \
	do { if (!(expr)) { \
		Panicf(SrcLoc::Here(), #expr, ##__VA_ARGS__); \
	} } while (false)
	
//--------------------------------------------------------------------------------------------------

}	// namespace JC