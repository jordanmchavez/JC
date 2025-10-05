#include "JC/Rng.h"

//--------------------------------------------------------------------------------------------------

static U64 rng_state[2] = { 1, 2 };

void Rng_Seed(U64 s[2]) {
	rng_state[0] = s[0];
	rng_state[1] = s[1];
}

static U64 Rng_Rotl(U64 x, U32 k) {
	return (x << k) | (x >> (64 - k));
}

static U64 Rng_Next() {
	const U64 s0 = rng_state[0];
	U64 s1 = rng_state[1];
	const U64 res = s0 + s1;
	s1 ^= s0;
	rng_state[0] = Rng_Rotl(s0, 24) ^ s1 ^ (s1 << 16);
	rng_state[1] = Rng_Rotl(s1, 37);
	return res;
}

U32 Rng_NextU32() {
	return (U32)Rng_Next();
}

U64 Rng_NextU64() {
	return Rng_Next();
}

F32 Rng_NextF32() {
	return (F32)((Rng_Next() >> 11) * 0x1.0p-53);
}

F64 Rng_NextF64() {
	return (Rng_Next() >> 11) * 0x1.0p-53;
}