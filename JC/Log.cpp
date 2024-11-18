#include "JC/Log.h"

#include "JC/Array.h"
#include "JC/Err.h"
#include "JC/Fmt.h"
#include "JC/Mem.h"

namespace JC {

//--------------------------------------------------------------------------------------------------

struct LogApiObj : LogApi {
	static constexpr u32 MaxLogFns = 32;

	LogFn* logFns[MaxLogFns];
	u32    logFnsLen = 0;

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

	void VPrint(TempMem* mem, SrcLoc sl, LogCategory category, s8 fmt, Args args) {
		char buf[1024];
		Array<char> arr = { .mem = mem, .data = buf, .cap = sizeof(buf) };
		VFmt(&arr, fmt, args);
		arr.Add('\n');
		arr.Add(0);
		for (u32 i = 0; i < logFnsLen; i++) {
			(*logFns[i])(mem, sl, category, arr.data, arr.len);
		}
	}

	void PrintErr(TempMem* mem, SrcLoc sl, Err* err) {
		char buf[1024];
		Array<char> arr = { .mem = mem, .data = buf, .cap = sizeof(buf) };
		AddErrStr(err, &arr);
		arr.Add('\n');
		arr.Add(0);
		for (u32 i = 0; i < logFnsLen; i++) {
			(*logFns[i])(mem, sl, LogCategory::Error, arr.data, arr.len);
		}
	}
};
//--------------------------------------------------------------------------------------------------

}	// namespace JC