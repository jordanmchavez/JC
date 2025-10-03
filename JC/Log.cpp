#include "JC/Log.h"

#include "JC/Array.h"
#include "JC/Fmt.h"

//--------------------------------------------------------------------------------------------------

static constexpr U32 Log_MaxFns  = 32;
static constexpr U32 Log_MaxLine = 4096;

static LogFn* log_fns[Log_MaxFns];
static U32    log_fnsLen;

//--------------------------------------------------------------------------------------------------

void Log_Printv(SrcLoc sl, LogLevel level, const char* fmt, Span<Arg const> args) {
	char line[Log_MaxLine];
	char* const lineEnd = line + LenOf(line) - 1;
	char* lineIt = Fmt_Printf(line, lineEnd, "{}{}({}): ", level == LogLevel::Error ? "!!! " : "", sl.file, sl.line);
	lineIt = Fmt_Printv(lineIt, lineEnd, fmt, args);
	*lineIt = 0;
	LogMsg msg = {
		.line    = line,
		.lineLen = (U32)(lineIt - line),
		.sl      = sl,
		.level   = level,
		.fmt     = fmt,
		.args    = args,
	};
	for (U32 i = 0; i < log_fnsLen; i++) {
		(*log_fns[i])(&msg);
	}
}

//--------------------------------------------------------------------------------------------------

void Log_AddFn(LogFn* fn) {
	Assert(log_fnsLen < Log_MaxFns);
	log_fns[log_fnsLen++] = fn;
}

//--------------------------------------------------------------------------------------------------

void Log_RemoveFn(LogFn* fn) {
	for (U32 i = 0; i < log_fnsLen; i++) {
		if (log_fns[i] == fn) {
			log_fns[i] = log_fns[--log_fnsLen];
		}
	}
}