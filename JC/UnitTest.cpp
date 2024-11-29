#include "JC/UnitTest.h"

#include "JC/Array.h"
#include "JC/Fmt.h"
#include "JC/Log.h"
#include "JC/Mem.h"
#include <stdio.h>

namespace JC {

//--------------------------------------------------------------------------------------------------

namespace UnitTest {
	static constexpr u32 MaxTests    = 1024;
	static constexpr u32 MaxSubtests = 1024;

	struct TestObj {
		s8      name = {};
		SrcLoc  sl   = {};
		TestFn* fn   = nullptr;
	};

	enum struct State { Run, Pop, Done };

	static LogApi*           logApi            = 0;
	static TestObj           tests[MaxTests]   = {};
	static u32               testsLen          = 0;
	static Subtest::Sig      cur[MaxSubtests]  = {};
	static u32               curLen            = 0;
	static Subtest::Sig      next[MaxSubtests] = {};
	static u32               nextLen           = 0;
	static Subtest::Sig      last[MaxSubtests] = {};
	static u32               lastLen           = 0;
	static State             state             = State::Done;
	static u32               checkFails        = 0;

	bool operator==(Subtest::Sig s1, Subtest::Sig s2) {
		// order by most likely fast fail
		return s1.sl.line == s2.sl.line && s1.name == s2.name && s1.sl.file == s2.sl.file;
	}

	TestRegistrar::TestRegistrar(s8 name, SrcLoc sl, TestFn* fn) {
		Assert(testsLen < MaxTests);
		tests[testsLen++] = {
			.name = name,
			.sl   = sl,
			.fn   = fn,
		};
	};

	Subtest::Subtest(s8 nameIn, SrcLoc slIn) {
		sig.name = nameIn;
		sig.sl   = slIn;
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
					--curLen;
					break;
				case State::Done:
					break;
			}
		}
	}

	bool Run(TempMem* tempMem, LogApi* logApiIn) {
		logApi = logApiIn;

		logApi->AddFn([](SrcLoc, LogCategory, const char* msg, u64 len) {
			fwrite(msg, 1, len, stdout);
			if (Sys::IsDebuggerPresent()) {
				Sys::DebuggerPrint(msg);
			}
		});

		u32 passedTests = 0;
		u32 failedTests = 0;
		for (u32 i = 0; i < testsLen; i++)  {
			nextLen = 0;
			do {
				u64 mark = tempMem->Mark();

				state      = State::Run;
				curLen     = 0;
				last[0]    = Subtest::Sig { .name = tests[i].name, .sl = tests[i].sl };
				lastLen    = 1;
				checkFails = 0;

				tests[i].fn(tempMem);

				Array<char> lastStr(tempMem);
				for (u32 j = 0; j < lastLen; j++) {
					lastStr.Add(last[j].name.data, last[j].name.len);
					lastStr.Add("::", 2);
				}
				lastStr.len -= 2;

				if (checkFails > 0) {
					Log("Failed: {}", s8(lastStr.data, lastStr.len));
					failedTests++;
				} else {
					Log("Passed: {}", s8(lastStr.data, lastStr.len));
					passedTests++;
				}

				tempMem->Reset(mark);
			} while (nextLen > 0);
		}

		Log("Total passed: {}", passedTests);
		Log("Total failed: {}", failedTests);
		return failedTests == 0;
	}

	bool CheckFailImpl(SrcLoc sl) {
		Log("***CHECK FAILED***");
		Log("{}({})", sl.file, sl.line);
		checkFails++;
		return false;
	}

	bool CheckExprFail(SrcLoc sl, s8 expr) {
		Log("***CHECK FAILED***");
		Log("{}({})", sl.file, sl.line);
		Log("  {}\n", expr);
		checkFails++;
		return false;
	}

	bool CheckRelFail(SrcLoc sl, s8 expr, Arg x, Arg y) {
		Log("***CHECK FAILED***");
		Log("{}({})", sl.file, sl.line);
		Log("  {}", expr);
		Log("  l: {}", x);
		Log("  r: {}\n", y);
		checkFails++;
		return false;
	}

	bool CheckSpanEqFail_Len(SrcLoc sl, s8 expr, u64 xLen, u64 yLen) {
		Log("***CHECK FAILED***");
		Log("{}({})", sl.file, sl.line);
		Log("  {}", expr);
		Log("  l len: {}", xLen);
		Log("  r len: {}\n", yLen);
		checkFails++;
		return false;
	}

	bool CheckSpanEqFail_Elem(SrcLoc sl, s8 expr, u64 i, Arg x, Arg y) {
		Log("***CHECK FAILED***");
		Log("{}({})", sl.file, sl.line);
		Log("  {}", expr);
		Log("  l[{}]: {}", i, x);
		Log("  r[{}]: {}\n", i, y);
		checkFails++;
		return false;
	}
}

//--------------------------------------------------------------------------------------------------

static u32 records[128];
static u32 recordsLen;

void Record(u32 u) {
	Assert(recordsLen < (sizeof(records) / sizeof(records[0])));
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
	CheckSpanEq(Span(records, recordsLen), Span<u32>({
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

}	// namespace JC