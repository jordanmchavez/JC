#pragma once

#include "JC/Common_Arg.h"
#include "JC/Common_SrcLoc.h"

#if defined _DEBUG
	#define Err_BreakOnErr
#endif	// _DEBUG

namespace JC::Err {

//--------------------------------------------------------------------------------------------------

struct [[nodiscard]] Err { U64 handle = 0; };

void AddArg(Err err, Str name, Arg::Arg arg);

template <class A1, class... A> void AddArgs(Err err, Str name1, A1 arg1, A... args) {
	AddArg(err, name1, Arg::Make(arg1));
	if constexpr (sizeof...(A) > 0) {
		AddArgs(err, args...);
	}
}

Err Make(Err prev, SrcLoc sl, Str ns, Str code);
Err Make(Err prev, SrcLoc sl, Str ns, U64 code);

template <class... A> Err Make(Err prev, SrcLoc sl, Str ns, Str code, A... args) {
	#if defined Err_BreakOnErr
		Sys_DbgBreak;
	#endif	// Err_BreakOnErr
	Err err = Make(prev, sl, ns, code);
	AddArgs(err, args...);
	return err;
}

template <class... A> Err Make(Err prev, SrcLoc sl, Str ns, U64 code, A... args) {
	#if defined Err_BreakOnErr
		Sys_DbgBreak;
	#endif	// Err_BreakOnErr
	Err err = Make(prev, sl, ns, code);
	AddArgs(err, args...);
	return err;
}

struct Code {
	Str const ns;
	Str const code;

                                                                                              Err operator()(                                                                                                                        SrcLoc sl = SrcLoc::Here()) const { return Make(Err(), sl, ns, code); }
	template <class A1                                                                      > Err operator()(Str n1, A1 a1,                                                                                                          SrcLoc sl = SrcLoc::Here()) const { return Make(Err(), sl, ns, code, n1, a1); }
	template <class A1, class A2                                                            > Err operator()(Str n1, A1 a1, Str n2, A2 a2,                                                                                           SrcLoc sl = SrcLoc::Here()) const { return Make(Err(), sl, ns, code, n1, a1, n2, a2); }
	template <class A1, class A2, class A3                                                  > Err operator()(Str n1, A1 a1, Str n2, A2 a2, Str n3, A3 a3,                                                                            SrcLoc sl = SrcLoc::Here()) const { return Make(Err(), sl, ns, code, n1, a1, n2, a2, n3, a3); }
	template <class A1, class A2, class A3, class A4                                        > Err operator()(Str n1, A1 a1, Str n2, A2 a2, Str n3, A3 a3, Str n4, A4 a4,                                                             SrcLoc sl = SrcLoc::Here()) const { return Make(Err(), sl, ns, code, n1, a1, n2, a2, n3, a3, n4, a4); }
	template <class A1, class A2, class A3, class A4, class A5                              > Err operator()(Str n1, A1 a1, Str n2, A2 a2, Str n3, A3 a3, Str n4, A4 a4, Str n5, A5 a5,                                              SrcLoc sl = SrcLoc::Here()) const { return Make(Err(), sl, ns, code, n1, a1, n2, a2, n3, a3, n4, a4, n5, a5); }
	template <class A1, class A2, class A3, class A4, class A5, class A6                    > Err operator()(Str n1, A1 a1, Str n2, A2 a2, Str n3, A3 a3, Str n4, A4 a4, Str n5, A5 a5, Str n6, A6 a6,                               SrcLoc sl = SrcLoc::Here()) const { return Make(Err(), sl, ns, code, n1, a1, n2, a2, n3, a3, n4, a4, n5, a5, n6, a6); }
	template <class A1, class A2, class A3, class A4, class A5, class A6, class A7          > Err operator()(Str n1, A1 a1, Str n2, A2 a2, Str n3, A3 a3, Str n4, A4 a4, Str n5, A5 a5, Str n6, A6 a6, Str n7, A7 a7,                SrcLoc sl = SrcLoc::Here()) const { return Make(Err(), sl, ns, code, n1, a1, n2, a2, n3, a3, n4, a4, n5, a5, n6, a6, n7, a7); }
	template <class A1, class A2, class A3, class A4, class A5, class A6, class A7, class A8> Err operator()(Str n1, A1 a1, Str n2, A2 a2, Str n3, A3 a3, Str n4, A4 a4, Str n5, A5 a5, Str n6, A6 a6, Str n7, A7 a7, Str n8, A8 a8, SrcLoc sl = SrcLoc::Here()) const { return Make(Err(), sl, ns, code, n1, a1, n2, a2, n3, a3, n4, a4, n5, a5, n6, a6, n7, a7, n8, a8); }
};

#define Err_Def(Ns_, Code_) constexpr Err::Code Err_##Code_ = { .ns = #Ns_, .code = #Code_ }


//--------------------------------------------------------------------------------------------------

}	// namespace JC::Err