#include "JC/Log.h"

#include "JC/Allocator.h"
#include "JC/Array.h"
#include "JC/Fmt.h"

namespace JC {

//--------------------------------------------------------------------------------------------------
/*
constexpr s8 FileNameOnly(s8 path) {
	for (const char* i = path.data + path.len - 1; i >= path.data; i--) {
		if (*i == '/' || *i == '\\') {
			return s8(i + 1, path.data + path.len);
		}
	}
	return path;
}
*/

struct LogFnObj {
	LogFn* fn;
	void*  userData;
};

struct LogApiImpl : LogApi {
	static constexpr u32 MaxLogFns = 32;

	TempAllocatorApi* tempAllocatorApi;
	LogFnObj          logFnObjs[MaxLogFns];
	u32               logFnObjsLen;

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
		for (u32 i = 0; i < logFnObjsLen; i++) {
			(*logFnObjs[i].fn)(logFnObjs[i].userData, file, line, category, msg);
		}
		tempAllocatorApi->Destroy(ta);
	}

	void AddFn(LogFn* fn, void* userData) override {
		JC_ASSERT(logFnObjsLen < MaxLogFns);
		logFnObjs[logFnObjsLen++] = { .fn = fn, .userData = userData };
	}

	void RemoveFn(LogFn* fn) override {
		for (u32 i = 0; i < logFnObjsLen; i++) {
			if (logFnObjs[i].fn == fn) {
				logFnObjs[i] = logFnObjs[logFnObjsLen - 1];
				logFnObjsLen--;
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