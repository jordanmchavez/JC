#include "JC/Log.h"

#include "JC/Array.h"
#include "JC/Fmt.h"
#include "JC/Mem.h"

namespace JC::Log {

//--------------------------------------------------------------------------------------------------

struct LoggerObj : Logger {
	static constexpr U32 MaxFns = 32;

	TempAllocator* tempAllocator = 0;
	Fn*            fns[MaxFns] = {};
	U32            fnsLen         = 0;

	void Init(TempAllocator* tempAllocatorIn) {
		tempAllocator = tempAllocatorIn;
	}

	void VPrintf(SrcLoc sl, Level level, const char* fmt, Span<const Arg> args) override {
		Array<char> arr(tempAllocator);
		Fmt::Printf(
			&arr,
			"{}({}):{} ",
			sl.file,
			sl.line,
			level == Level::Error ? " !!!" : ""
		);
		Fmt::VPrintf(&arr, fmt, args);
		arr.Add("\n", 2);
		for (U32 i = 0; i < fnsLen; i++) {
			(*fns[i])(arr.data, arr.len);
		}
	}

/*	void Error(Err err, SrcLoc sl) override {
		Array<char> arr(temp);
		Fmt(&arr, "{}({}): !!! Error:\n", sl.file, sl.line);

		for (Err e = err; e.handle; e = e.GetPrev()) {
			if (e.GetS8Code() != "") {
				Fmt(&arr, "{}({}): {}:{}\n", e.GetSrcLoc().file, e.GetSrcLoc().line, e.GetNs(), e.GetS8Code());
			} else {
				Fmt(&arr, "{}({}): {}:{}\n", e.GetSrcLoc().file, e.GetSrcLoc().line, e.GetNs(), e.GetI64Code());
			}
			Span<NamedArg> namedArgs = e.GetNamedArgs();
			for (U32 i = 0; i < namedArgs.len; i++) {
				Fmt(&arr, "  '{}' = {}\n", namedArgs[i].name, namedArgs[i].varg);
			}
		}
		arr.data[arr.len] = '\0';	// replace trailing '\n'
		for (U32 i = 0; i < fnsLen; i++) {
			(*fns[i])(arr.data, arr.len);
		}
	}
*/
/*
*/
};

static LoggerObj loggerObj;

Logger* InitLogger(TempAllocator* tempAllocator) {
	loggerObj.Init(tempAllocator);
	return &loggerObj;	
}

void AddFn(Fn* fn) {
	JC_ASSERT(loggerObj.fnsLen < LoggerObj::MaxFns);
	loggerObj.fns[loggerObj.fnsLen++] = fn;
}

void RemoveFn(Fn* fn) {
	for (U32 i = 0; i < loggerObj.fnsLen; i++) {
		if (loggerObj.fns[i] == fn) {
			loggerObj.fns[i] = loggerObj.fns[--loggerObj.fnsLen];
		}
	}
}
//--------------------------------------------------------------------------------------------------

}	// namespace JC::Log