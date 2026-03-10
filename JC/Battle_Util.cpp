#include "JC/Battle_Internal.h"

#include "JC/Math.h"

namespace JC::Battle {

//--------------------------------------------------------------------------------------------------

static U8 hexLut[(HexSize / 2) * (HexSize * 3 / 8)];

//--------------------------------------------------------------------------------------------------

void InitUtil() {
	memset(hexLut, 0, sizeof(hexLut));
	U8 start = (HexSize / 2) - 1;
	U8 end   = HexSize / 2;
	for (U8 y = 0; y < HexSize / 4; y++) {
		for (U8 x = 0; x < start; x++) {
			hexLut[((y / 2) * (HexSize / 2)) + (x / 2)] |= (1 << ((x & 1) * 4));
		}
		for (U8 x = end + 1; x < HexSize; x++) {
			hexLut[((y / 2) * (HexSize / 2)) + (x / 2)] |= (2 << ((x & 1) * 4));
		}
		start -= 2;
		end   += 2;
	}
}

//--------------------------------------------------------------------------------------------------

bool AreHexesAdjacent(Hex const* a, Hex const* b) {
	return
		a->neighbors[0] == b ||
		a->neighbors[1] == b ||
		a->neighbors[2] == b ||
		a->neighbors[3] == b ||
		a->neighbors[4] == b ||
		a->neighbors[5] == b
	;
}

//--------------------------------------------------------------------------------------------------

Vec2 ColRowToTopLeftWorldPos(I32 c, I32 r) {
	return {
		(F32)(c * HexSize + (r & 1) * (HexSize / 2)),
		(F32)(r * (HexSize * 3 / 4)),
	};
}

//--------------------------------------------------------------------------------------------------

U32 HexDistance(Hex const* a, Hex const* b) {
	I32 const aq = a->c - (a->r - (a->r & 1)) / 2;
	I32 const as = a->r;
	I32 const bq = b->c - (b->r - (b->r & 1)) / 2;
	I32 const bs = b->r;
	I32 const dq = bq - aq;
	I32 const ds = bs - as;
	I32 const dqSign = dq < 0 ? -1 : 1;
	I32 const dsSign = ds < 0 ? -1 : 1;
	if (dqSign == dsSign) {
		return Math::Abs(dq) + Math::Abs(ds);
	} else {
		return Max(Math::Abs(dq), Math::Abs(ds));
	}
}

//--------------------------------------------------------------------------------------------------

Hex* WorldPosToHex(Data* data, Vec2 p) {
	I32 const hsize   = (I32)HexSize;
	I32 const rowStep = hsize * 3 / 4;
	I32 const iy      = (I32)p.y;
	I32 r = iy / rowStep - (iy % rowStep != 0 && iy < 0 ? 1 : 0);	// floor division
	U8  const parity  = (U8)(r & 1);

	I32 const ix  = (I32)p.x - (I32)parity * (hsize / 2);
	I32 c = ix / hsize - (ix % hsize != 0 && ix < 0 ? 1 : 0);	// floor division

	I32 const lx = ix - c * hsize;
	I32 const ly = iy - r * rowStep;
	Assert(lx >= 0 && lx < hsize && ly >= 0 && ly < rowStep);

	U8 const l = (hexLut[(ly / 2) * (hsize / 2) + (lx / 2)] >> ((lx & 1) * 4)) & 0xf;
	switch (l) {
		case 1: c -= 1 - parity; r -= 1; break;
		case 2: c += parity;     r -= 1; break;
	}

	if (c >= 0 && c < MaxCols && r >= 0 && r < MaxRows) {
		return &data->hexes[c + (r * MaxCols)];
	}
	return nullptr;
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Battle