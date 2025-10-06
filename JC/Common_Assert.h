#pragma once

#include "JC/Common_Arg.h"
#include "JC/Common_Fmt.h"

namespace JC::Asrt {

//--------------------------------------------------------------------------------------------------

[[noreturn]] void Panicv(SrcLoc sl, char const* expr, char const* fmt, Span<Arg::Arg const> args);

[[noreturn]] inline void Panicf(SrcLoc sl, char const* expr) {
	Panicv(sl, expr, nullptr, {});
}

template <class... A> [[noreturn]] void Panicf(SrcLoc sl, char const* expr, Fmt::CheckStr<A...> fmt, A... args) {
	Panicv(sl, expr, fmt, { Arg::Make(args)... });
}

using PanicFn = void (SrcLoc sl, char const* expr, char const* msg);
PanicFn* SetPanicFn(PanicFn* panicFn);

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Asrt

#define Panic(fmt, ...) Asrt::Panicf(SrcLoc::Here(), nullptr, fmt, ##__VA_ARGS__)

#define Assert(expr, ...) \
	do { if (!(expr)) { \
		Asrt::Panicf(SrcLoc::Here(), #expr, ##__VA_ARGS__); \
	} } while (false)