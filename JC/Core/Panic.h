#pragma once

#include "JC/Core.h"

namespace JC {

//--------------------------------------------------------------------------------------------------

[[noreturn]] void VPanic(const char* expr, const char* msg, Span<Arg> args, SrcLoc sl);

template <class... A> [[noreturn]] void Panic(SrcLocWrapper<const char*> msgSl, A... args) {
	static_assert(sizeof...(A) % 2 == 0);
	VPanic(0, msgSl.val, Span<Arg>({ MakeArg(args)..., }), msgSl.sl);
}

[[noreturn]] inline void _AssertFail(SrcLoc sl, const char* expr) {
	VPanic(expr, "",  Span<Arg>{}, sl);
}
template <class... A> [[noreturn]] void _AssertFail(SrcLoc sl, const char* expr, SrcLocWrapper<const char*> msgSl, A... args) {
	static_assert(sizeof...(A) % 2 == 0);
	VPanic(expr, msgSl.val, Span<Arg>(MakeArg(args)...), msgSl.sl);
}

using PanicFn = void (const char* expr, const char* fmt, Span<Arg> args, SrcLoc sl);

PanicFn* SetPanicFn(PanicFn* panicFn);

#define Assert(expr, ...) \
	do { \
		if (!(expr)) { \
			_AssertFail(SrcLoc::Here(), #expr, ##__VA_ARGS__); \
		} \
	} while (false)

//--------------------------------------------------------------------------------------------------

}	// namespace JC