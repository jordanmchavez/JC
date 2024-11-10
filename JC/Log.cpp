#include "JC/Log.h"

#include "JC/Allocator.h"
#include "JC/Array.h"
#include "JC/Fmt.h"

namespace JC {

//--------------------------------------------------------------------------------------------------

struct LogApiImpl : LogApi {
	static constexpr u32 MaxLogFns = 32;

	TempAllocatorApi* tempAllocatorApi;
	LogFn*            logFns[MaxLogFns];
	u32               logFnsLen = 0;

	void Init(TempAllocatorApi* inTempAllocatorApi) {
		tempAllocatorApi = inTempAllocatorApi;
	}

	void VLog(s8 file, i32 line, LogCategory category, s8 fmt, Args args) override {
		char buf[1024];
		TempAllocator* ta = tempAllocatorApi->Create(buf, sizeof(buf));
		Array<char> arr;
		arr.Init(ta);
		VFmt(&arr, fmt, args);
		arr.Add('\n');
		const s8 msg = s8(arr.data, arr.len);
		for (u32 i = 0; i < logFnsLen; i++) {
			(*logFns[i])(file, line, category, msg);
		}
		tempAllocatorApi->Destroy(ta);
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
};

static LogApiImpl logApiImpl;

LogApi* LogApi::Get() {
	return &logApiImpl;
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC