#include "JC/UnitTest.h"

#include "JC/Array.h"
#include "JC/Log.h"

namespace JC::UnitTest {

//--------------------------------------------------------------------------------------------------

static constexpr U32 MaxTests    = 1024;
static constexpr U32 MaxSubtests = 1024;

struct TestObj {
	Str      name = {};
	SrcLoc  sl   = {};
	TestFn* fn   = nullptr;
};

enum struct State { Run, Pop, Done };

static TempAllocator* tempAllocator;
static Log::Logger*   logger;
static TestObj        tests[MaxTests];
static U32            testsLen;
static Subtest::Sig   cur[MaxSubtests];
static U32            curLen;
static Subtest::Sig   next[MaxSubtests];
static U32            nextLen;
static Subtest::Sig   last[MaxSubtests];
static U32            lastLen;
static State          state;
static U32            checkFails;

Bool operator==(Subtest::Sig s1, Subtest::Sig s2) {
	// order by most likely fast fail
	return s1.sl.line == s2.sl.line && s1.name == s2.name && s1.sl.file == s2.sl.file;
}

TestRegistrar::TestRegistrar(Str name, SrcLoc sl, TestFn* fn) {
	JC_ASSERT(testsLen < MaxTests);
	tests[testsLen++] = {
		.name = name,
		.sl   = sl,
		.fn   = fn,
	};
};

Subtest::Subtest(Str nameIn, SrcLoc slIn) {
	sig.name = nameIn;
	sig.sl   = slIn;
	switch (state) {
		case State::Run:
			if (nextLen <= curLen || next[curLen] == sig) {
				JC_ASSERT(curLen < MaxSubtests);
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
		JC_ASSERT(lastLen < MaxSubtests);
		last[lastLen++] = sig;
		switch (state) {
			case State::Run:
				nextLen = 0;
				state = State::Pop;
				[[fallthrough]];
			case State::Pop:
				--curLen;
				break;
			case State::Done:
				break;
		}
	}
}

Bool Run(TempAllocator* tempAllocatorIn, Log::Logger* loggerIn) {
	tempAllocator = tempAllocatorIn;
	logger        = loggerIn;

	U32 passedTests = 0;
	U32 failedTests = 0;
	for (U32 i = 0; i < testsLen; i++)  {
		nextLen = 0;
		do {
			state      = State::Run;
			curLen     = 0;
			last[0]    = Subtest::Sig { .name = tests[i].name, .sl = tests[i].sl };
			lastLen    = 1;
			checkFails = 0;

			tests[i].fn(tempAllocator);

			Array<char> lastStr(tempAllocator);
			for (U32 j = 0; j < lastLen; j++) {
				lastStr.Add(last[j].name.data, last[j].name.len);
				lastStr.Add("::", 2);
			}
			lastStr.len -= 2;

			if (checkFails > 0) {
				JC_LOG("Failed: {}", Str(lastStr.data, lastStr.len));
				failedTests++;
			} else {
				JC_LOG("Passed: {}", Str(lastStr.data, lastStr.len));
				passedTests++;
			}

			tempAllocator->Reset();

		} while (nextLen > 0);
	}

	JC_LOG("Total passed: {}", passedTests);
	JC_LOG("Total failed: {}", failedTests);
	return failedTests == 0;
}

Bool CheckFailImpl(SrcLoc sl) {
	JC_LOG("***CHECK FAILED***");
	JC_LOG("{}({})", sl.file, sl.line);
	checkFails++;
	return false;
}

Bool CheckExprFail(SrcLoc sl, Str expr) {
	JC_LOG("***CHECK FAILED***");
	JC_LOG("{}({})", sl.file, sl.line);
	JC_LOG("  {}\n", expr);
	checkFails++;
	return false;
}

Bool CheckRelFail(SrcLoc sl, Str expr, Arg x, Arg y) {
	JC_LOG("***CHECK FAILED***");
	JC_LOG("{}({})", sl.file, sl.line);
	JC_LOG("  {}", expr);
	JC_LOG("  l: {}", x);
	JC_LOG("  r: {}\n", y);
	checkFails++;
	return false;
}

Bool CheckSpanEqFail_Len(SrcLoc sl, Str expr, U64 xLen, U64 yLen) {
	JC_LOG("***CHECK FAILED***");
	JC_LOG("{}({})", sl.file, sl.line);
	JC_LOG("  {}", expr);
	JC_LOG("  l len: {}", xLen);
	JC_LOG("  r len: {}\n", yLen);
	checkFails++;
	return false;
}

Bool CheckSpanEqFail_Elem(SrcLoc sl, Str expr, U64 i, Arg x, Arg y) {
	JC_LOG("***CHECK FAILED***");
	JC_LOG("{}({})", sl.file, sl.line);
	JC_LOG("  {}", expr);
	JC_LOG("  l[{}]: {}", i, x);
	JC_LOG("  r[{}]: {}\n", i, y);
	checkFails++;
	return false;
}

//--------------------------------------------------------------------------------------------------

static U32 records[128];
static U32 recordsLen;

void Record(U32 u) {
	JC_ASSERT(recordsLen < (sizeof(records) / sizeof(records[0])));
	records[recordsLen++] = u;
}

UnitTest("UnitTest") {
	Record(0);
	SubTest("1") {
		Record(1);
		SubTest("2") {
			Record(2);
			SubTest("3") { Record(3); }
			SubTest("4") { Record(4); }
			SubTest("5") { Record(5); }
			SubTest("6") { Record(6); }
		}
		SubTest("7") {
			Record(7);
			SubTest("8") { Record(8); }
			SubTest("9") { Record(9); }
		}
	}
	SubTest("10") { Record(10); }
}

UnitTest("Test.Verify subtest recording")
{
	CheckSpanEq(Span(records, recordsLen), Span<U32>({
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

}	// namespace JC::UnitTest