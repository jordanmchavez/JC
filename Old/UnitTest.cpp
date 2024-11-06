#include "JC/Allocator.h"
#include "JC/Err.h"
#include "JC/Log.h"
#include "JC/UnitTest.h"

namespace JC {

//--------------------------------------------------------------------------------------------------

struct UnitTest {
	s8          name;
	UnitTestFn* fn;
};

struct UnitTestApiImpl : UnitTestApi {
	static constexpr u32 MaxUnitTests = 1024;

	LogApi*           logApi;
	TempAllocatorApi* tempAllocatorApi;
	Allocator*        allocator;
	UnitTest          unitTests[MaxUnitTests];
	u32               unitTestsLen;
	s8                curTestName;
	u32               numFailedChecks;

	void Init(LogApi* inLogApi, TempAllocatorApi* inTempAllocatorApi) {
		logApi           = inLogApi;
		tempAllocatorApi = inTempAllocatorApi;
		allocator        = tempAllocatorApi->Get();
		unitTestsLen     = 0;
	}

	bool Run() override {
		numFailedChecks = 0;
		for (u32 i = 0; i < unitTestsLen; i++) {
			curTestName = unitTests[i].name;
			unitTests[i].fn(this);
		}
		return numFailedChecks == 0;
	}

	void Register(s8 name, UnitTestFn* fn) {
		Assert(unitTestsLen < MaxUnitTests);
		unitTests[unitTestsLen++] = { .name = name, .fn = fn };
	}

	bool CheckFail(s8 file, i32 line, s8 expr) override {
		Log_Err("CHECK FAILED in test {} at {}({}): {}", curTestName, file, line, expr);
		numFailedChecks++;
		return false;
	}

	bool CheckFail(s8 file, i32 line, s8 expr, Arg arg1, s8 arg1Expr, Arg arg2, s8 arg2Expr) override {
		Log_Err("CHECK FAILED in test {} at {}({}): {}", curTestName, file, line, expr);
		Log_Err("  {} == {}", arg1Expr, arg1);
		Log_Err("  {} == {}", arg2Expr, arg2);
		numFailedChecks++;
		return false;
	}
};

static UnitTestApiImpl unitTestApiImpl;

UnitTestApi* UnitTestApi::Init(LogApi* logApi, TempAllocatorApi* tempAllocatorApi) {
	unitTestApiImpl.Init(logApi, tempAllocatorApi);
	return &unitTestApiImpl;
}

UnitTestRegistrar::UnitTestRegistrar(s8 name, UnitTestFn* fn) {
	unitTestApiImpl.Register(name, fn);
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC