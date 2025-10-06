#pragma once

#include "JC/Common_Arg.h"
#include "JC/Common_Mem.h"
#include "JC/Common_SrcLoc.h"

namespace JC::Unit {

//--------------------------------------------------------------------------------------------------

bool Run();

bool CheckFailImpl(SrcLoc sl);
bool CheckExprFail(SrcLoc sl, Str expr);
bool CheckRelFail(SrcLoc sl, Str expr, Arg::Arg x, Arg::Arg y);
bool CheckSpanEqFail_Len(SrcLoc sl, Str expr, U64 xLen, U64 yLen);
bool CheckSpanEqFail_Elem(SrcLoc sl, Str expr, U64 i, Arg::Arg x, Arg::Arg y);

template <class X, class Y> bool CheckEq(SrcLoc sl, Str expr, X x, Y y) {
	return (x == y) || CheckRelFail(sl, expr, Arg::Make(x), Arg::Make(y));
}

template <class X, class Y> bool CheckNeq(SrcLoc sl, Str expr, X x, Y y) {
	return (x != y) || CheckRelFail(sl, expr, Arg::Make(x), Arg::Make(y));
}

template <class X, class Y> bool CheckSpanEq(SrcLoc sl, Str expr, Span<X> x, Span<Y> y) {
	if (x.len != y.len) {
		return CheckSpanEqFail_Len(sl, expr, x.len, y.len);
	}
	for (U64 i = 0; i < x.len; i++) {
		if (x[i] != y[i]) {
			return CheckSpanEqFail_Elem(sl, expr, i, Arg::Make(x[i]), Arg::Make(y[i]));
		}
	}
	return true;
}

using TestFn = void([[maybe_unused]] Mem::Mem* testMem);

struct TestRegistrar {
	TestRegistrar(Str name, SrcLoc sl, TestFn* fn);
};

struct Sig {
	Str    name;
	SrcLoc sl;
};

struct Subtest {
	Sig  sig;
	bool shouldRun;

	Subtest(Str name, SrcLoc sl);
	~Subtest();
};

#define Unit_DbgBreak ([]() { Sys_DbgBreak; return false; }())

#define Unit_TestImpl(name, fn, registrarVar) \
	static void fn([[maybe_unused]] Mem::Mem* testMem); \
	static Unit::TestRegistrar registrarVar = Unit::TestRegistrar(name, SrcLoc::Here(), fn); \
	static void fn([[maybe_unused]] Mem::Mem* testMem)

#define Unit_Test(name) \
	Unit_TestImpl(name, MacroUniqueName(Unit_TestFn_), MacroUniqueName(Unit_TestRegistrar_))

#define Unit_SubTestImpl(name, subtestVar) \
	if (Unit::Subtest subtestVar = Unit::Subtest(name, SrcLoc::Here()); subtestVar.shouldRun)

#define Unit_SubTest(name) Unit_SubTestImpl(name, MacroUniqueName(UnitSubUnit_))

#define Unit_CheckFailAt(sl)         (Unit::CheckFailImpl(sl) || Unit_DbgBreak)
#define Unit_CheckFail()             (Unit::CheckFailImpl(SrcHere) || Unit_DbgBreak)
#define Unit_CheckTrueAt(sl, x)      ((x) || Unit::CheckExprFail(sl, #x) || Unit_DbgBreak)
#define Unit_CheckTrue(x)            Unit_CheckTrueAt(SrcLoc::Here(), x)
#define Unit_CheckFalseAt(sl, x)     (!(x) || Unit::CheckExprFail(sl, "NOT " #x) || Unit_DbgBreak)
#define Unit_CheckFalse(x)           Unit_CheckFalseAt(SrcLoc::Here(), x)
#define Unit_CheckEqAt(sl, x, y)     (Unit::CheckEq(sl, #x " == " #y, (x), (y)) || Unit_DbgBreak)
#define Unit_CheckEq(x, y)           Unit_CheckEqAt(SrcLoc::Here(), x, y)
#define Unit_CheckNeqAt(sl, x, y)    (Unit::CheckNeq(sl, #x " != " #y, (x), (y)) || Unit_DbgBreak)
#define Unit_CheckNeq(x, y)          Unit_CheckNeqAt(SrcLoc::Here(), x, y)
#define Unit_CheckSpanEqAt(sl, x, y) (Unit::CheckSpanEq(sl, #x " == " #y, (x), (y)) || Unit_DbgBreak)
#define Unit_CheckSpanEq(x, y)       Unit_CheckSpanEqAt(SrcLoc::Here(), x, y)

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Unit