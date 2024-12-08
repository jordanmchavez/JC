#pragma once

#include "JC/Bit.h"
#include "JC/UnitTest.h"

#if defined Compiler_Msvc
	#include <intrin.h>
	#pragma intrinsic(_BitScanReverse64)
	#pragma intrinsic(_BitScanForward64)
#endif	// Compiler_

namespace JC {

//--------------------------------------------------------------------------------------------------

u32 Bsr64(u64 u) {
	u32 idx;
	_BitScanReverse64((unsigned long*)&idx, u);
	return idx;
}

u32 Bsf64(u64 u) {
	u32 idx;
	_BitScanForward64((unsigned long*)&idx, u);
	return idx;
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

	CheckEq(AlignPow2(0), 0);
	CheckEq(AlignPow2(1), 1);
	CheckEq(AlignPow2(2), 2);
	for (u64 i = 2; i < 64; i++) {
		const u64 pow2 = (u64)1 << i;
		CheckEq(AlignPow2(pow2 - 1), pow2);
		CheckEq(AlignPow2(pow2    ), pow2);
		CheckEq(AlignPow2(pow2 + 1), pow2 << 1);
	}
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC