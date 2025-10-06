#include "JC/Common_Assert.h"

#include "JC/Common_Fmt.h"
#include "JC/Sys.h"

namespace JC::Asrt {

//--------------------------------------------------------------------------------------------------

static PanicFn* panicFn;

PanicFn* SetPanicFn(PanicFn* newPanicFn) {
	PanicFn* oldPanicFn = panicFn;
	panicFn = newPanicFn;
	return oldPanicFn;
}

[[noreturn]] void Panicv(SrcLoc sl, char const* expr, char const* fmt, Span<Arg::Arg const> args) {
	static bool recursive = false;
	if (recursive) {
		if (Sys::DbgPresent()) {
			Sys_DbgBreak;
		}
		Sys::Abort();
	}
	recursive = true;

	if (panicFn) {
		char msg[2048] = {};
		if (fmt) {
			*Fmt::Printv(msg, msg + LenOf(msg) - 1, fmt, args) = '\0';
		}
		panicFn(sl, expr, msg);
	} else {
		if (Sys::DbgPresent()) {
			Sys_DbgBreak;
		}
		Sys::Abort();
	}
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Asrt