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

	#define StrLen      __builtin_strlen
	#define StrCmp      strcmp
	#define MemCmp      __builtin_memcmp
	#define MemSet      memset
	#define MemCpy      memcpy
	#define MemMove     memmove
	#define BuiltinFile __builtin_FILE()
	#define BuiltinLine __builtin_LINE()
	#define IfConsteval if (__builtin_is_constant_evaluated())
	#define BuiltinIsEnum(T)   __is_enum(T)
#endif

constexpr u32 U32Max = 0xffffffff;
constexpr u64 U64Max = (u64)0xffffffffffffffff;

#define MacroConcat2(x, y) x##y
#define MacroConcat(x, y)  MacroConcat2(x, y)
#define MacroName(x) MacroConcat(x, __LINE__)
#define LenOf(a) (u64)(sizeof(a) / sizeof(a[0]))

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

constexpr u64 CxStrLen8(const char* s) {
	if (s == nullptr) {
		return 0;
	}
	IfConsteval {
		const char* i = s;
		while (*i) {
			i++;
		}
		return (u64)(i - s);
	} else {
		return StrLen(s);
	}
}

//--------------------------------------------------------------------------------------------------

constexpr u64 CxMemCmp(const void* p1, const void* p2, u64 len) {
	IfConsteval {
		const u8* i1 = (const u8*)p1;
		const u8* i2 = (const u8*)p2;
		while (len > 0) {
			if (*i1 != *i2) {
				return i1 - i2;
			}
			len--;
		}
		return 0;
	} else {
		return MemCmp(p1, p2, len);
	}
}

//--------------------------------------------------------------------------------------------------

struct s8 {
	const char* data = "";
	u64         len  = 0;

	constexpr      s8() = default;
	constexpr      s8(const s8& s) = default;
	constexpr      s8(const char* s);
	constexpr      s8(const char* d, u64 l);
	constexpr      s8(const char* b, const char* e);

	constexpr s8&  operator=(const s8& s) = default;
	constexpr s8&  operator=(const char* s);

	constexpr char operator[](u64 i) const;
};

//--------------------------------------------------------------------------------------------------

struct SrcLoc {
	s8  file = {};
	i32 line = 0;

	static consteval SrcLoc Here(s8 file = BuiltinFile, i32 line = BuiltinLine) {
		return SrcLoc { .file = file, .line = line };
	}
};

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

enum struct VArgType {
	Bool,
	Char,
	I64,
	U64,
	F64,
	Ptr,
	S8,
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
	else if constexpr (IsSameType<Underlying, s8>)                 { return { .type = VArgType::S8,   .s = { .data = val.data, .len = val.len } }; }
	else if constexpr (IsSameType<Underlying, char*>)              { return { .type = VArgType::S8,   .s = { .data = val,      .len = CxStrLen8(val) } }; }
	else if constexpr (IsSameType<Underlying, const char*>)        { return { .type = VArgType::S8,   .s = { .data = val,      .len = CxStrLen8(val) } }; }
	else if constexpr (IsPointer<Underlying>)                      { return { .type = VArgType::Ptr,  .p = val }; }
	else if constexpr (IsSameType<Underlying, decltype(nullptr)>)  { return { .type = VArgType::Ptr,  .p = nullptr }; }
	else if constexpr (IsEnum<Underlying>)                         { return { .type = VArgType::U64,  .u = (u64)val }; }
	else if constexpr (IsSameType<Underlying, VArg>)               { return val; }
	else if constexpr (IsSameType<Underlying, VArgs>)              { static_assert(AlwaysFalse<T>, "You passed Args as a placeholder variable: you probably meant to call VFmt() instead of Fmt()"); }
	else                                                           { static_assert(AlwaysFalse<T>, "Unsupported arg type"); }
}
template <u64 N> static constexpr VArg MakeVArg(char (&val)[N])       { return { .type = VArgType::S8, .s = { .data = val, .len = CxStrLen8(val) } }; }
template <u64 N> static constexpr VArg MakeVArg(const char (&val)[N]) { return { .type = VArgType::S8, .s = { .data = val, .len = CxStrLen8(val) } }; }

template <u32 N> struct VArgStore {
	VArg args[N > 0 ? N : 1] = {};
};

struct VArgs {
	const VArg* args = 0;
	u32        len  = 0;

	template <u32 N> constexpr VArgs(VArgStore<N> argStore) {
		args = argStore.args;
		len  = N;
	}
};

template <class... A> constexpr VArgStore<sizeof...(A)> MakeVArgs(A... args) {
	return VArgStore<sizeof...(A)> { MakeVArg(args)... };
}

//--------------------------------------------------------------------------------------------------

inline void BadFmtStr_UnmatchedOpenBrace() {}
inline void BadFmtStr_NotEnoughArgs() {}
inline void BadFmtStr_CloseBraceNotEscaped() {}
inline void BadFmtStr_TooManyArgs() {}
inline void BadFmtStr_BadPlaceholderSpec() {}

consteval const char* CheckFmtSpec(const char* i, const char* end) {
	bool flagsDone = false;
	while (!flagsDone && i < end) {
		switch (*i) {
			case '}': i++; return i;
			case '<': i++; break;
			case '+': i++; break;
			case ' ': i++; break;
			case '0': i++; flagsDone = true; break;
			default:       flagsDone = true; break;
		}
	}
	while (i < end && *i >= '0' && *i <= '9') {
		i++;
	}
	if (i < end && *i == '.') {
		i++;
		while (i < end && *i >= '0' && *i <= '9') {
			i++;
		}
	}
	if (i >= end) { BadFmtStr_BadPlaceholderSpec(); }

	switch (*i) {
		case 'x': i++; break;
		case 'X': i++; break;
		case 'b': i++; break;
		case 'f': i++; break;
		case 'e': i++; break;
		default:       break;
	}
	if (i >= end || *i != '}') { BadFmtStr_BadPlaceholderSpec(); }
	i++;
	return i;
}

consteval void CheckFmtStr(s8 fmt, size_t argsLen) {
	const char* i = fmt.data;
	const char* const end = i + fmt.len;
	u32 nextArg = 0;

	for (;;) {
		if (i >= end) {
			if (nextArg != argsLen) { BadFmtStr_TooManyArgs(); }
			return;
		} else if (*i == '{') {
			i++;
			if (i >= end) { BadFmtStr_UnmatchedOpenBrace(); }
			if (*i == '{') {
				i++;
			} else {
				i = CheckFmtSpec(i, end);
				if (nextArg >= argsLen) { BadFmtStr_NotEnoughArgs(); }
				nextArg++;
			}
		} else if (*i != '}') {
			i++;
		} else {
			i++;
			if (i >= end || *i != '}') { BadFmtStr_CloseBraceNotEscaped(); }
			i++;
		}
	}
}

//--------------------------------------------------------------------------------------------------

template <class... A> struct _FmtStr {
	s8 fmt;

	consteval _FmtStr(s8          fmt_) { CheckFmtStr(fmt_, sizeof...(A)); fmt = fmt_; }
	consteval _FmtStr(const char* fmt_) { CheckFmtStr(fmt_, sizeof...(A)); fmt = fmt_; }

	operator s8() const { return fmt; }
};

template <class... A> using FmtStr = _FmtStr<typename TypeIdentity<A>::Type...>;

template <class... A> struct _FmtStrSrcLoc {
	s8     fmt;
	SrcLoc sl;

	consteval _FmtStrSrcLoc(s8          fmt_, SrcLoc sl_ = SrcLoc::Here()) { CheckFmtStr(fmt_, sizeof...(A)); fmt = fmt_; sl = sl_; }
	consteval _FmtStrSrcLoc(const char* fmt_, SrcLoc sl_ = SrcLoc::Here()) { CheckFmtStr(fmt_, sizeof...(A)); fmt = fmt_; sl = sl_; }

	operator s8() const { return fmt; }
};

template <class... A> using FmtStrSrcLoc = _FmtStrSrcLoc<typename TypeIdentity<A>::Type...>;

//--------------------------------------------------------------------------------------------------

                      [[noreturn]] void VPanic(SrcLoc sl, s8 expr, s8 fmt, VArgs args);
template <class... A> [[noreturn]] void Panic(FmtStrSrcLoc<A...> fmtSl, A... args) { VPanic(fmtSl.sl, 0, fmtSl.fmt, MakeVArgs(args...)); }

                      [[noreturn]] inline void _AssertFail(SrcLoc sl, s8 expr)                              { VPanic(sl, expr, "",   MakeVArgs()); }
template <class... A> [[noreturn]]        void _AssertFail(SrcLoc sl, s8 expr, FmtStr<A...> fmt, A... args) { VPanic(sl, expr, fmt,  MakeVArgs(args...)); }

using PanicFn = void (SrcLoc sl, s8 expr, s8 fmt, VArgs args);
PanicFn* SetPanicFn(PanicFn* panicFn);

#define Assert(expr, ...) \
	do { \
		if (!(expr)) { \
			_AssertFail(SrcLoc::Here(), #expr, ##__VA_ARGS__); \
		} \
	} while (false)

//--------------------------------------------------------------------------------------------------

constexpr s8::s8(const char* s) {
	data = s;
	len  = CxStrLen8(s);
}

constexpr s8::s8(const char* d, u64 l) {
	data = d;
	len  = l;
}
constexpr s8::s8(const char* b, const char* e) {
	Assert(e >= b);
	data = b;
	len  = (u64)(e - b);
}
constexpr s8& s8::operator=(const char* s) {
	data = s;
	len = CxStrLen8(s);
	return *this;
}

constexpr char s8::operator[](u64 i) const {
	Assert(i <= len);
	return data[i];
}

constexpr bool operator==(s8 str1, s8 str2) { return str1.len == str2.len && MemCmp(str1.data, str2.data, str1.len) == 0; }
constexpr bool operator!=(s8 str1, s8 str2) { return str1.len != str2.len && MemCmp(str1.data, str2.data, str1.len) != 0; }

//--------------------------------------------------------------------------------------------------

struct Arena {
	u8* begin      = 0;
	u8* end        = 0;
	u8* endCommit  = 0;
	u8* endReserve = 0;

	void* Alloc(u64 size, SrcLoc sl = SrcLoc::Here());
	bool  Extend(void* p, u64 oldSize, u64 newSize, SrcLoc sl = SrcLoc::Here());
	u64   Mark();
	void  Reset(u64 mark);

	template <class T> T* AllocT(u64 n, SrcLoc sl = SrcLoc::Here()) { return (T*)Alloc(n * sizeof(T), sl); }
	template <class T> bool ExtendT(T* p, u64 oldN, u64 newN, SrcLoc sl = SrcLoc::Here()) { return Extend(p, oldN * sizeof(T), newN * sizeof(T), sl); }
};

Arena CreateArena(u64 reserveSize);
void  DestroyArena(Arena arena);

//--------------------------------------------------------------------------------------------------

struct ErrCode {
	s8  ns   = {};
	i64 code = 0;
};
constexpr bool operator==(ErrCode ec1, ErrCode ec2) { return ec1.code == ec2.code && ec1.ns == ec2.ns; }
constexpr bool operator!=(ErrCode ec1, ErrCode ec2) { return ec1.code != ec2.code || ec1.ns != ec2.ns; }

//--------------------------------------------------------------------------------------------------

struct ArenaSrcLoc {
	Arena* arena = 0;
	SrcLoc sl    = {};
	constexpr ArenaSrcLoc(Arena* arenaIn, SrcLoc slIn = SrcLoc::Here()) { arena = arenaIn; sl = slIn; }
};

struct ErrCodeSrcLoc {
	ErrCode ec = {};
	SrcLoc  sl = {};
	constexpr ErrCodeSrcLoc(ErrCode ecIn, SrcLoc slIn = SrcLoc::Here()) { ec = ecIn; sl = slIn; }
};

//--------------------------------------------------------------------------------------------------

struct [[nodiscard]] ErrArg {
	s8   name = {};
	VArg arg  = {};
};

struct [[nodiscard]] Err {
	Arena*  arena   = 0;
	Err*    prev    = 0;
	SrcLoc  sl      = {};
	ErrCode ec      = {};
	u32     argsLen = 0;
	ErrArg  args[1] = {};	// variable length

	template <class... A> Err* Push(ErrCodeSrcLoc ecSl, A... args) {
		static_assert(sizeof...(A) % 2 == 0);
		return VMakeErr(arena, this, ecSl.sl, ecSl.ec, MakeVArgs(args...));
	}
};

Err* VMakeErr(Arena* arena, Err* prev, SrcLoc sl, ErrCode ec, VArgs args);

template <class... A> Err* MakeErr(ArenaSrcLoc arenaSl, ErrCode ec, A... args) {
	static_assert(sizeof...(A) % 2 == 0);
	return VMakeErr(arenaSl.arena, 0, arenaSl.sl, ec, MakeVArgs(args...));
}

//--------------------------------------------------------------------------------------------------

template <class T = void> struct [[nodiscard]] Res;

template <> struct [[nodiscard]] Res<void> {
	Err* err = 0;

	constexpr Res() = default;
	constexpr Res(Err* e) { err = e; }	// implicit
	template <class T> constexpr Res(Res<T> r) { Assert(!r.hasVal); err = r.err; }

	constexpr operator bool() const { return err == 0; }

	constexpr void Ignore() const {}

	template <class... A> Err* Push(ErrCodeSrcLoc ecSl, A... args) {
		static_assert(sizeof...(A) % 2 == 0);
		Assert(err);
		return VMakeErr(err->arena, err, ecSl.sl, ecSl.ec, MakeVArgs(args...));
	}
};

template <class T> struct [[nodiscard]] Res {
	union {
		T    val;
		Err* err;
	};
	bool hasVal = false;

	constexpr Res()       {          hasVal = false; }
	constexpr Res(T v)    { val = v; hasVal = true;  }
	constexpr Res(Err* e) { err = e; hasVal = false; }
	constexpr Res(const Res&) = default;
	constexpr Res(Res<void> r) { Assert(r.err); err = r.err; hasVal = false; }
	template <class U> Res(Res<U> r) { Assert(!r.hasVal); err = r.err; hasVal = false; }

	constexpr operator bool() const { return hasVal; }

	constexpr Err* To(T& out) { if (hasVal) { out = val; return 0; } else { return err; } }
	constexpr T    Or(T def) { return hasVal ? val : def; }

	template <class... A> Err* Push(ErrCodeSrcLoc ecSl, A... args) {
		static_assert(sizeof...(A) % 2 == 0);
		Assert(!hasVal);
		return VMakeErr(err->arena, err, ecSl.sl, ecSl.ec, MakeVArgs(args...));
	}
};

constexpr Res<> Ok() { return Res<>(); }

//--------------------------------------------------------------------------------------------------

struct NullOpt {};
inline NullOpt nullOpt;

template <class T> struct [[nodiscard]] Opt {
	T    val    = {};
	bool hasVal = false;

	constexpr Opt() = default;
	constexpr Opt(T v)     { val = v; hasVal = true;  }
	constexpr Opt(NullOpt) {          hasVal = false; }

	constexpr operator bool() const { return hasVal; }

	constexpr T Or (T def) const { return hasVal ? val : def; };
};

//--------------------------------------------------------------------------------------------------

} namespace std {
	template <class T>
	struct initializer_list {
		const T* _begin = 0;
		const T* _end   = 0;

		constexpr initializer_list(const T* b, const T* e) { _begin = b; _end = e; }
		constexpr const T* begin() const { return _begin; }
		constexpr const T* end() const { return _end; }
		constexpr size_t size() const { return _end - _begin; }
	};
} namespace JC {

//--------------------------------------------------------------------------------------------------

template <class T>
struct Span {
	const T* data = 0;
	u64      len  = 0;

	constexpr Span() { data = nullptr; len = 0; }
	constexpr Span(T* d, u64 l) { data = d; len = l; }
	template <u64 N> constexpr Span(T (&a)[N]) { data = a; len = N; }
	constexpr Span(std::initializer_list<T> il) { data = il.begin(), len = il.size(); }
	constexpr Span(const Span&) = default;

	constexpr Span& operator=(const Span&) = default;
	
	constexpr const T& operator[](u64 i) const { return data[i]; }

	constexpr const T* Begin() const { return data; }
	constexpr const T* End()   const { return data + len; }
};

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

}	// namespace JC