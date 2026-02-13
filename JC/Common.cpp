#include "JC/Common.h"
#include "JC/Sys.h"

namespace JC {

//--------------------------------------------------------------------------------------------------

Str::Str(char const* s, U64 l) { Assert(s || l == 0); data = s; len = l; }

bool operator==(Str s1, Str s2) { return s1.len == s2.len && !memcmp(s1.data, s2.data, s1.len); }
bool operator!=(Str s1, Str s2) { return s1.len != s2.len ||  memcmp(s1.data, s2.data, s1.len); }

//--------------------------------------------------------------------------------------------------

static PanicFn* panicFn;

PanicFn* SetPanicFn(PanicFn* newPanicFn) {
	PanicFn* oldPanicFn = panicFn;
	panicFn = newPanicFn;
	return oldPanicFn;
}

[[noreturn]] void Panicv(SrcLoc sl, char const* expr, char const* fmt, Span<Arg const> args) {
	static bool recursive = false;
	if (recursive) {
		if (Sys::DbgPresent()) {
			DbgBreak;
		}
		Sys::Abort();
	}
	recursive = true;

	if (panicFn) {
		char msg[2048] = {};
		if (fmt) {
			*SPrintv(msg, msg + LenOf(msg) - 1, fmt, args) = '\0';
		}
		panicFn(sl, expr, msg);
	} else {
		if (Sys::DbgPresent()) {
			DbgBreak;
		}
		Sys::Abort();
	}
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC