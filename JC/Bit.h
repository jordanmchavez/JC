#pragma once

#include "JC/Common.h"

#if defined Compiler_Msvc
	#include <intrin.h>
	#pragma intrinsic(_BitScanReverse64)
	#pragma intrinsic(_BitScanForward64)
#else	// Compiler_
	#error("Unsupported compiler")
#endif	// Compiler_

namespace JC {

//--------------------------------------------------------------------------------------------------

u32 Bsr64(u64 u);
u32 Bsf64(u64 u);

//--------------------------------------------------------------------------------------------------

}	// namespace JC