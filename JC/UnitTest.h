#pragma once

#include "JC/Common.h"

#include "JC/Sys.h"

namespace JC {

//--------------------------------------------------------------------------------------------------

namespace UnitTest {
	void Run(TempAllocator* tempAllocator);

	bool CheckFail(s8 file, i32 line, s8 expr);
	bool CheckFail(s8 file, i32 line, s8 expr, Arg x, Arg y);

	template <class X, class Y> bool CheckEqAt(s8 file, i32 line, s8 expr, X x, Y y) {
		return (x == y) || CheckFail(file, line, expr, Arg::Make(x), Arg::Make(y));
	}

	using TestFn = void ();

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

#define TEST_DEBUGGER_BREAK ([]() { JC_DEBUGGER_BREAK; return false; }())

#define TEST_IMPL(name, fn, registrarVar) \
	static void fn(); \
	static JC::UnitTest::TestRegistrar registrarVar = UnitTest::TestRegistrar(name, __FILE__, __LINE__, fn); \
	static void fn()

#define TEST(name) \
	TEST_IMPL(name, JC_MACRO_NAME(UnitTestFn_), JC_MACRO_NAME(UnitTestRegistrar_))

#define SUBTEST_IMPL(name, subtestVar) \
	if (JC::UnitTest::Subtest subtestVar = JC::UnitTest::Subtest(name, __FILE__, __LINE__); subtestVar.shouldRun)
	
#define SUBTEST(name) SUBTEST_IMPL(name, JC_MACRO_NAME(UnitSubtest_))

#define CHECK_AT(file, line, x) \
	((x) || JC::UnitTest::CheckFail(file, line, #x) || TEST_DEBUGGER_BREAK)

#define CHECK_EQ_AT(file, line, x, y) \
	(JC::UnitTest::CheckEqAt(file, line, #x " == " #y, (x), (y)) || TEST_DEBUGGER_BREAK)

#define CHECK_EQ(x, y) \
	CHECK_EQ_AT(__FILE__, __LINE__, x, y)

//--------------------------------------------------------------------------------------------------

}	// namespace JC