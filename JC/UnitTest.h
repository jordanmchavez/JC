#pragma once

#include "JC/Common.h"

#include "JC/Sys.h"

namespace JC {

//--------------------------------------------------------------------------------------------------

namespace UnitTest {
	bool Run(Mem mem);

	bool CheckFail(Mem scratch, s8 file, i32 line, s8 expr);
	bool CheckRelFail(Mem scratch, s8 file, i32 line, s8 expr, Arg x, Arg y);
	bool CheckSpanEqFail_Len(Mem scratch, s8 file, i32 line, s8 expr, u64 xLen, u64 yLen);
	bool CheckSpanEqFail_Elem(Mem scratch, s8 file, i32 line, s8 expr, u64 i, Arg x, Arg y);

	template <class X, class Y> bool CheckEq(Mem scratch, s8 file, i32 line, s8 expr, X x, Y y) {
		return (x == y) || CheckRelFail(scratch, file, line, expr, Arg::Make(x), Arg::Make(y));
	}

	template <class X, class Y> bool CheckNeq(Mem scratch, s8 file, i32 line, s8 expr, X x, Y y) {
		return (x != y) || CheckRelFail(scratch, file, line, expr, Arg::Make(x), Arg::Make(y));
	}

	template <class X, class Y> bool CheckSpanEq(Mem scratch, s8 file, i32 line, s8 expr, Span<X> x, Span<Y> y) {
		if (x.len != y.len) {
			return CheckSpanEqFail_Len(scratch, file, line, expr, x.len, y.len);
		}
		for (u64 i = 0; i < x.len; i++) {
			if (x[i] != y[i]) {
				return CheckSpanEqFail_Elem(scratch, file, line, expr, i, Arg::Make(x[i]), Arg::Make(y[i]));
			}
		}
		return true;
	}

	using TestFn = void (Mem scratch);

	struct TestRegistrar {
		TestRegistrar(s8 name, s8 file, i32 line, TestFn* fn);
	};

	struct Subtest {
		struct Sig {
			s8   name;
			s8   file;
			i32  line = 0;
		};
		Sig  sig;
		bool shouldRun = false;

		Subtest(s8 name, s8 file, i32 line);
		~Subtest();
	};
};

#define TestDebuggerBreak ([]() { Sys_DebuggerBreak(); return false; }())

#define UnitTestImpl(name, fn, registrarVar) \
	static void fn([[maybe_unused]] Mem scratch); \
	static UnitTest::TestRegistrar registrarVar = UnitTest::TestRegistrar(name, __FILE__, __LINE__, fn); \
	static void fn([[maybe_unused]] Mem scratch)

#define UnitTest(name) \
	UnitTestImpl(name, MacroName(UnitTestFn_), MacroName(UnitTestRegistrar_))

#define SubTestImpl(name, subtestVar) \
	if (UnitTest::Subtest subtestVar = UnitTest::Subtest(name, __FILE__, __LINE__); subtestVar.shouldRun)

#define SubTest(name) SubTestImpl(name, MacroName(UnitSubtest_))

#define CheckAt(file, line, x)          ((x) || UnitTest::CheckFail(scratch, file, line, #x) || TestDebuggerBreak)
#define CheckEqAt(file, line, x, y)     (UnitTest::CheckEq(scratch, file, line, #x " == " #y, (x), (y)) || TestDebuggerBreak)
#define CheckNeqAt(file, line, x, y)    (UnitTest::CheckNeq(scratch, file, line, #x " != " #y, (x), (y)) || TestDebuggerBreak)
#define Check(x)                        CheckAt(__FILE__, __LINE__, x)
#define CheckEq(x, y)                   CheckEqAt(__FILE__, __LINE__, x, y)
#define CheckNeq(x, y)                  CheckNeqAt(__FILE__, __LINE__, x, y)
#define CheckSpanEqAt(file, line, x, y) (UnitTest::CheckSpanEq(scratch, file, line, #x " == " #y, (x), (y)) || TestDebuggerBreak)
#define CheckSpanEq(x, y)               CheckSpanEqAt(__FILE__, __LINE__, x, y)

//--------------------------------------------------------------------------------------------------

}	// namespace JC