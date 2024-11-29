#include "JC/Log.h"

#include "JC/Array.h"
#include "JC/Err.h"
#include "JC/Fmt.h"
#include "JC/Mem.h"

namespace JC {

//--------------------------------------------------------------------------------------------------

struct LogApiObj : LogApi {
	static constexpr u32 MaxLogFns = 32;

	TempMem* tempMem           = 0;
	LogFn*   logFns[MaxLogFns] = {};
	u32      logFnsLen         = 0;

	void Init(TempMem* tempMemIn) override {
		tempMem = tempMemIn;
	}

	void AddFn(LogFn* fn) override {
		Assert(logFnsLen < MaxLogFns);
		logFns[logFnsLen++] = fn;
	}

	void RemoveFn(LogFn* fn) override {
		for (u32 i = 0; i < logFnsLen; i++) {
			if (logFns[i] == fn) {
				logFns[i] = logFns[logFnsLen - 1];
				logFnsLen--;
			}
		}
	}

	void VPrint(SrcLoc sl, LogCategory category, s8 fmt, Args args) override {
		char buf[1024];
		Array<char> arr = { .mem = tempMem, .data = buf, .cap = sizeof(buf) };
		VFmt(&arr, fmt, args);
		arr.Add('\n');
		arr.Add(0);
		for (u32 i = 0; i < logFnsLen; i++) {
			(*logFns[i])(sl, category, arr.data, arr.len);
		}
	}

	void PrintErr(SrcLoc sl, Err* err) override {
		char buf[1024];
		Array<char> arr = { .mem = tempMem, .data = buf, .cap = sizeof(buf) };
		AddErrStr(err, &arr);
		arr.Add('\n');
		arr.Add(0);
		for (u32 i = 0; i < logFnsLen; i++) {
			(*logFns[i])(sl, LogCategory::Error, arr.data, arr.len);
		}
	}
};

LogApiObj logApi;

LogApi* LogApi::Get() {
	return &logApi;
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC