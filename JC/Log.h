#pragma once

#include "JC/Common.h"

namespace JC {

struct TempMem;

//--------------------------------------------------------------------------------------------------

enum struct LogCategory {
	Print,
	Error,
	Debug,
};

struct Log {
	virtual void VPrint(SrcLoc sl, LogCategory category, s8 fmt, Args args) = 0;
	virtual void PrintErr(Err* err, SrcLoc sl = SrcLoc::Here()) = 0;

	template <class... A> void Print(FmtStrSrcLoc<A...> fmtSl, A... args) { VPrint(fmtSl.sl, LogCategory::Print, fmtSl.fmt, MakeArgs(args...)); }
	template <class... A> void Error(FmtStrSrcLoc<A...> fmtSl, A... args) { VPrint(fmtSl.sl, LogCategory::Error, fmtSl.fmt, MakeArgs(args...)); }
	template <class... A> void Trace(FmtStrSrcLoc<A...> fmtSl, A... args) { VPrint(fmtSl.sl, LogCategory::Trace, fmtSl.fmt, MakeArgs(args...)); }
};

#define Logf(fmt, ...)   log->Print(fmt, ##__VA_ARGS__)
#define Errorf(fmt, ...) log->Error(fmt, ##__VA_ARGS__)
#define Tracef(fmt, ...) log->Trace(fmt, ##__VA_ARGS__)
#define LogErr(err)      log->PrintErr(err)

//--------------------------------------------------------------------------------------------------

using LogFn = void (const char* msg, u64 len);

struct LogApi {
	virtual void Init(TempMem* tempMem) = 0;
	virtual Log* GetLog()               = 0;
	virtual void AddFn(LogFn* fn)       = 0;
	virtual void RemoveFn(LogFn* fn)    = 0;
};

LogApi* GetLogApi();

//--------------------------------------------------------------------------------------------------

}	// namespace JC