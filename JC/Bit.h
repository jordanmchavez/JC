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

constexpr u64   AlignUp   (u64   u, u64 align) { return (u + (align - 1)) & ~(align - 1); }
constexpr u64   AlignDown (u64   u, u64 align) { return u & ~(align - 1); }
constexpr void* AlignPtrUp(void* p, u64 align) { return (void*)AlignUp((u64)p, align); }

constexpr u64 AlignPow2(u64 u) {
	if (!(u & (u - 1))) {
		return u;
	}
	u |= u >>  1;
	u |= u >>  2;
	u |= u >>  4;
	u |= u >>  8;
	u |= u >> 16;
	u |= u >> 32;
	return u + 1;
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC