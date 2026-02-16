#pragma once

#include "JC/Common.h"

namespace JC::Log {

//--------------------------------------------------------------------------------------------------

enum struct Level {
	Log,
	Error,
};

void Init(Mem permMem, Mem tempMem);

void Printv(SrcLoc sl, Level level, char const*  fmt, Span<Arg const> args);

template <class... A> void Printf(SrcLoc sl, Level level, CheckFmtStr<A...> fmt, A... args) {
	Printv(sl, level, fmt, { Arg::Make(args)..., });
}

void PrintErr(SrcLoc sl, const Err* err);
template <class T> void PrintErr(SrcLoc sl, Res<T> res) { PrintErr(sl, res.err); }

#define Logf(fmt, ...)   Log::Printf(SrcLoc::Here(), Log::Level::Log,   fmt, ##__VA_ARGS__)
#define Errorf(fmt, ...) Log::Printf(SrcLoc::Here(), Log::Level::Error, fmt, ##__VA_ARGS__)
#define LogErr(err) Log::PrintErr(SrcLoc::Here(), err);

struct Msg {
	SrcLoc          sl;
	Level           level;
	char const*     line;	// new-line and null-terminated
	U32             lineLen;
};
using Fn = void (Msg const* msg);

void AddFn(Fn fn);
void RemoveFn(Fn fn);

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Log