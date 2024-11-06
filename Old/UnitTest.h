#pragma once

#include "JC/Common.h"

namespace JC {

struct LogApi;
struct TempAllocatorApi;

//--------------------------------------------------------------------------------------------------

struct UnitTestApi {
	static UnitTestApi* Init(LogApi* logApi, TempAllocatorApi* tempAllocatorApi);

	virtual bool Run() = 0;
	virtual bool CheckFail(s8 file, i32 line, s8 expr) = 0;
	virtual bool CheckFail(s8 file, i32 line, s8 expr, Arg arg1, s8 arg1Expr, Arg arg2, s8 arg2Expr) = 0;
};

//--------------------------------------------------------------------------------------------------

using UnitTestFn = void (struct UnitTestApi* unitTestApi);

struct UnitTestRegistrar {
	UnitTestRegistrar(s8 name, UnitTestFn* fn);
};

#define UnitTest_Impl(name, fn, registrar) \
	static void fn(UnitTestApi* unitTestApi); \
	static JC::UnitTestRegistrar registrar(name, &fn); \
	static void fn(UnitTestApi* unitTestApi)

#define UnitTest_CatImpl(a, b) a##b
#define UnitTest_Cat(a, b) UnitTest_CatImpl(a, b)
#define UnitTest_UniqueVar(name) UnitTest_Cat(name, __LINE__)

#define UnitTest(name) UnitTest_Impl(name, UnitTest_UniqueVar(UnitTest_Fn), UnitTest_UniqueVar(UnitTest_Registrar))

//--------------------------------------------------------------------------------------------------

bool Debugger_IsPresent();

#define Check_Break []() { if (Debugger_IsPresent()) { Debugger_Break(); } return false; }()

#define Check_At(file, line, expr) ((expr) || unitTestApi->CheckFail(file, line, #expr) || Check_Break)
#define Check(expr) CHECK_AT(__FILE__, __LINE__, expr)

template <class X, class Y> bool CheckEqAt(UnitTestApi* unitTestApi, s8 file, i32 line, s8 expr, X x, s8 xExpr, Y y, s8 yExpr) {
	return (x == y) || unitTestApi->CheckFail(file, line, expr, Arg::Make(x), xExpr, Arg::Make(y), yExpr);
}

#define Check_EqAt(file, line, x, y) (CheckEqAt(unitTestApi, file, line, #x " == " #y, x, #x, y, #y) || Check_Break)
#define Check_Eq(x, y) Check_EqAt(__FILE__, __LINE__, x, y)

//--------------------------------------------------------------------------------------------------

}	// namespace JC