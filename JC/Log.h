#pragma once

#include "JC/Common.h"

namespace JC {

//--------------------------------------------------------------------------------------------------

enum struct LogCategory {
	Info,
	Error,
};

using LogFn = void(Mem* scratch, s8 file, i32 line, LogCategory category, const char* msg, u64 len);

struct LogApi {
	static LogApi* Get();

	virtual void AddFn(LogFn* fn) = 0;
	virtual void RemoveFn(LogFn* fn) = 0;

	virtual void VPrint(Mem* scratch, s8 file, i32 line, LogCategory category, s8 fmt, Args args) = 0;
	virtual void PrintErr(Mem* scratch, s8 file, i32 line, Err* err) = 0;

	template <class... A> void Print(Mem* scratch, s8 file, i32 line, LogCategory category, FmtStr<A...> fmt, A... args) {
		VPrint(scratch, file, line, category, fmt, Args::Make(args...));
	}
};

#define Log(scratch, fmt, ...) Log::Print(scratch, __FILE__, __LINE__, LogCategory::Info,  (fmt), ##__VA_ARGS__)
#define LogErr(scratch, err)   Log::PrintErr(scratch, __FILE__, __LINE__, err)

                                              
//--------------------------------------------------------------------------------------------------

}	// namespace JC