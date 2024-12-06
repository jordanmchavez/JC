#pragma once

#include "JC/Common.h"

namespace JC {

struct TempMem;

//--------------------------------------------------------------------------------------------------

enum struct LogCategory {
	Log,
	Error,
};

struct Log {
	virtual void VPrint(SrcLoc sl, LogCategory category, s8 fmt, Args args) = 0;
	virtual void Error(Err* err, SrcLoc sl = SrcLoc::Here()) = 0;

	template <class... A> void Print(FmtStrSrcLoc<A...> fmtSl, A... args) { VPrint(fmtSl.sl, LogCategory::Log, fmtSl.fmt, MakeArgs(args...)); }
	template <class... A> void Error(FmtStrSrcLoc<A...> fmtSl, A... args) { VPrint(fmtSl.sl, LogCategory::Error, fmtSl.fmt, MakeArgs(args...)); }
};

#define Logf(fmt, ...) log->Print(fmt, ##__VA_ARGS__)
#define Errorf(...)    log->Error(##__VA_ARGS__)

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