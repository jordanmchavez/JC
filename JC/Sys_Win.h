#pragma once

#include "JC/Common_Err.h"

#define WIN32_LEAN_AND_MEAN
#define VC_EXTRALEAN
#include <Windows.h>

namespace JC::Sys {

//--------------------------------------------------------------------------------------------------

#define Win_LastErr(fn, ...) \
	Err::Make(Err::Err(), SrcLoc::Here(), "Win", (U64)GetLastError(), "fn", fn, ##__VA_ARGS__)

constexpr bool IsValidHandle(HANDLE h) {
	return h != (HANDLE)0 && h != INVALID_HANDLE_VALUE;
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Sys