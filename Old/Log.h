#pragma once

#include "JC/Common.h"

namespace JC {

struct TempAllocatorApi;
struct ThreadApi;
struct TimeApi;

//--------------------------------------------------------------------------------------------------

enum struct LogType {
	Log,
	Err,
	Dbg,
};

struct Logger {
	virtual void Print(LogType type, s8 msg) = 0;
};

struct LogApi {
	static LogApi* Init(TempAllocatorApi* tempAllocatorApi, ThreadApi* threadApi, TimeApi* timeApi);

	virtual void AddLogger(Logger* logger) = 0;
	virtual void RemoveLogger(Logger* logger) = 0;
	virtual void VPrint(s8 fileNameOnly, i32 line, LogType type, s8 fmt, Args arg) = 0;
	virtual void Frame(u64 frame) = 0;
};

constexpr s8 FileNameOnly(s8 path) {
	for (const char* i = path.data + path.len - 1; i >= path.data; i--) {
		if (*i == '/' || *i == '\\') {
			return s8(i + 1, path.data + path.len);
		}
	}
	return path;
}

#define Log(fmt, ...)     logApi->VPrint(FileNameOnly(__FILE__), __LINE__, LogType::Log, (fmt), Args::Make(__VA_ARGS__))
#define Log_Err(fmt, ...) logApi->VPrint(FileNameOnly(__FILE__), __LINE__, LogType::Err, (fmt), Args::Make(__VA_ARGS__))
#define Log_Dbg(fmt, ...) logApi->VPrint(FileNameOnly(__FILE__), __LINE__, LogType::Dbg, (fmt), Args::Make(__VA_ARGS__))

//--------------------------------------------------------------------------------------------------

}	// namespace JC