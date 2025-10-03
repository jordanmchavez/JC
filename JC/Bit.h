#pragma once

#include "JC/Common.h"

//--------------------------------------------------------------------------------------------------

U32 Bsr64(U64 u);
U32 Bsf64(U64 u);

U32 PopCount32(U32 u);
U32 PopCount64(U64 u);

constexpr U64   AlignUp   (U64   u, U64 align) { return (u + (align - 1)) & ~(align - 1); }
constexpr void* AlignPtrUp(void* p, U64 align) { return (void*)AlignUp((U64)p, align); }

constexpr U64 AlignPow2(U64 u) {
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

constexpr Bool IsPow2(U64 u) { return (u & (u - 1)) == 0; }