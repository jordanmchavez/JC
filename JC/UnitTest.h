#pragma once

#include "JC/Common.h"

#include "JC/Sys.h"

namespace JC {

struct LogApi;
struct TempMem;

//--------------------------------------------------------------------------------------------------

namespace UnitTest {
	bool Run(TempMem* tempMem, LogApi* logApi);

	bool CheckFailImpl(SrcLoc sl);
	bool CheckExprFail(SrcLoc sl, s8 expr);
	bool CheckRelFail(SrcLoc sl, s8 expr, Arg x, Arg y);
	bool CheckSpanEqFail_Len(SrcLoc sl, s8 expr, u64 xLen, u64 yLen);
	bool CheckSpanEqFail_Elem(SrcLoc sl, s8 expr, u64 i, Arg x, Arg y);

	template <class X, class Y> bool CheckEq(SrcLoc sl, s8 expr, X x, Y y) {
		return (x == y) || CheckRelFail(sl, expr, Arg::Make(x), Arg::Make(y));
	}

	template <class X, class Y> bool CheckNeq(SrcLoc sl, s8 expr, X x, Y y) {
		return (x != y) || CheckRelFail(sl, expr, Arg::Make(x), Arg::Make(y));
	}

	template <class X, class Y> bool CheckSpanEq(SrcLoc sl, s8 expr, Span<X> x, Span<Y> y) {
		if (x.len != y.len) {
			return CheckSpanEqFail_Len(sl, expr, x.len, y.len);
		}
		for (u64 i = 0; i < x.len; i++) {
			if (x[i] != y[i]) {
				return CheckSpanEqFail_Elem(sl, expr, i, Arg::Make(x[i]), Arg::Make(y[i]));
			}
		}
		return true;
	}

	using TestFn = void (TempMem* tempMem);

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
};

#define TestDebuggerBreak ([]() { Sys_DebuggerBreak(); return false; }())

#define UnitTestImpl(name, fn, registrarVar) \
	static void fn([[maybe_unused]] TempMem* tempMem); \
	static UnitTest::TestRegistrar registrarVar = UnitTest::TestRegistrar(name, SrcHere, fn); \
	static void fn([[maybe_unused]] TempMem* tempMem)

#define UnitTest(name) \
	UnitTestImpl(name, MacroName(UnitTestFn_), MacroName(UnitTestRegistrar_))

#define SubTestImpl(name, subtestVar) \
	if (UnitTest::Subtest subtestVar = UnitTest::Subtest(name, SrcHere); subtestVar.shouldRun)

#define SubTest(name) SubTestImpl(name, MacroName(UnitSubtest_))

#define CheckFailAt(sl)         (UnitTest::CheckFailImpl(sl) || TestDebuggerBreak)
#define CheckFail()             (UnitTest::CheckFailImpl(SrcHere) || TestDebuggerBreak)
#define CheckTrueAt(sl, x)      ((x) || UnitTest::CheckExprFail(sl, #x) || TestDebuggerBreak)
#define CheckTrue(x)            CheckTrueAt(SrcHere, x)
#define CheckFalseAt(sl, x)     (!(x) || UnitTest::CheckExprFail(sl, "NOT " #x) || TestDebuggerBreak)
#define CheckFalse(x)           CheckFalseAt(SrcHere, x)
#define CheckEqAt(sl, x, y)     (UnitTest::CheckEq(sl, #x " == " #y, (x), (y)) || TestDebuggerBreak)
#define CheckEq(x, y)           CheckEqAt(SrcHere, x, y)
#define CheckNeqAt(sl, x, y)    (UnitTest::CheckNeq(sl, #x " != " #y, (x), (y)) || TestDebuggerBreak)
#define CheckNeq(x, y)          CheckNeqAt(SrcHere, x, y)
#define CheckSpanEqAt(sl, x, y) (UnitTest::CheckSpanEq(sl, #x " == " #y, (x), (y)) || TestDebuggerBreak)
#define CheckSpanEq(x, y)       CheckSpanEqAt(SrcHere, x, y)

//--------------------------------------------------------------------------------------------------

}	// namespace JC