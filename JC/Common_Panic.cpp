#include "JC/Common.h"

#include "JC/Fmt.h"
#include "JC/Sys.h"

//--------------------------------------------------------------------------------------------------

static PanicFn* panicFn;

PanicFn* SetPanicFn(PanicFn* newPanicFn) {
	PanicFn* oldPanicFn = panicFn;
	panicFn = newPanicFn;
	return oldPanicFn;
}

[[noreturn]] void Panicv(SrcLoc sl, char const* expr, char const* fmt, Arg const* args, U32 argsLen) {
	static Bool recursive = false;
	if (recursive) {
		if (Sys_DbgPresent()) {
			Dbg_Break;
		}
		Sys_Abort();
	}
	recursive = true;

	if (panicFn) {
		char msg[2048] = {};
		if (fmt) {
			*Fmt_Printv(msg, msg + LenOf(msg) - 1, fmt, Span<Arg const>(args, argsLen)) = '\0';
		}
		panicFn(sl, expr, msg);
	} else {
		if (Sys_DbgPresent()) {
			Dbg_Break;
		}
		Sys_Abort();
	}
}