#pragma once

#include "JC/Core.h"

namespace JC::Log {

//--------------------------------------------------------------------------------------------------

enum struct Level {
	Log,
	Error,
};

struct Logger {
	virtual               void VPrintf(Level level, const char*       fmt, Span<Arg> args, SrcLoc sl) = 0;
	template <class... A> void  Printf(Level level, Fmt::FmtStr<A...> fmt, A...      args, SrcLoc sl) { VPrintf(level, fmt, Span<Arg>({ MakeArg(args...), }), sl); }
};

#define Logf(fmt, ...)   logger->Printf(Log::Level::Log,   fmt, __VA_ARGS__, SrcLoc::Here())
#define Errorf(fmt, ...) logger->Printf(Log::Level::Error, fmt, __VA_ARGS__, SrcLoc::Here())

using Fn = void (const char* msg, u64 len);

Logger* InitLogger(Mem::TempAllocator* tempAllocator);
void AddFn(Fn fn);


//--------------------------------------------------------------------------------------------------

}	// namespace JC::Log