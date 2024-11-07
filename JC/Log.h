#pragma once

#include "JC/Common.h"

namespace JC {

//--------------------------------------------------------------------------------------------------

enum struct LogCategory {
	Info,
	Error,
};

using LogFn = void(s8 file, i32 line, LogCategory, s8 msg);

struct LogApi {
	static LogApi* Get();

	virtual void VLog(s8 file, i32 line, LogCategory category, s8 fmt, Args args) = 0;
	virtual void AddFn(LogFn* fn) = 0;
	virtual void RemoveFn(LogFn* fn) = 0;

	template <class... A> void Log(s8 file, i32 line, LogCategory category, FmtStr<A...> fmt, A... args) {
		VLog(file, line, category, fmt, Args::Make(args...));
	}
};

#define JC_LOG(fmt, ...)       logApi->Log(__FILE__, __LINE__, JC::LogCategory::Info,  (fmt), __VA_ARGS__)
#define JC_LOG_ERROR(fmt, ...) logApi->Log(__FILE__, __LINE__, JC::LogCategory::Error, (fmt), __VA_ARGS__)

//--------------------------------------------------------------------------------------------------

}	// namespace JC