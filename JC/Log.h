#pragma once

#include "JC/Common.h"

namespace JC::Log {

//--------------------------------------------------------------------------------------------------


enum struct Level {
	Log,
	Error,
};

struct Msg {
	SrcLoc          sl;
	Level           level;
	char const*     line;	// new-line and null-terminated
	U32             lineLen;
};

using Fn = void (Msg const* msg);

void    Init(Mem tempMem);
void    AddFn(Fn* fn);
void    RemoveFn(Fn* fn);
void    Printv(SrcLoc sl, Level level, char const*  fmt, Span<Arg const> args);
void    PrintErr(SrcLoc sl, const Err* err);

template <class... A> void Printf(SrcLoc sl, Level level, CheckFmtStr<A...> fmt, A... args) { Printv(sl, level, fmt, { Arg::Make(args)..., }); }
template <class T> void PrintErr(SrcLoc sl, Res<T> res) { PrintErr(sl, res.err); }

#define Logf(fmt, ...)   Log::Printf(SrcLoc::Here(), Log::Level::Log,   fmt, ##__VA_ARGS__)
#define Errorf(fmt, ...) Log::Printf(SrcLoc::Here(), Log::Level::Error, fmt, ##__VA_ARGS__)
#define LogErr(err)      Log::PrintErr(SrcLoc::Here(), err)

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Log