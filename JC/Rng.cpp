#include "JC/Rng.h"

namespace JC::Rng {

//--------------------------------------------------------------------------------------------------

static U64 xoroshiro128State[2] = { 1, 2 };
static U64 splitMix64State; /* The xoroshiro128State can be seeded with any value. */

static U64 SplitMix64() {
	splitMix64State += 0x9e3779b97f4a7c15;
	U64 z = splitMix64State;
	z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9;
	z = (z ^ (z >> 27)) * 0x94d049bb133111eb;
	return z ^ (z >> 31);
}

void Seed(U64 seed) {
	splitMix64State = seed;
	xoroshiro128State[0] = SplitMix64();
	xoroshiro128State[1] = SplitMix64();
}

static U64 Rotl(U64 x, U32 k) {
	return (x << k) | (x >> (64 - k));
}

static U64 Next() {
	U64 const s0 = xoroshiro128State[0];
	U64 s1 = xoroshiro128State[1];
	U64 const res = s0 + s1;
	s1 ^= s0;
	xoroshiro128State[0] = Rotl(s0, 24) ^ s1 ^ (s1 << 16);
	xoroshiro128State[1] = Rotl(s1, 37);
	return res;
}

U32 NextU32() {
	return (U32)Next();
}

U32 NextU32(U32 min, U32 max) {
	return min + ((U32)Next() % (max - min));
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