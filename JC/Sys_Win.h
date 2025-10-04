#pragma once

#include "JC/Common.h"

#define WIN32_LEAN_AND_MEAN
#define VC_EXTRALEAN
#include <Windows.h>

//--------------------------------------------------------------------------------------------------

#define Win_LastErr(fn, ...) \
	Err_Make(Err(), SrcLoc::Here(), "Win", (U64)GetLastError(), "fn", fn, ##__VA_ARGS__)

constexpr Bool Win_IsValidHandle(HANDLE h) {
	return h != (HANDLE)0 && h != INVALID_HANDLE_VALUE;
}