#pragma once

#include "JC/Common.h"

namespace JC::Log {

//--------------------------------------------------------------------------------------------------

enum struct Level {
	Log,
	Err,
};

struct Logger {
	virtual               void VPrintf(SrcLoc sl, Level level, Str          fmt, VArgs args) = 0;
	template <class... A> void  Printf(SrcLoc sl, Level level, FmtStr<A...> fmt, A...  args) { VPrintf(sl, level, fmt, MakeVArgs(args...)); }
};

#define Logf(fmt, ...) logger->Printf(SrcLoc::Here(), Log::Level::Log, fmt, __VA_ARGS__)
#define Errf(fmt, ...) logger->Printf(SrcLoc::Here(), Log::Level::Err, fmt, __VA_ARGS__)

using Fn = void (const char* msg, u64 len);

Logger* GetLogger();
void AddFn(Fn fn);


//--------------------------------------------------------------------------------------------------

}	// namespace JC::Log