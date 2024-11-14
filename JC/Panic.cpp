#include "JC/Panic.h"

#include "JC/Log.h"
#include "JC/Fmt.h"
#include "JC/Sys.h"

namespace JC {

//--------------------------------------------------------------------------------------------------

struct PanicApiImpl : PanicApi {
	LogApi* logApi = nullptr;

	void Init(LogApi* logApiIn) {
		logApi = logApiIn;
	}

	[[noreturn]] void VPanic(s8 file, i32 line, s8 expr, s8 fmt, Args args) override {
		char msg[1024];
		*VFmt(msg, msg + sizeof(msg) - 1, fmt, args) = '\0';
		msg[sizeof(msg) - 1] = 0;	// just in case
		JC_LOG(
			"***PANIC***\n"
			"{}({})\n"
			"expr: {}\n"
			"msg:  {}\n",
			file, line, expr, msg
		);
		if (Sys::IsDebuggerPresent()) {
			JC_DEBUGGER_BREAK;
		}
		Sys::Abort();
	}
};

static PanicApiImpl panicApiImpl;

PanicApi* PanicApi::Get() {
	return &panicApiImpl;
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC