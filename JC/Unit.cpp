#include "JC/Unit.h"

#include "JC/Array.h"
#include "JC/Fmt.h"
#include "JC/Log.h"
#include "JC/Mem.h"
#include "JC/Sys.h"

//--------------------------------------------------------------------------------------------------

static constexpr U32 Unit_MaxTests    = 1024;
static constexpr U32 Unit_MaxSubtests = 1024;

struct Unit_TestObj {
	Str          name;
	SrcLoc       sl;
	Unit_TestFn* testFn;
};

enum struct Unit_State { Run, Pop, Done };

static Unit_TestObj unit_tests[Unit_MaxTests];
static U32          unit_testsLen;
static Unit_Sig     unit_cur[Unit_MaxSubtests];
static U32          unit_curLen;
static Unit_Sig     unit_next[Unit_MaxSubtests];
static U32          unit_nextLen;
static Unit_Sig     unit_last[Unit_MaxSubtests];
static U32          unit_lastLen;
static Unit_State   unit_state;
static U32          unit_checkFails;

static Bool operator==(Unit_Sig s1, Unit_Sig s2) {
	// order by most likely fast fail
	return s1.sl.line == s2.sl.line && s1.name == s2.name && s1.sl.file == s2.sl.file;
}

Unit_TestRegistrar::Unit_TestRegistrar(Str name, SrcLoc sl, Unit_TestFn* testFn) {
	Assert(unit_testsLen < Unit_MaxTests);
	unit_tests[unit_testsLen++] = {
		.name   = name,
		.sl     = sl,
		.testFn = testFn,
	};
};

Unit_Subtest::Unit_Subtest(Str name, SrcLoc sl) {
	sig.name  = name;
	sig.sl    = sl;
	shouldRun = false;
	switch (unit_state) {
		case Unit_State::Run:
			if (unit_nextLen <= unit_curLen || unit_next[unit_curLen] == sig) {
				Assert(unit_curLen < Unit_MaxSubtests);
				unit_cur[unit_curLen++] = sig;
				shouldRun = true;
			}
			break;
		case Unit_State::Pop:
			memcpy(unit_next, unit_cur, sizeof(unit_cur[0]) * unit_curLen);
			unit_nextLen = unit_curLen;
			unit_next[unit_nextLen++] = sig;
			unit_state = Unit_State::Done;
			break;
		case Unit_State::Done:
			break;
	}
}

Unit_Subtest::~Unit_Subtest() {
	if (shouldRun) {
		Assert(unit_lastLen < Unit_MaxSubtests);
		unit_last[unit_lastLen++] = sig;
		switch (unit_state) {
			case Unit_State::Run:
				unit_nextLen = 0;
				unit_state = Unit_State::Pop;
				[[fallthrough]];
			case Unit_State::Pop:
				Assert(unit_curLen > 0);
				--unit_curLen;
				break;
			case Unit_State::Done:
				break;
		}
	}
}

Bool Unit_Run() {
	Mem* mem = Mem_Create((U64)8 * 1024 * 1024 * 1024);

	auto logFn = [](LogMsg const* msg) {
		Sys_Print(Str(msg->line, msg->lineLen));
		if (Sys_DbgPresent()) {
			Sys_DbgPrint(msg->line);
		}
	};
	Log_AddFn(logFn);

	U32 passedTests = 0;
	U32 failedTests = 0;
	for (U32 i = 0; i < unit_testsLen; i++)  {
		unit_nextLen = 0;
		do {
			unit_state      = Unit_State::Run;
			unit_curLen     = 0;
			unit_last[0]    = Unit_Sig { .name = unit_tests[i].name, .sl = unit_tests[i].sl };
			unit_lastLen    = 1;
			unit_checkFails = 0;

			unit_tests[i].testFn(mem);

			Array<char> buf(mem);
			if (unit_checkFails > 0) {
				Fmt_Printf(&buf, "Failed: ");
				failedTests++;
			} else {
				Fmt_Printf(&buf, "Passed: ");
				passedTests++;
			}
			for (U32 j = 0; j < unit_lastLen; j++) {
				Fmt_Printf(&buf, "%s::", unit_last[j].name);
			}
			buf.Remove(2);
			Logf("%s", Str(buf.data, (U32)buf.len));

			Mem_Reset(mem);
		} while (unit_nextLen > 0);
	}

	Logf("Total passed: %u", passedTests);
	Logf("Total failed: %u", failedTests);

	Log_RemoveFn(logFn);
	Mem_Destroy(mem);

	return failedTests == 0;
}

Bool Unit_CheckFailImpl(SrcLoc sl) {
	Logf("***CHECK FAILED***");
	Logf("%s(%u)", sl.file, sl.line);
	unit_checkFails++;
	return false;
}

Bool Unit_CheckExprFail(SrcLoc sl, Str expr) {
	Logf("***CHECK FAILED***");
	Logf("%s(%u)", sl.file, sl.line);
	Logf("  %s\n", expr);
	unit_checkFails++;
	return false;
}

Bool Unit_CheckRelFail(SrcLoc sl, Str expr, Arg x, Arg y) {
	Logf("***CHECK FAILED***");
	Logf("%s(%u)", sl.file, sl.line);
	Logf("  %s", expr);
	Logf("  l: %a", x);
	Logf("  r: %a\n", y);
	unit_checkFails++;
	return false;
}

Bool Unit_CheckSpanEqFail_Len(SrcLoc sl, Str expr, U64 xLen, U64 yLen) {
	Logf("***CHECK FAILED***");
	Logf("%s(%u)", sl.file, sl.line);
	Logf("  %s", expr);
	Logf("  l len: %u", xLen);
	Logf("  r len: %u\n", yLen);
	unit_checkFails++;
	return false;
}

Bool Unit_CheckSpanEqFail_Elem(SrcLoc sl, Str expr, U64 i, Arg x, Arg y) {
	Logf("***CHECK FAILED***");
	Logf("%s(%u)", sl.file, sl.line);
	Logf("  %s", expr);
	Logf("  l[%u]: %a", i, x);
	Logf("  r[%u]: %a\n", i, y);
	unit_checkFails++;
	return false;
}

//--------------------------------------------------------------------------------------------------

static U32 records[128];
static U32 recordsLen;

void Record(U32 u) {
	Assert(recordsLen < (sizeof(records) / sizeof(records[0])));
	records[recordsLen++] = u;
}

Unit_Test("UnitTest") {
	Record(0);
	Unit_SubTest("1") {
		Record(1);
		Unit_SubTest("2") {
			Record(2);
			Unit_SubTest("3") { Record(3); }
			Unit_SubTest("4") { Record(4); }
			Unit_SubTest("5") { Record(5); }
			Unit_SubTest("6") { Record(6); }
		}
		Unit_SubTest("7") {
			Record(7);
			Unit_SubTest("8") { Record(8); }
			Unit_SubTest("9") { Record(9); }
		}
	}
	Unit_SubTest("10") { Record(10); }
}

Unit_Test("Test.Verify subtest recording")
{
	Unit_CheckSpanEq(Span(records, recordsLen), Span<U32>({
		0, 1, 2, 3,
		0, 1, 2, 4,
		0, 1, 2, 5,
		0, 1, 2, 6,
		0, 1, 7, 8,
		0, 1, 7, 9,
		0, 10,
	}));

}