#pragma once

#include "JC/Common_Arg.h"
#include "JC/Common_SrcLoc.h"

#if defined _DEBUG
	#define Err_BreakOnErr
#endif	// _DEBUG

namespace JC::Err {

//--------------------------------------------------------------------------------------------------

constexpr U32 MaxNamedArgs = 32;

struct NamedArg {
	Str      name;
	Arg::Arg arg;
};

struct [[nodiscard]] Err {
	U64        frame;
	Err const* prev;
	SrcLoc     sl;
	Str        ns;
	Str        sCode;
	U64        uCode;
	NamedArg   namedArgs[MaxNamedArgs];
	U32        namedArgsLen;
};

void Frame(U64 frame);

inline void FillNamedArgs(NamedArg*) {}

template <class A1, class... A> void FillNamedArgs(NamedArg* namedArgs, Str name1, A1 arg1, A... args) {
	*namedArgs = { .name = name1, .arg = Arg::Make(arg1) };
	FillNamedArgs(namedArgs + 1, args...);
}

Err const* Makev(Err const* prev, SrcLoc sl, Str ns, Str sCode, U64 uCode, Span<NamedArg const> namedArgs);

template <class... A> Err const* Make(Err const* prev, SrcLoc sl, Str ns, Str sCode, U64 uCode, A... args) {
	static_assert(sizeof...(A) / 2 < MaxNamedArgs);
	static_assert((sizeof...(A) & 1) == 0);

	#if defined Err_BreakOnErr
		Sys_DbgBreak;
	#endif	// Err_BreakOnErr

	NamedArg namedArgs[sizeof...(A) + 1];
	FillNamedArgs(namedArgs, args...);
	return Makev(prev, sl, ns, sCode, uCode, namedArgs);
}
struct Code {
	Str const ns;
	Str const code;

                                                                                              Err const * operator()(                                                                                                                        SrcLoc sl = SrcLoc::Here()) const { return Make(nullptr, sl, ns, code, 0); }
	template <class A1                                                                      > Err const * operator()(Str n1, A1 a1,                                                                                                          SrcLoc sl = SrcLoc::Here()) const { return Make(nullptr, sl, ns, code, 0, n1, a1); }
	template <class A1, class A2                                                            > Err const * operator()(Str n1, A1 a1, Str n2, A2 a2,                                                                                           SrcLoc sl = SrcLoc::Here()) const { return Make(nullptr, sl, ns, code, 0, n1, a1, n2, a2); }
	template <class A1, class A2, class A3                                                  > Err const * operator()(Str n1, A1 a1, Str n2, A2 a2, Str n3, A3 a3,                                                                            SrcLoc sl = SrcLoc::Here()) const { return Make(nullptr, sl, ns, code, 0, n1, a1, n2, a2, n3, a3); }
	template <class A1, class A2, class A3, class A4                                        > Err const * operator()(Str n1, A1 a1, Str n2, A2 a2, Str n3, A3 a3, Str n4, A4 a4,                                                             SrcLoc sl = SrcLoc::Here()) const { return Make(nullptr, sl, ns, code, 0, n1, a1, n2, a2, n3, a3, n4, a4); }
	template <class A1, class A2, class A3, class A4, class A5                              > Err const * operator()(Str n1, A1 a1, Str n2, A2 a2, Str n3, A3 a3, Str n4, A4 a4, Str n5, A5 a5,                                              SrcLoc sl = SrcLoc::Here()) const { return Make(nullptr, sl, ns, code, 0, n1, a1, n2, a2, n3, a3, n4, a4, n5, a5); }
	template <class A1, class A2, class A3, class A4, class A5, class A6                    > Err const * operator()(Str n1, A1 a1, Str n2, A2 a2, Str n3, A3 a3, Str n4, A4 a4, Str n5, A5 a5, Str n6, A6 a6,                               SrcLoc sl = SrcLoc::Here()) const { return Make(nullptr, sl, ns, code, 0, n1, a1, n2, a2, n3, a3, n4, a4, n5, a5, n6, a6); }
	template <class A1, class A2, class A3, class A4, class A5, class A6, class A7          > Err const * operator()(Str n1, A1 a1, Str n2, A2 a2, Str n3, A3 a3, Str n4, A4 a4, Str n5, A5 a5, Str n6, A6 a6, Str n7, A7 a7,                SrcLoc sl = SrcLoc::Here()) const { return Make(nullptr, sl, ns, code, 0, n1, a1, n2, a2, n3, a3, n4, a4, n5, a5, n6, a6, n7, a7); }
	template <class A1, class A2, class A3, class A4, class A5, class A6, class A7, class A8> Err const * operator()(Str n1, A1 a1, Str n2, A2 a2, Str n3, A3 a3, Str n4, A4 a4, Str n5, A5 a5, Str n6, A6 a6, Str n7, A7 a7, Str n8, A8 a8, SrcLoc sl = SrcLoc::Here()) const { return Make(nullptr, sl, ns, code, 0, n1, a1, n2, a2, n3, a3, n4, a4, n5, a5, n6, a6, n7, a7, n8, a8); }
};

bool operator==(Code ec, Err const* err);
bool operator==(Err const* err, Code ec);

#define Err_Def(Ns_, Code_) constexpr Err::Code Err_##Code_ = { .ns = #Ns_, .code = #Code_ }

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Err