#include "JC/Log.h"

#include "JC/Array.h"
#include "JC/Fmt.h"

namespace JC::Log {

//--------------------------------------------------------------------------------------------------

struct DefaultLogger : Logger {
	static constexpr u32 MaxLogFns = 32;

	Mem::TempAllocator* tempAllocator = 0;
	Fn*                 fns[MaxLogFns] = {};
	u32                 fnsLen         = 0;

	void Init(Mem::TempAllocator* tempAllocatorIn) {
		tempAllocator = tempAllocatorIn;
	}

	void VPrintf(SrcLoc sl, Level level, const char* fmt, Span<Arg> args) override {
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
		for (u32 i = 0; i < fnsLen; i++) {
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
			for (u32 i = 0; i < namedArgs.len; i++) {
				Fmt(&arr, "  '{}' = {}\n", namedArgs[i].name, namedArgs[i].varg);
			}
		}
		arr.data[arr.len] = '\0';	// replace trailing '\n'
		for (u32 i = 0; i < logFnsLen; i++) {
			(*logFns[i])(arr.data, arr.len);
		}
	}
*/
/*
	void AddFn(LogFn* fn) {
		Assert(logFnsLen < MaxLogFns);
		logFns[logFnsLen++] = fn;
	}

	void RemoveFn(LogFn* fn) {
		for (u32 i = 0; i < logFnsLen; i++) {
			if (logFns[i] == fn) {
				logFns[i] = logFns[logFnsLen - 1];
				logFnsLen--;
			}
		}
	}
*/
};

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Log