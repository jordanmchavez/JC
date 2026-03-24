#pragma once

#include "JC/Common.h"

namespace JC::Draw { DefHandle(Sprite); }

namespace JC::Unit {

//--------------------------------------------------------------------------------------------------

Res<> Init(Mem permMem, Mem tempMem);
Res<> Load(Str path);

inline U32 Damage(U32 base) {
	U32 lo10    = base * 8;				// Fixed point 10
	U32 hi10    = base * 12;			// Fixed point 10
	U32 lo      = lo10 / 10;			// Damage floor
	U32 hi      = (hi10 + 9) / 10;		// Damage ceiling
	U32 loWidth = (lo + 1) * 10 - lo10;	// Width of low bucket
	U32 hiWidth = hi10 - (hi - 1) * 10;	// Width of high bucket

	U32 total = loWidth + hiWidth + (hi - lo - 1) * 10;
	U32 roll = Rng::NextU32(0, total - 1);
	if (roll < loWidth) { return lo; }
	roll -= loWidth;
	U32 interior = roll / 10;
	if (interior < (hi - lo - 1)) { return lo + 1 + interior; }
	return hi;
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Unit