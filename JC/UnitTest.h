#pragma once

#include "JC/Common.h"

#include "JC/Sys.h"

namespace JC {

struct TempMem;

//--------------------------------------------------------------------------------------------------

namespace UnitTest {
	bool Run(TempMem* tempMem);

	bool CheckFail(TempMem* tempMem, s8 file, i32 line, s8 expr);
	bool CheckRelFail(TempMem* tempMem, s8 file, i32 line, s8 expr, Arg x, Arg y);
	bool CheckSpanEqFail_Len(TempMem* tempMem, s8 file, i32 line, s8 expr, u64 xLen, u64 yLen);
	bool CheckSpanEqFail_Elem(TempMem* tempMem, s8 file, i32 line, s8 expr, u64 i, Arg x, Arg y);

	template <class X, class Y> bool CheckEq(TempMem* tempMem, s8 file, i32 line, s8 expr, X x, Y y) {
		return (x == y) || CheckRelFail(tempMem, file, line, expr, Arg::Make(x), Arg::Make(y));
	}

	template <class X, class Y> bool CheckNeq(TempMem* tempMem, s8 file, i32 line, s8 expr, X x, Y y) {
		return (x != y) || CheckRelFail(tempMem, file, line, expr, Arg::Make(x), Arg::Make(y));
	}

	template <class X, class Y> bool CheckSpanEq(TempMem* tempMem, s8 file, i32 line, s8 expr, Span<X> x, Span<Y> y) {
		if (x.len != y.len) {
			return CheckSpanEqFail_Len(tempMem, file, line, expr, x.len, y.len);
		}
		for (u64 i = 0; i < x.len; i++) {
			if (x[i] != y[i]) {
				return CheckSpanEqFail_Elem(tempMem, file, line, expr, i, Arg::Make(x[i]), Arg::Make(y[i]));
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

#define CheckAt(file, line, x)          ((x) || UnitTest::CheckFail(tempMem, file, line, #x) || TestDebuggerBreak)
#define CheckEqAt(file, line, x, y)     (UnitTest::CheckEq(tempMem, file, line, #x " == " #y, (x), (y)) || TestDebuggerBreak)
#define CheckNeqAt(file, line, x, y)    (UnitTest::CheckNeq(tempMem, file, line, #x " != " #y, (x), (y)) || TestDebuggerBreak)
#define Check(x)                        CheckAt(__FILE__, __LINE__, x)
#define CheckEq(x, y)                   CheckEqAt(__FILE__, __LINE__, x, y)
#define CheckNeq(x, y)                  CheckNeqAt(__FILE__, __LINE__, x, y)
#define CheckSpanEqAt(file, line, x, y) (UnitTest::CheckSpanEq(tempMem, file, line, #x " == " #y, (x), (y)) || TestDebuggerBreak)
#define CheckSpanEq(x, y)               CheckSpanEqAt(__FILE__, __LINE__, x, y)

//--------------------------------------------------------------------------------------------------

}	// namespace JC