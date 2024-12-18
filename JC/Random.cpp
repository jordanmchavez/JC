#include "JC/Random.h"

namespace JC::Random {

//--------------------------------------------------------------------------------------------------

static u64 state[2] = { 1, 2 };

void Seed(u64 s[2]) {
	state[0] = s[0];
	state[1] = s[1];
}

static u64 Rotl(u64 x, u32 k) {
	return (x << k) | (x >> (64 - k));
}

static u64 Next(u64 s[2]) {
	const u64 s0 = s[0];
	u64 s1 = s[1];
	const u64 res = s0 + s1;
	s1 ^= s0;
	s[0] = Rotl(s0, 24) ^ s1 ^ (s1 << 16);
	s[1] = Rotl(s1, 37);
	return res;
}

u64 NextU64() {
	return Next(state);
}

float NextF32() {
	return (f32)((Next(state) >> 11) * 0x1.0p-53);
}

f64 NextF64() {
	return (Next(state) >> 11) * 0x1.0p-53;
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Random