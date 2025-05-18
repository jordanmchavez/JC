#pragma once

#include "JC/Core.h"

#define WIN32_LEAN_AND_MEAN
#define VC_EXTRALEAN
#include <Windows.h>

namespace JC {

//--------------------------------------------------------------------------------------------------

template <class... A> struct [[nodiscard]] Err_WinLast : Err {
	static_assert(sizeof...(A) % 2 == 0);
	Err_WinLast(Str fn, A... args, SrcLoc sl = SrcLoc::Here()) {
		NamedArg namedArgs[1 + (sizeof...(A) / 2)];
		BuildNamedArgs(namedArgs, "fn", fn, args...);
		Init("Win", (u64)GetLastError(), Span<const NamedArg>(namedArgs, 1 + (sizeof...(A) / 2)), sl);
	}
};
template <typename... A> Err_WinLast(Str, A...) -> Err_WinLast<A...>;

template <class... A> struct [[nodiscard]] Err_Win : JC::Err {
	static_assert(sizeof...(A) % 2 == 0);
	Err_Win(u32 code, Str fn, A... args, SrcLoc sl = SrcLoc::Here()) : Err(sl, "win", "", code, "fn", fn, args...) {}
};
template <typename... A> Err_Win(u32, Str, A...) -> Err_Win<A...>;

constexpr bool IsValidHandle(HANDLE h) {
	return h != (HANDLE)0 && h != INVALID_HANDLE_VALUE;
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC