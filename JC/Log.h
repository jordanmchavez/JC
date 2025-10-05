#pragma once

#include "JC/Common.h"

//--------------------------------------------------------------------------------------------------

enum struct LogLevel {
	Log,
	Error,
};

void Log_Printv(SrcLoc sl, LogLevel level, char const*  fmt, Span<Arg const> args);

template <class... A> void Log_Printf(SrcLoc sl, LogLevel level, FmtStr<A...> fmt, A... args) {
	Log_Printv(sl, level, fmt, { Arg_Make(args)..., });
}

#define Logf(fmt, ...)   Log_Printf(SrcLoc_Here(), LogLevel::Log,   fmt, ##__VA_ARGS__)
#define Errorf(fmt, ...) Log_Printf(SrcLoc_Here(), LogLevel::Error, fmt, ##__VA_ARGS__)

struct LogMsg {
	char const*     line;
	U32             lineLen;
	SrcLoc          sl;
	LogLevel        level;
	const char*     fmt;
	Span<Arg const> args;
};
using LogFn = void (LogMsg const* msg);

void Log_AddFn(LogFn fn);
void Log_RemoveFn(LogFn fn);