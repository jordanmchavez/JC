#include "JC/Common.h"

#include "JC/Array.h"
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

void _VPanic(s8 file, i32 line, s8 expr, s8 fmt, Args args) {
	static bool recursive = false;
	if (recursive) {
		Sys::Abort();
	}
	recursive = true;

	if (panicFn) {
		panicFn(file, line, expr, fmt, args);
	} else {
		if (Sys::IsDebuggerPresent()) {
			Sys_DebuggerBreak();
		}
		Sys::Abort();
	}
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC