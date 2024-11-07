#include "JC/Log.h"

#include "JC/Allocator.h"
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

struct LogApiImpl : LogApi {
	static constexpr u32 MaxLogFns = 32;

	TempAllocator* tempAllocator     = nullptr;
	LogFn*         logFns[MaxLogFns] = { nullptr };
	u32            logFnsLen         = 0;

	void Init(TempAllocator* inTempAllocator) {
		tempAllocator = inTempAllocator;
	}

	void VLog(s8 file, i32 line, LogCategory category, s8 fmt, Args args) override {
		s8 msg = VFmt(tempAllocator, fmt, args);
		for (u32 i = 0; i < logFnsLen; i++) {
			(*logFns[i])(file, line, category, msg);
		}
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