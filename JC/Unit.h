#pragma once

#include "JC/Common.h"

//--------------------------------------------------------------------------------------------------

Bool Unit_Run();

Bool Unit_CheckFailImpl(SrcLoc sl);
Bool Unit_CheckExprFail(SrcLoc sl, Str expr);
Bool Unit_CheckRelFail(SrcLoc sl, Str expr, Arg x, Arg y);
Bool Unit_CheckSpanEqFail_Len(SrcLoc sl, Str expr, U64 xLen, U64 yLen);
Bool Unit_CheckSpanEqFail_Elem(SrcLoc sl, Str expr, U64 i, Arg x, Arg y);

template <class X, class Y> Bool Unit_CheckEq(SrcLoc sl, Str expr, X x, Y y) {
	return (x == y) || Unit_CheckRelFail(sl, expr, Arg_Make(x), Arg_Make(y));
}

template <class X, class Y> Bool Unit_CheckNeq(SrcLoc sl, Str expr, X x, Y y) {
	return (x != y) || Unit_CheckRelFail(sl, expr, Arg_Make(x), Arg_Make(y));
}

template <class X, class Y> Bool Unit_CheckSpanEq(SrcLoc sl, Str expr, Span<X> x, Span<Y> y) {
	if (x.len != y.len) {
		return Unit_CheckSpanEqFail_Len(sl, expr, x.len, y.len);
	}
	for (U64 i = 0; i < x.len; i++) {
		if (x[i] != y[i]) {
			return Unit_CheckSpanEqFail_Elem(sl, expr, i, Arg_Make(x[i]), Arg_Make(y[i]));
		}
	}
	return true;
}

using Unit_TestFn = void([[maybe_unused]] Mem* testMem);

struct Unit_TestRegistrar {
	Unit_TestRegistrar(Str name, SrcLoc sl, Unit_TestFn* fn);
};

struct Unit_Sig {
	Str    name;
	SrcLoc sl;
};

struct Unit_Subtest {
	Unit_Sig sig;
	Bool     shouldRun;

	Unit_Subtest(Str name, SrcLoc sl);
	~Unit_Subtest();
};

#define Unit_DbgBreak ([]() { Dbg_Break; return false; }())

#define Unit_TestImpl(name, fn, registrarVar) \
	static void fn([[maybe_unused]] Mem* testMem); \
	static Unit_TestRegistrar registrarVar = Unit_TestRegistrar(name, SrcLoc_Here(), fn); \
	static void fn([[maybe_unused]] Mem* testMem)

#define Unit_Test(name) \
	Unit_TestImpl(name, MacroUniqueName(Unit_TestFn_), MacroUniqueName(Unit_TestRegistrar_))

#define Unit_SubTestImpl(name, subtestVar) \
	if (Unit_Subtest subtestVar = Unit_Subtest(name, SrcLoc_Here()); subtestVar.shouldRun)

#define Unit_SubTest(name) Unit_SubTestImpl(name, MacroUniqueName(UnitSubUnit_))

#define Unit_CheckFailAt(sl)         (Unit_CheckFailImpl(sl) || Unit_DbgBreak)
#define Unit_CheckFail()             (Unit_CheckFailImpl(SrcHere) || Unit_DbgBreak)
#define Unit_CheckTrueAt(sl, x)      ((x) || Unit_CheckExprFail(sl, #x) || Unit_DbgBreak)
#define Unit_CheckTrue(x)            Unit_CheckTrueAt(SrcLoc_Here(), x)
#define Unit_CheckFalseAt(sl, x)     (!(x) || Unit_CheckExprFail(sl, "NOT " #x) || Unit_DbgBreak)
#define Unit_CheckFalse(x)           Unit_CheckFalseAt(SrcLoc_Here(), x)
#define Unit_CheckEqAt(sl, x, y)     (Unit_CheckEq(sl, #x " == " #y, (x), (y)) || Unit_DbgBreak)
#define Unit_CheckEq(x, y)           Unit_CheckEqAt(SrcLoc_Here(), x, y)
#define Unit_CheckNeqAt(sl, x, y)    (Unit_CheckNeq(sl, #x " != " #y, (x), (y)) || Unit_DbgBreak)
#define Unit_CheckNeq(x, y)          Unit_CheckNeqAt(SrcLoc_Here(), x, y)
#define Unit_CheckSpanEqAt(sl, x, y) (Unit_CheckSpanEq(sl, #x " == " #y, (x), (y)) || Unit_DbgBreak)
#define Unit_CheckSpanEq(x, y)       Unit_CheckSpanEqAt(SrcLoc_Here(), x, y)