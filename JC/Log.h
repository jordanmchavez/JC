#pragma once

#include "JC/Common.h"

namespace JC {

struct TempMem;

//--------------------------------------------------------------------------------------------------

enum struct LogCategory {
	Info,
	Error,
};

using LogFn = void(SrcLoc sl, LogCategory category, const char* msg, u64 len);

struct LogApi {
	static LogApi* Get();

	virtual void Init(TempMem* tempMem) = 0;

	virtual void AddFn(LogFn* fn) = 0;
	virtual void RemoveFn(LogFn* fn) = 0;

	virtual void VPrint(SrcLoc sl, LogCategory category, s8 fmt, Args args) = 0;
	virtual void PrintErr(SrcLoc sl, Err* err) = 0;

	template <class... A> void Print(SrcLoc sl, LogCategory category, FmtStr<A...> fmt, A... args) {
		VPrint(sl, category, fmt, Args::Make(args...));
	}
};

#define Log(fmt, ...)      logApi->Print(SrcHere, LogCategory::Info,  (fmt), ##__VA_ARGS__)
#define LogError(fmt, ...) logApi->Print(SrcHere, LogCategory::Error, (fmt), ##__VA_ARGS__)
#define LogErr(err)        logApi->PrintErr(SrcHere, err)

//--------------------------------------------------------------------------------------------------

}	// namespace JC