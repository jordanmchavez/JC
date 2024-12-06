#include "JC/Log.h"

#include "JC/Array.h"
#include "JC/Fmt.h"
#include "JC/Mem.h"

namespace JC {

//--------------------------------------------------------------------------------------------------

struct LogObj : Log {
	static constexpr u32 MaxLogFns = 32;

	TempMem* tempMem = 0;
	LogFn*   logFns[MaxLogFns] = {};
	u32      logFnsLen         = 0;

	void VPrint(SrcLoc sl, LogCategory category, s8 fmt, Args args) override {
		Array<char> arr(tempMem);
		Fmt(
			&arr,
			"{}{}({}): ",
			category == LogCategory::Error ? "!!! " : "",
			sl.file,
			sl.line
		);
		VFmt(&arr, fmt, args);
		arr.Add("\n", 2);
		for (u32 i = 0; i < logFnsLen; i++) {
			(*logFns[i])(arr.data, arr.len);
		}
	}

	void Error(Err* err, SrcLoc sl) override {
		Array<char> arr(tempMem);
		Fmt(
			&arr,
			"!!! {}({}): ",
			sl.file,
			sl.line
		);
		for (Err* e = err; e; e = e->prev) {
			Fmt(&arr, "{}-{}:", e->ec.ns, e->ec.code);
		}
		arr.data[arr.len - 1] = '\n';	// overwrite trailing ':'
		for (Err* e = err; e; e = e->prev) {
			Fmt(&arr, "  {}({}): {}-{}\n", e->sl.file, e->sl.line, e->ec.ns, e->ec.code);
			for (u32 i = 0; i < e->argsLen; i++) {
				Fmt(&arr, "    '{}' = {}\n", e->args[i].name, e->args[i].arg);
			}
		}
		arr.len--;	// remove trailing '\n'
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

//--------------------------------------------------------------------------------------------------

struct LogApiObj : LogApi {
	LogObj log = {};

	void Init(TempMem* tempMem) override {
		log.tempMem = tempMem;
	}

	void AddFn   (LogFn* fn) override { log.AddFn(fn); }
	void RemoveFn(LogFn* fn) override { log.RemoveFn(fn); }

	Log* GetLog() override { return &log; }
};

static LogApiObj logApi = {};

LogApi* GetLogApi() {
	return &logApi;
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC