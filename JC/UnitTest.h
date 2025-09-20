#pragma once

#include "JC/Core.h"

namespace JC::Log { struct Logger; }

namespace JC::UnitTest {

//--------------------------------------------------------------------------------------------------

Bool Run(Mem::TempAllocator* tempAllocator, Log::Logger* logger);

Bool CheckFailImpl(SrcLoc sl);
Bool CheckExprFail(SrcLoc sl, Str expr);
Bool CheckRelFail(SrcLoc sl, Str expr, Arg x, Arg y);
Bool CheckSpanEqFail_Len(SrcLoc sl, Str expr, U64 xLen, U64 yLen);
Bool CheckSpanEqFail_Elem(SrcLoc sl, Str expr, U64 i, Arg x, Arg y);

template <class X, class Y> Bool CheckEq(SrcLoc sl, Str expr, X x, Y y) {
	return (x == y) || CheckRelFail(sl, expr, MakeArg(x), MakeArg(y));
}

template <class X, class Y> Bool CheckNeq(SrcLoc sl, Str expr, X x, Y y) {
	return (x != y) || CheckRelFail(sl, expr, MakeArg(x), MakeArg(y));
}

template <class X, class Y> Bool CheckSpanEq(SrcLoc sl, Str expr, Span<X> x, Span<Y> y) {
	if (x.len != y.len) {
		return CheckSpanEqFail_Len(sl, expr, x.len, y.len);
	}
	for (U64 i = 0; i < x.len; i++) {
		if (x[i] != y[i]) {
			return CheckSpanEqFail_Elem(sl, expr, i, MakeArg(x[i]), MakeArg(y[i]));
		}
	}
	return true;
}

using TestFn = void([[maybe_unused]] Mem::TempAllocator* testAllocator);

struct TestRegistrar {
	TestRegistrar(Str name, SrcLoc sl, TestFn* fn);
};

struct Subtest {
	struct Sig {
		Str    name = {};
		SrcLoc sl   = {};
	};
	Sig  sig       = {};
	Bool shouldRun = false;

	Subtest(Str name, SrcLoc sl);
	~Subtest();
};

#define TestDebuggerBreak ([]() { JC_DEBUGGER_BREAK; return false; }())

#define UnitTestImpl(name, fn, registrarVar) \
	static void fn([[maybe_unused]] JC::Mem::TempAllocator* testAllocator); \
	static UnitTest::TestRegistrar registrarVar = UnitTest::TestRegistrar(name, SrcLoc::Here(), fn); \
	static void fn([[maybe_unused]] JC::Mem::TempAllocator* testAllocator)

#define UnitTest(name) \
	UnitTestImpl(name, JC_MACRO_UNIQUE_NAME(UnitTestFn_), JC_MACRO_UNIQUE_NAME(UnitTestRegistrar_))

#define SubTestImpl(name, subtestVar) \
	if (UnitTest::Subtest subtestVar = UnitTest::Subtest(name, SrcLoc::Here()); subtestVar.shouldRun)

#define SubTest(name) SubTestImpl(name, JC_MACRO_UNIQUE_NAME(UnitSubtest_))

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