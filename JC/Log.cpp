#include "JC/Log.h"

#include "JC/Allocator.h"
#include "JC/Array.h"
#include "JC/Fmt.h"

namespace JC {

//--------------------------------------------------------------------------------------------------

struct LogApiImpl : LogApi {
	static constexpr u32 MaxLogFns = 32;

	TempAllocatorApi* tempAllocatorApi;
	char              taBuf[1024];
	TempAllocator*    ta = nullptr;
	LogFn*            logFns[MaxLogFns];
	u32               logFnsLen = 0;

	void Init(TempAllocatorApi* tempAllocatorApiIn) {
		tempAllocatorApi = tempAllocatorApiIn;
		ta = tempAllocatorApi->Create(taBuf, sizeof(taBuf));
	}

	void AddFn(LogFn* fn) override {
		JC_ASSERT(logFnsLen < MaxLogFns);
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

	void VLog(s8 file, i32 line, LogCategory category, s8 fmt, Args args) override {
		char buf[256];
		Array<char> arr = { scratch, buf, 0, sizeof(buf) };
		arr.Init(ta);
		VFmt(&arr, fmt, args);
		arr.Add('\n');
		const s8 msg = s8(arr.data, arr.len);
		for (u32 i = 0; i < logFnsLen; i++) {
			(*logFns[i])(file, line, category, msg);
		}
	}

	void LogErr(s8 file, i32 line, Err* err) override {
		tempAllocatorApi->Reset(ta);
		Array<char> arr;
		arr.Init(ta);
		err->Str(&arr);
		arr.Add('\n');
		const s8 msg(arr.data, arr.len);
		for (u32 i = 0; i < logFnsLen; i++) {
			(*logFns[i])(file, line, LogCategory::Error, msg);
		}
		tempAllocatorApi->Destroy(ta);
	}
};

static LogApiImpl logApiImpl;

LogApi* LogApi::Get() {
	return &logApiImpl;
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC