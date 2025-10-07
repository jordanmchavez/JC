#include "JC/Rng.h"

namespace JC::Rng {

//--------------------------------------------------------------------------------------------------

static U64 state[2] = { 1, 2 };

void Seed(U64 s[2]) {
	state[0] = s[0];
	state[1] = s[1];
}

static U64 Rotl(U64 x, U32 k) {
	return (x << k) | (x >> (64 - k));
}

static U64 Next() {
	const U64 s0 = state[0];
	U64 s1 = state[1];
	const U64 res = s0 + s1;
	s1 ^= s0;
	state[0] = Rotl(s0, 24) ^ s1 ^ (s1 << 16);
	state[1] = Rotl(s1, 37);
	return res;
}

U32 NextU32() {
	return (U32)Next();
}

U64 NextU64() {
	return Next();
}

F32 NextF32() {
	return (F32)((Next() >> 11) * 0x1.0p-53);
}

F64 NextF64() {
	return (Next() >> 11) * 0x1.0p-53;
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Rng