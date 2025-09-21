#include "JC/Core.h"

#include "JC/Fmt.h"
#include "JC/Sys.h"

namespace JC {

//--------------------------------------------------------------------------------------------------

static PanicFn* panicFn;

PanicFn* SetPanicFn(PanicFn* newPanicFn) {
	PanicFn* oldPanicFn = panicFn;
	panicFn = newPanicFn;
	return oldPanicFn;
}

void VPanic(SrcLoc sl, const char* expr, const char* fmt, Span<const Arg> args) {
	static Bool recursive = false;
	if (recursive) {
		if (Sys::IsDebuggerPresent()) {
			JC_DEBUGGER_BREAK;
		}
		Sys::Abort();
	}
	recursive = true;

	if (panicFn) {
		char msg[2048] = {};
		if (fmt) {
			*Fmt::VPrintf(msg, msg + JC_LENOF(msg) - 1, fmt, args) = '\0';
		}
		panicFn(sl, expr, msg);
	} else {
		if (Sys::IsDebuggerPresent()) {
			JC_DEBUGGER_BREAK;
		}
		Sys::Abort();
	}
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC