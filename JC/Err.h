#pragma once

#include "JC/Core.h"

namespace Mem { struct TempAllocator; }

namespace JC::Err {

//--------------------------------------------------------------------------------------------------

struct NamedArg {
	Str name;
	Arg arg;
};

struct Err {
	static constexpr U32 MaxNamedArgs = 32;

	Err*     prev;
	SrcLoc   sl;
	Str      ns;
	Str      code;
	NamedArg namedArgs[MaxNamedArgs];
	U32      namedArgsLen;
};

template <class A1, class... A> void FillNamedArgs(NamedArg* namedArgs, const char* name, A1 arg1, A... args) {
	namedArgs->name = name;
	namedArgs->arg = MakeArg(arg1);
	if constexpr (sizeof...(A) > 0) {
		FillArgs(namedArgs + 1, args...);
	}
}

inline void FillArgs(NamedArg*) {}

Err* Make(Err* prev, SrcLoc sl, Str ns, Str code, Span<const NamedArg> namedArgs);
Err* Make(Err* prev, SrcLoc sl, Str ns, U64 code, Span<const NamedArg> namedArgs);

Str MakeStr(const Err* err);

template <class...A> Err* Make(Err* prev, SrcLoc sl, Code ec, A... args) {
	static_assert((sizeof...(A) & 1) == 0);
	static_assert((sizeof...(A) / 2) <= Err::MaxNamedArgs);

	NamedArg namedArgs[(sizeof...(A) / 2) + 1];	// +1 to avoid zero-sized array in no-args case
	FillNamedArgs(namedArgs, args...);
	return MakeErr(prev, sl, ec, Span<const NamedArg>(namedArgs, sizeof...(A) / 2));
}
	
inline bool operator==(Code ec,  Err* err) { return ec.ns == err->ns && ec.code == err->code; }
inline bool operator==(Err* err, Code ec)  { return ec.ns == err->ns && ec.code == err->code; }

#define JC_ERR(Code, ...) Err::Make(0, SrcLoc::Here(), EC_##Code, ##__VA_ARGS__)
#define JC_PUSH_ERR(prev, Code, ...) Err::Make(prev, SrcLoc::Here(), EC_##Code, ##__VA_ARGS__)

#define JC_CHECK_RES(Expr) \
	if (Res<> _r = (Expr); !_r) { \
		return Err::Make(_r.err, SrcLoc::Here(), Err::Code { .ns = "", .code = "" }); \
	}

void Init(Mem::TempAllocator* tempAllocator);

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Err