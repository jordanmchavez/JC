#pragma once

#if defined _MSC_VER
	#include <string.h>	// memset/memmove/memcpy
#endif

namespace JC {

//--------------------------------------------------------------------------------------------------

#if defined _MSC_VER
	#define JC_COMPILER_MSVC
	#define JC_OS_WINDOWS

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

	#define JC_STRLEN       __builtin_strlen
	#define JC_MEMCMP       __builtin_memcmp
	#define JC_MEMSET       memset
	#define JC_MEMCPY       memcpy
	#define JC_MEMMOVE      memmove
	#define JC_FILE         __builtin_FILE()
	#define JC_LINE         __builtin_LINE()
	#define JC_IF_CONSTEVAL if (__builtin_is_constant_evaluated())
	#define JC_IS_ENUM(T)   __is_enum(T)

#else
	#error("Unsupported compiler")
#endif

#define JC_CONCAT2(x, y) x##y
#define JC_CONCAT(x, y)  JC_CONCAT2(x, y)
#define JC_MACRO_NAME(x) JC_CONCAT(x, __LINE__)
#define JC_LEN(a) (sizeof(a) / sizeof(a[0]))

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
	constexpr      s8(const char* s)                { data = s; len = StrLen8(s); }
	constexpr      s8(const char* d, u64 l)         { data = d; len = l; }
	constexpr      s8(const char* b, const char* e) { data = b; len = (u64)(e - b); }

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

template <class T> struct Array;

//--------------------------------------------------------------------------------------------------

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
template <class T>            inline constexpr bool IsEnum                      = JC_IS_ENUM(T);
template <class...>           inline constexpr bool AlwaysFalse                 = false; 

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

struct Args;

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

	template <class T>
	static constexpr Arg Make(T val) {
		using Underlying = typename RemoveConst<typename RemoveVolatile<typename RemoveRef<T>::Type>::Type>::Type;
		     if constexpr (IsSameType<Underlying, bool>)               { return { .type = ArgType::Bool, .b = val }; }
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
		else if constexpr (IsSameType<Underlying, s8>)                 { return { .type = ArgType::S8,   .s = { .data = val.data, .len = val.len } }; }
		else if constexpr (IsSameType<Underlying, char*>)              { return { .type = ArgType::S8,   .s = { .data = val,      .len = StrLen8(val) } }; }
		else if constexpr (IsSameType<Underlying, const char*>)        { return { .type = ArgType::S8,   .s = { .data = val,      .len = StrLen8(val) } }; }
		else if constexpr (IsPointer<Underlying>)                      { return { .type = ArgType::Ptr,  .p = val }; }
		else if constexpr (IsSameType<Underlying, decltype(nullptr)>)  { return { .type = ArgType::Ptr,  .p = nullptr }; }
		else if constexpr (IsEnum<Underlying>)                         { return { .type = ArgType::U64,  .u = (u64)val }; }
		else if constexpr (IsSameType<Underlying, Arg>)                { return val; }
		else if constexpr (IsSameType<Underlying, Args>)               { static_assert(AlwaysFalse<T>, "You passed Args as a placeholder variable: you probably meant to call VFmt() instead of Fmt()"); }
		else                                                           { static_assert(AlwaysFalse<T>, "Unsupported arg type"); }
	}
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
	static inline void BadPlaceholderSpec() {}

	consteval _FmtStr(s8          inFmt) { Init(inFmt); }
	consteval _FmtStr(const char* inFmt) { Init(inFmt); }

	consteval const char* CheckSpec(const char* i, const char* end) {
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
		if (i >= end) { BadPlaceholderSpec(); }

		switch (*i) {
			case 'x': i++; break;
			case 'X': i++; break;
			case 'b': i++; break;
			case 'f': i++; break;
			case 'e': i++; break;
			default:       break;
		}
		if (i >= end || *i != '}') { BadPlaceholderSpec(); }
		i++;
		return i;
	}

	consteval void Init(s8 inFmt) {
		constexpr size_t  argsLen = sizeof...(A);
		constexpr ArgType argTypes[argsLen > 0 ? argsLen : 1] = { Arg::Make(A()).type... };
		const char* i = inFmt.data;
		const char* const end = i + inFmt.len;
		u32 nextArg = 0;

		for (;;) {
			if (i >= end) {
				if (nextArg != argsLen) { TooManyArgs(); }
				fmt = inFmt;
				return;
			} else if (*i == '{') {
				i++;
				if (i >= end) { UnmatchedOpenBrace(); }
				if (*i == '{') {
					i++;
				} else {
					i = CheckSpec(i, end);
					if (nextArg >= argsLen) { NotEnoughArgs(); }
					nextArg++;
				}
			} else if (*i != '}') {
				i++;
			} else {
				i++;
				if (i >= end || *i != '}') { CloseBraceNotEscaped(); }
				i++;
			}
		}
	}
	operator s8() const { return fmt; }
};

template <class T> struct TypeIdentity { using Type = T; };
template <class... A> using FmtStr = _FmtStr<typename TypeIdentity<A>::Type...>;

//--------------------------------------------------------------------------------------------------

struct Allocator {
	virtual void* Alloc(u64 size, SrcLoc srcLoc = SrcLoc::Here()) = 0;
	virtual void* Realloc(const void* p, u64 oldSize, u64 newSize, SrcLoc srcLoc = SrcLoc::Here()) = 0;
	virtual void  Free(const void* p, u64 size) = 0;
};

struct TempAllocator : Allocator {
	void Free(const void*, u64) override {}
};

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
			Panic(__FILE__, __LINE__, #expr, ##__VA_ARGS__); \
		} \
	} while (false)

#define JC_PANIC(fmt, ...) \
	Panic(__FILE__, __LINE__, nullptr, (fmt), __VA_ARGS__)

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

	static Err* VMake(TempAllocator* ta, Err* prev, s8 file, i32 line, ErrCode ec, const ErrArg* errArgs, u32 errArgsLen);

	template <class T, class... A> static void FillErrArgs(ErrArg* errArgs, s8 name, T arg, A... args) {
		errArgs[0] = { .name = name, .arg = Arg::Make(arg) };
		if constexpr (sizeof...(A) > 0) {
			Err::FillErrArgs(&errArgs[1], args...);
		}
	}
	template <class... A> static Err* Make(TempAllocator* ta, Err* prev, s8 file, i32 line, ErrCode ec, A... args) {
		static_assert(sizeof...(A) % 2 == 0);
		constexpr u32 ErrArgsLen = sizeof...(A) / 2;
		ErrArg errArgs[ErrArgsLen > 0 ? ErrArgsLen : 1];
		if constexpr (ErrArgsLen > 0) {
			FillErrArgs(errArgs, args...);
		}
		return VMake(ta, prev, file, line, ec, errArgs, ErrArgsLen);
	}

	void Str(Array<char>* arr);
};

#define JC_ERR(tempAllocator, ec, ...) Err::Make(tempAllocator, nullptr, __FILE__, __LINE__, ec, ##__VA_ARGS__)

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

	constexpr operator bool() const { return err == nullptr; }

	constexpr void Ignore() const {}
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

} namespace std {
	template <class T>
	struct initializer_list {
		const T* _begin;
		const T* _end;

		constexpr initializer_list(const T* b, const T* e) { _begin = b; _end = e; }
		constexpr const T* begin() const { return _begin; }
		constexpr const T* end() const { return _end; }
		constexpr size_t size() const { return _end - _begin; }
	};
} namespace JC {

//--------------------------------------------------------------------------------------------------

template <class T>
struct Span {
	const T* data;
	u64      len;

	constexpr Span() { data = nullptr; len = 0; }
	constexpr Span(T* d, u64 l) { data = d; len = l; }
	template <u64 N> constexpr Span(T (&a)[N]) { data = a; len = N; }
	constexpr Span(std::initializer_list<T> il) { data = il.begin(), len = il.size(); }
	constexpr Span(const Span&) = default;

	constexpr Span& operator=(const Span&) = default;
	
	constexpr T operator[](u64 i) const { return data[i]; }

	constexpr const T* Begin() const { return data; }
	constexpr const T* End()   const { return data + len; }
};

//--------------------------------------------------------------------------------------------------

template <class T> constexpr T Min(T x, T y) { return x < y ? x : y; }
template <class T> constexpr T Max(T x, T y) { return x > y ? x : y; }
template <class T> constexpr T Clamp(T lo, T x, T hi) { return x < lo ? lo : (x > hi ? hi : x); }

//--------------------------------------------------------------------------------------------------

}	// namespace JC