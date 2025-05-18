#pragma once

#include "JC/Core.h"

namespace JC {

//--------------------------------------------------------------------------------------------------

[[noreturn]] void VPanic(const char* expr, const char* msg, Span<const NamedArg> namedArgs, SrcLoc sl);

template <class... A> [[noreturn]] void Panic(SrcLocWrapper<const char*> msgSl, A... args) {
	NamedArg namedArgs[1 + sizeof...(A) / 2];
	BuildNamedArgs(namedArgs, args...);
	VPanic(0, msgSl.val, Span<const NamedArg>(namedArgs, sizeof...(A) / 2), msgSl.sl);
}

[[noreturn]] inline void _AssertFail(const char* expr, SrcLoc sl) {
	VPanic(expr, "",  {}, sl);
}
template <class... A> [[noreturn]] void _AssertFail(const char* expr, const char* msg, A... args, SrcLoc sl) {
	NamedArg namedArgs[1 + sizeof...(A) / 2];
	BuildNamedArgs(namedArgs, args...);
	VPanic(expr, msg, Span<const NamedArg>(namedArgs, sizeof...(A) / 2), sl);
}

using PanicFn = void (const char* expr, const char* fmt, Span<const NamedArg> namedArgs, SrcLoc sl);

PanicFn* SetPanicFn(PanicFn* panicFn);

#define Assert(expr, ...) \
	do { \
		if (!(expr)) { \
			_AssertFail(#expr, ##__VA_ARGS__, SrcLoc::Here()); \
		} \
	} while (false)

//--------------------------------------------------------------------------------------------------

}	// namespace JC