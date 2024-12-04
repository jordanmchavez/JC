#pragma once

#include "JC/Common.h"

namespace JC {

struct Err;
struct Mem;

//--------------------------------------------------------------------------------------------------

s8 MakeWinErrorDesc(u32 code);

template <class... A> Err* _MakeWinErr (SrcLoc sl, u32 code, s8 fn, A... args) { return _MakeErr(0, sl, ec, "fn", fn, "desc", MakeWinErrorDesc(code),           args...); }
template <class... A> Err* _MakeLastErr(SrcLoc sl,           s8 fn, A... args) { return _MakeErr(0, sl, ec, "fn", fn, "desc", MakeWinErrorDesc(GetLastError()), args...); }

#define MakeWinErr( fn, code, ...) _MakeWinErr (SrcLoc::Here(), code, #fn, ##__VA_ARGS__)
#define MakeLastErr(fn,       ...) _MakeLastErr(SrcLoc::Here(),       #fn, ##__VA_ARGS__)

//--------------------------------------------------------------------------------------------------

}	// namespace JC