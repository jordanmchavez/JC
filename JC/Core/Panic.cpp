#include "JC/Core.h"	// not Core/Panic.h to preserve core inclusion order

#include "JC/Sys.h"

namespace JC {

//--------------------------------------------------------------------------------------------------

static PanicFn* panicFn;

PanicFn* SetPanicFn(PanicFn* newPanicFn) {
	PanicFn* oldPanicFn = panicFn;
	panicFn = newPanicFn;
	return oldPanicFn;
}

void VPanic(const char* expr, const char* fmt, Span<NamedArg> namedArgs, SrcLoc sl) {
	static bool recursive = false;
	if (recursive) {
		Sys::Abort();
	}
	recursive = true;

	if (panicFn) {
		panicFn(expr, fmt, namedArgs, sl);
	} else {
		if (Sys::IsDebuggerPresent()) {
			Sys_DebuggerBreak();
		}
		Sys::Abort();
	}
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC