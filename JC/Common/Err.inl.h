#pragma once

#include "JC/Common.h"

namespace JC {

//--------------------------------------------------------------------------------------------------

template <class...A> Err::Err(SrcLoc sl, Str ns, Str code, A... args) {
	static_assert(sizeof...(A) % 2 == 0);
	Init(sl, ns, code, MakeArgs(args...));
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC