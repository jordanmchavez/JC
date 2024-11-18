#pragma once

#include "JC/Common.h"

namespace JC {

struct TempMem;

//--------------------------------------------------------------------------------------------------

enum struct LogCategory {
	Info,
	Error,
};

using LogFn = void(TempMem* mem, SrcLoc sl, LogCategory category, const char* msg, u64 len);

struct LogApi {
	static LogApi* Get();

	virtual void AddFn(LogFn* fn) = 0;
	virtual void RemoveFn(LogFn* fn) = 0;

	virtual void VPrint(TempMem* mem, SrcLoc sl, LogCategory category, s8 fmt, Args args) = 0;
	virtual void PrintErr(TempMem* mem, SrcLoc sl, Err* err) = 0;

	template <class... A> void Print(TempMem* mem, SrcLoc sl, LogCategory category, FmtStr<A...> fmt, A... args) {
		VPrint(mem, sl, category, fmt, Args::Make(args...));
	}
};

#define Log(mem, fmt, ...) logApi->Print(mem, SrcHere, LogCategory::Info,  (fmt), ##__VA_ARGS__)
#define LogErr(mem, err)   logApi->PrintErr(mem, SrcHere, err)

                                              
//--------------------------------------------------------------------------------------------------

}	// namespace JC