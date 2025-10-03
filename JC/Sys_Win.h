#pragma once

#include "JC/Common.h"

#define WIN32_LEAN_AND_MEAN
#define VC_EXTRALEAN
#include <Windows.h>

//--------------------------------------------------------------------------------------------------
/*
template <class... A> struct [[nodiscard]] Err_WinLast : Err {
	Err_WinLast(Str fn, A... args, SrcLoc sl = SrcLoc::Here())
		: Err(Err(), sl, "Win", (U64)GetLastError(), "fn", fn, args...)
		{}
};
template <typename... A> Err_WinLast(Str, A...) -> Err_WinLast<A...>;

template <class... A> struct [[nodiscard]] Err_Win : JC::Err {
	Err_Win(U32 code, Str fn, A... args, SrcLoc sl = SrcLoc::Here())
		: Err(Err(), sl, "Win", code, "fn", fn, args...)
		{}
};
template <typename... A> Err_Win(U32, Str, A...) -> Err_Win<A...>;

constexpr Bool IsValidHandle(HANDLE h) {
	return h != (HANDLE)0 && h != INVALID_HANDLE_VALUE;
}
*/