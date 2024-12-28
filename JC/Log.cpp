#include "JC/Log.h"

#include "JC/Array.h"
#include "JC/Fmt.h"

namespace JC {

//--------------------------------------------------------------------------------------------------

struct LogObj : Log {
	static constexpr u32 MaxLogFns = 32;

	Arena* temp              = 0;
	LogFn* logFns[MaxLogFns] = {};
	u32    logFnsLen         = 0;

	void Init(Arena* tempIn) override {
		temp = tempIn;
	}

	void VPrint(SrcLoc sl, LogCategory category, s8 fmt, VArgs args) override {
		Array<char> arr(temp);
		Fmt(
			&arr,
			"{}({}):{} ",
			sl.file,
			sl.line,
			category == LogCategory::Error ? " !!!" : ""
		);
		VFmt(&arr, fmt, args);
		arr.Add("\n", 2);
		for (u32 i = 0; i < logFnsLen; i++) {
			(*logFns[i])(arr.data, arr.len);
		}
	}

	void Error(Err err, SrcLoc sl) override {
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
};

static LogObj logObj;

Log* GetLog() { return &logObj; }

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Log