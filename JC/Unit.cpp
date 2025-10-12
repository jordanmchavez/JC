#include "JC/Unit.h"

#include "JC/Common_Assert.h"
#include "JC/Common_Fmt.h"
#include "JC/Log.h"
#include "JC/Sys.h"

namespace JC::Unit {

//--------------------------------------------------------------------------------------------------

static constexpr U32 MaxTests    = 1024;
static constexpr U32 MaxSubtests = 1024;

struct TestObj {
	Str     name;
	SrcLoc  sl;
	TestFn* testFn;
};

enum struct State { Run, Pop, Done };

static TestObj tests[MaxTests];
static U32     testsLen;
static Sig     cur[MaxSubtests];
static U32     curLen;
static Sig     next[MaxSubtests];
static U32     nextLen;
static Sig     last[MaxSubtests];
static U32     lastLen;
static State   state;
static U32     checkFails;

static bool operator==(Sig s1, Sig s2) {
	// order by most likely fast fail
	return s1.sl.line == s2.sl.line && s1.name == s2.name && s1.sl.file == s2.sl.file;
}

TestRegistrar::TestRegistrar(Str name, SrcLoc sl, TestFn* testFn) {
	Assert(testsLen < MaxTests);
	tests[testsLen++] = {
		.name   = name,
		.sl     = sl,
		.testFn = testFn,
	};
};

Subtest::Subtest(Str name, SrcLoc sl) {
	sig.name  = name;
	sig.sl    = sl;
	shouldRun = false;
	switch (state) {
		case State::Run:
			if (nextLen <= curLen || next[curLen] == sig) {
				Assert(curLen < MaxSubtests);
				cur[curLen++] = sig;
				shouldRun = true;
			}
			break;
		case State::Pop:
			memcpy(next, cur, sizeof(cur[0]) * curLen);
			nextLen = curLen;
			next[nextLen++] = sig;
			state = State::Done;
			break;
		case State::Done:
			break;
	}
}

Subtest::~Subtest() {
	if (shouldRun) {
		Assert(lastLen < MaxSubtests);
		last[lastLen++] = sig;
		switch (state) {
			case State::Run:
				nextLen = 0;
				state = State::Pop;
				[[fallthrough]];
			case State::Pop:
				Assert(curLen > 0);
				--curLen;
				break;
			case State::Done:
				break;
		}
	}
}

bool Run() {
	Mem::Mem permMem = Mem::Create(16 * GB);
	Mem::Mem tempMem = Mem::Create(16 * GB);

	Log::Init(permMem, tempMem);

	auto logFn = [](Log::Msg const* msg) {
		Sys::Print(Str(msg->line, msg->lineLen));
		if (Sys::DbgPresent()) {
			Sys::DbgPrint(msg->line);
		}
	};
	Log::AddFn(logFn);

	U32 passedTests = 0;
	U32 failedTests = 0;
	for (U32 i = 0; i < testsLen; i++)  {
		nextLen = 0;
		do {
			state      = State::Run;
			curLen     = 0;
			last[0]    = Sig { .name = tests[i].name, .sl = tests[i].sl };
			lastLen    = 1;
			checkFails = 0;

			tests[i].testFn(tempMem);

			Fmt::PrintBuf pb(tempMem);
			if (checkFails > 0) {
				pb.Printf("Failed: ");
				failedTests++;
			} else {
				pb.Printf("Passed: ");
				passedTests++;
			}
			for (U32 j = 0; j < lastLen; j++) {
				pb.Printf("%s::", last[j].name);
			}
			pb.Remove(2);
			Logf("%s", pb.ToStr());

			Mem::Reset(tempMem, 0);
		} while (nextLen > 0);
	}

	Logf("Total passed: %u", passedTests);
	Logf("Total failed: %u", failedTests);

	Log::RemoveFn(logFn);

	return failedTests == 0;
}

bool CheckFailImpl(SrcLoc sl) {
	Logf("***CHECK FAILED***");
	Logf("%s(%u)", sl.file, sl.line);
	checkFails++;
	return false;
}

bool CheckExprFail(SrcLoc sl, Str expr) {
	Logf("***CHECK FAILED***");
	Logf("%s(%u)", sl.file, sl.line);
	Logf("  %s\n", expr);
	checkFails++;
	return false;
}

bool CheckRelFail(SrcLoc sl, Str expr, Arg::Arg x, Arg::Arg y) {
	Logf("***CHECK FAILED***");
	Logf("%s(%u)", sl.file, sl.line);
	Logf("  %s", expr);
	Logf("  l: %a", x);
	Logf("  r: %a\n", y);
	checkFails++;
	return false;
}

bool CheckSpanEqFail_Len(SrcLoc sl, Str expr, U64 xLen, U64 yLen) {
	Logf("***CHECK FAILED***");
	Logf("%s(%u)", sl.file, sl.line);
	Logf("  %s", expr);
	Logf("  l len: %u", xLen);
	Logf("  r len: %u\n", yLen);
	checkFails++;
	return false;
}

bool CheckSpanEqFail_Elem(SrcLoc sl, Str expr, U64 i, Arg::Arg x, Arg::Arg y) {
	Logf("***CHECK FAILED***");
	Logf("%s(%u)", sl.file, sl.line);
	Logf("  %s", expr);
	Logf("  l[%u]: %a", i, x);
	Logf("  r[%u]: %a\n", i, y);
	checkFails++;
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

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Unit