#include "JC/UnitTest.h"

#include "JC/Allocator.h"
#include "JC/Array.h"
#include "JC/Fmt.h"
#include "JC/Log.h"
#include <stdio.h>

namespace JC {

//--------------------------------------------------------------------------------------------------

namespace UnitTest {
	static constexpr u32 MaxTests    = 1024;
	static constexpr u32 MaxSubtests = 1024;

	struct TestObj {
		s8      name;
		s8      file;
		i32     line = 0;
		TestFn* fn = nullptr;
	};

	enum struct State { Run, Pop, Done };

	static LogApi*           logApi;
	static TempAllocatorApi* tempAllocatorApi;
	static TempAllocator*    tempAllocator;
	static TestObj           tests[MaxTests];
	static u32               testsLen;
	static Subtest::Sig      cur[MaxSubtests];
	static u32               curLen;
	static Subtest::Sig      next[MaxSubtests];
	static u32               nextLen;
	static Subtest::Sig      last[MaxSubtests];
	static u32               lastLen;
	static State             state;
	static u32               checkFails;

	bool operator==(Subtest::Sig s1, Subtest::Sig s2) {
		// order by most likely fast fail
		return s1.line == s2.line && s1.name == s2.name && s1.file == s2.file;
	}

	TestRegistrar::TestRegistrar(s8 name, s8 file, i32 line, TestFn* fn) {
		JC_ASSERT(testsLen < MaxTests);
		tests[testsLen++] = {
			.name         = name,
			.file         = file,
			.line         = line,
			.fn           = fn,
		};
	};

	Subtest::Subtest(s8 nameIn, s8 fileIn, i32 lineIn) {
		sig.name = nameIn;
		sig.file = fileIn;
		sig.line = lineIn;
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

	bool Run(LogApi* inLogApi, TempAllocatorApi* inTempAllocatorApi) {
		logApi = inLogApi;
		tempAllocatorApi = inTempAllocatorApi;

		logApi->AddFn([](s8, i32, LogCategory, s8 msg) {
			fwrite(msg.data, 1, msg.len, stdout);
			if (Sys::IsDebuggerPresent()) {
				Sys::DebuggerPrint(tempAllocator, msg);
			}
		});

		u32 passedTests = 0;
		u32 failedTests = 0;
		for (u32 i = 0; i < testsLen; i++)  {
			tempAllocator = tempAllocatorApi->Create();
			nextLen = 0;
			do {
				state      = State::Run;
				curLen     = 0;
				last[0]    = Subtest::Sig { .name = tests[i].name, .file = tests[i].file, .line = tests[i].line };
				lastLen    = 1;
				checkFails = 0;

				tests[i].fn();

				Array<char> lastStr;
				lastStr.Init(tempAllocator);
				for (u32 j = 0; j < lastLen; j++) {
					lastStr.Add(last[j].name.data, last[j].name.len);
					lastStr.Add(':');
				}
				lastStr.len--;

				if (checkFails > 0) {
					JC_LOG("Failed: {}", s8(lastStr.data, lastStr.len));
					failedTests++;
				} else {
					JC_LOG("Passed: {}", s8(lastStr.data, lastStr.len));
					passedTests++;
				}
			} while (nextLen > 0);
			tempAllocatorApi->Destroy(tempAllocator);
		}

		JC_LOG("Total passed: {}", passedTests);
		JC_LOG("Total failed: {}", failedTests);
		return failedTests == 0;
	}

	TempAllocator* GetTempAllocator() {
		return tempAllocator;
	}

	bool CheckFail(s8 file, i32 line, s8 expr) {
		JC_LOG("***CHECK FAILED***");
		JC_LOG("{}({})", file, line);
		JC_LOG("  {}\n", expr);
		checkFails++;
		return false;
	}

	bool CheckRelFail(s8 file, i32 line, s8 expr, Arg x, Arg y) {
		JC_LOG("***CHECK FAILED***");
		JC_LOG("{}({})", file, line);
		JC_LOG("  {}", expr);
		JC_LOG("  l: {}", x);
		JC_LOG("  r: {}\n", y);
		checkFails++;
		return false;
	}

	bool CheckSpanEqFail_Len(s8 file, i32 line, s8 expr, u64 xLen, u64 yLen) {
		JC_LOG("***CHECK FAILED***");
		JC_LOG("{}({})", file, line);
		JC_LOG("  {}", expr);
		JC_LOG("  l len: {}", xLen);
		JC_LOG("  r len: {}\n", yLen);
		checkFails++;
		return false;
	}

	bool CheckSpanEqFail_Elem(s8 file, i32 line, s8 expr, u64 i, Arg x, Arg y) {
		JC_LOG("***CHECK FAILED***");
		JC_LOG("{}({})", file, line);
		JC_LOG("  {}", expr);
		JC_LOG("  l[{}]: {}", i, x);
		JC_LOG("  r[{}]: {}\n", i, y);
		checkFails++;
		return false;
	}
}

//--------------------------------------------------------------------------------------------------

static u32 records[128];
static u32 recordsLen;

void Record(u32 u) {
	JC_ASSERT(recordsLen < (sizeof(records) / sizeof(records[0])));
	records[recordsLen++] = u;
}

TEST("UnitTest") {
	Record(0);
	SUBTEST("1") {
		Record(1);
		SUBTEST("2") {
			Record(2);
			SUBTEST("3") { Record(3); }
			SUBTEST("4") { Record(4); }
			SUBTEST("5") { Record(5); }
			SUBTEST("6") { Record(6); }
		}
		SUBTEST("7") {
			Record(7);
			SUBTEST("8") { Record(8); }
			SUBTEST("9") { Record(9); }
		}
	}
	SUBTEST("10") { Record(10); }
}

TEST("Test.Verify subtest recording")
{
	CHECK_SPAN_EQ(Span(records, recordsLen), Span<u32>({
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