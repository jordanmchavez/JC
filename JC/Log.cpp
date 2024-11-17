#include "JC/Log.h"

#include "JC/Array.h"
#include "JC/Fmt.h"

namespace JC {

//--------------------------------------------------------------------------------------------------

namespace Log {
	static constexpr u32 MaxLogFns = 32;

	LogFn* logFns[MaxLogFns];
	u32    logFnsLen = 0;
}

void Log::AddFn(LogFn* fn) {
	Assert(logFnsLen < MaxLogFns);
	logFns[logFnsLen++] = fn;
}

void Log::RemoveFn(LogFn* fn) {
	for (u32 i = 0; i < logFnsLen; i++) {
		if (logFns[i] == fn) {
			logFns[i] = logFns[logFnsLen - 1];
			logFnsLen--;
		}
	}
}

void Log::VPrint(Mem scratch, s8 file, i32 line, LogCategory category, s8 fmt, Args args) {
	char buf[1024];
	Array<char> arr = { .mem = &scratch, .data = buf, .cap = sizeof(buf) };
	VFmt(&arr, fmt, args);
	arr.Add('\n');
	arr.Add(0);
	for (u32 i = 0; i < logFnsLen; i++) {
		(*logFns[i])(scratch, file, line, category, arr.data, arr.len);
	}
}

void Log::PrintErr(Mem scratch, s8 file, i32 line, Err* err) {
	char buf[1024];
	Array<char> arr = { .mem = &scratch, .data = buf, .cap = sizeof(buf) };
	err->Str(&arr);
	arr.Add('\n');
	arr.Add(0);
	for (u32 i = 0; i < logFnsLen; i++) {
		(*logFns[i])(scratch, file, line, LogCategory::Error, arr.data, arr.len);
	}
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC