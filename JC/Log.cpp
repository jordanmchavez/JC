#include "JC/Log.h"

#include "JC/Array.h"

namespace JC::Log {

//--------------------------------------------------------------------------------------------------

static constexpr U32 MaxFns  = 32;
static constexpr U32 MaxLine = 4096;

static Fn* fns[MaxFns];
static U32 fnsLen;

//--------------------------------------------------------------------------------------------------

void Printv(SrcLoc sl, Level level, char const* fmt, Span<Arg::Arg const> args) {
	char line[MaxLine];
	char* const lineEnd = line + LenOf(line) - 2;
	char* lineIt = Fmt::Printf(line, lineEnd, "%s%s(%u): ", level == Level::Error ? "!!! " : "", sl.file, sl.line);
	lineIt = Fmt::Printv(lineIt, lineEnd, fmt, args);
	*lineIt++ = '\n';
	*lineIt = 0;
	Msg msg = {
		.line    = line,
		.lineLen = (U32)(lineIt - line),
		.sl      = sl,
		.level   = level,
		.fmt     = fmt,
		.args    = args,
	};
	for (U32 i = 0; i < fnsLen; i++) {
		(*fns[i])(&msg);
	}
}

//--------------------------------------------------------------------------------------------------

void AddFn(Fn* fn) {
	Assert(fnsLen < MaxFns);
	fns[fnsLen++] = fn;
}

//--------------------------------------------------------------------------------------------------

void RemoveFn(Fn* fn) {
	for (U32 i = 0; i < fnsLen; i++) {
		if (fns[i] == fn) {
			fns[i] = fns[--fnsLen];
		}
	}
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Log