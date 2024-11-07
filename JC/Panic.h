#pragma once

#include "JC/Common.h"

namespace JC {

struct LogApi;

//--------------------------------------------------------------------------------------------------

struct PanicApi {

	static PanicApi* Get();

	virtual void Init(LogApi* logApi) = 0;
	[[noreturn]] virtual void VPanic(s8 file, i32 line, s8 expr, s8 fmt, Args args);
};

PanicApi* SetPanicApi(PanicApi* panicApi);

//--------------------------------------------------------------------------------------------------

}	// namespace JC