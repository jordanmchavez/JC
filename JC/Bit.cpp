#pragma once

#include "JC/Bit.h"
#include "JC/UnitTest.h"

#if defined Compiler_Msvc
	#include <intrin.h>
	#pragma intrinsic(_BitScanReverse64)
	#pragma intrinsic(_BitScanForward64)
	#pragma intrinsic(__popcnt64)
#endif	// Compiler_

namespace JC::Bit {

//--------------------------------------------------------------------------------------------------

U32 Bsr64(U64 u) {
	#if defined Compiler_Msvc
		U32 idx;
		_BitScanReverse64((unsigned long*)&idx, u);
		return idx;
	#endif	// Compiler_
}

U32 Bsf64(U64 u) {
	#if defined Compiler_Msvc
		U32 idx;
		_BitScanForward64((unsigned long*)&idx, u);
		return idx;
	#endif	// Compiler_Msvc_
}

//--------------------------------------------------------------------------------------------------

U32 PopCount32(U32 u) {
	#if defined Compiler_Msvc
		return (U32)__popcnt(u);
	#endif	// Compiler_
}

U32 PopCount64(U64 u) {
	#if defined Compiler_Msvc
		return (U32)__popcnt64(u);
	#endif	// Compiler_
}

//--------------------------------------------------------------------------------------------------

UnitTest("Bit") {
	CheckEq(Bsf64(0),                  0u);
	CheckEq(Bsr64(0),                  0u);
	CheckEq(Bsf64(1),                  0u);
	CheckEq(Bsr64(1),                  0u);
	CheckEq(Bsf64(0x80000000),         31u);
	CheckEq(Bsf64(0x80008000),         15u);
	CheckEq(Bsr64(0x80000008),         31u);
	CheckEq(Bsr64(0x7fffffff),         30u);
	CheckEq(Bsr64(0x80000000),         31u);
	CheckEq(Bsr64(0x100000000),        32u);
	CheckEq(Bsr64(0xffffffffffffffff), 63u);

	CheckEq(PopCount64(0), 0u);
	CheckEq(PopCount64(1), 1u);
	CheckEq(PopCount64((U64)0b10101010), 4u);
	CheckEq(PopCount64((U64)0xffffffffffffffff), 64u);

	CheckEq(AlignUp(0, 0), 0);
	CheckEq(AlignUp(0, 8), 0);
	CheckEq(AlignUp(1, 8), 8);
	CheckEq(AlignUp(7, 8), 8);
	CheckEq(AlignUp(8, 8), 8);
	CheckEq(AlignUp(9, 8), 16);

	CheckEq(AlignPow2(0), 0);
	CheckEq(AlignPow2(1), 1);
	CheckEq(AlignPow2(2), 2);
	for (U64 i = 2; i < 64; i++) {
		const U64 pow2 = (U64)1 << i;
		CheckEq(AlignPow2(pow2 - 1), pow2);
		CheckEq(AlignPow2(pow2    ), pow2);
		CheckEq(AlignPow2(pow2 + 1), pow2 << 1);
	}
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Bit