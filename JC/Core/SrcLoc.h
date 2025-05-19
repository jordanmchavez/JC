#pragma once

#include "JC/Core.h"

namespace JC {

//--------------------------------------------------------------------------------------------------

struct SrcLoc {
	const char* file = {};
	U32         line = 0;

	static consteval SrcLoc Here(const char* file = BuiltinFile, U32 line = BuiltinLine) {
		return SrcLoc { .file = file, .line = line };
	}
};

template <class T> struct SrcLocWrapper {
	T      val;
	SrcLoc sl;

	SrcLocWrapper(T v, SrcLoc slIn = SrcLoc::Here()) { val = v; sl = slIn; }
};

//--------------------------------------------------------------------------------------------------

}	// namespace JC