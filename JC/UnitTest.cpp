#include "JC/UnitTest.h"

#include "JC/Array.h"
#include "JC/Fmt.h"
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

	static TempAllocator* tempAllocator = nullptr;
	static TestObj        tests[MaxTests];
	static u32            testsLen = 0;
	static Subtest::Sig   cur[MaxSubtests];
	static u32            curLen;
	static Subtest::Sig   next[MaxSubtests];
	static u32            nextLen;
	static Subtest::Sig   last[MaxSubtests];
	static u32            lastLen;
	static State          state = State::Run;
	static u32            checkFails = 0;

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

	Subtest::Subtest(s8 inName, s8 inFile, i32 inLine) {
		sig.name = inName;
		sig.file = inFile;
		sig.line = inLine;
		switch (state) {
			case State::Run:
				if (nextLen <= curLen || next[curLen] == sig) {
					JC_ASSERT(curLen < MaxSubtests);
					cur[curLen++] = sig;
					shouldRun = true;
				}
				break;
			case State::Pop:
				memcpy(cur, next, sizeof(next[0]) * nextLen);
				curLen = nextLen;
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

	void VPrint(s8 fmt, Args args) {
		const s8 msg = VFmt(tempAllocator, fmt, args);
		fwrite(msg.data, 1, msg.len, stdout);
		if (Sys::IsDebuggerPresent()) {
			Sys::DebuggerPrint(tempAllocator, msg);
		}
	}

	template <class... A> void Print(FmtStr<A...> fmt, A... args) {
		VPrint(fmt, Args::Make(args...));
	}

	void Run(TempAllocator* inTempAllocator) {
		tempAllocator = inTempAllocator;

		for (u32 i = 0; i < testsLen; i++)  {
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
					Print("Failed: {}\n", s8(lastStr.data, lastStr.len));
				} else {
					Print("Passed: {}asdfaf\n", s8(lastStr.data, lastStr.len));
				}
			} while (nextLen > 0);
		}
	}

	bool CheckFail(s8 file, i32 line, s8 expr) {
		Print("***CHECK FAILED***\n");
		Print("{}({})\n", file, line);
		Print("{}\n\n", expr);
		checkFails++;
		return false;
	}

	bool CheckFail(s8 file, i32 line, s8 expr, Arg x, Arg y) {
		Print("***CHECK FAILED***\n");
		Print("{}({})\n", file, line);
		Print("{}\n", expr);
		Print("left:  {}\n", x);
		Print("right: {}\n\n", y);
		checkFails++;
		return false;
	}

}

//--------------------------------------------------------------------------------------------------

static i32 runRecord[128];
static i32 runRecordLen;

TEST("UnitTest")
{
	runRecord[runRecordLen++] = 0;
	SUBTEST("1")
	{
		runRecord[runRecordLen++] = 1;
		SUBTEST("2")
		{
			runRecord[runRecordLen++] = 2;
			SUBTEST("3")
			{
				runRecord[runRecordLen++] = 3;
			}
			SUBTEST("4")
			{
				runRecord[runRecordLen++] = 4;
			}
			SUBTEST("5")
			{
				runRecord[runRecordLen++] = 5;
			}
			SUBTEST("6")
			{
				runRecord[runRecordLen++] = 6;
			}
		}
		SUBTEST("7")
		{
			runRecord[runRecordLen++] = 7;
			SUBTEST("8") 
			{
				runRecord[runRecordLen++] = 8;
			}
			SUBTEST("9")
			{
				runRecord[runRecordLen++] = 9;
			}
		}
	}
	SUBTEST("10")
	{
		runRecord[runRecordLen++] = 10;
	}
}

TEST("Test.Verify subtest recording")
{
	CHECK_EQ(runRecord[0], 0);
		0, 1, 2, 3,
		0, 1, 2, 4,
		0, 1, 2, 5,
		0, 1, 2, 6,
		0, 1, 7, 8,
		0, 1, 7, 9,
		0, 10;
}






}	// namespace JC