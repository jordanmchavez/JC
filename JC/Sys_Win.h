#pragma once

#include "JC/Common.h"

namespace JC {

struct Mem;

//--------------------------------------------------------------------------------------------------

s8 MakeWinErrorDesc(Mem* mem, u32 code);

template <class... A>
Err* _MakeWinErr(SrcLoc sl, s8 fn, A... args) {
	return _MakeErr(0, sl, ec, "fn", fn, "desc", MakeWinErrorDesc(, args...);
}

#define MakeWinErr(fn, ...) \
	_MakeWinErr(SrcLoc::Here(), #fn, ##__VA_ARGS__)

//--------------------------------------------------------------------------------------------------

}	// namespace JC