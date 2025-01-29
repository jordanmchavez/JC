#pragma once

#include "JC/Core.h"

namespace JC {

//--------------------------------------------------------------------------------------------------

[[noreturn]] void VPanic(const char* expr, const char* msg, Span<NamedArg> namedArgs, SrcLoc sl);

template <class... A> [[noreturn]] void Panic(SrcLocWrapper<const char*> msgSl, A... args) {
	NamedArg namedArgs[sizeof...(A) / 2 + 1];
	BuildNamedArgs(namedArgs, args...);
	VPanic(0, msgSl.val, Span<NamedArg>(namedArgs, sizeof...(A) / 2), msgSl.sl);
}

[[noreturn]] inline void _AssertFail(const char* expr, SrcLoc sl) {
	VPanic(expr, "",  Span<NamedArg>{}, sl);
}
template <class... A> [[noreturn]] void _AssertFail(const char* expr, const char* msg, A... args, SrcLoc sl) {
	NamedArg namedArgs[sizeof...(A) / 2 + 1];
	BuildNamedArgs(namedArgs, args...);
	VPanic(expr, msg, Span<NamedArg>(namedArgs, sizeof...(A) / 2), sl);
}

using PanicFn = void (const char* expr, const char* fmt, Span<NamedArg> namedArgs, SrcLoc sl);

PanicFn* SetPanicFn(PanicFn* panicFn);

#define Assert(expr, ...) \
	do { \
		if (!(expr)) { \
			_AssertFail(#expr, ##__VA_ARGS__, SrcLoc::Here()); \
		} \
	} while (false)

//--------------------------------------------------------------------------------------------------

}	// namespace JC