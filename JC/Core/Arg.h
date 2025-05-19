#pragma once

#include "JC/Core.h"

namespace JC {

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

}	// namespace JC