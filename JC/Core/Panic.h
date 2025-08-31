#pragma once

#include "JC/Core.h"

namespace JC {

//--------------------------------------------------------------------------------------------------

[[noreturn]] void VPanic(SrcLoc sl, const char* expr, const char* fmt, Span<const Arg> args);


template <class... A> [[noreturn]] void Panic(SrcLoc sl, FmtStr<A...> fmt, A... args) {
	VPanic(sl, 0, fmt, { MakeArg(args)... });
}

using PanicFn = void (SrcLoc sl, const char* expr, const char* msg);
PanicFn* SetPanicFn(PanicFn* panicFn);

[[noreturn]] inline void AssertFail(SrcLoc sl, const char* expr) {
	VPanic(sl, expr, 0, {});
}

template <class... A> [[noreturn]] void AssertFail(SrcLoc sl, const char* expr, FmtStr<A...> fmt, A... args) {
	VPanic(sl, expr, fmt, { MakeArg<A>(args)... });
}

#define JC_PANIC(fmt, ...) JC::Panic(SrcLoc::Here(), fmt, ##__VA_ARGS__)

//--------------------------------------------------------------------------------------------------

#define JC_ASSERT(expr, ...) \
	do { \
		if (!(expr)) { \
			JC::AssertFail(SrcLoc::Here(), #expr, ##__VA_ARGS__); \
		} \
	} while (false)

//--------------------------------------------------------------------------------------------------

}	// namespace JC