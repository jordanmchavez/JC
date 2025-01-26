#pragma once

#include "JC/Common.h"

namespace JC {

//--------------------------------------------------------------------------------------------------

                      [[noreturn]] void VPanic(SrcLoc sl, Str expr, Str fmt, VArgs vargs);
template <class... A> [[noreturn]] void Panic(FmtStrSrcLoc<A...> fmtSl, A... args) { VPanic(fmtSl.sl, 0, fmtSl.fmt, MakeVArgs(args...)); }

                      [[noreturn]] inline void _AssertFail(SrcLoc sl, Str expr)                              { VPanic(sl, expr, "",   MakeVArgs()); }
template <class... A> [[noreturn]]        void _AssertFail(SrcLoc sl, Str expr, FmtStr<A...> fmt, A... args) { VPanic(sl, expr, fmt,  MakeVArgs(args...)); }

using PanicFn = void (SrcLoc sl, Str expr, Str fmt, VArgs vargs);

PanicFn* SetPanicFn(PanicFn* panicFn);

#define Assert(expr, ...) \
	do { \
		if (!(expr)) { \
			_AssertFail(SrcLoc::Here(), #expr, ##__VA_ARGS__); \
		} \
	} while (false)

//--------------------------------------------------------------------------------------------------

}	// namespace JC