#pragma once

#include "JC/Common.h"

namespace JC {

//--------------------------------------------------------------------------------------------------

enum struct LogCategory {
	Log,
	Error,
};

using LogFn = void (const char* msg, u64 len);

struct Log {
	virtual void Init(Arena* temp) = 0;

	virtual void VPrint(SrcLoc sl, LogCategory category, s8 fmt, VArgs args) = 0;
	virtual void Error(Err err, SrcLoc sl = SrcLoc::Here()) = 0;

	template <class... A> void Print(FmtStrSrcLoc<A...> fmtSl, A... args) { VPrint(fmtSl.sl, LogCategory::Log, fmtSl.fmt, MakeVArgs(args...)); }
	template <class... A> void Error(FmtStrSrcLoc<A...> fmtSl, A... args) { VPrint(fmtSl.sl, LogCategory::Error, fmtSl.fmt, MakeVArgs(args...)); }

	virtual void AddFn(LogFn* fn) = 0;
	virtual void RemoveFn(LogFn* fn) = 0;
};

#define Logf(fmt, ...) log->Print(fmt, ##__VA_ARGS__)
#define Errorf(...)    log->Error(##__VA_ARGS__)

Log* GetLog();

//--------------------------------------------------------------------------------------------------

}	// namespace JC: