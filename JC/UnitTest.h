#pragma once

#include "JC/Common.h"
#include "JC/Sys.h"

namespace JC { struct Log; }

namespace JC::UnitTest {

//--------------------------------------------------------------------------------------------------

bool Run(Log* log);

bool CheckFailImpl(SrcLoc sl);
bool CheckExprFail(SrcLoc sl, s8 expr);
bool CheckRelFail(SrcLoc sl, s8 expr, VArg x, VArg y);
bool CheckSpanEqFail_Len(SrcLoc sl, s8 expr, u64 xLen, u64 yLen);
bool CheckSpanEqFail_Elem(SrcLoc sl, s8 expr, u64 i, VArg x, VArg y);

template <class X, class Y> bool CheckEq(SrcLoc sl, s8 expr, X x, Y y) {
	return (x == y) || CheckRelFail(sl, expr, MakeVArg(x), MakeVArg(y));
}

template <class X, class Y> bool CheckNeq(SrcLoc sl, s8 expr, X x, Y y) {
	return (x != y) || CheckRelFail(sl, expr, MakeVArg(x), MakeVArg(y));
}

template <class X, class Y> bool CheckSpanEq(SrcLoc sl, s8 expr, Span<X> x, Span<Y> y) {
	if (x.len != y.len) {
		return CheckSpanEqFail_Len(sl, expr, x.len, y.len);
	}
	for (u64 i = 0; i < x.len; i++) {
		if (x[i] != y[i]) {
			return CheckSpanEqFail_Elem(sl, expr, i, MakeVArg(x[i]), MakeVArg(y[i]));
		}
	}
	return true;
}

using TestFn = void (Arena* temp);

struct TestRegistrar {
	TestRegistrar(s8 name, SrcLoc sl, TestFn* fn);
};

struct Subtest {
	struct Sig {
		s8     name = {};
		SrcLoc sl   = {};
	};
	Sig  sig       = {};
	bool shouldRun = false;

	Subtest(s8 name, SrcLoc sl);
	~Subtest();
};

#define TestDebuggerBreak ([]() { Sys_DebuggerBreak(); return false; }())

#define UnitTestImpl(name, fn, registrarVar) \
	static void fn([[maybe_unused]] Arena* temp); \
	static UnitTest::TestRegistrar registrarVar = UnitTest::TestRegistrar(name, SrcLoc::Here(), fn); \
	static void fn([[maybe_unused]] Arena* temp)

#define UnitTest(name) \
	UnitTestImpl(name, MacroName(UnitTestFn_), MacroName(UnitTestRegistrar_))

#define SubTestImpl(name, subtestVar) \
	if (UnitTest::Subtest subtestVar = UnitTest::Subtest(name, SrcLoc::Here()); subtestVar.shouldRun)

#define SubTest(name) SubTestImpl(name, MacroName(UnitSubtest_))

#define CheckFailAt(sl)         (UnitTest::CheckFailImpl(sl) || TestDebuggerBreak)
#define CheckFail()             (UnitTest::CheckFailImpl(SrcHere) || TestDebuggerBreak)
#define CheckTrueAt(sl, x)      ((x) || UnitTest::CheckExprFail(sl, #x) || TestDebuggerBreak)
#define CheckTrue(x)            CheckTrueAt(SrcLoc::Here(), x)
#define CheckFalseAt(sl, x)     (!(x) || UnitTest::CheckExprFail(sl, "NOT " #x) || TestDebuggerBreak)
#define CheckFalse(x)           CheckFalseAt(SrcLoc::Here(), x)
#define CheckEqAt(sl, x, y)     (UnitTest::CheckEq(sl, #x " == " #y, (x), (y)) || TestDebuggerBreak)
#define CheckEq(x, y)           CheckEqAt(SrcLoc::Here(), x, y)
#define CheckNeqAt(sl, x, y)    (UnitTest::CheckNeq(sl, #x " != " #y, (x), (y)) || TestDebuggerBreak)
#define CheckNeq(x, y)          CheckNeqAt(SrcLoc::Here(), x, y)
#define CheckSpanEqAt(sl, x, y) (UnitTest::CheckSpanEq(sl, #x " == " #y, (x), (y)) || TestDebuggerBreak)
#define CheckSpanEq(x, y)       CheckSpanEqAt(SrcLoc::Here(), x, y)

//--------------------------------------------------------------------------------------------------

}	// namespace JC::UnitTest