#pragma once

#include "JC/Common.h"

#if defined JC_COMPILER_MSVC
	#include <intrin.h>
	#pragma intrinsic(_BitScanReverse64)
	#pragma intrinsic(_BitScanForward64)
#else
	#error("Unsupported compiler")
#endif	// JC_COMPILER

namespace JC {

//--------------------------------------------------------------------------------------------------

inline u32 Bsr64(u64 u)
{
	u32 idx;
	_BitScanReverse64((unsigned long*)&idx, u);
	return idx;
}

inline u32 Bsf64(u64 u)
{
	u32 idx;
	_BitScanForward64((unsigned long*)&idx, u);
	return idx;
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC