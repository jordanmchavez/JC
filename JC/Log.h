#pragma once

#include "JC/Core.h"

namespace JC::Log {

//--------------------------------------------------------------------------------------------------

enum struct Level {
	Log,
	Error,
};

struct Logger {
	virtual               void VPrintf(SrcLoc sl, Level level, const char*  fmt, Span<const Arg> args) = 0;
	template <class... A> void  Printf(SrcLoc sl, Level level, FmtStr<A...> fmt, A...            args) { VPrintf(sl, level, fmt, { MakeArg(args)..., }); }
};

#define JC_LOG(fmt, ...)   logger->Printf(SrcLoc::Here(), Log::Level::Log,   fmt, ##__VA_ARGS__)
#define JC_LOG_ERROR(fmt, ...) logger->Printf(SrcLoc::Here(), Log::Level::Error, fmt, ##__VA_ARGS__)

using Fn = void (const char* msg, U64 len);

Logger* InitLogger(Mem::TempAllocator* tempAllocator);
void AddFn(Fn fn);

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Log