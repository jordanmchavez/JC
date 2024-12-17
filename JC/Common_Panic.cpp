#include "JC/Common.h"

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

void VPanic(SrcLoc sl, s8 expr, s8 fmt, VArgs args) {
	static bool recursive = false;
	if (recursive) {
		Sys::Abort();
	}
	recursive = true;

	if (panicFn) {
		panicFn(sl, expr, fmt, args);
	} else {
		if (Sys::IsDebuggerPresent()) {
			Sys_DebuggerBreak();
		}
		Sys::Abort();
	}
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC