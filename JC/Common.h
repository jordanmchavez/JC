#pragma once

#if defined _MSC_VER
	#include <string.h>	// memset/memmove/memcpy
#endif

namespace JC {

//--------------------------------------------------------------------------------------------------

#if defined _MSC_VER
	#define JC_COMPILER_MSVC
	#define JC_OS_WIN

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

	#define JC_STRLEN  __builtin_strlen
	#define JC_MEMCMP  __builtin_memcmp
	#define JC_MEMSET  memset
	#define JC_MEMCPY  memcpy
	#define JC_MEMMOVE memmove
	#define JC_FILE    __builtin_FILE()
	#define JC_LINE    __builtin_LINE()

	#define JC_IF_CONSTEVAL if (__builtin_is_constant_evaluated())

#else
	#error("Unsupported compiler")
#endif

#define JC_CONCAT2(x, y) x##y
#define JC_CONCAT(x, y)  JC_CONCAT2(x, y)
#define JC_MACRO_NAME(x) JC_CONCAT(x, __LINE__)

//--------------------------------------------------------------------------------------------------

constexpr u64 StrLen8(const char* s) {
	if (s == nullptr) {
		return 0;
	}
	JC_IF_CONSTEVAL {
		const char* i = s;
		while (*i) {
			i++;
		}
		return (u64)(i - s);
	} else {
		return JC_STRLEN(s);
	}
}

//--------------------------------------------------------------------------------------------------

constexpr u64 MemCmp(const void* p1, const void* p2, u64 len) {
	JC_IF_CONSTEVAL {
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
		return JC_MEMCMP(p1, p2, len);
	}
}

//--------------------------------------------------------------------------------------------------

struct s8 {
	const char* data = nullptr;
	u64         len  = 0;

	constexpr      s8() = default;
	constexpr      s8(const s8& s) = default;
	constexpr      s8(const char* s)        { data = s; len = StrLen8(s); }
	constexpr      s8(const char* d, u64 l) { data = d; len = l; }

	constexpr s8&  operator=(const s8& s) = default;
	constexpr s8&  operator=(const char* s) { data = s; len = StrLen8(s); return *this; }

	constexpr char operator[](u64 i) const  { return data[i]; }
};

constexpr bool operator==(s8 str1, s8 str2) { return str1.len == str2.len && MemCmp(str1.data, str2.data, str1.len) == 0; }
constexpr bool operator!=(s8 str1, s8 str2) { return str1.len != str2.len && MemCmp(str1.data, str2.data, str1.len) != 0; }

//--------------------------------------------------------------------------------------------------

struct SrcLoc {
	s8  file = 0;
	i32 line = 0;

	static consteval SrcLoc Here(s8 file = JC_FILE, i32 line = JC_LINE) {
		return SrcLoc { .file = file, .line = line };
	}
};

//--------------------------------------------------------------------------------------------------

struct Allocator {
	virtual void* Alloc(u64 size, SrcLoc srcLoc = SrcLoc::Here()) = 0;
	virtual void* Realloc(const void* p, u64 size, u64 newSize, SrcLoc srcLoc = SrcLoc::Here()) = 0;
	virtual void  Free(const void* p, u64 size) = 0;
};

struct TempAllocator : Allocator {};

//--------------------------------------------------------------------------------------------------

enum struct ArgType {
	Bool,
	Char,
	I64,
	U64,
	F64,
	Ptr,
	S8,
};

struct ArgStr {
	const char* data;
	u64         len;
};


struct Arg {
	ArgType type;
	union {
		bool        b;
		char        c;
		i64         i;
		u64         u;
		f64         f;
		ArgStr      s;
		const void* p;
	};

	static constexpr Arg Make(Arg val)                { return val; }
	static constexpr Arg Make(bool val)               { return { .type = ArgType::Bool, .b = val }; }
	static constexpr Arg Make(char val)               { return { .type = ArgType::Char, .c = val }; }
	static constexpr Arg Make(signed char val)        { return { .type = ArgType::I64,  .i = val }; }
	static constexpr Arg Make(signed short val)       { return { .type = ArgType::U64,  .i = val }; }
	static constexpr Arg Make(signed int val)         { return { .type = ArgType::I64,  .i = val }; }
	static constexpr Arg Make(signed long val)        { return { .type = ArgType::I64,  .i = val }; }
	static constexpr Arg Make(signed long long val)   { return { .type = ArgType::I64,  .i = val }; }
	static constexpr Arg Make(unsigned char val)      { return { .type = ArgType::U64,  .u = val }; }
	static constexpr Arg Make(unsigned short val)     { return { .type = ArgType::U64,  .u = val }; }
	static constexpr Arg Make(unsigned int val)       { return { .type = ArgType::U64,  .u = val }; }
	static constexpr Arg Make(unsigned long val)      { return { .type = ArgType::U64,  .u = val }; }
	static constexpr Arg Make(unsigned long long val) { return { .type = ArgType::U64,  .u = val }; }
	static constexpr Arg Make(float val)              { return { .type = ArgType::F64,  .f = val }; }
	static constexpr Arg Make(double val)             { return { .type = ArgType::F64,  .f = val }; }
	static constexpr Arg Make(const void* val)        { return { .type = ArgType::Ptr,  .p = val }; }
	static constexpr Arg Make(decltype(nullptr))      { return { .type = ArgType::Ptr,  .p = 0 }; }
	static constexpr Arg Make(s8 val)                 { return { .type = ArgType::S8,   .s = { .data = val.data, .len = val.len } }; }
	static constexpr Arg Make(const char* val)        { return { .type = ArgType::S8,   .s = { .data = val,      .len = StrLen8(val) } }; }

	template <u64 N> static constexpr Arg Make(char (&val)[N])       { return { .type = ArgType::S8, .s = { .data = val, .len = StrLen8(val) } }; }
	template <u64 N> static constexpr Arg Make(const char (&val)[N]) { return { .type = ArgType::S8, .s = { .data = val, .len = StrLen8(val) } }; }
};

template <u32 N> struct ArgStore {
	Arg args[N > 0 ? N : 1];
};

struct Args {
	const Arg* args;
	u32        len;

	template <u32 N> constexpr Args(ArgStore<N> argStore) {
		args = argStore.args;
		len  = N;
	}

	template <class... A> static constexpr ArgStore<sizeof...(A)> Make(A... args) {
		return ArgStore<sizeof...(A)> { Arg::Make(args)... };
	}
};

//--------------------------------------------------------------------------------------------------

template <class... A> struct _FmtStr {
	s8 fmt;

	static inline void UnmatchedOpenBrace() {}
	static inline void NotEnoughArgs() {}
	static inline void CloseBraceNotEscaped() {}
	static inline void TooManyArgs() {}

	consteval _FmtStr(s8          inFmt) { Init(inFmt); }
	consteval _FmtStr(const char* inFmt) { Init(inFmt); }

	consteval void Init(s8 inFmt) {
		constexpr size_t  argsLen = sizeof...(A);
		constexpr ArgType argTypes[argsLen > 0 ? argsLen : 1] = { Arg::Make(A()).type... };
		const char* i = inFmt.data;
		const char* const end = i + inFmt.len;
		u32 nextArg = 0;
		while (i < end) {
			if (*i == '{') {
				++i;
				if (i >= end) { UnmatchedOpenBrace(); }
				if (*i != '{') {
					if (nextArg >= argsLen) { NotEnoughArgs(); }
					++nextArg;
					while (*i != '}') {
						++i;
						if (i >= end) { UnmatchedOpenBrace(); }
					}
				}
			} else if (*i == '}') {
				++i;
				if (i >= end || *i != '}') { CloseBraceNotEscaped(); }
			}
			++i;
		}
		if (nextArg != argsLen) { TooManyArgs(); }
		fmt = inFmt;
	}
	operator s8() const { return fmt; }
};

template <class T> struct TypeIdentity { using Type = T; };
//template <class T> using TypeIdentity = _TypeIdentity<T>::Type;
template <class... A> using FmtStr = _FmtStr<typename TypeIdentity<A>::Type...>;

//--------------------------------------------------------------------------------------------------

[[noreturn]] void VPanic(s8 file, i32 line, s8 expr, s8 fmt, Args args);

[[noreturn]] inline void Panic(s8 file, i32 line, s8 expr) {
	VPanic(file, line, expr, "", Args::Make());
}
	
template <class... A> [[noreturn]] void Panic(s8 file, i32 line, s8 expr, s8 fmt, A... args) {
	VPanic(file, line, expr, fmt, Args::Make(args...));
}

#define JC_ASSERT(expr, ...) \
	do { \
		if (!(expr)) { \
			Panic(__FILE__, __LINE__, #expr, __VA_ARGS__); \
		} \
	} while (false)

#define JC_PANIC(fmt, ...) \
	Panic(__FILE__, __LINE__, nullptr, (fmt), Args::Make(__VA_ARGS__))

//--------------------------------------------------------------------------------------------------

struct [[nodiscard]] ErrCode {
	s8  ns;
	u64 code;
};
constexpr bool operator==(ErrCode ec1, ErrCode ec2) { return ec1.code == ec2.code && ec1.ns == ec2.ns; }
constexpr bool operator!=(ErrCode ec1, ErrCode ec2) { return ec1.code != ec2.code || ec1.ns != ec2.ns; }

struct [[nodiscard]] ErrArg {
	s8  name;
	Arg arg;
};

struct [[nodiscard]] Err {
	Err*    prev = nullptr;
	s8      file;
	i32     line = 0;
	ErrCode ec;
	u32     argsLen = 0;
	ErrArg  args[1];	// variable length

	static Err* VMake(TempAllocator* alloc, Err* prev, s8 file, i32 line, ErrCode ec, const ErrArg* errArgs, u32 errArgsLen);

	template <class T, class... A> static void FillErrArgs(ErrArg* errArgs, s8 name, T arg, A... args) {
		errArgs[0] = { .name = name, .arg = Arg::Make(arg) };
		if constexpr (sizeof...(A) > 0) {
			Err::FillErrArgs(&errArgs[1], args...);
		}
	}
	template <class... A> static Err* Make(TempAllocator* alloc, Err* prev, s8 file, i32 line, ErrCode ec, A... args) {
		static_assert(sizeof...(A) % 2 == 0);
		constexpr u32 ErrArgsLen = sizeof...(A) / 2;
		ErrArg errArgs[ErrArgsLen > 0 ? ErrArgsLen : 1];
		if constexpr (ErrArgsLen > 0) {
			FillErrArgs(errArgs, args...);
		}
		return VMake(alloc, prev, file, line, ec, errArgs, ErrArgsLen);
	}
};

#define JC_ERR(ec, ...) Err::Make(tempAllocator, nullptr, __FILE__, __LINE__, ec, __VA_ARGS__)

//--------------------------------------------------------------------------------------------------

template <class T = void> struct [[nodiscard]] Res {
	union {
		T    val;
		Err* err;
	};
	bool hasVal;

	constexpr Res()       {          hasVal = false; }
	constexpr Res(T v)    { val = v; hasVal = true;  }
	constexpr Res(Err* e) { err = e; hasVal = false; }

	constexpr operator bool() const { return hasVal; }

	constexpr T Or(T def) const { return hasVal ? val : def; }
};

template <> struct [[nodiscard]] Res<void> {
	Err* err;

	constexpr Res() = default;
	constexpr Res(Err* e) { err = e; }

	constexpr operator bool() const { return err != nullptr; }
};

constexpr Res<> Ok() { return Res<>(); }

//--------------------------------------------------------------------------------------------------

struct NullOpt {};
inline NullOpt nullOpt;

template <class T> struct [[nodiscard]] Opt {
	T val;
	bool hasVal;

	constexpr Opt()        {          hasVal = false; }
	constexpr Opt(T v)     { val = v; hasVal = true;  }
	constexpr Opt(NullOpt) {          hasVal = false; }

	constexpr operator bool() const { return hasVal; }

	constexpr T Or (T def) const { return hasVal ? val : def; };
};

//--------------------------------------------------------------------------------------------------

template <class T> constexpr T Min(T x, T y) { return x < y ? x : y; }
template <class T> constexpr T Max(T x, T y) { return x > y ? x : y; }
template <class T> constexpr T Clamp(T lo, T x, T hi) { return x < lo ? lo : (x > hi ? hi : x); }

//--------------------------------------------------------------------------------------------------

}	// namespace JC