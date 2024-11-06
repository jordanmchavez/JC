#include "JC/Allocator.h"
#include "JC/Array.h"
#include "JC/Err.h"
#include "JC/Fmt.h"
#include "JC/Log.h"
#include "JC/Sys.h"

namespace JC {

//--------------------------------------------------------------------------------------------------

struct LogApiImpl : LogApi {
	static constexpr u32 MaxLoggers = 32;

	TempAllocatorApi* tempAllocatorApi;
	ThreadApi*        threadApi;
	TimeApi*          timeApi;
	Allocator*        tempAllocator;
	Mutex             mutex;
	Logger*           loggers[MaxLoggers];
	u32               loggersLen;
	u64               frame;

	void Init(TempAllocatorApi* inTempAllocatorApi, ThreadApi* inThreadApi, TimeApi* inTimeApi) {
		tempAllocatorApi = inTempAllocatorApi;
		threadApi        = inThreadApi;
		timeApi          = inTimeApi;
		tempAllocator    = tempAllocatorApi->Get();
	}

	void AddLogger(Logger* logger) override {
		Assert(loggersLen < MaxLoggers);
		threadApi->LockMutex(mutex);
		loggers[loggersLen] = logger;
		loggersLen++;
		threadApi->UnlockMutex(mutex);
	}

	void RemoveLogger(Logger* logger) override {
		threadApi->LockMutex(mutex);
		for (u32 i = 0; i < loggersLen; i++) {
			if (loggers[i] == logger) {
				loggers[i] = loggers[loggersLen - 1];
				--loggersLen;
				return;
			}
		}
		threadApi->UnlockMutex(mutex);
	}

	void VPrint(s8 fileNameOnly, i32 line, LogType type, s8 fmt, Args args) override {
		Array<char> a;
		a.Init(tempAllocator);
		const Date date = timeApi->TimeToDate(timeApi->Now());
		Fmt(
			&a,
			"{}[{02}:{02}:{02}.{03} {04}] {}({}): ",
			type == LogType::Err ? "!!!" : "",
			date.hour, date.minute, date.second, date.millisecond,
			frame % 1000,
			fileNameOnly,
			line
		);
		VFmt(&a, fmt, args);
		a.Add('\n');

		const s8 msg(a.data, a.len);
		threadApi->LockMutex(mutex);
		for (u32 i = 0; i < loggersLen; i++) {
			loggers[i]->Print(type, msg);
		}
		threadApi->UnlockMutex(mutex);
	}

	void Frame(u64 inFrame) override {
		frame = inFrame;
	}
};

static LogApiImpl logApiImpl;

LogApi* LogApi::Init(TempAllocatorApi* tempAllocatorApi, ThreadApi* threadApi, TimeApi* timeApi) {
	logApiImpl.Init(tempAllocatorApi, threadApi, timeApi);
	return &logApiImpl;
}


//--------------------------------------------------------------------------------------------------

}	// namespace JC