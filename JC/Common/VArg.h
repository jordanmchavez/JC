#pragma once

#include "JC/Common.h"

namespace JC {

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

}	// namespace JC