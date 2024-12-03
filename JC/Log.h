#pragma once

#include "JC/Common.h"

namespace JC {

struct TempMem;

//--------------------------------------------------------------------------------------------------

enum struct LogCategory {
	Print,
	Error,
};

struct Log {
	virtual void VPrint(SrcLoc sl, LogCategory category, s8 fmt, Args args) = 0;

	template <class... A>
	void Print(SrcLoc sl, LogCategory category, FmtStr<A...> fmt, A... args) {
		VPrint(sl, category, fmt, MakeArgs(args...));
	}
};

#define Logf(fmt, ...) log->Print(SrcLoc::Here(), LogCategory::Print, fmt, ##__VA_ARGS__)
#define LogErrorf(fmt, ...) log->Print(SrcLoc::Here(), LogCategory::Error, fmt, ##__VA_ARGS__)

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