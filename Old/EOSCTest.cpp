// Copyright Epic Games, Inc. All Rights Reserved.

#include "EOSCommon/Test.h"

#include "EOSCommon/Assert.h"
#include "EOSCommon/CmdLine.h"
#include "EOSCommon/Platform.h"
#include "EOSCommon/Res.h"
#include "EOSCommon/Str.h"
#include <cstdio>
#include <string>
#include <vector>
#include <unordered_map>

namespace EOSC::Test
{
	struct FTest
	{
		uint32_t	     Idx{0};
		std::string_view Name;
		FTestFn*	     Fn {nullptr};
		std::string_view File;
		int			     Line{0};
		uint32_t         FailedChecks{0};
	};

	enum struct EState
	{
		Run,
		Pop,
		Done,
	};

	void FTestPanicApi::Panic(
		const char*,    // FileName
		int,            // Line
		const char*,    // AssertCond
		const char*,    // Fmt
		FFmtArgs		// Args
	)
	{
		if (std::uncaught_exceptions() == 0)
		{
			throw FPanicEx();
		}
	};

	IDbgApi*          DbgApi{nullptr};
	IPanicApi*        PanicApi{nullptr};
	IPanicApi*        OldPanicApi{nullptr};
	FTestPanicApi     TestPanicApi;
	FCmdLine          CmdLine;
	EState            State{EState::Run};
	std::vector<FSig> Cur;
	std::vector<FSig> Next;
	std::vector<FSig> Last;
	uint32_t          Fails{0};

	// These must be singleton getters to avoid static initialization order fiasco
	inline std::vector<FTest>& GetTests()
	{
		static std::vector<FTest> Tests;
		return Tests;
	}

	bool operator==(const FSig& Sig1, const FSig& Sig2)
	{
		return Sig1.Name == Sig2.Name && Sig1.File == Sig2.File && Sig1.Line == Sig2.Line;
	}

	FSubtest::FSubtest(std::string_view Name, std::string_view File, int Line)
		: Sig { Name, File, Line }
	{
        switch (State)
        {
            case EState::Run:
                if (Next.size() <= Cur.size() || Next[Cur.size()] == Sig)
                {
                    Cur.emplace_back(Sig);
                    bRun = true;
                }
                break;
            case EState::Pop:
                Next = Cur;
                Next.emplace_back(Sig);
                State = EState::Done;
                break;
            case EState::Done:
				break;
        }
	}

	FSubtest::~FSubtest()
	{
        if (!bRun)
        {
            return;
        }

		Last.emplace_back(Sig);
        switch (State)
        {
            case EState::Run:
                Next.clear();
                State = EState::Pop;
                [[fallthrough]];
            case EState::Pop:
                Cur.pop_back();
                break;
            case EState::Done:
                break;
        }
	}

	FSubtest::operator bool() const
	{
		return bRun;
	}

	void VPrint(std::string_view Fmt, FFmtArgs Args)
	{
		const std::string Msg{VFormat(Fmt, Args)};
		printf("%s", Msg.c_str());
		if (DbgApi->IsPresent())
		{
			DbgApi->Print(Msg.c_str());
		}
	}

	int RunImpl()
	{
		const bool bVerbose = CmdLine.Param("test-verbose").has_value();

		TRes<> Res;
		for (const FTest& Test : GetTests())
		{
			Next.clear();
			do
			{
				Fails = 0;
				State = EState::Run;
				Cur.clear();
				Last.clear();
				Last.emplace_back(FSig { Test.Name, Test.File, Test.Line });
				Test.Fn();

				std::string LastStr;
				for (const FSig& Sig : Last)
				{
					LastStr += Sig.Name;
					LastStr += "::";
				}
				LastStr.pop_back();
				LastStr.pop_back();

				if (Fails > 0)
				{
					Print("Test failed: {}\n", LastStr);
				}
				else if (bVerbose)
				{
					Print("Test passed: {}\n", LastStr);

				}
			} while (!Next.empty());
		}

		return 0;
	}

	void Init()
	{
		DbgApi = CreateDbgApi();
		PanicApi = CreatePanicApi(DbgApi);
		GPanicApi = PanicApi;
	}

	void Shutdown()
	{
		DestroyDbgApi(DbgApi);
	}

	int Run(const char* const InCmdLine)
	{
		Init();
		if (TRes<> Res{CmdLine.Init(InCmdLine)}; !Res)
		{
			Print(Res.Err().LogStr());
			Shutdown();
			return 1;
		}
		const int RetCode{RunImpl()};
		Shutdown();
		return RetCode;
	}

	int Run(int Argc, const char* const* Argv)
	{
		Init();
		if (TRes<> Res{CmdLine.Init(Argc, Argv)}; !Res)
		{
			Print(Res.Err().LogStr());
			Shutdown();
			return 1;
		}
		const int RetCode{RunImpl()};
		Shutdown();
		return RetCode;
	}
	
	int Register(std::string_view Name, FTestFn* Fn, std::string_view File, int Line)
	{
		GetTests().emplace_back(FTest { static_cast<u32>(GetTests().size()), Name, Fn, File, Line });
		return 0;
	}

	void CheckFail()
	{
		if (DbgApi->IsPresent())
		{
			DbgApi->Break();
		}
		Fails++;
	}

	bool Check(bool Val, std::string_view Expr, std::string_view File, int Line)
	{
		if (!Val)
		{
			Print("{}({})\n", File, Line);
			Print("  Check failed\n");
			Print("  {}\n", Expr);
			CheckFail();
			return false;
		}
		return true;
	}

	void PrePanic()
	{
		OldPanicApi = GPanicApi;
		GPanicApi = &TestPanicApi;
	}

	void PostPanic()
	{
		GPanicApi = OldPanicApi;
	}

	void CheckPanicFail(std::string_view File, int Line)
	{
		Print("{}({})\n", File, Line);
		Print("  Check panic failed\n");
		CheckFail();
	}

}	// namespace EOSC::Test