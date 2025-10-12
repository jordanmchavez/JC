#pragma once

#include "JC/Common_Arg.h"
#include "JC/Common_Fmt.h"
#include "JC/Common_Mem.h"
#include "JC/Common_SrcLoc.h"
#include "JC/Common_Std.h"

namespace JC::Log {

//--------------------------------------------------------------------------------------------------

enum struct Level {
	Log,
	Error,
};

void Init(Mem::Mem permMem, Mem::Mem tempMem);

void Printv(SrcLoc sl, Level level, char const*  fmt, Span<Arg::Arg const> args);

template <class... A> void Printf(SrcLoc sl, Level level, Fmt::CheckStr<A...> fmt, A... args) {
	Printv(sl, level, fmt, { Arg::Make(args)..., });
}

#define Logf(fmt, ...)   Log::Printf(SrcLoc::Here(), Log::Level::Log,   fmt, ##__VA_ARGS__)
#define Errorf(fmt, ...) Log::Printf(SrcLoc::Here(), Log::Level::Error, fmt, ##__VA_ARGS__)

struct Msg {
	char const*          line;
	U64                  lineLen;
	SrcLoc               sl;
	Level                level;
	char const*          fmt;
	Span<Arg::Arg const> args;
};
using Fn = void (Msg const* msg);

void AddFn(Fn fn);
void RemoveFn(Fn fn);

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Log