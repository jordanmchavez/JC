#pragma once

#include "JC/Common.h"

namespace JC {

//--------------------------------------------------------------------------------------------------

struct SrcLoc {
	Str file = {};
	u32 line = 0;

	static consteval SrcLoc Here(Str file = BuiltinFile, u32 line = BuiltinLine) {
		return SrcLoc { .file = file, .line = line };
	}
};

//--------------------------------------------------------------------------------------------------

}	// namespace JC