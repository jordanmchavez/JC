#pragma once

#include "JC/Common.h"

#include "JC/Sys.h"

namespace JC {

struct LogApi;
struct TempAllocatorApi;

//--------------------------------------------------------------------------------------------------

namespace UnitTest {
	void Run(LogApi* logApi, TempAllocatorApi* tempAllocatorApi);

	TempAllocator* GetTempAllocator();

	bool CheckFail(s8 file, i32 line, s8 expr);
	bool CheckRelFail(s8 file, i32 line, s8 expr, Arg x, Arg y);
	bool CheckSpanEqFail_Len(s8 file, i32 line, s8 expr, u64 xLen, u64 yLen);
	bool CheckSpanEqFail_Elem(s8 file, i32 line, s8 expr, u64 i, Arg x, Arg y);

	template <class X, class Y> bool CheckEq(s8 file, i32 line, s8 expr, X x, Y y) {
		return (x == y) || CheckRelFail(file, line, expr, Arg::Make(x), Arg::Make(y));
	}

	template <class X, class Y> bool CheckNeq(s8 file, i32 line, s8 expr, X x, Y y) {
		return (x != y) || CheckRelFail(file, line, expr, Arg::Make(x), Arg::Make(y));
	}

	template <class X, class Y> bool CheckSpanEq(s8 file, i32 line, s8 expr, Span<X> x, Span<Y> y) {
		if (x.len != y.len) {
			return CheckSpanEqFail_Len(file, line, expr, x.len, y.len);
		}
		for (u64 i = 0; i < x.len; i++) {
			if (x[i] != y[i]) {
				return CheckSpanEqFail_Elem(file, line, expr, i, Arg::Make(x[i]), Arg::Make(y[i]));
			}
		}
		return true;
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

#define CHECK_AT(file, line, x)        ((x) || JC::UnitTest::CheckFail(file, line, #x) || TEST_DEBUGGER_BREAK)
#define CHECK_EQ_AT(file, line, x, y)  (JC::UnitTest::CheckEq(file, line, #x " == " #y, (x), (y)) || TEST_DEBUGGER_BREAK)
#define CHECK_NEQ_AT(file, line, x, y) (JC::UnitTest::CheckNeq(file, line, #x " != " #y, (x), (y)) || TEST_DEBUGGER_BREAK)

#define CHECK(x)        CHECK_AT(__FILE__, __LINE__, x)
#define CHECK_EQ(x, y)  CHECK_EQ_AT(__FILE__, __LINE__, x, y)
#define CHECK_NEQ(x, y) CHECK_NEQ_AT(__FILE__, __LINE__, x, y)

#define CHECK_SPAN_EQ_AT(file, line, x, y) \
	(JC::UnitTest::CheckSpanEq(file, line, #x " == " #y, (x), (y)) || TEST_DEBUGGER_BREAK)

#define CHECK_SPAN_EQ(x, y) \
	CHECK_SPAN_EQ_AT(__FILE__, __LINE__, x, y)

//--------------------------------------------------------------------------------------------------

}	// namespace JC