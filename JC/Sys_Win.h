#pragma once

#include "JC/Core.h"

#define WIN32_LEAN_AND_MEAN
#define VC_EXTRALEAN
#include <Windows.h>

namespace JC {

//--------------------------------------------------------------------------------------------------

#define JC_WIN_ERR(fn, ...) \
	Err::Make(0, SrcLoc::Here(), "Win", (U64)GetLastError(), "fn", fn, ##__VA_ARGS__)

constexpr Bool IsValidHandle(HANDLE h) {
	return h != (HANDLE)0 && h != INVALID_HANDLE_VALUE;
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC