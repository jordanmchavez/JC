#include "JC/Core.h"

#include "JC/Fmt.h"
#include "JC/Sys.h"

namespace JC {

//--------------------------------------------------------------------------------------------------

Bool operator==(Str         s1, Str         s2) { return s1.len == s2.len && !memcmp(s1.data, s2.data, s1.len); }
Bool operator==(const char* s1, Str         s2) { JC_ASSERT(s1); return !strcmp(s1, s2.data); }
Bool operator==(Str         s1, const char* s2) { JC_ASSERT(s2); return !strcmp(s1.data, s2); }
Bool operator!=(Str         s1, Str         s2) { return s1.len != s2.len || memcmp(s1.data, s2.data, s1.len); }
Bool operator!=(const char* s1, Str         s2) { JC_ASSERT(s1); return strcmp(s1,s2.data); }
Bool operator!=(Str         s1, const char* s2) { JC_ASSERT(s2); return strcmp(s1.data, s2); }

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