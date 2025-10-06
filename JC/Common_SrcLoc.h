#pragma once

#include "JC/Common_Platform.h"
#include "JC/Common_Std.h"

namespace JC {

//--------------------------------------------------------------------------------------------------

struct SrcLoc {
	char const* file;
	U32         line;

#if defined Compiler_Msvc
	static consteval SrcLoc Here(char const* file = __builtin_FILE(), U32 line = (U32)__builtin_LINE()) {
#endif	// Compiler
		return SrcLoc { .file = file, .line = line };
	}
};

//--------------------------------------------------------------------------------------------------

}	// namespace JC;